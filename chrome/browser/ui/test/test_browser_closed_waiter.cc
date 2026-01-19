// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/test/test_browser_closed_waiter.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"

TestBrowserClosedWaiter::TestBrowserClosedWaiter(Browser* browser)
    : browser_(browser) {
  browser_collection_observation_.Observe(
      ProfileBrowserCollection::GetForProfile(browser_->profile()));
}

TestBrowserClosedWaiter::~TestBrowserClosedWaiter() = default;

bool TestBrowserClosedWaiter::WaitUntilClosed() {
  return future_.Wait();
}

void TestBrowserClosedWaiter::OnBrowserClosed(BrowserWindowInterface* browser) {
  if (browser_ == browser) {
    browser_ = nullptr;  // Make raw_ptr happy.
    future_.SetValue();
  }
}
