// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"

namespace chrome {

ScopedTabbedBrowserDisplayer::ScopedTabbedBrowserDisplayer(Profile* profile) {
  browser_ = FindTabbedBrowser(profile, false);
  if (!browser_ && Browser::GetCreationStatusForProfile(profile) ==
                       Browser::CreationStatus::kOk) {
    browser_ = Browser::Create(Browser::CreateParams(profile, true));
  }
}

ScopedTabbedBrowserDisplayer::~ScopedTabbedBrowserDisplayer() {
  if (!browser_)
    return;

  // Make sure to restore the window, since window()->Show() will not unminimize
  // it.
  if (browser_->window()->IsMinimized())
    browser_->window()->Restore();

  browser_->window()->Show();
}

}  // namespace chrome
