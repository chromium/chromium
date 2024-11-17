// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_PUBLIC_FEATURES_H_
#define COMPONENTS_DATA_SHARING_PUBLIC_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace data_sharing::features {

// Core feature flag for data sharing. Disabling this feature ensures an empty
// implementation of the service is returned.
BASE_DECLARE_FEATURE(kDataSharingFeature);

// Join only feature flag for data sharing. Enabled partial data sharing related
// functionalities.
BASE_DECLARE_FEATURE(kDataSharingJoinOnly);

// Only for Android. Compose related UI and feature flag for data sharing.
// Enabling the feature ensures new Data Sharing UI is shown.
BASE_DECLARE_FEATURE(kDataSharingAndroidV2);

extern const base::FeatureParam<std::string> kDataSharingURL;

}  // namespace data_sharing::features

#endif  // COMPONENTS_DATA_SHARING_PUBLIC_FEATURES_H_
