void CEbenezerDlg::InitServerCommands()
{
	static Command<CEbenezerDlg> commandTable[] = 
	{
		// Command				Handler											Help message
		{ "notice",				&CEbenezerDlg::HandleNoticeCommand,				"Sends a server-wide chat notice." },
		{ "kill",				&CEbenezerDlg::HandleKillUserCommand,			"Disconnects the specified player" },
		{ "open1",				&CEbenezerDlg::HandleWar1OpenCommand,			"Opens war zone 1" },
		{ "open2",				&CEbenezerDlg::HandleWar2OpenCommand,			"Opens war zone 2" },
		{ "open3",				&CEbenezerDlg::HandleWar3OpenCommand,			"Opens war zone 3" },
		{ "snowopen",			&CEbenezerDlg::HandleSnowWarOpenCommand,		"Opens the snow war zone" },
		{ "close",				&CEbenezerDlg::HandleWarCloseCommand,			"Closes the active war zone" },
		{ "down",				&CEbenezerDlg::HandleShutdownCommand,			"Shuts down the server" },
		{ "pause",				&CEbenezerDlg::HandlePauseCommand,				"Prevents users from connecting to the server" },
		{ "resume",				&CEbenezerDlg::HandleResumeCommand,				"Allows users to resume connecting to the server" },
		{ "discount",			&CEbenezerDlg::HandleDiscountCommand,			"Enables server discounts for the winning nation of the last war" },
		{ "alldiscount",		&CEbenezerDlg::HandleGlobalDiscountCommand,		"Enables server discounts for everyone" },
		{ "offdiscount",		&CEbenezerDlg::HandleDiscountOffCommand,		"Disables server discounts" },
		{ "captain",			&CEbenezerDlg::HandleCaptainCommand,			"Sets the captains/commanders for the war" },
		{ "santa",				&CEbenezerDlg::HandleSantaCommand,				"Enables a flying Santa Claus." },
		{ "offsanta",			&CEbenezerDlg::HandleSantaOffCommand,			"Disables a flying Santa Claus/angel." },
		{ "angel",				&CEbenezerDlg::HandleAngelCommand,				"Enables a flying angel." },
		{ "offangel",			&CEbenezerDlg::HandleSantaOffCommand,			"Disables a flying Santa Claus/angel." },
		{ "permanent",			&CEbenezerDlg::HandlePermanentChatCommand,		"Sets the permanent chat bar to the specified text." },
		{ "offpermanent",		&CEbenezerDlg::HandlePermanentChatOffCommand,	"Resets the permanent chat bar text." },
		{ "reload_notice",		&CEbenezerDlg::HandleReloadNoticeCommand,		"Reloads the in-game notice list." },
	};

	init_command_table(CEbenezerDlg, commandTable, s_commandTable);
}

void CEbenezerDlg::CleanupServerCommands() { free_command_table(s_commandTable); }

void CUser::InitChatCommands()
{
	static Command<CUser> commandTable[] = 
	{
		// Command				Handler											Help message
		{ "test",				&CUser::HandleTestCommand,						"Test command" },
		{ "give_item",			&CUser::HandleGiveItemCommand,					"Gives a player an item. Arguments: character name | item ID | [optional stack size]" },
		{ "zonechange",			&CUser::HandleZoneChangeCommand,				"Teleports you to the specified zone. Arguments: zone ID" },
		{ "open1",				&CUser::HandleWar1OpenCommand,					"Opens war zone 1" },
		{ "open2",				&CUser::HandleWar2OpenCommand,					"Opens war zone 2" },
		{ "open3",				&CUser::HandleWar3OpenCommand,					"Opens war zone 3" },
		{ "snowopen",			&CUser::HandleSnowWarOpenCommand,				"Opens the snow war zone" },
		{ "close",				&CUser::HandleWarCloseCommand,					"Closes the active war zone" },

	};

	init_command_table(CUser, commandTable, s_commandTable);
}

void CUser::CleanupChatCommands() { free_command_table(s_commandTable); }

void CUser::Chat(Packet & pkt)
{
	Packet result;
	uint16 sessID;
	uint8 type = pkt.read<uint8>(), bNation;
	string chatstr, finalstr, strSender, * strMessage;

	bool isAnnouncement = false;

	if (isMuted())
		return;	

	pkt >> chatstr;
	if (chatstr.empty() || chatstr.size() > 128)
		return;

	// Process GM commands
	if (isGM() && ProcessChatCommand(chatstr))
		return;

#if 0 // Removed this - all it seems to do is cause chat to break for GMs (is it 19xx+ only?)
	if( isGM() && type == GENERAL_CHAT)
		type = 0x14;
#endif

	// Handle GM notice & announcement commands
	if (type == PUBLIC_CHAT || type == ANNOUNCEMENT_CHAT)
	{
		// Trying to use a GM command without authorisation? Bad player!
		if (!isGM())
			return;

		if (type == ANNOUNCEMENT_CHAT)
			type = WAR_SYSTEM_CHAT;

		// This is horrible, but we'll live with it for now.
		// Pull the notice string (#### NOTICE : %s ####) from the database.
		// Format the chat string around it, so our chat data is within the notice
		g_pMain->GetServerResource(IDP_ANNOUNCEMENT, &finalstr, chatstr.c_str());
		isAnnouncement = true;
	}


	if (isAnnouncement)
	{
		// GM notice/announcements show no name, so don't bother setting it.
		strMessage = &finalstr; // use the formatted message from the user
		bNation = KARUS; // arbitrary nation
		sessID = -1;
	}
	else
	{
		strMessage = &chatstr; // use the raw message from the user
		strSender = GetName(); // everything else uses a name, so set it
		bNation = GetNation();
		sessID = GetSocketID();
	}

	ChatPacket::Construct(&result, type, strMessage, &strSender, bNation, sessID);

	switch (type) 
	{
	case GENERAL_CHAT:
		g_pMain->Send_NearRegion(&result, GetMap(), GetRegionX(), GetRegionZ(), GetX(), GetZ());
		break;

	case PRIVATE_CHAT:
	{
		CUser *pUser = g_pMain->GetUserPtr(m_sPrivateChatUser);
		if (pUser != nullptr) 
			pUser->Send(&result);
	} break;

	case PARTY_CHAT:
		if (isInParty())
			g_pMain->Send_PartyMember(m_sPartyIndex, &result);
		break;

	case SHOUT_CHAT:
		if (m_sMp < (m_iMaxMp / 5))
			break;

		// Characters under level 35 require 3,000 coins to shout.
		if (!isGM()
			&& GetLevel() < 35
			&& !GoldLose(SHOUT_COIN_REQUIREMENT))
			break;

		MSpChange(-(m_iMaxMp / 5));
		SendToRegion(&result);
		break;

	case KNIGHTS_CHAT:
		if (isInClan())
			g_pMain->Send_KnightsMember(GetClanID(), &result);
		break;
	case PUBLIC_CHAT:
	case ANNOUNCEMENT_CHAT:
		if (isGM())
			g_pMain->Send_All(&result);
		break;
	case COMMAND_CHAT:
		if (getFame() == COMMAND_CAPTAIN)
			g_pMain->Send_CommandChat(&result, m_bNation, this);
		break;
	case MERCHANT_CHAT:
		if (isMerchanting())
			SendToRegion(&result);
		break;
	case ALLIANCE_CHAT:
		if (isInClan())
		{
			CKnights *pKnights = g_pMain->GetClanPtr(GetClanID());
			if (pKnights != nullptr && pKnights->m_sAlliance > 0)
				g_pMain->Send_KnightsAlliance(pKnights->m_sAlliance, &result);
		}
		break;
	case WAR_SYSTEM_CHAT:
		if (isGM())
			g_pMain->Send_All(&result);
		break;
	}
}

void CUser::ChatTargetSelect(Packet & pkt)
{
	uint8 type = pkt.read<uint8>();

	// TO-DO: Replace this with an enum
	// Attempt to find target player in-game
	if (type == 1)
	{
		Packet result(WIZ_CHAT_TARGET, type);
		std::string strUserID;
		pkt >> strUserID;
		if (strUserID.empty() || strUserID.size() > MAX_ID_SIZE)
			return;

		CUser *pUser = g_pMain->GetUserPtr(strUserID, TYPE_CHARACTER);
		if (pUser == nullptr || pUser == this)
		{
			result << int16(0); 
		}
		else if (pUser->isBlockingPrivateChat())
		{
			result << int16(-1);
		}
		else
		{
			m_sPrivateChatUser = pUser->GetSocketID();
			result << int16(1) << pUser->GetName();
		}
		Send(&result);
	}
	else if (type == 3)
	{
		//Questions
		uint8 unk1;
		string sMessage;
		pkt >> unk1 >> sMessage;

	}
	// Allow/block PMs
	else
	{
		m_bBlockPrivateChat = pkt.read<bool>(); 
	}
}

/**
 * @brief	Sends a notice to all users in the current zone
 * 			upon death.
 *
 * @param	pKiller	The killer.
 */
void CUser::SendDeathNotice(CUser * pKiller, DeathNoticeType noticeType)
{
	if (pKiller == nullptr)
		return;

	Packet result(WIZ_CHAT, uint8(DEATH_NOTICE));

	result.SByte();
	result	<< GetNation()
			<< uint8(noticeType)
			<< pKiller->GetID() // session ID?
			<< pKiller->GetName()
			<< GetID() // session ID?
			<< GetName()
			<< uint16(GetX()) << uint16(GetZ());

	SendToRegion(&result); // TO-DO: Should send to all users in a zone.
}

bool CUser::ProcessChatCommand(std::string & message)
{
	// Commands require at least 2 characters
	if (message.size() <= 1
		// If the prefix isn't correct
		|| message[0] != CHAT_COMMAND_PREFIX
		// or if we're saying, say, ++++ (faster than looking for the command in the map)
		|| message[1] == CHAT_COMMAND_PREFIX)
		// we're not a command.
		return false;

	// Split up the command by spaces
	CommandArgs vargs = StrSplit(message, " ");
	std::string command = vargs.front(); // grab the first word (the command)
	vargs.pop_front(); // remove the command from the argument list

	// Make the command lowercase, for 'case-insensitive' checking.
	STRTOLOWER(command);

	// Command doesn't exist
	ChatCommandTable::iterator itr = s_commandTable.find(command.c_str() + 1); // skip the prefix character
	if (itr == s_commandTable.end())
		return false;

	// Run the command
	return (this->*(itr->second->Handler))(vargs, message.c_str() + command.size() + 1, itr->second->Help);
}

COMMAND_HANDLER(CUser::HandleTestCommand)
{
	return true;
}

COMMAND_HANDLER(CUser::HandleGiveItemCommand)
{
	// Char name | item ID | [stack size]
	if (vargs.size() < 2)
	{
		// send description
		return true;
	}

	std::string strUserID = vargs.front();
	vargs.pop_front();

	CUser *pUser = g_pMain->GetUserPtr(strUserID, TYPE_CHARACTER);
	if (pUser == nullptr)
	{
		// send error message saying the character does not exist or is not online
		return true;
	}

	uint32 nItemID = atoi(vargs.front().c_str());
	vargs.pop_front();
	_ITEM_TABLE *pItem = g_pMain->GetItemPtr(nItemID);
	if (pItem == nullptr)
	{
		// send error message saying the item does not exist
		return true;
	}

	uint16 sCount = 1;
	if (!vargs.empty())
		sCount = atoi(vargs.front().c_str());


	if (!pUser->GiveItem(nItemID, sCount))
	{
		// send error message saying the item couldn't be added
	}

	return true;
}

COMMAND_HANDLER(CUser::HandleZoneChangeCommand)
{
	if (vargs.empty())
	{
		// send description
		return true;
	}

	// Behave as in official (we'll fix this later)
	int nZoneID = atoi(vargs.front().c_str());
	ZoneChange(nZoneID, m_curx, m_curz);
	return true;
}

bool CEbenezerDlg::ProcessServerCommand(std::string & message)
{
	// Commands require at least 2 characters
	if (message.size() <= 1
		// If the prefix isn't correct
		|| message[0] != SERVER_COMMAND_PREFIX)
		// we're not a command.
		return false;

	// Split up the command by spaces
	CommandArgs vargs = StrSplit(message, " ");
	std::string command = vargs.front(); // grab the first word (the command)
	vargs.pop_front(); // remove the command from the argument list

	// Make the command lowercase, for 'case-insensitive' checking.
	STRTOLOWER(command);

	// Command doesn't exist
	ServerCommandTable::iterator itr = s_commandTable.find(command.c_str() + 1); // skip the prefix character
	if (itr == s_commandTable.end())
		return false;

	// Run the command
	return (this->*(itr->second->Handler))(vargs, message.c_str() + command.size() + 1, itr->second->Help);
}

COMMAND_HANDLER(CEbenezerDlg::HandleNoticeCommand)
{
	if (vargs.empty())
		return true;

	SendNotice(args);
	return true;
}

COMMAND_HANDLER(CEbenezerDlg::HandleKillUserCommand)
{
	if (vargs.empty())
	{
		// send error saying we need another argument
		return true;
	}

	std::string strUserID = vargs.front();
	CUser *pUser = GetUserPtr(strUserID, TYPE_CHARACTER);
	if (pUser == nullptr)
	{
		// send error saying that user was not found
		return true;
	}
	
	// Disconnect the player
	pUser->Disconnect();

	// send a message saying the player was disconnected
	return true;
}

COMMAND_HANDLER(CUser::HandleWar1OpenCommand) { return g_pMain->HandleWar1OpenCommand(vargs, args, description); }
COMMAND_HANDLER(CEbenezerDlg::HandleWar1OpenCommand)
{
	BattleZoneOpen(BATTLEZONE_OPEN, 1);
	return true;
}

COMMAND_HANDLER(CUser::HandleWar2OpenCommand) { return g_pMain->HandleWar2OpenCommand(vargs, args, description); }
COMMAND_HANDLER(CEbenezerDlg::HandleWar2OpenCommand)
{
	BattleZoneOpen(BATTLEZONE_OPEN, 2);
	return true;
}

COMMAND_HANDLER(CUser::HandleWar3OpenCommand) { return g_pMain->HandleWar3OpenCommand(vargs, args, description); }
COMMAND_HANDLER(CEbenezerDlg::HandleWar3OpenCommand)
{
	BattleZoneOpen(BATTLEZONE_OPEN, 3);
	return true;
}

COMMAND_HANDLER(CUser::HandleSnowWarOpenCommand) { return g_pMain->HandleSnowWarOpenCommand(vargs, args, description); }
COMMAND_HANDLER(CEbenezerDlg::HandleSnowWarOpenCommand)
{
	BattleZoneOpen(SNOW_BATTLEZONE_OPEN);
	return true;
}

COMMAND_HANDLER(CUser::HandleWarCloseCommand) { return g_pMain->HandleWarCloseCommand(vargs, args, description); }
COMMAND_HANDLER(CEbenezerDlg::HandleWarCloseCommand)
{
	m_byBanishFlag = 1;
	return true;
}

COMMAND_HANDLER(CEbenezerDlg::HandleShutdownCommand)
{
	g_socketMgr.SuspendServer();
	printf("Server shutdown, %d users kicked out.\n", KickOutAllUsers());
	return true;
}

COMMAND_HANDLER(CEbenezerDlg::HandlePauseCommand)
{
	g_socketMgr.SuspendServer();
	printf("Server no longer accepting connections.\n");
	return true;
}

COMMAND_HANDLER(CEbenezerDlg::HandleResumeCommand)
{
	g_socketMgr.ResumeServer();
	printf("Server accepting connections.\n");
	return true;
}

COMMAND_HANDLER(CEbenezerDlg::HandleDiscountCommand)
{
	m_sDiscount = 1;
	return true;
}

COMMAND_HANDLER(CEbenezerDlg::HandleGlobalDiscountCommand)
{
	m_sDiscount = 2;
	return true;
}

COMMAND_HANDLER(CEbenezerDlg::HandleDiscountOffCommand)
{
	m_sDiscount = 0;
	return true;
}

COMMAND_HANDLER(CEbenezerDlg::HandleCaptainCommand)
{
	LoadKnightsRankTable();
	return true;
}

COMMAND_HANDLER(CEbenezerDlg::HandleSantaCommand)
{
	m_bSantaOrAngel = FLYING_SANTA;
	return true;
}

COMMAND_HANDLER(CEbenezerDlg::HandleSantaOffCommand)
{
	m_bSantaOrAngel = FLYING_NONE;
	return true;
}

COMMAND_HANDLER(CEbenezerDlg::HandleAngelCommand)
{
	m_bSantaOrAngel = FLYING_ANGEL;
	return true;
}

COMMAND_HANDLER(CEbenezerDlg::HandlePermanentChatCommand)
{
	if (vargs.empty())
	{
		// send error saying we need args (unlike the previous implementation of this command)
		return true;
	}

	SetPermanentMessage("%s", args);
	return true;
}

void CEbenezerDlg::SetPermanentMessage(const char * format, ...)
{
	char buffer[128];
	va_list ap;
	va_start(ap, format);
	vsnprintf(buffer, 128, format, ap);
	va_end(ap);

	m_bPermanentChatMode = true;
	m_strPermanentChat = buffer;

	Packet result;
	ChatPacket::Construct(&result, PERMANENT_CHAT, &m_strPermanentChat);
	Send_All(&result);
}

COMMAND_HANDLER(CEbenezerDlg::HandlePermanentChatOffCommand)
{
	Packet result;
	ChatPacket::Construct(&result, END_PERMANENT_CHAT);
	m_bPermanentChatMode = false;
	Send_All(&result);
	return true;
}

COMMAND_HANDLER(CEbenezerDlg::HandleReloadNoticeCommand)
{
	// Reload the notice data
	LoadNoticeData();

	// and update all logged in players
	// if we're using the new notice format that's always shown
#if __VERSION >= 1453
	SessionMap & sessMap = g_socketMgr.GetActiveSessionMap();
	foreach (itr, sessMap)
	{
		CUser * pUser = TO_USER(itr->second);
		if (pUser->isInGame())
			pUser->SendNotice();
	}
	g_socketMgr.ReleaseLock();
#endif
	return true;
}

void CEbenezerDlg::SendFormattedResource(uint32 nResourceID, uint8 byNation, bool bIsNotice, ...)
{
	_SERVER_RESOURCE *pResource = m_ServerResourceArray.GetData(nResourceID);
	if (pResource == nullptr)
		return;

	string buffer;
	va_list ap;
	va_start(ap, bIsNotice);
	_string_format(pResource->strResource, &buffer, ap);
	va_end(ap);

	if (bIsNotice)
		SendNotice(buffer.c_str(), byNation);
	else
		SendAnnouncement(buffer.c_str(), byNation);
}
