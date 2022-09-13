// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_async_observer.h"

#include "base/run_loop.h"

namespace autofill {
namespace test {

TestAutofillAsyncObserver::TestAutofillAsyncObserver(
    NotificationType notification_type,
    bool detach_on_notify)
    : AutofillObserver(notification_type, detach_on_notify), run_loop_() {}

TestAutofillAsyncObserver::~TestAutofillAsyncObserver() = default;

void TestAutofillAsyncObserver::OnNotify() {
  run_loop_.Quit();
}

void TestAutofillAsyncObserver::Wait() {
  run_loop_.Run();
}

}  // namespace test
}  // namespace autofill
