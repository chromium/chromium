// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_infobar_observer.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

StartupInfoBarObserver::StartupInfoBarObserver(
    Profile& profile,
    ProfileBrowserCollection& profile_browser_collection,
    AddInfoBarsCallback callback)
    : profile_(profile), add_infobars_callback_(std::move(callback)) {
  profile_observation_.Observe(&profile);
  browser_collection_observation_.Observe(&profile_browser_collection);
}

StartupInfoBarObserver::~StartupInfoBarObserver() = default;

// static
void StartupInfoBarObserver::ObserveProfile(Profile& profile,
                                            AddInfoBarsCallback callback) {
  auto* collection = ProfileBrowserCollection::GetForProfile(&profile);
  if (!profile.AllowsBrowserWindows() || !collection) {
    return;
  }
  profile.SetUserData(&kStartupInfoBarObserverKey,
                      base::WrapUnique(new StartupInfoBarObserver(
                          profile, *collection, std::move(callback))));
}

void StartupInfoBarObserver::OnBrowserCreated(BrowserWindowInterface* browser) {
  browser_ = browser;
  browser_collection_observation_.Reset();

  if (!browser->GetTabStripModel()->GetActiveWebContents()) {
    // The browser doesn't contain any tabs yet, wait for the first tab to be
    // activated.
    browser_->GetTabStripModel()->AddObserver(this);
    return;
  }
  AddInfoBarsAndReset();
}

void StartupInfoBarObserver::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (!tab_strip_model->GetActiveWebContents()) {
    return;
  }

  AddInfoBarsAndReset();
}

void StartupInfoBarObserver::OnTabStripModelDestroyed(
    TabStripModel* tab_strip_model) {
  // If TabStripModel is being destroyed before we can add infobars, reset
  // observations and delete this object.
  Reset();
}

void StartupInfoBarObserver::OnProfileWillBeDestroyed(Profile* profile) {
  // Corresponding observation lists get destroyed before UserData, so we need
  // to remove ourselves as observers here.
  browser_collection_observation_.Reset();
  profile_observation_.Reset();
}

void StartupInfoBarObserver::Reset() {
  // This will securely destroy `this` and invoke the destructor.
  profile_->RemoveUserData(&kStartupInfoBarObserverKey);
}

void StartupInfoBarObserver::AddInfoBarsAndReset() {
  std::move(add_infobars_callback_).Run(browser_);
  Reset();
}
