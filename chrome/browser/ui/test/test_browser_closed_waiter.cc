// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/test/test_browser_closed_waiter.h"

TestBrowserClosedWaiter::TestBrowserClosedWaiter(
    BrowserWindowInterface* browser)
    : observer_(browser) {}

TestBrowserClosedWaiter::~TestBrowserClosedWaiter() = default;

bool TestBrowserClosedWaiter::WaitUntilClosed() {
  observer_.Wait();
  return true;
}
