// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/in_product_help/global_media_controls_in_product_help.h"

#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/in_product_help/in_product_help.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"

GlobalMediaControlsInProductHelp::GlobalMediaControlsInProductHelp(
    Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
  BrowserList::AddObserver(this);
}

GlobalMediaControlsInProductHelp::~GlobalMediaControlsInProductHelp() {
  BrowserList::RemoveObserver(this);
  StopListening();
}

void GlobalMediaControlsInProductHelp::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  // We only care about when the active tab has changed.
  if (!selection.active_tab_changed())
    return;

  // If this is replacing the tab with media, then we don't want to show the
  // IPH.
  if (selection.reason == CHANGE_REASON_REPLACED)
    return;

  // If the last tab was playing media, then media was backgrounded.
  if (current_tab_likely_playing_media_)
    OnMediaBackgrounded();
}

void GlobalMediaControlsInProductHelp::OnBrowserClosing(Browser* browser) {
  // If the window we're watching is closing, stop listening to it.
  if (browser->tab_strip_model() == observed_tab_strip_model_)
    StopListening();
}

void GlobalMediaControlsInProductHelp::OnBrowserSetLastActive(
    Browser* browser) {
  // If we're no longer on the same window, stop listening.
  if (observed_tab_strip_model_ != browser->tab_strip_model())
    StopListening();
}

void GlobalMediaControlsInProductHelp::OnMediaDialogOpened() {
  GetTracker()->NotifyEvent(
      feature_engagement::events::kGlobalMediaControlsOpened);
}

void GlobalMediaControlsInProductHelp::OnMediaButtonHidden() {
  // Media has stopped playing. Stop watching for active tab changes.
  StopListening();
}

void GlobalMediaControlsInProductHelp::OnMediaButtonEnabled() {
  // If the current window isn't for our profile, then the playing media that
  // caused the toolbar icon to be enabled is definitely not in the foreground
  // tab (since it can't be in this window). Therefore, we don't assume the
  // current tab is playing media.
  Browser* browser = BrowserList::GetInstance()->GetLastActive();
  if (!browser || browser->profile() != profile_)
    return;

  // We're likely on a tab that's playing media. Watch for active tab changes.
  current_tab_likely_playing_media_ = true;

  // If we're already watching this window, do nothing.
  if (observed_tab_strip_model_ == browser->tab_strip_model())
    return;

  // If we were watching a different one, stop.
  if (observed_tab_strip_model_)
    observed_tab_strip_model_->RemoveObserver(this);

  // Watch the current one.
  observed_tab_strip_model_ = browser->tab_strip_model();
  observed_tab_strip_model_->AddObserver(this);
}

void GlobalMediaControlsInProductHelp::OnMediaButtonDisabled() {
  // Media has stopped playing. Stop watching for active tab changes.
  StopListening();
}

void GlobalMediaControlsInProductHelp::HelpDismissed() {
  GetTracker()->Dismissed(feature_engagement::kIPHGlobalMediaControlsFeature);
}

void GlobalMediaControlsInProductHelp::OnMediaBackgrounded() {
  StopListening();
  GetTracker()->NotifyEvent(feature_engagement::events::kMediaBackgrounded);

  if (GetTracker()->ShouldTriggerHelpUI(
          feature_engagement::kIPHGlobalMediaControlsFeature)) {
    auto* browser = BrowserList::GetInstance()->GetLastActive();
    DCHECK(browser);
    browser->window()->ShowInProductHelpPromo(
        InProductHelpFeature::kGlobalMediaControls);
  }
}

void GlobalMediaControlsInProductHelp::StopListening() {
  current_tab_likely_playing_media_ = false;
  if (observed_tab_strip_model_) {
    observed_tab_strip_model_->RemoveObserver(this);
    observed_tab_strip_model_ = nullptr;
  }
}

feature_engagement::Tracker* GlobalMediaControlsInProductHelp::GetTracker() {
  return feature_engagement::TrackerFactory::GetForBrowserContext(profile_);
}
