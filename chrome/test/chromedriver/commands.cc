// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/commands.h"

#include <stddef.h>

#include <algorithm>
#include <list>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop_current.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/test/chromedriver/capabilities.h"
#include "chrome/test/chromedriver/chrome/browser_info.h"
#include "chrome/test/chromedriver/chrome/chrome.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/logging.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/session_thread_map.h"
#include "chrome/test/chromedriver/util.h"

void ExecuteGetStatus(
    const base::DictionaryValue& params,
    const std::string& session_id,
    const CommandCallback& callback) {
  base::DictionaryValue build;
  build.SetString("version", "alpha");

  base::DictionaryValue os;
  os.SetString("name", base::SysInfo::OperatingSystemName());
  os.SetString("version", base::SysInfo::OperatingSystemVersion());
  os.SetString("arch", base::SysInfo::OperatingSystemArchitecture());

  base::DictionaryValue info;
  info.SetKey("build", std::move(build));
  info.SetKey("os", std::move(os));
  callback.Run(Status(kOk), base::Value::ToUniquePtrValue(std::move(info)),
               std::string(), false);
}

void ExecuteCreateSession(
    SessionThreadMap* session_thread_map,
    const Command& init_session_cmd,
    const base::DictionaryValue& params,
    const std::string& session_id,
    const CommandCallback& callback) {
  std::string new_id = session_id;
  if (new_id.empty())
    new_id = GenerateId();
  std::unique_ptr<Session> session(new Session(new_id));
  std::unique_ptr<base::Thread> thread(new base::Thread(new_id));
  if (!thread->Start()) {
    callback.Run(
        Status(kUnknownError, "failed to start a thread for the new session"),
        std::unique_ptr<base::Value>(), std::string(),
        session->w3c_compliant);
    return;
  }

  thread->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&SetThreadLocalSession, std::move(session)));
  session_thread_map
      ->insert(std::make_pair(new_id, make_linked_ptr(thread.release())));
  init_session_cmd.Run(params, new_id, callback);
}

namespace {

void OnGetSession(const base::WeakPtr<size_t>& session_remaining_count,
                  const base::Closure& all_get_session_func,
                  base::ListValue* session_list,
                  const Status& status,
                  std::unique_ptr<base::Value> value,
                  const std::string& session_id,
                  bool w3c_compliant) {
  if (!session_remaining_count)
    return;

  (*session_remaining_count)--;

  std::unique_ptr<base::DictionaryValue> session(new base::DictionaryValue());
  session->SetString("id", session_id);
  session->SetKey("capabilities",
                  base::Value::FromUniquePtrValue(std::move(value)));
  session_list->Append(std::move(session));

  if (!*session_remaining_count) {
    all_get_session_func.Run();
  }
}

}  // namespace

void ExecuteGetSessions(const Command& session_capabilities_command,
                        SessionThreadMap* session_thread_map,
                        const base::DictionaryValue& params,
                        const std::string& session_id,
                        const CommandCallback& callback) {
  size_t get_remaining_count = session_thread_map->size();
  base::WeakPtrFactory<size_t> weak_ptr_factory(&get_remaining_count);
  std::unique_ptr<base::ListValue> session_list(new base::ListValue());

  if (!get_remaining_count) {
    callback.Run(Status(kOk), std::move(session_list), session_id, false);
    return;
  }

  base::RunLoop run_loop;

  for (SessionThreadMap::const_iterator iter = session_thread_map->begin();
       iter != session_thread_map->end();
       ++iter) {
    session_capabilities_command.Run(params,
                                     iter->first,
                                     base::Bind(
                                               &OnGetSession,
                                               weak_ptr_factory.GetWeakPtr(),
                                               run_loop.QuitClosure(),
                                               session_list.get()));
  }
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::TimeDelta::FromSeconds(10));
  base::MessageLoopCurrent::Get()->SetNestableTasksAllowed(true);
  run_loop.Run();

  callback.Run(Status(kOk), std::move(session_list), session_id, false);
}

namespace {

void OnSessionQuit(const base::WeakPtr<size_t>& quit_remaining_count,
                   const base::Closure& all_quit_func,
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

void ExecuteQuitAll(
    const Command& quit_command,
    SessionThreadMap* session_thread_map,
    const base::DictionaryValue& params,
    const std::string& session_id,
    const CommandCallback& callback) {
  size_t quit_remaining_count = session_thread_map->size();
  base::WeakPtrFactory<size_t> weak_ptr_factory(&quit_remaining_count);
  if (!quit_remaining_count) {
    callback.Run(Status(kOk), std::unique_ptr<base::Value>(),
                 session_id, false);
    return;
  }
  base::RunLoop run_loop;
  for (SessionThreadMap::const_iterator iter = session_thread_map->begin();
       iter != session_thread_map->end();
       ++iter) {
    quit_command.Run(params,
                     iter->first,
                     base::Bind(&OnSessionQuit,
                                weak_ptr_factory.GetWeakPtr(),
                                run_loop.QuitClosure()));
  }
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::TimeDelta::FromSeconds(10));
  // Uses a nested run loop to block this thread until all the quit
  // commands have executed, or the timeout expires.
  base::MessageLoopCurrent::Get()->SetNestableTasksAllowed(true);
  run_loop.Run();
  callback.Run(Status(kOk), std::unique_ptr<base::Value>(),
               session_id, false);
}

namespace {

void TerminateSessionThreadOnCommandThread(SessionThreadMap* session_thread_map,
                                           const std::string& session_id) {
  session_thread_map->erase(session_id);
}

void ExecuteSessionCommandOnSessionThread(
    const char* command_name,
    const SessionCommand& command,
    bool return_ok_without_session,
    std::unique_ptr<base::DictionaryValue> params,
    scoped_refptr<base::SingleThreadTaskRunner> cmd_task_runner,
    const CommandCallback& callback_on_cmd,
    const base::Closure& terminate_on_cmd) {
  Session* session = GetThreadLocalSession();

  if (!session) {
    cmd_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            callback_on_cmd,
            Status(return_ok_without_session ? kOk : kInvalidSessionId),
            std::unique_ptr<base::Value>(), std::string(), kW3CDefault));
    return;
  }

  if (IsVLogOn(0)) {
    if (!session->driver_log ||
        session->driver_log->min_level() != Log::Level::kOff) {
      // Note: ChromeDriver log-replay depends on the format of this logging.
      // see chromedriver/log_replay/client_replay.py
      VLOG(0) << "[" << session->id << "] "
              << "COMMAND " << command_name << " "
              << FormatValueForDisplay(*params);
    }
  }

  // Notify |session|'s |CommandListener|s of the command.
  // Will mark |session| for deletion if an error is encountered.
  Status status = NotifyCommandListenersBeforeCommand(session, command_name);

  // Only run the command if we were able to notify all listeners successfully.
  // Otherwise, pass error to callback, delete |session|, and do not continue.
  std::unique_ptr<base::Value> value;
  if (status.IsError()) {
    LOG(ERROR) << status.message();
  } else {
    status = command.Run(session, *params, &value);

    if (status.IsError() && session->chrome) {
      if (!session->quit && session->chrome->HasCrashedWebView()) {
        session->quit = true;
        std::string message("session deleted because of page crash");
        if (!session->detach) {
          Status quit_status = session->chrome->Quit();
          if (quit_status.IsError())
            message += ", but failed to kill browser:" + quit_status.message();
        }
        status = Status(kUnknownError, message, status);
      } else if (status.code() == kDisconnected) {
        // Some commands, like clicking a button or link which closes the
        // window, may result in a kDisconnected error code.
        std::list<std::string> web_view_ids;
        Status status_tmp = session->chrome->GetWebViewIds(
          &web_view_ids, session->w3c_compliant);
        if (status_tmp.IsError() && status_tmp.code() != kChromeNotReachable) {
          status.AddDetails(
              "failed to check if window was closed: " + status_tmp.message());
        } else if (!base::ContainsValue(web_view_ids, session->window)) {
          status = Status(kOk);
        }
      }
      if (status.IsError()) {
        const BrowserInfo* browser_info = session->chrome->GetBrowserInfo();
        status.AddDetails("Session info: " + browser_info->browser_name + "=" +
                          browser_info->browser_version);
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
        // Note: ChromeDriver log-replay depends on the format of this logging.
        // see chromedriver/log_replay/client_replay.py
        VLOG(0) << "[" << session->id << "] "
                << "RESPONSE " << command_name
                << (result.length() ? " " + result : "");
      }
    }

    if (status.IsOk() && session->auto_reporting_enabled) {
      std::string message = session->GetFirstBrowserError();
      if (!message.empty())
        status = Status(kUnknownError, message);
    }
  }

  cmd_task_runner->PostTask(
      FROM_HERE, base::BindOnce(callback_on_cmd, status, std::move(value),
                                session->id, session->w3c_compliant));

  if (session->quit) {
    SetThreadLocalSession(std::unique_ptr<Session>());
    delete session;
    cmd_task_runner->PostTask(FROM_HERE, terminate_on_cmd);
  }
}

}  // namespace

void ExecuteSessionCommand(
    SessionThreadMap* session_thread_map,
    const char* command_name,
    const SessionCommand& command,
    bool return_ok_without_session,
    const base::DictionaryValue& params,
    const std::string& session_id,
    const CommandCallback& callback) {
  auto iter = session_thread_map->find(session_id);
  if (iter == session_thread_map->end()) {
    Status status(return_ok_without_session ? kOk : kInvalidSessionId);
    callback.Run(status, std::unique_ptr<base::Value>(), session_id,
                 kW3CDefault);
  } else {
    iter->second->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ExecuteSessionCommandOnSessionThread, command_name,
                       command, return_ok_without_session,
                       base::WrapUnique(params.DeepCopy()),
                       base::ThreadTaskRunnerHandle::Get(), callback,
                       base::Bind(&TerminateSessionThreadOnCommandThread,
                                  session_thread_map, session_id)));
  }
}

namespace internal {

void CreateSessionOnSessionThreadForTesting(const std::string& id) {
  SetThreadLocalSession(std::make_unique<Session>(id));
}

}  // namespace internal
