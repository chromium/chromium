// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_COMMAND_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_COMMAND_MANAGER_H_

#include <deque>
#include <map>
#include <memory>

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"

class Profile;

namespace content {
class WebContents;
}

namespace web_app {

class WebAppProvider;
enum class WebAppUrlLoaderResult;

// The command manager is used to schedule commands or callbacks to write & read
// from the WebAppProvider system. To use, simply call `ScheduleCommand` to
// schedule the given command or a CallbackCommand with given callback.
//
// Commands will be executed (`StartWithLock()` will be called) in-order based
// on command's `WebAppCommandLock`, the `WebAppCommandLock` specifies which
// apps or particular entities it wants to lock on. The next command will not
// execute until `SignalCompletionAndSelfDestruct()` was called by the last
// command.
class WebAppCommandManager {
 public:
  using PassKey = base::PassKey<WebAppCommandManager>;

  explicit WebAppCommandManager(Profile* profile);
  ~WebAppCommandManager();

  void SetProvider(base::PassKey<WebAppProvider>, WebAppProvider& provider);

  // Starts running commands.
  void Start();

  // Enqueues the given command in the queue corresponding to the command's
  // `lock_description()`. `Start()` will always be called asynchronously.
  void ScheduleCommand(std::unique_ptr<WebAppCommand> command,
                       const base::Location& location = FROM_HERE);

  // Called on system shutdown. This call is also forwarded to any commands that
  // have been `Start()`ed.
  void Shutdown();

  // Outputs a debug value of the state of the commands system, including
  // running and queued commands.
  base::Value ToDebugValue();

  void LogToInstallManager(base::Value::Dict);

  // Returns whether an installation is already scheduled with the same web
  // contents.
  bool IsInstallingForWebContents(
      const content::WebContents* web_contents) const;

  std::size_t GetCommandCountForTesting() { return commands_.size(); }

  void AwaitAllCommandsCompleteForTesting();

  bool has_web_contents_for_testing() const {
    return shared_web_contents_.get();
  }

  WebAppLockManager& lock_manager() { return lock_manager_; }

  // Only used by `WebAppLockManager` to give web contents access to certain
  // locks.
  content::WebContents* EnsureWebContentsCreated(
      base::PassKey<WebAppLockManager>);

 protected:
  friend class WebAppCommand;

  void OnCommandComplete(WebAppCommand* running_command,
                         CommandResult result,
                         base::OnceClosure completion_callback);

 private:
  void AddValueToLog(base::Value value);

  void OnLockAcquired(WebAppCommand::Id command_id,
                      base::OnceClosure start_command);

  void StartCommand(WebAppCommand* command, base::OnceClosure start_command);

  content::WebContents* EnsureWebContentsCreated();

  SEQUENCE_CHECKER(command_sequence_checker_);

  std::vector<std::unique_ptr<WebAppCommand>> commands_waiting_for_start_;

  raw_ptr<Profile> profile_ = nullptr;
  raw_ptr<WebAppProvider> provider_ = nullptr;

  std::unique_ptr<content::WebContents> shared_web_contents_;

  bool started_ = false;
  bool is_in_shutdown_ = false;
  std::deque<base::Value> command_debug_log_;

  WebAppLockManager lock_manager_;

  std::map<WebAppCommand::Id, std::unique_ptr<WebAppCommand>> commands_{};

  std::unique_ptr<base::RunLoop> run_loop_for_testing_;

  base::WeakPtrFactory<WebAppCommandManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_COMMAND_MANAGER_H_
