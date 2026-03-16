// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

namespace chrome {

ScopedTabbedBrowserDisplayer::ScopedTabbedBrowserDisplayer(Profile* profile) {
  browser_ = FindTabbedBrowser(profile, false);
  if (!browser_ && Browser::GetCreationStatusForProfile(profile) ==
                       Browser::CreationStatus::kOk) {
    Browser::CreateParams params(profile, /*user_gesture=*/true);
    browser_ = Browser::Create(params);
  }
}

Browser* ScopedTabbedBrowserDisplayer::browser() {
  return browser_ ? browser_->GetBrowserForMigrationOnly() : nullptr;
}

ScopedTabbedBrowserDisplayer::~ScopedTabbedBrowserDisplayer() {
  if (!browser_) {
    return;
  }

  // Make sure to restore the window, since GetWindow()->Show() will not
  // unminimize it.
  if (browser_->GetWindow()->IsMinimized()) {
    browser_->GetWindow()->Restore();
  }

  browser_->GetWindow()->Show();
}

}  // namespace chrome
