// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/payments/test_event_waiter.h"

#include <iostream>

namespace payments {

std::ostream& operator<<(std::ostream& out, TestEvent event) {
  switch (event) {
    case TestEvent::kCanMakePaymentCalled:
      out << "CanMakePaymentCalled";
      break;
    case TestEvent::kCanMakePaymentReturned:
      out << "CanMakePaymentReturned";
      break;
    case TestEvent::kHasEnrolledInstrumentCalled:
      out << "HasEnrolledInstrumentCalled";
      break;
    case TestEvent::kHasEnrolledInstrumentReturned:
      out << "HasEnrolledInstrumentReturned";
      break;
    case TestEvent::kConnectionTerminated:
      out << "ConnectionTerminated";
      break;
    case TestEvent::kNotSupportedError:
      out << "NotSupportedError";
      break;
    case TestEvent::kAbortCalled:
      out << "AbortCalled";
      break;
    case TestEvent::kAppListReady:
      out << "AppListReady";
      break;
    case TestEvent::kErrorDisplayed:
      out << "ErrorDisplayed";
      break;
    case TestEvent::kPaymentCompleted:
      out << "PaymentCompleted";
      break;
    case TestEvent::kUIDisplayed:
      out << "UIDisplayed";
      break;
  }
  return out;
}

EventWaiter::EventWaiter(std::list<TestEvent> expected_event_sequence,
                         bool wait_for_single_event)
    : expected_events_(std::move(expected_event_sequence)),
      wait_for_single_event_(wait_for_single_event) {
  if (wait_for_single_event)
    DCHECK_EQ(1U, expected_events_.size());
  else
    DCHECK_GT(expected_events_.size(), 0U);
}

EventWaiter::~EventWaiter() = default;

bool EventWaiter::Wait() {
  if (expected_events_.empty())
    return true;

  DCHECK(!run_loop_.running());
  run_loop_.Run();
  return expected_events_.empty();
}

void EventWaiter::OnEvent(TestEvent current_event) {
  if (expected_events_.empty())
    return;

  // While waiting for a single event, ignore arrival of other events.
  if (wait_for_single_event_ && expected_events_.front() != current_event) {
    return;
  }

  DCHECK_EQ(expected_events_.front(), current_event);
  expected_events_.pop_front();

  // Only quit the loop if no other events are expected.
  if (expected_events_.empty() && run_loop_.running())
    run_loop_.Quit();
}

}  // namespace payments
