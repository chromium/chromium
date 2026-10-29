// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"

#include "base/time/time.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/waap/initial_webui_window_metrics_manager.h"
#include "chrome/browser/ui/waap/waap_utils.h"

namespace chrome {

ScopedTabbedBrowserDisplayer::ScopedTabbedBrowserDisplayer(Profile* profile) {
  browser_ =
      ProfileBrowserCollection::GetForProfile(profile)->FindTabbedBrowser();
  if (!browser_ && GetBrowserWindowCreationStatusForProfile(*profile) ==
                       BrowserWindowInterface::CreationStatus::kOk) {
    base::TimeTicks now = base::TimeTicks::Now();
    browser_ = CreateBrowserWindow(
        BrowserWindowCreateParams(profile, /*from_user_gesture=*/true));
    if (auto* manager = InitialWebUIWindowMetricsManager::From(browser_)) {
      manager->SetWindowCreationInfo(
          waap::NewWindowCreationSource::kNewEmptyWindow, now);
    }
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
