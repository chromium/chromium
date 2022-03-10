// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_COMMAND_H_
#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {

class WebAppCommandManager;

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
// * Destruction can occur without `Start()` being called. If the system shuts
//   down and the command was never started, then it will simply be destructed.
// * `OnShutdown()` and `OnBeforeForcedUninstallFromSync()` are only called if
//   the command has been started.
// * `SignalCompletionAndSelfDestruct()` can ONLY be called if `Start()` has
//   been called. Otherwise it will CHECK-fail.
class WebAppCommand {
 public:
  using Id = int;
  explicit WebAppCommand(WebAppCommandQueueId queue_id);
  virtual ~WebAppCommand();

  // Unique id generated for this command. Currently only used for debug values.
  Id id() const { return id_; }
  // If this command was scheduled from another command (calling
  // `StartNestedCommand`), then this value is populated. currently only used
  // for debug values.
  absl::optional<Id> parent_id() const { return parent_id_; }

  // The queue for this command.
  WebAppCommandQueueId queue_id() const { return queue_id_; }

  // Returns if the command has been started yet.
  bool IsStarted() const { return command_manager_ != nullptr; }

  // Returns a debug value to log the state of the command. Used in
  // chrome://web-app-internals.
  virtual base::Value ToDebugValue() const = 0;

 protected:
  // Triggered by the WebAppCommandManager. Signals that this command can
  // start its operations. When this command is complete, it should call
  // `SignalCompletionAndSelfDestruct` to signal it's completion and destruct
  // itself. Note: It is not guaranteed that the web app this command was
  // created for is still installed. All state must be re-checked when method
  // this is called.
  virtual void Start() = 0;

  // This is called when the sync system has to be force uninstall a web app
  // that matches the `queue_id()` AND `Start()` has been called on this
  // command. The web app should still be in the registry at the time of this
  // method call, but it will be immediately deleted afterwards.
  virtual void OnBeforeForcedUninstallFromSync() = 0;

  // Signals the system is shutting down. Used to cancel any pending operations,
  // if possible, to prevent re-entry. Only called if the command has been
  // started.
  virtual void OnShutdown() = 0;

  // Calling this will destroy the command and allow the next command in the
  // queue to run. The caller can optionally schedule chained commands.
  // Arguments:
  // `call_after_destruction`: If the command has a closure that
  //                           needs to be called on the completion  of the
  //                           command, it can be passed here   to ensure it is
  //                           called after this  command is destructed and any
  //                           chained  commands are queued.
  // `chained_commands`: If this operation requires more commands to be run
  //                     next, then those commands can be specified here. If
  //                     the `queue_id()` of any of these commands matches the
  //                     existing command, then those commands will be
  //                     executed next (in order). If not, then they will be
  //                     placed a the end of the queue for their respective
  //                     `queue_id()`.
  // Note: This can ONLY be called if `Start()` has been called (`IsStarted()`
  // is true). Otherwise it will CHECK-fail.
  void SignalCompletionAndSelfDestruct(
      CommandResult result,
      base::OnceClosure call_after_destruction,
      std::vector<std::unique_ptr<WebAppCommand>> chained_commands);

  SEQUENCE_CHECKER(command_sequence_checker_);

 private:
  friend class WebAppCommandManager;

  // Start called by the WebAppCommandManager.
  void Start(WebAppCommandManager* command_manager);

  base::WeakPtr<WebAppCommand> AsWeakPtr();

  Id id_;
  absl::optional<Id> parent_id_;
  absl::optional<AppId> queue_id_;
  WebAppCommandManager* command_manager_ = nullptr;

  base::WeakPtrFactory<WebAppCommand> weak_factory_{this};
};
}  // namespace web_app
#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_COMMAND_H_
