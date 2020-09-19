// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_BASE_SESSION_SERVICE_COMMANDS_H_
#define COMPONENTS_SESSIONS_CORE_BASE_SESSION_SERVICE_COMMANDS_H_

#include <memory>
#include <string>

#include "base/optional.h"
#include "components/sessions/core/serialized_user_agent_override.h"
#include "components/sessions/core/session_command.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/sessions_export.h"

namespace sessions {
class SessionCommand;
class SerializedNavigationEntry;

// These commands create and read common base commands for SessionService and
// PersistentTabRestoreService.

// Creates a SessionCommand that represents a navigation.
std::unique_ptr<SessionCommand> CreateUpdateTabNavigationCommand(
    SessionCommand::id_type command_id,
    SessionID tab_id,
    const sessions::SerializedNavigationEntry& navigation);

// Creates a SessionCommand that represents marking a tab as an application.
std::unique_ptr<SessionCommand> CreateSetTabExtensionAppIDCommand(
    SessionCommand::id_type command_id,
    SessionID tab_id,
    const std::string& extension_id);

// Creates a SessionCommand that containing user agent override used by a
// tab's navigations.
std::unique_ptr<SessionCommand> CreateSetTabUserAgentOverrideCommand(
    SessionCommand::id_type command_id,
    SessionID tab_id,
    const SerializedUserAgentOverride& user_agent_override);

// Creates a SessionCommand stores a browser window's app name.
std::unique_ptr<SessionCommand> CreateSetWindowAppNameCommand(
    SessionCommand::id_type command_id,
    SessionID window_id,
    const std::string& app_name);

// Creates a SessionCommand storing a browser window's user title.
std::unique_ptr<SessionCommand> CreateSetWindowUserTitleCommand(
    SessionCommand::id_type command_id,
    SessionID window_id,
    const std::string& app_name);

// Converts a SessionCommand previously created by
// CreateUpdateTabNavigationCommand into a
// SerializedNavigationEntry. Returns true on success. If
// successful |tab_id| is set to the id of the restored tab.
bool RestoreUpdateTabNavigationCommand(
    const SessionCommand& command,
    sessions::SerializedNavigationEntry* navigation,
    SessionID* tab_id);

// Extracts a SessionCommand as previously created by
// CreateSetTabExtensionAppIDCommand into the tab id and application
// extension id.
bool RestoreSetTabExtensionAppIDCommand(const SessionCommand& command,
                                        SessionID* tab_id,
                                        std::string* extension_app_id);

// Extracts a SessionCommand as previously created by
// CreateSetTabUserAgentOverrideCommand into the tab id and user agent and
// structured user agent.
bool RestoreSetTabUserAgentOverrideCommand2(
    const SessionCommand& command,
    SessionID* tab_id,
    std::string* user_agent_override,
    base::Optional<std::string>* opaque_ua_metadata_override);

// Backwards compatible version that restores versions that didn't have
// structured user agent override.
bool RestoreSetTabUserAgentOverrideCommand(const SessionCommand& command,
                                           SessionID* tab_id,
                                           std::string* user_agent_override);

// Extracts a SessionCommand as previously created by
// CreateSetWindowAppNameCommand into the window id and application name.
bool RestoreSetWindowAppNameCommand(const SessionCommand& command,
                                    SessionID* window_id,
                                    std::string* app_name);

// Extracts a SessionCommand as previously created by
// CreateSetWindowUserTitleCommand into the window id and user title.
bool RestoreSetWindowUserTitleCommand(const SessionCommand& command,
                                      SessionID* window_id,
                                      std::string* user_title);

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_BASE_SESSION_SERVICE_COMMANDS_H_
