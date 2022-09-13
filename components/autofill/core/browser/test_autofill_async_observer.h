// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_ASYNC_OBSERVER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_ASYNC_OBSERVER_H_

#include "base/observer_list_types.h"
#include "base/run_loop.h"
#include "components/autofill/core/browser/autofill_observer.h"

namespace autofill {
namespace test {

// Observer which allows to block a thread (wait) until it gets notified with a
// specific NotificationType. Blocks the thread via a base::RunLoop.
// This is a very useful mechanism for browsers tests.
class TestAutofillAsyncObserver : public AutofillObserver {
 public:
  // |notification_type| is the notification type that this observer observes.
  // |detach_on_notify| will let the AutofillSubject know that this
  // observer only wants to watch for the first notification of that type.
  TestAutofillAsyncObserver(NotificationType notification_type,
                            bool detach_on_notify = false);

  ~TestAutofillAsyncObserver() override;

  // Invoked by the watched AutofillSubject, this will effectively quit the
  // current run loop.
  void OnNotify() override;

  // Blocks the thread until the expected notification occurs.
  void Wait();

 private:
  base::RunLoop run_loop_;
};

}  // namespace test
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_ASYNC_OBSERVER_H_
