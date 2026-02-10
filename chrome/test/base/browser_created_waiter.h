// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_BROWSER_CREATED_WAITER_H_
#define CHROME_TEST_BASE_BROWSER_CREATED_WAITER_H_

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"

class BrowserWindowInterface;
class GlobalBrowserCollection;

// Observes the global list of BrowserWindowInterfaces and waits for any new
// browser to be created. Exists because browser creation on some platforms
// (Android) is asynchronous.
class BrowserCreatedWaiter : public BrowserCollectionObserver {
 public:
  BrowserCreatedWaiter();
  BrowserCreatedWaiter(const BrowserCreatedWaiter&) = delete;
  BrowserCreatedWaiter& operator=(const BrowserCreatedWaiter&) = delete;
  ~BrowserCreatedWaiter() override;

  // Waits until a browser opens and returns it. Returns immediately if a
  // browser has been opened since this object was created.
  BrowserWindowInterface* Wait();

 protected:
  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override;

 private:
  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      observation_{this};
  raw_ptr<BrowserWindowInterface> browser_ = nullptr;
  base::RunLoop run_loop_;
};

#endif  // CHROME_TEST_BASE_BROWSER_CREATED_WAITER_H_
