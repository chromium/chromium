// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/test/test_browser_closed_waiter.h"

#include "chrome/browser/ui/browser_list.h"

TestBrowserClosedWaiter::TestBrowserClosedWaiter(Browser* browser)
    : browser_(browser) {
  BrowserList::AddObserver(this);
}

TestBrowserClosedWaiter::~TestBrowserClosedWaiter() {
  BrowserList::RemoveObserver(this);
}

bool TestBrowserClosedWaiter::WaitUntilClosed() {
  return future_.Wait();
}

void TestBrowserClosedWaiter::OnBrowserRemoved(Browser* browser) {
  if (browser_ == browser) {
    browser_ = nullptr;  // Make raw_ptr happy.
    future_.SetValue();
  }
}
