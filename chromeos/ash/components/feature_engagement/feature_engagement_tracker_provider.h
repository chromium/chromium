// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_FEATURE_ENGAGEMENT_FEATURE_ENGAGEMENT_TRACKER_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_FEATURE_ENGAGEMENT_FEATURE_ENGAGEMENT_TRACKER_PROVIDER_H_

#include "base/component_export.h"

class AccountId;

namespace feature_engagement {
class Tracker;
}  // namespace feature_engagement

namespace ash {

// Provides the feature_engagement::Tracker associated with a user to ChromeOS
// callers without forcing them to depend on
// //chrome/browser/feature_engagement's Profile-keyed factory. The concrete
// implementation lives in //chrome (see
// //chrome/browser/ash/browser_delegate/keyed_service_provider/
// feature_engagement_tracker_provider_impl.h).
class COMPONENT_EXPORT(FEATURE_ENGAGEMENT_TRACKER_PROVIDER)
    FeatureEngagementTrackerProvider {
 public:
  FeatureEngagementTrackerProvider();
  FeatureEngagementTrackerProvider(const FeatureEngagementTrackerProvider&) =
      delete;
  FeatureEngagementTrackerProvider& operator=(
      const FeatureEngagementTrackerProvider&) = delete;
  virtual ~FeatureEngagementTrackerProvider();

  // Returns the process-wide singleton.
  static FeatureEngagementTrackerProvider& Get();

  // Returns the Tracker associated with `account_id`, or nullptr if none is
  // available. The returned pointer is owned by the BrowserContext-keyed
  // service infrastructure; callers must not delete it.
  virtual feature_engagement::Tracker* Find(const AccountId& account_id) = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_FEATURE_ENGAGEMENT_FEATURE_ENGAGEMENT_TRACKER_PROVIDER_H_
