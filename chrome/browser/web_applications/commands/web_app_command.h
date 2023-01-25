// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_COMMAND_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class WebContents;
}

namespace web_app {

class LockDescription;
class WebAppCommandManager;
class WebAppLockManager;

enum class CommandResult { kSuccess, kFailure, kShutdown };

// Each command has a queue id, which is either an `AppId` corresponding to a
// specific web app, or `absl::nullopt` for the global queue. The global queue
// is independent (does not block) of other queues.
using WebAppCommandQueueId = absl::optional<AppId>;

// Encapsulates code that reads or modifies the WebAppProvider system. All
// reading or writing to the system should occur in a WebAppCommand to ensure
// that it is isolated. Reading can also happen in any WebAppRegistrar observer.
//
// Commands can only be started by either enqueueing the command in the
// WebAppCommandManager or by having the command be "chained" from another
// command.
// When a command is complete, it can call `SignalCompletionAndSelfDestruct` to
// signal completion and self-destruct. The command can pass a list of "chained"
// commands to run next as part of this operation. This allows for commands to
// re-use each other easily.
//
// Invariants:
// * Destruction can occur without `StartWithLock()` being called. If the system
// shuts
//   down and the command was never started, then it will simply be destructed.
// * `OnShutdown()` and `OnSyncSourceRemoved()` are only called if
//   the command has been started.
// * `SignalCompletionAndSelfDestruct()` can ONLY be called if `StartWithLock()`
//    has been called. Otherwise it will CHECK-fail.
class WebAppCommand {
 public:
  using Id = int;
  explicit WebAppCommand(const std::string& name);
  virtual ~WebAppCommand();

  // Returns if the command has been started yet.
  bool IsStarted() const { return command_manager() != nullptr; }

  // Unique id generated for this command. Currently only used for debug values.
  Id id() const { return id_; }

  const std::string& name() const { return name_; }

  // Returns a debug value to log the state of the command. Used in
  // chrome://web-app-internals.
  virtual base::Value ToDebugValue() const = 0;

 protected:
  virtual const LockDescription& lock_description() const = 0;

  // Returns the pre-existing web contents the installation was
  // initiated with. Only implements this when the command is used for
  // installation and uses a pre-existing web contents.
  virtual content::WebContents* GetInstallingWebContents();

  using LockAcquiredCallback =
      base::OnceCallback<void(base::OnceClosure start_command)>;

  // Triggered by the WebAppCommandManager. Request lock and start the command
  // after the lock is acquired.
  virtual void RequestLock(WebAppCommandManager* command_manager,
                           WebAppLockManager* lock_manager,
                           LockAcquiredCallback on_lock_acquired) = 0;

  // This is called when the sync system has triggered an uninstall for an app
  // id that is relevant to this command and this command is running
  // (`StartWithLock()` has been called). Relevance is determined by the
  // `WebAppCommandLock::IsAppLocked()` function for this command's lock). The
  // web app should still be in the registry, but it will no longer have the
  // `WebAppManagement::kSync` source and `is_uninstalling()` will return true.
  virtual void OnSyncSourceRemoved() = 0;

  // Signals the system is shutting down. Used to cancel any pending operations,
  // if possible, to prevent re-entry. Only called if the command has been
  // started.
  virtual void OnShutdown() = 0;

  // Calling this will destroy the command and allow the next command in the
  // queue to run. The caller can optionally schedule chained commands.
  // Arguments:
  // `call_after_destruction`: If the command has a closure that
  //                           needs to be called on the completion  of the
  //                           command, it can be passed here to ensure it is
  //                           called after this  command is destructed and any
  //                           chained  commands are queued.
  // Note: This can ONLY be called if `StartWithLock()` has been called
  // (`IsStarted()` is true). Otherwise it will CHECK-fail.
  void SignalCompletionAndSelfDestruct(
      CommandResult result,
      base::OnceClosure call_after_destruction);

  virtual WebAppCommandManager* command_manager() const;

  SEQUENCE_CHECKER(command_sequence_checker_);

 private:
  friend class WebAppCommandManager;

  base::WeakPtr<WebAppCommand> AsWeakPtr();

  Id id_;
  std::string name_;
  raw_ptr<WebAppCommandManager> command_manager_ = nullptr;

  base::WeakPtrFactory<WebAppCommand> weak_factory_{this};
};

template <typename LockType>
class WebAppCommandTemplate : public WebAppCommand {
 public:
  explicit WebAppCommandTemplate(const std::string& name);
  ~WebAppCommandTemplate() override;

  // Triggered after lock is acquired. Signals that this command can
  // start its operations. When this command is complete, it should call
  // `SignalCompletionAndSelfDestruct` to signal it's completion and destruct
  // itself. Note: It is not guaranteed that the web app this command was
  // created for is still installed. All state must be re-checked when this
  // method is called.
  virtual void StartWithLock(std::unique_ptr<LockType> lock) = 0;

 protected:
  // WebAppCommand:
  WebAppCommandManager* command_manager() const override {
    return command_manager_;
  }

 private:
  // WebAppCommand:
  void RequestLock(WebAppCommandManager* command_manager,
                   WebAppLockManager* lock_manager,
                   LockAcquiredCallback on_lock_acquired) override;

  void PrepareForStart(WebAppCommandManager* command_manager,
                       LockAcquiredCallback on_lock_acquired,
                       std::unique_ptr<LockType> lock);

  raw_ptr<WebAppCommandManager> command_manager_ = nullptr;

  base::WeakPtrFactory<WebAppCommandTemplate> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_COMMAND_H_
