// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_COMMANDS_H_
#define CHROME_TEST_CHROMEDRIVER_COMMANDS_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/values.h"
#include "chrome/test/chromedriver/command.h"
#include "chrome/test/chromedriver/session_connection_map.h"
#include "chrome/test/chromedriver/session_thread_map.h"

struct Session;
class Status;

// Gets status/info about ChromeDriver.
void ExecuteGetStatus(const base::Value::Dict& params,
                      const std::string& session_id,
                      const CommandCallback& callback);

// Gets session.status about ChromeDriver.
void ExecuteBidiSessionStatus(const base::Value::Dict& params,
                              const std::string& session_id,
                              const CommandCallback& callback);

// Creates a new session.
void ExecuteCreateSession(SessionThreadMap* session_thread_map,
                          const Command& init_session_cmd,
                          const base::Value::Dict& params,
                          const std::string& host,
                          const CommandCallback& callback);

// Creates a new BiDi session.
void ExecuteBidiSessionNew(SessionThreadMap* session_thread_map,
                           const Command& init_session_cmd,
                           const base::Value::Dict& params,
                           const std::string& resource,
                           const CommandCallback& callback);

// Gets all sessions
void ExecuteGetSessions(const Command& session_capabilities_command,
                        SessionThreadMap* session_thread_map,
                        const base::Value::Dict& params,
                        const std::string& session_id,
                        const CommandCallback& callback);

// Quits all sessions.
void ExecuteQuitAll(const Command& quit_command,
                    SessionThreadMap* session_thread_map,
                    const base::Value::Dict& params,
                    const std::string& session_id,
                    const CommandCallback& callback);

typedef base::RepeatingCallback<Status(Session* session,
                                       const base::Value::Dict&,
                                       std::unique_ptr<base::Value>*)>
    SessionCommand;

// Executes a given session command, after acquiring access to the appropriate
// session.
void ExecuteSessionCommand(SessionThreadMap* session_thread_map,
                           SessionConnectionMap* session_connection_map,
                           const char* command_name,
                           const SessionCommand& command,
                           bool w3c_standard_command,
                           bool return_ok_without_session,
                           const base::Value::Dict& params,
                           const std::string& session_id,
                           const CommandCallback& callback);

namespace internal {
void CreateSessionOnSessionThreadForTesting(const std::string& id);
}  // namespace internal

#endif  // CHROME_TEST_CHROMEDRIVER_COMMANDS_H_
