// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_BROWSER_CLOSED_WAITER_H_
#define CHROME_TEST_BASE_BROWSER_CLOSED_WAITER_H_

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"

class BrowserWindowInterface;
class GlobalBrowserCollection;

// Observes the global list of BrowserWindowInterfaces and waits for a specified
// browser to close.
class BrowserClosedWaiter : public BrowserCollectionObserver {
 public:
  explicit BrowserClosedWaiter(BrowserWindowInterface* browser);
  BrowserClosedWaiter(const BrowserClosedWaiter&) = delete;
  BrowserClosedWaiter& operator=(const BrowserClosedWaiter&) = delete;
  ~BrowserClosedWaiter() override;

  // Waits until the specified browser closes. Returns immediately if the
  // browser is already closed.
  void Wait();

 protected:
  // BrowserCollectionObserver:
  void OnBrowserClosed(BrowserWindowInterface* browser) override;

 private:
  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      observation_{this};
  raw_ptr<BrowserWindowInterface> browser_ = nullptr;
  bool closed_ = false;
  base::RunLoop run_loop_;
};

#endif  // CHROME_TEST_BASE_BROWSER_CLOSED_WAITER_H_
