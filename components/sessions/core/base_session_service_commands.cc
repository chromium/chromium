// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/base_session_service_commands.h"

#include <stddef.h>

#include <memory>

#include "base/pickle.h"
#include "components/sessions/core/command_storage_backend.h"
#include "components/sessions/core/session_types.h"

namespace sessions {
namespace {

// Helper used by CreateUpdateTabNavigationCommand(). It writes |str| to
// |pickle|, if and only if |str| fits within (|max_bytes| - |*bytes_written|).
// |bytes_written| is incremented to reflect the data written.
void WriteStringToPickle(base::Pickle& pickle,
                         int* bytes_written,
                         int max_bytes,
                         const std::string& str) {
  int num_bytes = str.size() * sizeof(char);
  if (*bytes_written + num_bytes < max_bytes) {
    *bytes_written += num_bytes;
    pickle.WriteString(str);
  } else {
    pickle.WriteString(std::string());
  }
}

bool ReadSessionIdFromPickle(base::PickleIterator* iterator, SessionID* id) {
  SessionID::id_type value;
  if (!iterator->ReadInt(&value)) {
    return false;
  }
  *id = SessionID::FromSerializedValue(value);
  return true;
}

}  // namespace

std::unique_ptr<SessionCommand> CreateUpdateTabNavigationCommand(
    SessionCommand::id_type command_id,
    SessionID tab_id,
    const sessions::SerializedNavigationEntry& navigation) {
  // Use pickle to handle marshalling.
  base::Pickle pickle;
  pickle.WriteInt(tab_id.id());
  // We only allow navigations up to 63k (which should be completely
  // reasonable).
  static const size_t max_state_size =
      std::numeric_limits<SessionCommand::size_type>::max() - 1024;
  navigation.WriteToPickle(max_state_size, &pickle);
  return std::make_unique<SessionCommand>(command_id, pickle);
}

std::unique_ptr<SessionCommand> CreateSetTabExtensionAppIDCommand(
    SessionCommand::id_type command_id,
    SessionID tab_id,
    const std::string& extension_id) {
  // Use pickle to handle marshalling.
  base::Pickle pickle;
  pickle.WriteInt(tab_id.id());

  // Enforce a max for ids. They should never be anywhere near this size.
  static const SessionCommand::size_type max_id_size =
      std::numeric_limits<SessionCommand::size_type>::max() - 1024;

  int bytes_written = 0;

  WriteStringToPickle(pickle, &bytes_written, max_id_size, extension_id);

  return std::make_unique<SessionCommand>(command_id, pickle);
}

std::unique_ptr<SessionCommand> CreateSetTabUserAgentOverrideCommand(
    SessionCommand::id_type command_id,
    SessionID tab_id,
    const SerializedUserAgentOverride& user_agent_override) {
  // Use pickle to handle marshalling.
  base::Pickle pickle;
  pickle.WriteInt(tab_id.id());

  // Enforce a max for the user agent length.  They should never be anywhere
  // near this size.
  static const SessionCommand::size_type max_user_agent_size =
      (std::numeric_limits<SessionCommand::size_type>::max() - 1024) / 2;

  int bytes_written = 0;

  WriteStringToPickle(pickle, &bytes_written, max_user_agent_size,
                      user_agent_override.ua_string_override);

  bool have_usable_ua_metadata_override =
      user_agent_override.opaque_ua_metadata_override.has_value() &&
      user_agent_override.opaque_ua_metadata_override->size() <
          max_user_agent_size;
  pickle.WriteBool(have_usable_ua_metadata_override);
  if (have_usable_ua_metadata_override) {
    bytes_written = 0;
    WriteStringToPickle(
        pickle, &bytes_written, max_user_agent_size,
        user_agent_override.opaque_ua_metadata_override.value());
  }

  return std::make_unique<SessionCommand>(command_id, pickle);
}

std::unique_ptr<SessionCommand> CreateSetWindowAppNameCommand(
    SessionCommand::id_type command_id,
    SessionID window_id,
    const std::string& app_name) {
  // Use pickle to handle marshalling.
  base::Pickle pickle;
  pickle.WriteInt(window_id.id());

  // Enforce a max for ids. They should never be anywhere near this size.
  static const SessionCommand::size_type max_id_size =
      std::numeric_limits<SessionCommand::size_type>::max() - 1024;

  int bytes_written = 0;

  WriteStringToPickle(pickle, &bytes_written, max_id_size, app_name);

  return std::make_unique<SessionCommand>(command_id, pickle);
}

std::unique_ptr<SessionCommand> CreateSetWindowUserTitleCommand(
    SessionCommand::id_type command_id,
    SessionID window_id,
    const std::string& user_title) {
  // Use pickle to handle marshalling.
  base::Pickle pickle;
  pickle.WriteInt(window_id.id());

  // Enforce a max for ids. They should never be anywhere near this size.
  static const SessionCommand::size_type max_id_size =
      std::numeric_limits<SessionCommand::size_type>::max() - 1024;

  int bytes_written = 0;

  WriteStringToPickle(pickle, &bytes_written, max_id_size, user_title);

  return std::make_unique<SessionCommand>(command_id, pickle);
}

std::unique_ptr<SessionCommand> CreateAddExtraDataCommand(
    SessionCommand::id_type command,
    SessionID session_id,
    const std::string& key,
    const std::string& data) {
  base::Pickle pickle;
  pickle.WriteInt(session_id.id());

  // Enforce a max for ids. They should never be anywhere near this size.
  static const SessionCommand::size_type max_id_size =
      std::numeric_limits<SessionCommand::size_type>::max() - 1024;
  int total_bytes_written = 0;
  WriteStringToPickle(pickle, &total_bytes_written, max_id_size, key);
  WriteStringToPickle(pickle, &total_bytes_written, max_id_size, data);

  return std::make_unique<SessionCommand>(command, pickle);
}

bool RestoreUpdateTabNavigationCommand(
    const SessionCommand& command,
    sessions::SerializedNavigationEntry* navigation,
    SessionID* tab_id) {
  base::Pickle pickle = command.PayloadAsPickle();
  base::PickleIterator iterator(pickle);

  return ReadSessionIdFromPickle(&iterator, tab_id) &&
         navigation->ReadFromPickle(&iterator);
}

bool RestoreSetTabExtensionAppIDCommand(const SessionCommand& command,
                                        SessionID* tab_id,
                                        std::string* extension_app_id) {
  base::Pickle pickle = command.PayloadAsPickle();
  base::PickleIterator iterator(pickle);

  return ReadSessionIdFromPickle(&iterator, tab_id) &&
         iterator.ReadString(extension_app_id);
}

bool RestoreSetTabUserAgentOverrideCommand(const SessionCommand& command,
                                           SessionID* tab_id,
                                           std::string* user_agent_override) {
  base::Pickle pickle = command.PayloadAsPickle();
  base::PickleIterator iterator(pickle);

  return ReadSessionIdFromPickle(&iterator, tab_id) &&
         iterator.ReadString(user_agent_override);
}

bool RestoreSetTabUserAgentOverrideCommand2(
    const SessionCommand& command,
    SessionID* tab_id,
    std::string* user_agent_override,
    std::optional<std::string>* opaque_ua_metadata_override) {
  base::Pickle pickle = command.PayloadAsPickle();
  base::PickleIterator iterator(pickle);

  if (!ReadSessionIdFromPickle(&iterator, tab_id)) {
    return false;
  }
  if (!iterator.ReadString(user_agent_override)) {
    return false;
  }
  // See if there is UA metadata override.
  bool has_ua_metadata_override;
  if (!iterator.ReadBool(&has_ua_metadata_override)) {
    return false;
  }
  if (!has_ua_metadata_override) {
    *opaque_ua_metadata_override = std::nullopt;
    return true;
  }

  std::string ua_metadata_override_value;
  if (!iterator.ReadString(&ua_metadata_override_value)) {
    return false;
  }

  *opaque_ua_metadata_override = std::move(ua_metadata_override_value);
  return true;
}

bool RestoreSetWindowAppNameCommand(const SessionCommand& command,
                                    SessionID* window_id,
                                    std::string* app_name) {
  base::Pickle pickle = command.PayloadAsPickle();
  base::PickleIterator iterator(pickle);

  return ReadSessionIdFromPickle(&iterator, window_id) &&
         iterator.ReadString(app_name);
}

bool RestoreSetWindowUserTitleCommand(const SessionCommand& command,
                                      SessionID* window_id,
                                      std::string* user_title) {
  base::Pickle pickle = command.PayloadAsPickle();
  base::PickleIterator iterator(pickle);

  return ReadSessionIdFromPickle(&iterator, window_id) &&
         iterator.ReadString(user_title);
}

bool RestoreAddExtraDataCommand(const SessionCommand& command,
                                SessionID* session_id,
                                std::string* key,
                                std::string* data) {
  base::Pickle pickle = command.PayloadAsPickle();
  base::PickleIterator it(pickle);

  return ReadSessionIdFromPickle(&it, session_id) && it.ReadString(key) &&
         it.ReadString(data);
}

}  // namespace sessions
