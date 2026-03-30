// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_COMMAND_H_

#include <iterator>
#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/time_formatting.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/to_string.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/command_result.h"
#include "chrome/browser/web_applications/commands/internal/command_internal.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/common/web_app_id.h"

namespace content {
class WebContents;
}

namespace web_app {

class LockDescription;
class WebAppCommandManager;
class WebAppLockManager;

// Encapsulates code that reads or modifies the WebAppProvider system. All
// reading or writing to the system should occur in a WebAppCommand to ensure
// that it is isolated. Reading can also happen in any WebAppRegistrar observer.
//
// Commands allow an operation to:
// - Ensure that resources are not being used by another operation (e.g. no
//   other operation is operating on the given app id).
// - Automatically handles edge cases like profile shutdown.
// - Prevent any possible re-entry bugs by allowing any final callback to be
//   called after the command is destructed.
// - Record detailed logs that are exposed in chrome://web-app-internals.
//
// For simple operations that require holding on to lock only for single
// synchronous function calls, WebAppCommandScheduler::ScheduleCallback*
// can be used instead of creating a sub-class.
//
// To create a command sub-class, extend the below type `WebAppCommand,
// which allows specification of the type of lock to retrieve. For example:
//
// class GetAppInformationForMySystem
//    : public WebAppCommand<AppLock, CallbackArgType> {
//   GetAppInformationForMySystem(ReportBackInformationCallback callback)
//       : WebAppCommand(std::move(callback),
//         /*args_for_shutdown*/=CallbackArgType::kShutdownValue) {}
//   ...
//   void StartWithLock(std::unique_ptr<AppLock> lock) {
//     ...
//
//     ...
//     CompleteAndSelfDestruct(
//         CommandResult::kSuccess,
//         lock.<information>());
//   }
//   ...
//
//   // Implement this if installing from an external web contents.
//   WebContents* GetInstallingWebContents(...) override;
// };
//
// See the `WebAppLockManager` for information about the available locks & how
// they work.
//
// Commands can only be started by enqueueing the command in the
// WebAppCommandManager, which is done by the WebAppCommandScheduler. When a
// command is complete, it can call `CompleteAndSelfDestruct` to signal
// completion and self-destruct.
//
// Call pattern of commands:
// - StartWithLock(),
// - <subclass stuff>
// - <subclass calls CompleteAndSelfDestruct()>.
//
// The command can use the following optional features:
// - Populate the `GetMutableDebugValue()` with information that is useful for
//   debugging - this shown in chrome://web-app-internals and printed in failed
//   tests.
// - To prevent multiple installs occurring at the same time for a given
//   `WebContents`, installations that install from an external `WebContents`
//   should override `GetInstallingWebContents()` and return that WebContents.
// - `OnShutdown()` can be overridden to do stateless tasks like recording
//   metrics.
//
// Invariants:
// * Destruction can occur without `StartWithLock()` being called. If the system
//   shuts down and the command was never started, then it will simply be
//   destructed and the `callback` will be called with the
//   `args_for_shutdown`, if they exist.
//
// TODO(dmurph): Add an example of a CL that creates a command.
template <typename LockType, typename... CallbackArgs>
class WebAppCommand : public internal::CommandWithLock<LockType> {
 public:
  using PassKey = base::PassKey<WebAppCommand>;
  using LockDescription = LockType::LockDescription;
  using CallbackType = base::OnceCallback<void(CallbackArgs...)>;
  using ShutdownArgumentsTuple = std::tuple<std::decay_t<CallbackArgs>...>;

  // Special constructor if the callback doesn't take any arguments. There is no
  // need to specify an empty tuple.
  template <std::size_t i = sizeof...(CallbackArgs)>
    requires(i == 0)
  WebAppCommand(const std::string& name,
                LockDescription initial_lock_request,
                CallbackType callback)
      : internal::CommandWithLock<LockType>(name,
                                            std::move(initial_lock_request)),
        callback_(std::move(callback)) {
    CHECK(!callback_.is_null());
  }

  template <std::size_t i = sizeof...(CallbackArgs)>
    requires(i >= 1)
  WebAppCommand(const std::string& name,
                LockDescription initial_lock_request,
                CallbackType callback,
                ShutdownArgumentsTuple args_for_shutdown)
      : internal::CommandWithLock<LockType>(name,
                                            std::move(initial_lock_request)),
        callback_(std::move(callback)),
        args_for_shutdown_(std::move(args_for_shutdown)) {
    CHECK(!callback_.is_null());
  }

  ~WebAppCommand() override = default;

  // Binds the `args_for_shutdown` provided in constructor with the callback,
  // and returns the result. This is called from the WebAppCommandManager during
  // shutdown to call all of the callbacks of uncompleted commands with the
  // appropriate shutdown values.
  base::OnceClosure TakeCallbackWithShutdownArgs(
      base::PassKey<WebAppCommandManager>) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(
        internal::CommandBase::command_sequence_checker_);
    CHECK(!callback_.is_null());
    internal::CommandBase::GetMutableDebugValue().Set("!command_result",
                                                      "kShutdown");
    internal::CommandBase::GetMutableDebugValue().Set(
        "!result", base::ToString(args_for_shutdown_));

    return internal::BindTupleToOnceClosure(std::move(callback_),
                                            std::move(args_for_shutdown_));
  }

 protected:
  // Calling this will destroy the command and allow the next command in the
  // queue to run. This will do the following in order:
  // 1) Destroy this object.
  // 2) Call this command's `callback` with the given `args`.
  // The `result` reports if the command encountered any unknown errors.
  // TODO(dmurph): Use `result` in metrics. https://b/304553492.
  void CompleteAndSelfDestruct(CommandResult result,
                               CallbackArgs... args_for_callback,
                               const base::Location& location = FROM_HERE) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(
        internal::CommandBase::command_sequence_checker_);

    base::DictValue* metadata =
        internal::CommandBase::GetMutableDebugValue().EnsureDict("!metadata");
    CHECK(internal::CommandBase::command_manager())
        << "Command was never given to the command manager: "
        << internal::CommandBase::GetMutableDebugValue().DebugString();
    metadata->Set("command_result",
                  result == CommandResult::kSuccess ? "kSuccess" : "kFailure");
    if constexpr (sizeof...(CallbackArgs) == 1) {
      metadata->Set("!result", base::ToString(args_for_callback...));
    } else if constexpr (sizeof...(CallbackArgs) > 1) {
      metadata->Set(
          "!result",
          base::ToString(std::tie<CallbackArgs&...>(args_for_callback...)));
    }
    metadata->Set("completion_location", base::ToString(location));
    if (base::FeatureList::IsEnabled(features::kRecordWebAppDebugInfo)) {
      metadata->Set("completed_at", base::TimeFormatTimeOfDayWithMilliseconds(
                                        base::Time::Now()));
    }

    // Note: `BindOnce` should correctly handle copying any ref or move
    // arguments internally. This allows the callback arguments to contain ref
    // types (which are standard for mojo callbacks) or move-only types.
    internal::CommandBase::CompleteAndSelfDestructInternal(
        result,
        base::BindOnce(std::move(callback_),
                       std::forward<CallbackArgs>(args_for_callback)...));
  }

 private:
  CallbackType callback_;
  ShutdownArgumentsTuple args_for_shutdown_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_COMMAND_H_
