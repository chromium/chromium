// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/common/supervised_user_constants.h"

#include "components/supervised_user/core/common/pref_names.h"

namespace supervised_user {
namespace {

GURL KidsManagementBaseURL() {
  return GURL("https://kidsmanagement-pa.googleapis.com/kidsmanagement/v1/");
}

const char kGetFamilyProfileURL[] = "families/mine?alt=json";
const char kGetFamilyMembersURL[] = "families/mine/members?alt=json";
const char kPermissionRequestsURL[] = "people/me/permissionRequests";
const char kClassifyURLRequestURL[] = "people/me:classifyUrl";

}  // namespace

const char kAuthorizationHeaderFormat[] = "Bearer %s";
const char kCameraMicDisabled[] = "CameraMicDisabled";
const char kContentPackDefaultFilteringBehavior[] =
    "ContentPackDefaultFilteringBehavior";
const char kContentPackManualBehaviorHosts[] = "ContentPackManualBehaviorHosts";
const char kContentPackManualBehaviorURLs[] = "ContentPackManualBehaviorURLs";
const char kCookiesAlwaysAllowed[] = "CookiesAlwaysAllowed";
const char kForceSafeSearch[] = "ForceSafeSearch";
const char kGeolocationDisabled[] = "GeolocationDisabled";
const char kSafeSitesEnabled[] = "SafeSites";
const char kSigninAllowed[] = "SigninAllowed";
const char kUserName[] = "UserName";

const char kChildAccountSUID[] = "ChildAccountSUID";

const char kChromeAvatarIndex[] = "chrome-avatar-index";
const char kChromeOSAvatarIndex[] = "chromeos-avatar-index";

const char kChromeOSPasswordData[] = "chromeos-password-data";

const char* const kCustodianInfoPrefs[] = {
    prefs::kSupervisedUserCustodianName,
    prefs::kSupervisedUserCustodianEmail,
    prefs::kSupervisedUserCustodianObfuscatedGaiaId,
    prefs::kSupervisedUserCustodianProfileURL,
    prefs::kSupervisedUserCustodianProfileImageURL,
    prefs::kSupervisedUserSecondCustodianName,
    prefs::kSupervisedUserSecondCustodianEmail,
    prefs::kSupervisedUserSecondCustodianObfuscatedGaiaId,
    prefs::kSupervisedUserSecondCustodianProfileURL,
    prefs::kSupervisedUserSecondCustodianProfileImageURL,
};

const base::FilePath::CharType kSupervisedUserSettingsFilename[] =
    FILE_PATH_LITERAL("Managed Mode Settings");

const base::FilePath::CharType kDenylistFilename[] =
    FILE_PATH_LITERAL("su-denylist.bin");

GURL KidsManagementGetFamilyProfileURL() {
  return KidsManagementBaseURL().Resolve(kGetFamilyProfileURL);
}

GURL KidsManagementGetFamilyMembersURL() {
  return KidsManagementBaseURL().Resolve(kGetFamilyMembersURL);
}

GURL KidsManagementPermissionRequestsURL() {
  return KidsManagementBaseURL().Resolve(kPermissionRequestsURL);
}

GURL KidsManagementClassifyURLRequestURL() {
  return KidsManagementBaseURL().Resolve(kClassifyURLRequestURL);
}

}  // namespace supervised_user
