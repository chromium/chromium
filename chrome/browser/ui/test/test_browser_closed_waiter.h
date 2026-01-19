// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TEST_TEST_BROWSER_CLOSED_WAITER_H_
#define CHROME_BROWSER_UI_TEST_TEST_BROWSER_CLOSED_WAITER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"

class Browser;
class BrowserWindowInterface;

// A helper class to wait for a particular browser to be closed.
class TestBrowserClosedWaiter : public BrowserCollectionObserver {
 public:
  explicit TestBrowserClosedWaiter(Browser* browser);

  ~TestBrowserClosedWaiter() override;

  [[nodiscard]] bool WaitUntilClosed();

 private:
  void OnBrowserClosed(BrowserWindowInterface* browser) override;

  raw_ptr<Browser> browser_ = nullptr;
  base::ScopedObservation<ProfileBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
  base::test::TestFuture<void> future_;
};

#endif  // CHROME_BROWSER_UI_TEST_TEST_BROWSER_CLOSED_WAITER_H_
