// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_PARSERS_PARSER_UTILS_PARSE_TASKS_REMAINING_COUNTER_H_
#define CHROME_CHROME_CLEANER_PARSERS_PARSER_UTILS_PARSE_TASKS_REMAINING_COUNTER_H_

#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"

namespace chrome_cleaner {

class ParseTasksRemainingCounter
    : public base::RefCountedThreadSafe<ParseTasksRemainingCounter> {
 public:
  ParseTasksRemainingCounter(size_t count, base::WaitableEvent* done);
  void Increment();
  void Decrement();

 private:
  friend class base::RefCountedThreadSafe<ParseTasksRemainingCounter>;
  ~ParseTasksRemainingCounter() = default;

  size_t count_;
  base::WaitableEvent* done_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_PARSERS_PARSER_UTILS_PARSE_TASKS_REMAINING_COUNTER_H_
