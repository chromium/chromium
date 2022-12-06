// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/callback_command.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/full_system_lock.h"
#include "chrome/browser/web_applications/locks/lock.h"
#include "chrome/browser/web_applications/locks/noop_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"

namespace web_app {

template <class LockType, class DescriptionType>
CallbackCommand<LockType, DescriptionType>::CallbackCommand(
    const std::string& name,
    std::unique_ptr<DescriptionType> lock_description,
    base::OnceCallback<void(LockType& lock)> callback)
    : CallbackCommand<LockType, DescriptionType>::CallbackCommand(
          name,
          std::move(lock_description),
          // Return an empty base::Value() as the debug value.
          std::move(callback).Then(
              base::BindOnce([]() { return base::Value(); }))) {}

template <class LockType, class DescriptionType>
CallbackCommand<LockType, DescriptionType>::CallbackCommand(
    const std::string& name,
    std::unique_ptr<DescriptionType> lock_description,
    base::OnceCallback<base::Value(LockType& lock)> callback)
    : WebAppCommandTemplate<LockType>(name),
      lock_description_(std::move(lock_description)),
      callback_(std::move(callback)) {
  DCHECK(lock_description_);
}

template <class LockType, class DescriptionType>
CallbackCommand<LockType, DescriptionType>::~CallbackCommand() = default;

template <class LockType, class DescriptionType>
void CallbackCommand<LockType, DescriptionType>::StartWithLock(
    std::unique_ptr<LockType> lock) {
  lock_ = std::move(lock);
  debug_value_ = std::move(callback_).Run(*lock_.get());
  this->SignalCompletionAndSelfDestruct(CommandResult::kSuccess,
                                        base::DoNothing());
}

template <class LockType, class DescriptionType>
LockDescription& CallbackCommand<LockType, DescriptionType>::lock_description()
    const {
  return *lock_description_;
}

template <class LockType, class DescriptionType>
base::Value CallbackCommand<LockType, DescriptionType>::ToDebugValue() const {
  return debug_value_.Clone();
}

template class CallbackCommand<NoopLock>;
template class CallbackCommand<SharedWebContentsLock>;
template class CallbackCommand<AppLock>;
template class CallbackCommand<SharedWebContentsWithAppLock>;
template class CallbackCommand<FullSystemLock>;

}  // namespace web_app
