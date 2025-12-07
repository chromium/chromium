// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/condition_variable.h"

#include "base/time/time.h"
#include "chrome/services/sharing/nearby/platform/mutex.h"

namespace nearby::chrome {

ConditionVariable::ConditionVariable(Mutex* mutex)
    : mutex_(mutex), condition_variable_(&mutex_->lock_) {}

ConditionVariable::~ConditionVariable() = default;

Exception ConditionVariable::Wait() {
  condition_variable_.Wait();
  return {Exception::kSuccess};
}

Exception ConditionVariable::Wait(absl::Duration timeout) {
  condition_variable_.TimedWait(
      base::Microseconds(absl::ToInt64Microseconds(timeout)));
  return {Exception::kSuccess};
}

void ConditionVariable::Notify() {
  condition_variable_.Broadcast();
}

}  // namespace nearby::chrome
