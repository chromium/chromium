// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_IOS_TEST_EVENT_WAITER_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_IOS_TEST_EVENT_WAITER_H_

#include <list>

#import "base/test/ios/wait_util.h"
#import "base/time/time.h"

namespace autofill {

// IOSTestEventWaiter is used to wait on given events that may have occurred
// before call to Wait(), or after, in which case a |timeout| should be provided
// to wait for those events to occur until |timeout| expires.
//
// Usage:
// waiter_ = std::make_unique<IOSTestEventWaiter>({ ... });
//
// Do stuff, which (a)synchronously calls waiter_->OnEvent(...).
//
// waiter_->Wait();
template <typename Event>
class IOSTestEventWaiter {
 public:
  IOSTestEventWaiter(std::list<Event> expected_events, base::TimeDelta timeout);

  IOSTestEventWaiter(const IOSTestEventWaiter&) = delete;
  IOSTestEventWaiter& operator=(const IOSTestEventWaiter&) = delete;

  ~IOSTestEventWaiter() = default;

  // Either returns true right away if all events were observed between this
  // object's construction and this call to Wait(); or returns true if that
  // condition is met before |timeout|; Otherwise returns false. If |timeout| is
  // zero, a reasonable default is used. Returns false if the current NSRunLoop
  // is already running.
  bool Wait();

  // Observes an event. Returns false if the event is unexpected and true
  // Otherwise.
  bool OnEvent(Event event);

 private:
  std::list<Event> expected_events_;
  bool runloop_running_;
  base::TimeDelta timeout_;
};

template <typename Event>
IOSTestEventWaiter<Event>::IOSTestEventWaiter(std::list<Event> expected_events,
                                              base::TimeDelta timeout)
    : expected_events_(std::move(expected_events)),
      runloop_running_(false),
      timeout_(timeout) {}

template <typename Event>
bool IOSTestEventWaiter<Event>::Wait() {
  if (expected_events_.empty())
    return true;

  if (runloop_running_)
    return false;

  runloop_running_ = true;
  bool result = base::test::ios::WaitUntilConditionOrTimeout(timeout_, ^{
    return expected_events_.empty();
  });
  runloop_running_ = false;

  return result;
}

template <typename Event>
bool IOSTestEventWaiter<Event>::OnEvent(Event event) {
  if (expected_events_.empty() || expected_events_.front() != event)
    return false;

  expected_events_.pop_front();
  return true;
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_IOS_TEST_EVENT_WAITER_H_
