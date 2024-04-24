// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/commands.h"

#include <stddef.h>

#include <algorithm>
#include <functional>
#include <list>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chrome/test/chromedriver/capabilities.h"
#include "chrome/test/chromedriver/chrome/browser_info.h"
#include "chrome/test/chromedriver/chrome/chrome.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/constants/version.h"
#include "chrome/test/chromedriver/logging.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/session_commands.h"
#include "chrome/test/chromedriver/session_thread_map.h"
#include "chrome/test/chromedriver/util.h"

namespace {
void WriteChromeDriverExtendedStatus(base::Value::Dict& info) {
  base::Value::Dict build;
  build.Set("version", kChromeDriverVersion);
  info.Set("build", std::move(build));

  base::Value::Dict os;
  os.Set("name", base::SysInfo::OperatingSystemName());
  os.Set("version", base::SysInfo::OperatingSystemVersion());
  os.Set("arch", base::SysInfo::OperatingSystemArchitecture());
  info.Set("os", std::move(os));
}
}  // namespace

void ExecuteGetStatus(const base::Value::Dict& params,
                      const std::string& session_id,
                      const CommandCallback& callback) {
  // W3C defined data:
  // ChromeDriver doesn't have a preset limit on number of active sessions,
  // so we are always ready.
  base::Value::Dict info;
  info.Set("ready", true);
  info.Set("message", base::StringPrintf("%s ready for new sessions.",
                                         kChromeDriverProductShortName));

  // ChromeDriver specific data:
  WriteChromeDriverExtendedStatus(info);

  callback.Run(Status(kOk), std::make_unique<base::Value>(std::move(info)),
               std::string(), kW3CDefault);
}

void ExecuteBidiSessionStatus(const base::Value::Dict& params,
                              const std::string& session_id,
                              const CommandCallback& callback) {
  base::Value::Dict info;
  if (session_id.empty()) {
    info.Set("ready", true);
    info.Set("message", base::StringPrintf("%s ready for new sessions.",
                                           kChromeDriverProductShortName));
  } else {
    info.Set("ready", false);
    // The error message is borrowed from BiDiMapper code.
    // See bidiMapper/domains/session/SessionProcessor.ts of chromium-bidi
    // repository.
    info.Set("message", "already connected");
  }

  // ChromeDriver specific data:
  WriteChromeDriverExtendedStatus(info);

  callback.Run(Status(kOk), std::make_unique<base::Value>(std::move(info)),
               session_id, kW3CDefault);
}

void ExecuteCreateSession(SessionThreadMap* session_thread_map,
                          const Command& init_session_cmd,
                          const base::Value::Dict& params,
                          const std::string& host,
                          const CommandCallback& callback) {
  std::string new_id = GenerateId();
  std::unique_ptr<Session> session = std::make_unique<Session>(new_id, host);
  std::unique_ptr<SessionThreadInfo> thread_info =
      std::make_unique<SessionThreadInfo>(new_id, GetW3CSetting(params));
  if (!thread_info->thread()->Start()) {
    callback.Run(
        Status(kUnknownError, "failed to start a thread for the new session"),
        std::unique_ptr<base::Value>(), std::string(),
        session->w3c_compliant);
    return;
  }

  thread_info->thread()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&SetThreadLocalSession, std::move(session)));
  session_thread_map->emplace(new_id, std::move(thread_info));
  init_session_cmd.Run(params, new_id, callback);
}

void ExecuteBidiSessionNew(SessionThreadMap* session_thread_map,
                           const Command& init_session_cmd,
                           const base::Value::Dict& params,
                           const std::string& resource,
                           const CommandCallback& callback) {
  if (!resource.empty()) {
    callback.Run(Status{kSessionNotCreated, "session already exists"}, nullptr,
                 resource, kW3CDefault);
    return;
  }
  base::Value::Dict new_params;
  const base::Value::Dict* capabilities =
      params.FindDictByDottedPath("params.capabilities");
  if (capabilities) {
    new_params.Set("capabilities", capabilities->Clone());
  }
  new_params.SetByDottedPath("capabilities.alwaysMatch.webSocketUrl", true);
  ExecuteCreateSession(session_thread_map, init_session_cmd, new_params,
                       resource, callback);
}

namespace {

void OnGetSession(const base::WeakPtr<size_t>& session_remaining_count,
                  const base::RepeatingClosure& all_get_session_func,
                  base::Value::List& session_list,
                  const Status& status,
                  std::unique_ptr<base::Value> value,
                  const std::string& session_id,
                  bool w3c_compliant) {
  if (!session_remaining_count)
    return;

  (*session_remaining_count)--;

  if (value) {
    base::Value::Dict session;
    session.Set("id", session_id);
    session.Set("capabilities",
                base::Value::FromUniquePtrValue(std::move(value)));
    session_list.Append(std::move(session));
  }

  if (!*session_remaining_count) {
    all_get_session_func.Run();
  }
}

}  // namespace

void ExecuteGetSessions(const Command& session_capabilities_command,
                        SessionThreadMap* session_thread_map,
                        const base::Value::Dict& params,
                        const std::string& session_id,
                        const CommandCallback& callback) {
  size_t get_remaining_count = session_thread_map->size();
  base::WeakPtrFactory<size_t> weak_ptr_factory(&get_remaining_count);
  base::Value::List session_list;

  if (!get_remaining_count) {
    callback.Run(Status(kOk),
                 std::make_unique<base::Value>(std::move(session_list)),
                 session_id, false);
    return;
  }

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);

  for (auto iter = session_thread_map->begin();
       iter != session_thread_map->end(); ++iter) {
    session_capabilities_command.Run(
        params, iter->first,
        base::BindRepeating(&OnGetSession, weak_ptr_factory.GetWeakPtr(),
                            run_loop.QuitClosure(), std::ref(session_list)));
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Seconds(10));
  run_loop.Run();

  callback.Run(Status(kOk),
               std::make_unique<base::Value>(std::move(session_list)),
               session_id, false);
}

namespace {

void OnSessionQuit(const base::WeakPtr<size_t>& quit_remaining_count,
                   const base::RepeatingClosure& all_quit_func,
                   const Status& status,
                   std::unique_ptr<base::Value> value,
                   const std::string& session_id,
                   bool w3c_compliant) {
  // |quit_remaining_count| may no longer be valid if a timeout occurred.
  if (!quit_remaining_count)
    return;

  (*quit_remaining_count)--;
  if (!*quit_remaining_count)
    all_quit_func.Run();
}

}  // namespace

void ExecuteQuitAll(const Command& quit_command,
                    SessionThreadMap* session_thread_map,
                    const base::Value::Dict& params,
                    const std::string& session_id,
                    const CommandCallback& callback) {
  size_t quit_remaining_count = session_thread_map->size();
  base::WeakPtrFactory<size_t> weak_ptr_factory(&quit_remaining_count);
  if (!quit_remaining_count) {
    callback.Run(Status(kOk), std::unique_ptr<base::Value>(),
                 session_id, false);
    return;
  }
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  for (auto iter = session_thread_map->begin();
       iter != session_thread_map->end(); ++iter) {
    quit_command.Run(
        params, iter->first,
        base::BindRepeating(&OnSessionQuit, weak_ptr_factory.GetWeakPtr(),
                            run_loop.QuitClosure()));
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Seconds(10));
  // Uses a nested run loop to block this thread until all the quit
  // commands have executed, or the timeout expires.
  run_loop.Run();
  callback.Run(Status(kOk), std::unique_ptr<base::Value>(),
               session_id, false);
}

namespace {

void TerminateSessionThreadOnCommandThread(
    SessionThreadMap* session_thread_map,
    SessionConnectionMap* session_connection_map,
    const std::string& session_id) {
  session_thread_map->erase(session_id);
  session_connection_map->erase(session_id);
}

void ExecuteSessionCommandOnSessionThread(
    const char* command_name,
    const std::string& session_id,
    const SessionCommand& command,
    bool w3c_standard_command,
    bool return_ok_without_session,
    const base::Value::Dict& params,
    scoped_refptr<base::SingleThreadTaskRunner> cmd_task_runner,
    const CommandCallback& callback_on_cmd,
    const base::RepeatingClosure& terminate_on_cmd) {
  Session* session = GetThreadLocalSession();

  if (!session) {
    cmd_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            callback_on_cmd,
            Status(return_ok_without_session ? kOk : kInvalidSessionId),
            std::unique_ptr<base::Value>(), session_id, kW3CDefault));
    return;
  }

  if (IsVLogOn(0)) {
    if (!session->driver_log ||
        session->driver_log->min_level() != Log::Level::kOff) {
      // Note: ChromeDriver log-replay depends on the format of this logging.
      // see chromedriver/log_replay/client_replay.py
      VLOG(0) << "[" << session->id << "] "
              << "COMMAND " << command_name << " "
              << PrettyPrintValue(base::Value(params.Clone()));
    }
  }

  // Notify |session|'s |CommandListener|s of the command.
  // Will mark |session| for deletion if an error is encountered.
  Status status = NotifyCommandListenersBeforeCommand(session, command_name);

  std::unique_ptr<base::Value> value;
  if (session->w3c_compliant && !w3c_standard_command) {
    status = Status(kUnknownCommand,
                    "Cannot call non W3C standard command while in W3C mode");
    if (IsVLogOn(0)) {
      std::string result;
      result = "ERROR " + status.message();
      if (!session->driver_log ||
          session->driver_log->min_level() != Log::Level::kOff) {
        // Note: ChromeDriver log-replay depends on the format of this
        // logging. see chromedriver/log_replay/client_replay.py
        VLOG(0) << "[" << session->id << "] "
                << "RESPONSE " << command_name
                << (result.length() ? " " + result : "");
      }
    }
  } else {
    // Only run the command if we were able to notify all listeners
    // successfully.
    // Otherwise, pass error to callback, delete |session|, and do not continue.
    if (status.IsError()) {
      LOG(ERROR) << status.message();
    } else {
      status = command.Run(session, params, &value);

      if (status.IsError() && session->chrome) {
        if (!session->quit && session->chrome->HasCrashedWebView()) {
          session->quit = true;
          std::string message("session deleted because of page crash");
          if (!session->detach) {
            Status quit_status = session->chrome->Quit();
            if (quit_status.IsError())
              message +=
                  ", but failed to kill browser:" + quit_status.message();
          }
          status = Status(kUnknownError, message, status);
        } else if (status.code() == kDisconnected ||
                   status.code() == kTargetDetached) {
          // Some commands, like clicking a button or link which closes the
          // window, may result in a kDisconnected error code.
          std::list<std::string> web_view_ids;
          Status status_tmp = session->chrome->GetWebViewIds(
              &web_view_ids, session->w3c_compliant);
          if (status_tmp.IsError()) {
            status.AddDetails("failed to check if window was closed: " +
                              status_tmp.message());
          } else if (!base::Contains(web_view_ids, session->window)) {
            status = Status(kOk);
          }
        }
        if (status.IsError()) {
          const BrowserInfo* browser_info = session->chrome->GetBrowserInfo();
          status.AddDetails("Session info: " + browser_info->browser_name +
                            "=" + browser_info->browser_version);
        }
      }

      if (IsVLogOn(0)) {
        std::string result;
        if (status.IsError()) {
          result = "ERROR " + status.message();
        } else if (value) {
          result = FormatValueForDisplay(*value);
        }
        if (!session->driver_log ||
            session->driver_log->min_level() != Log::Level::kOff) {
          // Note: ChromeDriver log-replay depends on the format of this
          // logging. see chromedriver/log_replay/client_replay.py
          VLOG(0) << "[" << session->id << "] "
                  << "RESPONSE " << command_name
                  << (result.length() ? " " + result : "");
        }
      }
    }
  }

  cmd_task_runner->PostTask(
      FROM_HERE, base::BindOnce(callback_on_cmd, status, std::move(value),
                                session->id, session->w3c_compliant));

  if (session->quit) {
    session->CloseAllConnections();
    SetThreadLocalSession(std::unique_ptr<Session>());
    delete session;
    cmd_task_runner->PostTask(FROM_HERE, terminate_on_cmd);
  }
}

}  // namespace

void ExecuteSessionCommand(SessionThreadMap* session_thread_map,
                           SessionConnectionMap* session_connection_map,
                           const char* command_name,
                           const SessionCommand& command,
                           bool w3c_standard_command,
                           bool return_ok_without_session,
                           const base::Value::Dict& params,
                           const std::string& session_id,
                           const CommandCallback& callback) {
  auto iter = session_thread_map->find(session_id);
  if (iter == session_thread_map->end()) {
    Status status(return_ok_without_session ? kOk : kInvalidSessionId);
    callback.Run(status, std::unique_ptr<base::Value>(), session_id,
                 kW3CDefault);
  } else {
    iter->second->thread()->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ExecuteSessionCommandOnSessionThread, command_name, session_id,
            command, w3c_standard_command, return_ok_without_session,
            params.Clone(), base::SingleThreadTaskRunner::GetCurrentDefault(),
            callback,
            base::BindRepeating(&TerminateSessionThreadOnCommandThread,
                                session_thread_map, session_connection_map,
                                session_id)));
  }
}

namespace internal {

void CreateSessionOnSessionThreadForTesting(const std::string& id) {
  SetThreadLocalSession(std::make_unique<Session>(id));
}

}  // namespace internal
