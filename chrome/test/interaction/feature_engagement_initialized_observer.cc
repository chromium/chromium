// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/feature_engagement_initialized_observer.h"

#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/feature_engagement/public/tracker.h"
#include "ui/base/interaction/state_observer.h"

DEFINE_STATE_IDENTIFIER_VALUE(FeatureEngagementInitializedObserver,
                              kFeatureEngagementInitializedState);

FeatureEngagementInitializedObserver::FeatureEngagementInitializedObserver(
    Browser* browser)
    : tracker_(feature_engagement::TrackerFactory::GetForBrowserContext(
          browser->profile())) {
  if (tracker_) {
    tracker_->AddOnInitializedCallback(base::BindOnce(
        &FeatureEngagementInitializedObserver::OnTrackerInitialized,
        weak_ptr_factory_.GetWeakPtr()));
  }
}

FeatureEngagementInitializedObserver::~FeatureEngagementInitializedObserver() =
    default;

bool FeatureEngagementInitializedObserver::GetStateObserverInitialState()
    const {
  return tracker_ && tracker_->IsInitialized();
}

void FeatureEngagementInitializedObserver::OnTrackerInitialized(bool success) {
  OnStateObserverStateChanged(success);
}
