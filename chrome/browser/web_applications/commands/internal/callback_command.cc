// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/internal/callback_command.h"

#include <utility>

#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/noop_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"

namespace web_app::internal {

template <typename LockType>
CallbackCommand<LockType>::CallbackCommand(
    const std::string& name,
    DescriptionType lock_description,
    CallbackType callback_closure,
    CompletionCallbackType completion_callback)
    : web_app::WebAppCommand<LockType>(name,
                                       std::move(lock_description),
                                       std::move(completion_callback)),
      callback_(std::move(callback_closure)) {}

template <typename LockType>
CallbackCommand<LockType>::~CallbackCommand() {}

template <typename LockType>
void CallbackCommand<LockType>::StartWithLock(std::unique_ptr<LockType> lock) {
  std::move(callback_).Run(*lock,
                           internal::CommandBase::GetMutableDebugValue());
  WebAppCommand<LockType>::CompleteAndSelfDestruct(CommandResult::kSuccess);
  return;
}

template class CallbackCommand<NoopLock>;
template class CallbackCommand<SharedWebContentsLock>;
template class CallbackCommand<AppLock>;
template class CallbackCommand<SharedWebContentsWithAppLock>;
template class CallbackCommand<AllAppsLock>;

}  // namespace web_app::internal
