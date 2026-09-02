// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"

#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"

namespace chrome {

ScopedTabbedBrowserDisplayer::ScopedTabbedBrowserDisplayer(Profile* profile) {
  browser_ =
      ProfileBrowserCollection::GetForProfile(profile)->FindTabbedBrowser();
  if (!browser_ && GetBrowserWindowCreationStatusForProfile(*profile) ==
                       BrowserWindowInterface::CreationStatus::kOk) {
    BrowserWindowCreateParams params(profile, /*from_user_gesture=*/true);
    browser_ = CreateBrowserWindow(std::move(params));
  }
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
