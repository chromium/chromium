// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INTERNAL_COMMAND_INTERNAL_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INTERNAL_COMMAND_INTERNAL_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/pass_key.h"
#include "base/values.h"

enum class CommandResult;

namespace base {
class Location;
}

namespace content {
class WebContents;
}

namespace web_app {

class WebAppCommandManager;
class WebAppLockManager;

namespace internal {

// Base class of the command, allowing non-templated storage by the
// CommandManager.
class CommandBase {
 public:
  using Id = int;
  explicit CommandBase(std::string name);
  virtual ~CommandBase();

  // Optionally override this to returns the pre-existing web contents the
  // installation was initiated with. Only implement this when the command is
  // used for installation and uses a pre-existing web contents.
  virtual content::WebContents* GetInstallingWebContents(
      base::PassKey<WebAppCommandManager>);

  // Optionally override this to be notified when shutdown happens. Do not
  // change any state in this function, it should only be used for stateless
  // operations like recording metrics.
  // TODO(b/304553492): Remove this after per-command success/failure/shutdown
  // metrics are implemented.
  virtual void OnShutdown(base::PassKey<WebAppCommandManager>) const;

  // Returns if the command has been started yet.
  bool IsStarted() const;

  // Unique id generated for this command.
  Id id() const { return id_; }

  // Debug value used for chrome://web-app-internals. This should not be read
  // from or relied upon for any business logic under any circumstances.
  const base::Value::Dict& GetDebugValue() const;

  base::WeakPtr<CommandBase> GetBaseCommandWeakPtr();

  void SetScheduledLocation(base::PassKey<WebAppCommandManager>,
                            const base::Location& location);

  // Sets the command manager, allowing `CompleteAndSelfDestruct` to
  // work correctly.
  void SetCommandManager(base::PassKey<WebAppCommandManager>,
                         WebAppCommandManager* command_manager);

  // Triggered by the WebAppCommandManager. Request lock and start the command
  // after the lock is acquired.
  using LockAcquiredCallback =
      base::OnceCallback<void(base::OnceClosure start_command)>;
  virtual void RequestLock(base::PassKey<WebAppCommandManager>,
                           WebAppLockManager* lock_manager,
                           LockAcquiredCallback on_lock_acquired,
                           const base::Location& location) = 0;

  // Returns if the command manager should prepare the shared web contents by
  // loading about:blank.
  virtual bool ShouldPrepareWebContentsBeforeStart(
      base::PassKey<WebAppCommandManager>) const = 0;

  // Called by the WebAppCommandManager when this command needs to be destroyed
  // before `StartWithLock` is called.
  virtual base::OnceClosure TakeCallbackWithShutdownArgs(
      base::PassKey<WebAppCommandManager>) = 0;

 protected:
  WebAppCommandManager* command_manager() const;

  // Debug value used for chrome://web-app-internals. Commands can add
  // information here to help document the configuration, operation, and results
  // of a command's run. This should not be read from or relied upon for any
  // business logic under any circumstances.
  // Note that
  base::Value::Dict& GetMutableDebugValue();

  void SetStarted();

  void CompleteAndSelfDestructInternal(CommandResult result,
                                       base::OnceClosure after_destruction);

  SEQUENCE_CHECKER(command_sequence_checker_);

 private:
  const Id id_;
  const std::string name_;

  base::Value::Dict debug_value_;
  bool started_ = false;
  raw_ptr<WebAppCommandManager> command_manager_ = nullptr;

  base::WeakPtrFactory<CommandBase> weak_factory_{this};
};

// This base class implements all of the lock-specific logic, with per-lock
// specializations in the .cc file.
template <typename LockType>
class CommandWithLock : public CommandBase {
 public:
  using LockDescription = LockType::LockDescription;
  explicit CommandWithLock(const std::string& name,
                           LockDescription initial_lock_request);

  ~CommandWithLock() override;

  void RequestLock(base::PassKey<WebAppCommandManager>,
                   WebAppLockManager* lock_manager,
                   LockAcquiredCallback on_lock_acquired,
                   const base::Location& location) final;

  bool ShouldPrepareWebContentsBeforeStart(
      base::PassKey<WebAppCommandManager>) const final;

  const LockDescription& InitialLockRequestForTesting() const {
    return initial_lock_request_;
  }

 protected:
  // Triggered after lock is acquired. Signals that this command can
  // start its operations. When this command is complete, it should call-
  // `CompleteAndSelfDestruct` to signal it's completion and destruct
  // itself. Note: It is not guaranteed that the web app this command was
  // created for is still installed. All state must be re-checked when this
  // method is called.
  virtual void StartWithLock(std::unique_ptr<LockType> lock) = 0;

 private:
  void PrepareForStart(LockAcquiredCallback on_lock_acquired);

  const LockDescription initial_lock_request_;
  std::unique_ptr<LockType> initial_lock_;
  base::WeakPtrFactory<CommandWithLock<LockType>> weak_factory_{this};
};

}  // namespace internal
}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INTERNAL_COMMAND_INTERNAL_H_
