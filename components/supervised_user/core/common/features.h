// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_COMMON_FEATURES_H_
#define COMPONENTS_SUPERVISED_USER_CORE_COMMON_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace supervised_user {

BASE_DECLARE_FEATURE(kWebFilterInterstitialRefresh);

BASE_DECLARE_FEATURE(kLocalWebApprovals);
extern const char kLocalWebApprovalsPreferredButtonLocal[];
extern const char kLocalWebApprovalsPreferredButtonRemote[];

BASE_DECLARE_FEATURE(kSynchronousSignInChecking);

// Flags related to supervision features on Desktop and iOS platforms.
BASE_DECLARE_FEATURE(kEnableSupervisionOnDesktopAndIOS);
BASE_DECLARE_FEATURE(kFilterWebsitesForSupervisedUsersOnDesktopAndIOS);
BASE_DECLARE_FEATURE(kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
BASE_DECLARE_FEATURE(kSupervisedPrefsControlledBySupervisedStore);
BASE_DECLARE_FEATURE(kEnableManagedByParentUi);
extern const base::FeatureParam<std::string> kManagedByParentUiMoreInfoUrl;
BASE_DECLARE_FEATURE(kClearingCookiesKeepsSupervisedUsersSignedIn);

// Enables inkoking the CreatePermissionRequest service through a proto fetcher.
BASE_DECLARE_FEATURE(kEnableCreatePermissionRequestFetcher);

// Returns whether banner can be displayed to the user after website filtering
// is enabled
bool CanDisplayFirstTimeInterstitialBanner();

BASE_DECLARE_FEATURE(kLocalExtensionApprovalsV2);

BASE_DECLARE_FEATURE(kRetireStaticDenyList);

BASE_DECLARE_FEATURE(kEnableProtoApiForClassifyUrl);

BASE_DECLARE_FEATURE(kUpdateSupervisedUserFactoryCreation);

// Returns whether refreshed version of the website filter interstitial is
// enabled.
bool IsWebFilterInterstitialRefreshEnabled();

// Returns whether local parent approvals on Family Link user's device are
// enabled.
// Local web approvals are only available when refreshed version of web
// filter interstitial is enabled.
bool IsLocalWebApprovalsEnabled();

// Returns whether the local parent approval should be displayed as the
// preferred option.
// This should only be called if IsLocalWebApprovalsEnabled() returns true.
bool IsLocalWebApprovalThePreferredButton();

// Returns whether the ClassifyUrl call uses proto apis.
bool IsProtoApiForClassifyUrlEnabled();

// Returns whether the First Run Experience will rely on checking the sign-in
// status synchronously - http://b/264382308.
bool IsSynchronousSignInCheckingEnabled();

// Returns whether the new local extension approval experience is enabled.
bool IsLocalExtensionApprovalsV2Enabled();

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_COMMON_FEATURES_H_
