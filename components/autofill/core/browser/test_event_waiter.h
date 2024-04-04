// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_EVENT_WAITER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_EVENT_WAITER_H_

#include <list>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

// EventWaiter is used to wait on specific events that may have occured
// before the call to Wait(), or after, in which case a RunLoop is used.
//
// Usage:
// waiter_ = std::make_unique<EventWaiter>({ ... });
//
// Do stuff, which (a)synchronously calls waiter_->OnEvent(...).
//
// waiter_->Wait();  <- Will either return right away if events were observed,
//                   <- or use a RunLoop's Run/Quit to wait.
//
// Optionally, event waiters can be quit the RunLoop after timing out.
template <typename Event>
class EventWaiter {
 public:
  explicit EventWaiter(std::list<Event> expected_event_sequence,
                       base::TimeDelta timeout = base::Seconds(0),
                       base::Location location = FROM_HERE);

  EventWaiter(const EventWaiter&) = delete;
  EventWaiter& operator=(const EventWaiter&) = delete;

  ~EventWaiter();

  // Either returns right away if all events were observed between this
  // object's construction and this call to Wait(), or use a RunLoop to wait
  // for them.
  [[nodiscard]] testing::AssertionResult Wait();

  // Observes an event (quits the RunLoop if we are done waiting).
  void OnEvent(Event event);

 private:
  std::list<Event> expected_events_;
  base::TimeDelta timeout_;
  base::Location location_;
  base::RunLoop run_loop_;
  // Collects failure messages that occur during Wait().
  std::list<testing::Message> failure_messages_;
};

template <typename Event>
EventWaiter<Event>::EventWaiter(std::list<Event> expected_event_sequence,
                                base::TimeDelta timeout,
                                base::Location location)
    : expected_events_(std::move(expected_event_sequence)),
      timeout_(timeout),
      location_(location) {
  if (!timeout.is_zero()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop_.QuitClosure(), timeout);
  }
}

template <typename Event>
EventWaiter<Event>::~EventWaiter() {}

template <typename Event>
testing::AssertionResult EventWaiter<Event>::Wait() {
  if (expected_events_.empty())
    return testing::AssertionSuccess();

  DCHECK(!run_loop_.running());
  run_loop_.Run();

  if (!expected_events_.empty()) {
    failure_messages_.push_back(
        testing::Message()
        << expected_events_.size()
        << " expected event(s) still pending after RunLoop timeout of "
        << timeout_ << ", ");
  }

  if (failure_messages_.empty()) {
    return testing::AssertionSuccess();
  }

  testing::AssertionResult failure_result = testing::AssertionFailure();
  for (auto message : failure_messages_) {
    failure_result << message;
  }
  failure_result << "from EventWaiter created in " << location_.ToString();
  return failure_result;
}

template <typename Event>
void EventWaiter<Event>::OnEvent(Event actual_event) {
  if (expected_events_.empty())
    return;

  if (expected_events_.front() != actual_event) {
    failure_messages_.push_back(
        testing::Message() << "Expected:'"
                           << ::testing::PrintToString(expected_events_.front())
                           << "' but received:`"
                           << ::testing::PrintToString(actual_event) << "', ");
  }
  expected_events_.pop_front();
  // Only quit the loop if no other events are expected.
  if (expected_events_.empty() && run_loop_.running())
    run_loop_.Quit();
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_EVENT_WAITER_H_
