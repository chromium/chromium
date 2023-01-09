// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_PAYMENTS_TEST_EVENT_WAITER_H_
#define CHROME_TEST_PAYMENTS_TEST_EVENT_WAITER_H_

#include <iosfwd>
#include <list>

#include "base/run_loop.h"

namespace payments {

enum class TestEvent : int32_t {
  kCanMakePaymentCalled,
  kCanMakePaymentReturned,
  kHasEnrolledInstrumentCalled,
  kHasEnrolledInstrumentReturned,
  kConnectionTerminated,
  kNotSupportedError,
  kAbortCalled,
  // Received when the list of available apps is created and .show() is called.
  // Note that this does not wait for the payment sheet UI to be actually shown.
  kAppListReady,
  kErrorDisplayed,
  kPaymentCompleted,
  kUIDisplayed,
};

std::ostream& operator<<(std::ostream& out, TestEvent event);

// EventWaiter is used to wait for specified payments TestEvent(s) that may have
// occurred before the call to Wait(), or after, in which case a RunLoop is
// used. While waiting for an event sequence, events must occur in the specified
// order, and while waiting for a single event, arrival of any other event gets
// ignored.
class EventWaiter {
 public:
  EventWaiter(std::list<TestEvent> expected_event_sequence,
              bool wait_for_single_event);

  EventWaiter(const EventWaiter&) = delete;
  EventWaiter& operator=(const EventWaiter&) = delete;

  ~EventWaiter();

  // Either returns right away if all events were observed between this
  // object's construction and this call to Wait(), or use a RunLoop to wait
  // for them.
  bool Wait();

  // Observes arriving events (quits the RunLoop if we are done waiting).
  void OnEvent(TestEvent current_event);

 private:
  std::list<TestEvent> expected_events_;
  base::RunLoop run_loop_;

  // When set to true, the event waiter ignores arrival of any other events
  // while waiting for the expected event to arrive.
  bool wait_for_single_event_;
};

}  // namespace payments

#endif  // CHROME_TEST_PAYMENTS_TEST_EVENT_WAITER_H_
