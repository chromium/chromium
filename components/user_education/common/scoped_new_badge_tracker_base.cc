// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/scoped_new_badge_tracker_base.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "components/feature_engagement/public/tracker.h"

namespace user_education {

ScopedNewBadgeTrackerBase::ScopedNewBadgeTrackerBase(
    feature_engagement::Tracker* tracker)
    : tracker_(tracker) {}

// TODO(crbug.com/1258216): When we have the ability to do concurrent FE promos,
// dismiss all of the badge promos here instead of in TryShowNewBadge().
ScopedNewBadgeTrackerBase::~ScopedNewBadgeTrackerBase() = default;

bool ScopedNewBadgeTrackerBase::TryShowNewBadge(
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
  if (result) {
    active_badge_features_.insert(&badge_feature);
    // TODO(crbug.com/1258216): Immediately dismiss to work around an issue
    // where the FE backend disallows concurrent promos; move the call to
    // Dismiss() to the destructor when concurrency is added.
    //
    // Note that "Dismiss" in this case does not dismiss the UI. It's telling
    // the FE backend that the promo is done so that other promos can run. A
    // badge showing in a menu should not block e.g. other badges from
    // displaying (never mind help bubbles).
    tracker_->Dismissed(badge_feature);
  }
  return result;
}

void ScopedNewBadgeTrackerBase::ActionPerformed(const char* event_name) {
  if (tracker_)
    tracker_->NotifyEvent(event_name);
}

}  // namespace user_education
