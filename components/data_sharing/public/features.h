// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_PUBLIC_FEATURES_H_
#define COMPONENTS_DATA_SHARING_PUBLIC_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "components/data_sharing/public/server_environment.h"

namespace data_sharing::features {

// Feature flag for enabling collaboration in entreprise v2.
BASE_DECLARE_FEATURE(kCollaborationEntrepriseV2);

// Core feature flag for data sharing. Disabling this feature ensures an empty
// implementation of the service is returned.
BASE_DECLARE_FEATURE(kDataSharingFeature);

// Migration flag of the SharedTabGroupAccountDataSpecifics into
// //components/data_sharing.
BASE_DECLARE_FEATURE(kDataSharingAccountDataMigration);

// Join only feature flag for data sharing. Enabled partial data sharing related
// functionalities.
BASE_DECLARE_FEATURE(kDataSharingJoinOnly);

// Feature flag for server environment configuration based on string> By default
// autopush server environment is set.
BASE_DECLARE_FEATURE(kDataSharingNonProductionEnvironment);

// Feature flag for turning off the data types for shared tab groups when the
// version is out of date. Enabling the flag will turn off the data types.
// Note: Do not clean up this feature as it is meant to be used in unforeseen
// situations as a kill switch in future from finch when the shared tab groups
// feature becomes incompatible for the current chrome client.
BASE_DECLARE_FEATURE(kSharedDataTypesKillSwitch);

// Feature flag to show UI that prompts the user to update Chrome when the
// version is out of date.
// Note: Do not clean up this feature as it is meant to be used in unforeseen
// situations as a kill switch in future from finch when the shared tab groups
// feature becomes incompatible for the current chrome client.
BASE_DECLARE_FEATURE(kDataSharingEnableUpdateChromeUI);

extern const base::FeatureParam<std::string> kDataSharingURL;
extern const base::FeatureParam<std::string> kLearnMoreSharedTabGroupPageURL;
extern const base::FeatureParam<std::string> kLearnAboutBlockedAccountsURL;
extern const base::FeatureParam<std::string> kActivityLogsURL;

// Controls how often the group data should be polled from the server in the
// absence of any other updates (such as upon receiving a CollaboratioGroup
// changes).
extern const base::FeatureParam<base::TimeDelta>
    kDataSharingGroupDataPeriodicPollingInterval;

// Returns whether the data sharing functionality is enabled. This is true if
// either the main data sharing feature (`kDataSharingFeature`) or the
// join-only mode (`kDataSharingJoinOnly`) is enabled.
bool IsDataSharingFunctionalityEnabled();

// Returns whether the URL should be intercepted for versioning.
// This returns false only when the client is considered out-of-date
// (`kSharedDataTypesKillSwitch` is enabled) and the "Update Chrome" UI is
// disabled. In this scenario, the navigation throttle will not be
// installed, and the user will see the web fallback for the sharing URL.
// Note : Don't use this method for other versioning related checks.
bool ShouldInterceptUrlForVersioning();

// Returns the ServerEnvironment to be used for making RPCs call for data
// sharing service.
ServerEnvironment GetServerEnvironment();

}  // namespace data_sharing::features

#endif  // COMPONENTS_DATA_SHARING_PUBLIC_FEATURES_H_
