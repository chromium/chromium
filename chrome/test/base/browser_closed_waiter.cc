// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/browser_closed_waiter.h"

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"

BrowserClosedWaiter::BrowserClosedWaiter(BrowserWindowInterface* browser)
    : browser_(browser) {
  observation_.Observe(GlobalBrowserCollection::GetInstance());
}

BrowserClosedWaiter::~BrowserClosedWaiter() = default;

void BrowserClosedWaiter::Wait() {
  if (closed_) {
    return;
  }
  run_loop_.Run();
}

void BrowserClosedWaiter::OnBrowserClosed(BrowserWindowInterface* browser) {
  if (browser == browser_) {
    browser_ = nullptr;
    closed_ = true;
    if (run_loop_.running()) {
      run_loop_.Quit();
    }
  }
}
