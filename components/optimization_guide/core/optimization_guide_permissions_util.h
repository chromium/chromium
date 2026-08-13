// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_PERMISSIONS_UTIL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_PERMISSIONS_UTIL_H_

class PrefService;

namespace optimization_guide {

// Disables checking of user permissions to fetch hints for testing.
inline constexpr char kDisableCheckingUserPermissionsForTestingSwitch[] =
    "disable-checking-optimization-guide-user-permissions";

// Enables overriding Google API key configuration check for permissions.
inline constexpr char kGoogleApiKeyConfigurationCheckOverrideSwitch[] =
    "optimization-guide-google-api-key-configuration-check-override";

// Returns true if Google API key configuration check should be skipped.
bool ShouldSkipGoogleApiKeyConfigurationCheck();

// Returns true if the user, as represented by |profile| is permitted to make
// calls to the remote Optimization Guide Service.
//
// Note that this does not include the additional enterprise policy check that
// gates model downloads.
bool IsUserPermittedToFetchFromRemoteOptimizationGuide(
    bool is_off_the_record,
    PrefService* pref_service);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_PERMISSIONS_UTIL_H_
