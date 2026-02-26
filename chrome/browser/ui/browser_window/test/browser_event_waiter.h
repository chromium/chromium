// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_TEST_BROWSER_EVENT_WAITER_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_TEST_BROWSER_EVENT_WAITER_H_

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"

class BrowserWindowInterface;
class GlobalBrowserCollection;

// TODO(crbug.com/480103891): Refactor this out with the rest of the utilities
// with similar functionality in ui_test_utils and interactive_test_utils.
class BrowserEventWaiter : public BrowserCollectionObserver {
 public:
  enum class Event { CREATED, CLOSED, ACTIVATED, DEACTIVATED };

  // Start observing a GlobalBrowserCollection for |event|. The destructor will
  // wait for the event to happen (if it hasn't already).
  explicit BrowserEventWaiter(Event event);

  // Start observing a GlobalBrowserCollection for |event| on |browser|. The
  // destructor will wait for the event to happen (if it hasn't already).
  BrowserEventWaiter(Event event, BrowserWindowInterface* browser);

  // Block and wait for the desired event, or finish immediately if it has
  // already happened.
  ~BrowserEventWaiter() override;

 private:
  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override;
  void OnBrowserClosed(BrowserWindowInterface* browser) override;
  void OnBrowserActivated(BrowserWindowInterface* browser) override;
  void OnBrowserDeactivated(BrowserWindowInterface* browser) override;

  // Called when any event comes in. Causes the RunLoop to quit if the event is
  // the one we're interested in.
  void OnBrowserEvent(Event event, BrowserWindowInterface* browser);

  Event event_;
  raw_ptr<BrowserWindowInterface> browser_;
  base::RunLoop run_loop_;
  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_TEST_BROWSER_EVENT_WAITER_H_
