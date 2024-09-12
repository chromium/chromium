// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_SESSION_SERVICE_COMMANDS_H_
#define COMPONENTS_SESSIONS_CORE_SESSION_SERVICE_COMMANDS_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "components/sessions/core/command_storage_manager.h"
#include "components/sessions/core/session_command.h"
#include "components/sessions/core/session_types.h"
#include "components/sessions/core/sessions_export.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/base/ui_base_types.h"

namespace sessions {

class SessionCommand;

// The following functions create sequentialized change commands which are
// used to reconstruct the current/previous session state.
SESSIONS_EXPORT std::unique_ptr<SessionCommand>
CreateSetSelectedTabInWindowCommand(SessionID window_id, int index);
SESSIONS_EXPORT std::unique_ptr<SessionCommand> CreateSetTabWindowCommand(
    SessionID window_id,
    SessionID tab_id);
SESSIONS_EXPORT std::unique_ptr<SessionCommand> CreateSetWindowBoundsCommand(
    SessionID window_id,
    const gfx::Rect& bounds,
    ui::mojom::WindowShowState show_state);
SESSIONS_EXPORT std::unique_ptr<SessionCommand>
CreateSetTabIndexInWindowCommand(SessionID tab_id, int new_index);
SESSIONS_EXPORT std::unique_ptr<SessionCommand> CreateTabClosedCommand(
    SessionID tab_id);
SESSIONS_EXPORT std::unique_ptr<SessionCommand> CreateWindowClosedCommand(
    SessionID tab_id);
SESSIONS_EXPORT std::unique_ptr<SessionCommand>
CreateSetSelectedNavigationIndexCommand(SessionID tab_id, int index);
SESSIONS_EXPORT std::unique_ptr<SessionCommand> CreateSetWindowTypeCommand(
    SessionID window_id,
    SessionWindow::WindowType type);
SESSIONS_EXPORT std::unique_ptr<SessionCommand> CreateTabGroupCommand(
    SessionID tab_id,
    std::optional<tab_groups::TabGroupId> group);
SESSIONS_EXPORT std::unique_ptr<SessionCommand>
CreateTabGroupMetadataUpdateCommand(
    const tab_groups::TabGroupId group,
    const tab_groups::TabGroupVisualData* visual_data,
    const std::optional<std::string> saved_guid = std::nullopt);
SESSIONS_EXPORT std::unique_ptr<SessionCommand> CreatePinnedStateCommand(
    SessionID tab_id,
    bool is_pinned);
SESSIONS_EXPORT std::unique_ptr<SessionCommand>
CreateSessionStorageAssociatedCommand(
    SessionID tab_id,
    const std::string& session_storage_persistent_id);
SESSIONS_EXPORT std::unique_ptr<SessionCommand> CreateSetActiveWindowCommand(
    SessionID window_id);
SESSIONS_EXPORT std::unique_ptr<SessionCommand>
CreateTabNavigationPathPrunedCommand(SessionID tab_id, int index, int count);
SESSIONS_EXPORT std::unique_ptr<SessionCommand>
CreateUpdateTabNavigationCommand(
    SessionID tab_id,
    const sessions::SerializedNavigationEntry& navigation);
SESSIONS_EXPORT std::unique_ptr<SessionCommand>
CreateSetTabExtensionAppIDCommand(SessionID tab_id,
                                  const std::string& extension_id);
SESSIONS_EXPORT std::unique_ptr<SessionCommand>
CreateSetTabUserAgentOverrideCommand(
    SessionID tab_id,
    const SerializedUserAgentOverride& user_agent_override);
SESSIONS_EXPORT std::unique_ptr<SessionCommand> CreateSetWindowAppNameCommand(
    SessionID window_id,
    const std::string& app_name);
SESSIONS_EXPORT std::unique_ptr<SessionCommand> CreateSetWindowUserTitleCommand(
    SessionID window_id,
    const std::string& user_title);
SESSIONS_EXPORT std::unique_ptr<SessionCommand> CreateLastActiveTimeCommand(
    SessionID tab_id,
    base::Time last_active_time);

SESSIONS_EXPORT std::unique_ptr<SessionCommand> CreateSetWindowWorkspaceCommand(
    SessionID window_id,
    const std::string& workspace);

SESSIONS_EXPORT std::unique_ptr<SessionCommand>
CreateSetWindowVisibleOnAllWorkspacesCommand(SessionID window_id,
                                             bool visible_on_all_workspaces);

SESSIONS_EXPORT std::unique_ptr<SessionCommand> CreateSetTabGuidCommand(
    SessionID tab_id,
    const std::string& guid);

SESSIONS_EXPORT std::unique_ptr<SessionCommand> CreateSetTabDataCommand(
    SessionID tab_id,
    const std::map<std::string, std::string>& data);

SESSIONS_EXPORT std::unique_ptr<SessionCommand> CreateAddTabExtraDataCommand(
    SessionID tab_id,
    const std::string& key,
    const std::string& data);

SESSIONS_EXPORT std::unique_ptr<SessionCommand> CreateAddWindowExtraDataCommand(
    SessionID window_id,
    const std::string& key,
    const std::string& data);

// Searches for a pending command using |command_storage_manager| that can be
// replaced with |command|. If one is found, pending command is removed, the
// command is added to the pending commands (taken ownership) and true is
// returned.
SESSIONS_EXPORT bool ReplacePendingCommand(
    CommandStorageManager* command_storage_manager,
    std::unique_ptr<SessionCommand>* command);

// Returns true if provided |command| either closes a window or a tab.
SESSIONS_EXPORT bool IsClosingCommand(SessionCommand* command);

// Converts a list of commands into SessionWindows. On return any valid
// windows are added to |valid_windows|. |active_window_id| will be set with the
// id of the last active window, but it's only valid when this id corresponds
// to the id of one of the windows in |valid_windows|.
SESSIONS_EXPORT void RestoreSessionFromCommands(
    const std::vector<std::unique_ptr<SessionCommand>>& commands,
    std::vector<std::unique_ptr<SessionWindow>>* valid_windows,
    SessionID* active_window_id);

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_SESSION_SERVICE_COMMANDS_H_
