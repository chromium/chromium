// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_PUBLIC_FEATURES_H_
#define COMPONENTS_DATA_SHARING_PUBLIC_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "components/data_sharing/public/server_environment.h"

namespace data_sharing::features {

// Feature flag for enabling collaboration on automotive.
BASE_DECLARE_FEATURE(kCollaborationAutomotive);

// Feature flag for enabling collaboration in entreprise v2.
BASE_DECLARE_FEATURE(kCollaborationEntrepriseV2);

// Core feature flag for data sharing. Disabling this feature ensures an empty
// implementation of the service is returned.
BASE_DECLARE_FEATURE(kDataSharingFeature);

// Join only feature flag for data sharing. Enabled partial data sharing related
// functionalities.
BASE_DECLARE_FEATURE(kDataSharingJoinOnly);

// Feature flag for server environment configuration based on string> By default
// autopush server environment is set.
BASE_DECLARE_FEATURE(kDataSharingNonProductionEnvironment);

extern const base::FeatureParam<std::string> kDataSharingURL;
extern const base::FeatureParam<std::string> kLearnMoreSharedTabGroupPageURL;
extern const base::FeatureParam<std::string> kLearnAboutBlockedAccountsURL;
extern const base::FeatureParam<std::string> kActivityLogsURL;

// Controls how often the group data should be polled from the server in the
// absence of any other updates (such as upon receiving a CollaboratioGroup
// changes).
extern const base::FeatureParam<base::TimeDelta>
    kDataSharingGroupDataPeriodicPollingInterval;

bool IsDataSharingFunctionalityEnabled();

}  // namespace data_sharing::features

#endif  // COMPONENTS_DATA_SHARING_PUBLIC_FEATURES_H_
