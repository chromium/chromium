// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/scoped_new_badge_tracker.h"

#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"

ScopedNewBadgeTracker::ScopedNewBadgeTracker(content::BrowserContext* profile)
    : ScopedNewBadgeTrackerBase(
          feature_engagement::TrackerFactory::GetForBrowserContext(profile)) {}

ScopedNewBadgeTracker::ScopedNewBadgeTracker(Browser* browser)
    : ScopedNewBadgeTracker(browser->profile()) {}
