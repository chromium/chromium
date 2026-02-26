// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/test/browser_event_waiter.h"

#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"

BrowserEventWaiter::BrowserEventWaiter(Event event)
    : BrowserEventWaiter(event, nullptr) {}

BrowserEventWaiter::BrowserEventWaiter(Event event,
                                       BrowserWindowInterface* browser)
    : event_(event), browser_(browser) {
  browser_collection_observation_.Observe(
      GlobalBrowserCollection::GetInstance());
}

BrowserEventWaiter::~BrowserEventWaiter() {
  run_loop_.Run();
}

void BrowserEventWaiter::OnBrowserCreated(BrowserWindowInterface* browser) {
  OnBrowserEvent(Event::CREATED, browser);
}

void BrowserEventWaiter::OnBrowserClosed(BrowserWindowInterface* browser) {
  OnBrowserEvent(Event::CLOSED, browser);
}

void BrowserEventWaiter::OnBrowserActivated(BrowserWindowInterface* browser) {
  OnBrowserEvent(Event::ACTIVATED, browser);
}

void BrowserEventWaiter::OnBrowserDeactivated(BrowserWindowInterface* browser) {
  OnBrowserEvent(Event::DEACTIVATED, browser);
}

void BrowserEventWaiter::OnBrowserEvent(Event event,
                                        BrowserWindowInterface* browser) {
  if (event == event_ && (browser_ == nullptr || browser == browser_)) {
    run_loop_.Quit();

    // Avoid potential dangling pointer on CLOSED events.
    browser_ = nullptr;
  }
}
