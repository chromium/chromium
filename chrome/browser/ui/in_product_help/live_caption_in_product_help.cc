// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/in_product_help/live_caption_in_product_help.h"

#include "base/feature_list.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "media/base/media_switches.h"

LiveCaptionInProductHelp::LiveCaptionInProductHelp(Profile* profile)
    : profile_(profile) {}

LiveCaptionInProductHelp::~LiveCaptionInProductHelp() = default;

void LiveCaptionInProductHelp::OnMediaButtonEnabled() {
  Browser* browser = BrowserList::GetInstance()->GetLastActive();
  if (!browser || browser->profile() != profile_)
    return;

  if (GetTracker()->ShouldTriggerHelpUI(
          feature_engagement::kIPHLiveCaptionFeature)) {
    browser->window()->ShowInProductHelpPromo(
        InProductHelpFeature::kLiveCaption);
  }
}

void LiveCaptionInProductHelp::HelpDismissed() {
  GetTracker()->Dismissed(feature_engagement::kIPHLiveCaptionFeature);
}

feature_engagement::Tracker* LiveCaptionInProductHelp::GetTracker() {
  return feature_engagement::TrackerFactory::GetForBrowserContext(profile_);
}
