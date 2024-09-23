// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_FEATURES_H_
#define COMPONENTS_SHARING_MESSAGE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

BASE_DECLARE_FEATURE(kClickToCall);

// Feature flag for matching device expiration to pulse interval.
BASE_DECLARE_FEATURE(kSharingMatchPulseInterval);

// The delta from the pulse interval in hours after which a device is considered
// expired, for Desktop devices. Chrome on Desktop is expected to update the
// last updated timestamp quite frequently because it can do this when
// backgrounded. Such devices can be marked stale aggressively if they did not
// update for more than an interval.
extern const base::FeatureParam<int> kSharingPulseDeltaDesktopHours;

// The delta from the pulse interval in hours after which a device is considered
// expired, for Android devices. Chrome on Android is expected to update the
// last updated timestamp less frequently because it does not do this when
// backgrounded. Such devices cannot be marked stale aggressively.
extern const base::FeatureParam<int> kSharingPulseDeltaAndroidHours;

#endif  // COMPONENTS_SHARING_MESSAGE_FEATURES_H_
