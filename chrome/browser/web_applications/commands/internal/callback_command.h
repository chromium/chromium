// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INTERNAL_CALLBACK_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INTERNAL_CALLBACK_COMMAND_H_

#include <memory>
#include <string>
#include <tuple>
#include <type_traits>

#include "base/functional/callback.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/command_result.h"
#include "chrome/browser/web_applications/commands/internal/command_internal.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/noop_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"

namespace web_app::internal {

// The callback commands allow simple operations that don't involve asynchronous
// work to be commands without needing to create a dedicated class, but still
// offer all of the advantages of commands. All "callback commands" take two
// callbacks, the one to be run as the command, and the one to be run on
// completion. There are specializations here to allow different types of
// completion callback support:
// - CallbackCommand: Equivalent of base::PostTakeAndReply. This is used for
//                    completion callbacks that are simple closures - they don't
//                    take any results from the command callback.
// - CallbackCommandWithResult: Equivalent of base::PostTakeAndReplyWithResult.
//                              This is used if the completion callback takes a
//                              single argument, and thus the command callback
//                              can return that single argument.
//
// For a solution that allows multiple return values / multiple arguments in the
// completion callback, see this file in https://crrev.com/c/5105100/6/.
//
// Note: Prefer using WebAppCommandScheduler::ScheduleCallbackWithLock
// instead of this class directly.
template <typename LockType>
class CallbackCommand : public WebAppCommand<LockType> {
 public:
  using DescriptionType = typename LockType::LockDescription;
  // This is the callback used as the command - it accepts the lock (and debug
  // value).
  using CallbackType =
      base::OnceCallback<void(LockType& lock, base::Value::Dict& debug_value)>;
  // This is the completion callback that is called after the callback above is
  // finished & the command is destroyed.
  using CompletionCallbackType = base::OnceClosure;

  CallbackCommand(const std::string& name,
                  DescriptionType lock_description,
                  CallbackType callback_closure,
                  CompletionCallbackType completion_callback);

  ~CallbackCommand();

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<LockType> lock) override;

 private:
  CallbackType callback_;
};

// Equivalent of base::PostTaskAndReplyWithResult for the command system.
template <typename LockType, typename ItemType>
class CallbackCommandWithResult : public WebAppCommand<LockType, ItemType> {
 public:
  using DescriptionType = typename LockType::LockDescription;
  using ReturnType = std::decay_t<ItemType>;
  // This is the callback used as the command - it accepts the lock (and debug
  // value) and returns the value sent to the completion callback.
  using CallbackType =
      base::OnceCallback<ReturnType(LockType& lock,
                                    base::Value::Dict& debug_value)>;
  // This is the completion callback that is called after the callback above is
  // finished & the command is destroyed. It is called with the object returned
  // from the above callback.
  using CompletionCallbackType = base::OnceCallback<void(ItemType)>;

  CallbackCommandWithResult(const std::string& name,
                            DescriptionType lock_description,
                            CallbackType command_callback,
                            CompletionCallbackType completion_callback,
                            ItemType arg_for_shutdown)
      : WebAppCommand<LockType, ItemType>(
            name,
            std::move(lock_description),
            std::move(completion_callback),
            std::make_tuple(std::move(arg_for_shutdown))),
        callback_(std::move(command_callback)) {}

  ~CallbackCommandWithResult() override {}

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<LockType> lock) override {
    CHECK(lock);
    CHECK(lock->IsGranted());
    ReturnType result = std::move(callback_).Run(
        *lock, internal::CommandBase::GetMutableDebugValue());
    WebAppCommand<LockType, ItemType>::CompleteAndSelfDestruct(
        CommandResult::kSuccess, std::move(result));
  }

 private:
  CallbackType callback_;
};

}  // namespace web_app::internal

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INTERNAL_CALLBACK_COMMAND_H_
