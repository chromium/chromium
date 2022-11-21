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
                       base::TimeDelta timeout = base::Seconds(0));

  EventWaiter(const EventWaiter&) = delete;
  EventWaiter& operator=(const EventWaiter&) = delete;

  ~EventWaiter();

  // Either returns right away if all events were observed between this
  // object's construction and this call to Wait(), or use a RunLoop to wait
  // for them.
  bool Wait();

  // Observes an event (quits the RunLoop if we are done waiting).
  void OnEvent(Event event);

 private:
  std::list<Event> expected_events_;
  base::RunLoop run_loop_;
};

template <typename Event>
EventWaiter<Event>::EventWaiter(std::list<Event> expected_event_sequence,
                                base::TimeDelta timeout)
    : expected_events_(std::move(expected_event_sequence)) {
  if (!timeout.is_zero()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop_.QuitClosure(), timeout);
  }
}

template <typename Event>
EventWaiter<Event>::~EventWaiter() {}

template <typename Event>
bool EventWaiter<Event>::Wait() {
  if (expected_events_.empty())
    return true;

  DCHECK(!run_loop_.running());
  run_loop_.Run();
  return expected_events_.empty();
}

template <typename Event>
void EventWaiter<Event>::OnEvent(Event actual_event) {
  if (expected_events_.empty())
    return;

  ASSERT_EQ(expected_events_.front(), actual_event);
  expected_events_.pop_front();
  // Only quit the loop if no other events are expected.
  if (expected_events_.empty() && run_loop_.running())
    run_loop_.Quit();
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_EVENT_WAITER_H_
