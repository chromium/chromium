// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/count_down_latch.h"

#include "base/time/time.h"

namespace nearby::chrome {

CountDownLatch::CountDownLatch(int32_t count)
    : count_(count),
      count_waitable_event_(
          base::WaitableEvent::ResetPolicy::MANUAL,
          count_.IsZero() ? base::WaitableEvent::InitialState::SIGNALED
                          : base::WaitableEvent::InitialState::NOT_SIGNALED) {
  DCHECK_GE(count, 0);
}

CountDownLatch::~CountDownLatch() = default;

Exception CountDownLatch::Await() {
  count_waitable_event_.Wait();
  return {Exception::kSuccess};
}

ExceptionOr<bool> CountDownLatch::Await(absl::Duration timeout) {
  // Return true if |count_waitable_event_| is signaled before TimedAwait()
  // times out. Otherwise, this returns false due to timing out.
  return ExceptionOr<bool>(count_waitable_event_.TimedWait(
      base::Microseconds(absl::ToInt64Microseconds(timeout))));
}

void CountDownLatch::CountDown() {
  // Signal |count_waitable_event_| when (and only the one exact time when)
  // |count_| decrements to 0.
  if (!count_.Decrement())
    count_waitable_event_.Signal();
}

}  // namespace nearby::chrome
