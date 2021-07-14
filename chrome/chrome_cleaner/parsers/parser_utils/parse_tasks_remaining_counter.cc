// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/parsers/parser_utils/parse_tasks_remaining_counter.h"

#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"

namespace chrome_cleaner {

ParseTasksRemainingCounter::ParseTasksRemainingCounter(
    size_t count,
    base::WaitableEvent* done)
    : count_(count), done_(done) {
  DCHECK(count_ > 0) << "Must be constructed with a positive count.";
}

void ParseTasksRemainingCounter::Increment() {
  DCHECK(count_ > 0)
      << "Once decremented to zero, Increment should never be called.";
  count_++;
}

void ParseTasksRemainingCounter::Decrement() {
  DCHECK(count_);
  count_--;
  if (count_ == 0) {
    done_->Signal();
  }
}

}  // namespace chrome_cleaner
