// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/scoped_new_badge_tracker.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "components/feature_engagement/public/tracker.h"

ScopedNewBadgeTracker::ScopedNewBadgeTracker(content::BrowserContext* profile)
    : tracker_(
          feature_engagement::TrackerFactory::GetForBrowserContext(profile)) {}

ScopedNewBadgeTracker::~ScopedNewBadgeTracker() {
  for (const base::Feature* new_badge_feature : active_badge_features_)
    tracker_->Dismissed(*new_badge_feature);
}

bool ScopedNewBadgeTracker::TryShowNewBadge(
    const base::Feature& badge_feature,
    const base::Feature* promoted_feature) {
  // In the event of a submenu that the user could open multiple times while
  // navigating the same top-level menu, and we don't want to count those as
  // separate times the user sees the New Badge:
  if (base::Contains(active_badge_features_, &badge_feature))
    return true;

  // If there is no tracker available or the feature being promoted is disabled,
  // do not show the New Badge.
  if (!tracker_)
    return false;
  if (promoted_feature && !base::FeatureList::IsEnabled(*promoted_feature))
    return false;

  const bool result = tracker_->ShouldTriggerHelpUI(badge_feature);
  if (result)
    active_badge_features_.insert(&badge_feature);
  return result;
}

void ScopedNewBadgeTracker::ActionPerformed(const char* event_name) {
  if (tracker_)
    tracker_->NotifyEvent(event_name);
}
