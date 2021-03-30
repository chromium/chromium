// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/ios/browser/features.h"

namespace signin {

const base::Feature kForceStartupSigninPromo{"ForceStartupSigninPromo",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSimplifySignOutIOS{"SimplifySignOutIOS",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

bool ForceStartupSigninPromo() {
  return base::FeatureList::IsEnabled(kForceStartupSigninPromo);
}

const base::Feature kRestoreGaiaCookiesIfDeleted{
    "RestoreGAIACookiesIfDeleted", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kRestoreGaiaCookiesOnUserAction{
    "RestoreGAIACookiesOnUserAction", base::FEATURE_DISABLED_BY_DEFAULT};

const char kDelayThresholdMinutesToUpdateGaiaCookie[] =
    "minutes-delay-to-restore-gaia-cookies-if-deleted";

const base::Feature kSigninNotificationInfobarUsernameInTitle{
    "SigninNotificationInfobarUsernameInTitle",
    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kDisableSSOEditing{"DisableSSOEditing",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

bool IsSSOEditingEnabled() {
  return !base::FeatureList::IsEnabled(signin::kDisableSSOEditing);
}

const base::Feature kSSOAccountCreationInChromeTab{
    "SSOAccountCreationInChromeTab", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsSSOAccountCreationInChromeTabEnabled() {
  return base::FeatureList::IsEnabled(signin::kSSOAccountCreationInChromeTab);
}

const base::Feature kSSODisableAccountCreation{
    "SSODisableAccountCreation", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsSSOAccountCreationEnabled() {
  return !base::FeatureList::IsEnabled(signin::kSSODisableAccountCreation);
}

}  // namespace signin
