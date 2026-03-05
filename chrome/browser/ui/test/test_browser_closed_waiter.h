// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TEST_TEST_BROWSER_CLOSED_WAITER_H_
#define CHROME_BROWSER_UI_TEST_TEST_BROWSER_CLOSED_WAITER_H_

#include "chrome/test/base/ui_test_utils.h"

class BrowserWindowInterface;

// A helper class to wait for a particular browser to be closed.
// DEPRECATED: Use ui_test_utils::BrowserDestroyedObserver directly.
class TestBrowserClosedWaiter {
 public:
  explicit TestBrowserClosedWaiter(BrowserWindowInterface* browser);

  ~TestBrowserClosedWaiter();

  [[nodiscard]] bool WaitUntilClosed();

 private:
  ui_test_utils::BrowserDestroyedObserver observer_;
};

#endif  // CHROME_BROWSER_UI_TEST_TEST_BROWSER_CLOSED_WAITER_H_
