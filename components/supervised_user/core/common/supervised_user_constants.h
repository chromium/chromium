// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_COMMON_SUPERVISED_USER_CONSTANTS_H_
#define COMPONENTS_SUPERVISED_USER_CORE_COMMON_SUPERVISED_USER_CONSTANTS_H_

#include "base/files/file_path.h"
#include "url/gurl.h"

namespace supervised_user {

// Keys for supervised user settings. These are configured remotely and mapped
// to preferences by the SupervisedUserPrefStore.
extern const char kAuthorizationHeader[];
extern const char kCameraMicDisabled[];
extern const char kContentPackDefaultFilteringBehavior[];
extern const char kContentPackManualBehaviorHosts[];
extern const char kContentPackManualBehaviorURLs[];
extern const char kCookiesAlwaysAllowed[];
extern const char kForceSafeSearch[];
extern const char kGeolocationDisabled[];
extern const char kSafeSitesEnabled[];
extern const char kSigninAllowed[];
extern const char kSigninAllowedOnNextStartup[];

// A special supervised user ID used for child accounts.
extern const char kChildAccountSUID[];

// Keys for supervised user shared settings. These can be configured remotely or
// SupervisedUserPrefMappingService.
extern const char kChromeAvatarIndex[];
extern const char kChromeOSAvatarIndex[];
extern const char kChromeOSPasswordData[];

// A group of preferences of both primary and secondary custodians.
extern const char* const kCustodianInfoPrefs[10];

// Filenames.
extern const base::FilePath::CharType kSupervisedUserSettingsFilename[];

extern const char kSyncGoogleDashboardURL[];

// URLs for RPCs in the KidsManagement service.
GURL KidsManagementGetFamilyMembersURL();
GURL KidsManagementPermissionRequestsURL();
GURL KidsManagementClassifyURLRequestURL();

// Histogram name to log FamilyLink user type segmentation.
extern const char kFamilyLinkUserLogSegmentHistogramName[];

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_COMMON_SUPERVISED_USER_CONSTANTS_H_
