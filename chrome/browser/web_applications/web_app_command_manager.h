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
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/web_applications/commands/internal/command_internal.h"
#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"
#include "components/webapps/common/web_app_id.h"

class Profile;
class ProfileManager;

namespace content {
class WebContents;
}

namespace webapps {
class WebAppUrlLoader;
}

namespace web_app {
class WebAppProvider;

// The command manager is used to schedule commands or callbacks to write & read
// from the WebAppProvider system. To use, simply call `ScheduleCommand` to
// schedule the given command or a CallbackCommand with given callback.
//
// Commands will be executed (`StartWithLock()` will be called) in-order based
// on command's `Lock`, the `Lock` specifies which apps or particular entities
// it wants to lock on. The next command will not execute until
// `CompleteAndSelfDestruct()` was called by the last command.
class WebAppCommandManager : public ProfileManagerObserver {
 public:
  using PassKey = base::PassKey<WebAppCommandManager>;

  explicit WebAppCommandManager(Profile* profile);
  ~WebAppCommandManager() override;

  void SetProvider(base::PassKey<WebAppProvider>, WebAppProvider& provider);

  // Starts running commands.
  void Start();

  // Enqueues the given command in the queue corresponding to the command's
  // `lock_description()`. `Start()` will always be called asynchronously.
  void ScheduleCommand(std::unique_ptr<internal::CommandBase> command,
                       const base::Location& location = FROM_HERE);

  // Clears shared web contents if it is not used by any commands.
  void ClearSharedWebContentsIfUnused();

  // Called on system shutdown. This call is also forwarded to any commands that
  // have been `Start()`ed.
  void Shutdown();

  // Outputs a debug value of the state of the commands system, including
  // running and queued commands. This is not designed to be used in production
  // or tests, and the format can change frequently (so do not use it).
  base::Value ToDebugValue();

  void LogToInstallManager(base::Value::Dict);

  // Returns whether an installation is already scheduled with the same web
  // contents.
  bool IsInstallingForWebContents(
      const content::WebContents* web_contents) const;

  std::size_t GetCommandCountForTesting();
  int GetStartedCommandCountForTesting();

  std::size_t GetCommandsInstallingForWebContentsForTesting();

  void AwaitAllCommandsCompleteForTesting();

  content::WebContents* web_contents_for_testing() const {
    return shared_web_contents_.get();
  }
  void SetOnWebContentsCreatedCallbackForTesting(
      base::OnceClosure on_web_contents_created);

  WebAppLockManager& lock_manager() { return lock_manager_; }

  // Only used by `WebAppLockManager` to give web contents access to certain
  // locks.
  content::WebContents* EnsureWebContentsCreated(
      base::PassKey<WebAppLockManager>);

  void OnCommandComplete(base::PassKey<internal::CommandBase>,
                         internal::CommandBase* running_command,
                         CommandResult result,
                         base::OnceClosure completion_callback);

  // ProfileManagerObserver:
  void OnProfileMarkedForPermanentDeletion(
      Profile* profile_to_be_deleted) override;
  void OnProfileManagerDestroying() override;

 private:
  void AddCommandToLog(const internal::CommandBase& value);
  void AddValueToLog(base::Value value);

  void StartCommand(base::WeakPtr<internal::CommandBase> command,
                    base::OnceClosure start_command);

  content::WebContents* EnsureWebContentsCreated();

  SEQUENCE_CHECKER(command_sequence_checker_);

  std::vector<std::pair<std::unique_ptr<internal::CommandBase>, base::Location>>
      commands_waiting_for_start_;

  raw_ptr<Profile> profile_ = nullptr;
  raw_ptr<WebAppProvider> provider_ = nullptr;

  std::unique_ptr<content::WebContents> shared_web_contents_;
  std::unique_ptr<webapps::WebAppUrlLoader> url_loader_;

  bool started_ = false;
  bool is_in_shutdown_ = false;
  std::deque<base::Value> command_debug_log_;

  WebAppLockManager lock_manager_;

  std::map<internal::CommandBase::Id, std::unique_ptr<internal::CommandBase>>
      commands_;

  base::OnceClosure on_web_contents_created_for_testing_;
  std::unique_ptr<base::RunLoop> run_loop_for_testing_;

  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};
  base::WeakPtrFactory<WebAppCommandManager>
      weak_ptr_factory_reset_on_shutdown_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_COMMAND_MANAGER_H_
