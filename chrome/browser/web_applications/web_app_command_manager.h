// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_COMMAND_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_COMMAND_MANAGER_H_

#include <deque>
#include <map>
#include <memory>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_url_loader.h"
#include "components/services/storage/indexed_db/locks/disjoint_range_lock_manager.h"
#include "components/services/storage/indexed_db/locks/leveled_lock_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace content {
class WebContents;
}

namespace web_app {

class WebAppInstallManager;

// The command manager is used to schedule commands or callbacks to write & read
// from the WebAppProvider system. To use, simply call `ScheduleCommand` to
// schedule the given command or a CallbackCommand with given callback.
//
// Commands will be executed (`Start()` will be called) in-order based on
// command's `WebAppCommandLock`, the `WebAppCommandLock` specifies which apps
// or particular entities it wants to lock on. The next command will not execute
// until `SignalCompletionAndSelfDestruct()` was called by the last command.
class WebAppCommandManager {
 public:
  using PassKey = base::PassKey<WebAppCommandManager>;

  explicit WebAppCommandManager(Profile* profile);
  ~WebAppCommandManager();

  // Enqueues the given command in the queue corresponding to the command's
  // `queue_id()`. `Start()` will always be called asynchronously.
  void ScheduleCommand(std::unique_ptr<WebAppCommand> command);

  // Called on system shutdown. This call is also forwarded to any commands that
  // have been `Start()`ed.
  void Shutdown();

  // Called by the sync integration when a list of apps have had their sync
  // sources removed and `is_uninstalling()` set to true. Any commands that
  // whose `queue_id()`s match an id in `app_id` who have also been `Start()`ed
  // will also be notified.
  void NotifySyncSourceRemoved(const std::vector<AppId>& app_ids);

  // Outputs a debug value of the state of the commands system, including
  // running and queued commands.
  base::Value ToDebugValue();

  void SetSubsystems(WebAppInstallManager* install_manager);
  void LogToInstallManager(base::Value);

  // Returns whether an installation is already scheduled with the same web
  // contents.
  bool IsInstallingForWebContents(
      const content::WebContents* web_contents) const;

  std::size_t GetCommandCountForTesting() { return commands_.size(); }

  void AwaitAllCommandsCompleteForTesting();

  // TODO(https://crbug.com/1329934): Figure out better ownership of this.
  void SetUrlLoaderForTesting(std::unique_ptr<WebAppUrlLoader> url_loader);

  bool has_web_contents_for_testing() const {
    return shared_web_contents_.get();
  }

 protected:
  friend class WebAppCommand;

  void OnCommandComplete(WebAppCommand* running_command,
                         CommandResult result,
                         base::OnceClosure completion_callback);

 private:
  void AddValueToLog(base::Value value);

  void OnLockAcquired(WebAppCommand::Id command_id);

  void StartCommandOrPrepareForLoad(WebAppCommand* command);

  void OnAboutBlankLoadedForCommandStart(WebAppCommand* command,
                                         WebAppUrlLoader::Result result);

  content::WebContents* EnsureWebContentsCreated();

  struct CommandState {
    explicit CommandState(std::unique_ptr<WebAppCommand> command);
    ~CommandState();

    std::unique_ptr<WebAppCommand> command;
    content::LeveledLockHolder lock_holder;
  };

  SEQUENCE_CHECKER(command_sequence_checker_);

  std::map<WebAppCommand::Id, CommandState> commands_{};

  raw_ptr<Profile> profile_;
  // TODO(https://crbug.com/1329934): Figure out better ownership of this.
  // Perhaps set as subsystem?
  std::unique_ptr<WebAppUrlLoader> url_loader_;
  std::unique_ptr<content::WebContents> shared_web_contents_;

  bool is_in_shutdown_ = false;
  std::deque<base::Value> command_debug_log_;

  content::DisjointRangeLockManager lock_manager_{
      static_cast<int>(WebAppCommandLock::LockLevel::kMaxValue) + 1};

  raw_ptr<WebAppInstallManager> install_manager_;

  std::unique_ptr<base::RunLoop> run_loop_for_testing_;

  base::WeakPtrFactory<WebAppCommandManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_COMMAND_MANAGER_H_
