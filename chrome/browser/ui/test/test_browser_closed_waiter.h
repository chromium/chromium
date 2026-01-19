// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TEST_TEST_BROWSER_CLOSED_WAITER_H_
#define CHROME_BROWSER_UI_TEST_TEST_BROWSER_CLOSED_WAITER_H_

#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser_list_observer.h"

// A helper class to wait for a particular browser to be closed.
class TestBrowserClosedWaiter : public BrowserListObserver {
 public:
  explicit TestBrowserClosedWaiter(Browser* browser);

  ~TestBrowserClosedWaiter() override;

  [[nodiscard]] bool WaitUntilClosed();

 private:
  void OnBrowserRemoved(Browser* browser) override;

  raw_ptr<Browser> browser_ = nullptr;
  base::test::TestFuture<void> future_;
};

#endif  // CHROME_BROWSER_UI_TEST_TEST_BROWSER_CLOSED_WAITER_H_
