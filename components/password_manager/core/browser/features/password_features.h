// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FEATURES_PASSWORD_FEATURES_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FEATURES_PASSWORD_FEATURES_H_

// This file defines all password manager features used in the browser process.
// Prefer adding new features here instead of "core/common/".
#include "base/feature_list.h"
#include "build/build_config.h"

namespace password_manager::features {
// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
BASE_DECLARE_FEATURE(kBiometricAuthenticationForFilling);
#endif
#if BUILDFLAG(IS_MAC)
BASE_DECLARE_FEATURE(kBiometricAuthenticationInSettings);
#endif

BASE_DECLARE_FEATURE(kBiometricTouchToFill);
BASE_DECLARE_FEATURE(kClearUndecryptablePasswordsOnSync);
BASE_DECLARE_FEATURE(kDisablePasswordsDropdownForCvcFields);

BASE_DECLARE_FEATURE(kEnablePasswordsAccountStorage);

#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kFillingAcrossAffiliatedWebsitesAndroid);
#endif
BASE_DECLARE_FEATURE(kFillingAcrossGroupedSites);

BASE_DECLARE_FEATURE(kFillOnAccountSelect);

#if BUILDFLAG(IS_IOS)
BASE_DECLARE_FEATURE(kOneReadLoginDatabaseMigration);
#endif

BASE_DECLARE_FEATURE(kSharedPasswordNotificationUI);

BASE_DECLARE_FEATURE(kPasswordManagerEnableReceiverService);
BASE_DECLARE_FEATURE(kPasswordManagerEnableSenderService);

BASE_DECLARE_FEATURE(kPasswordManagerLogToTerminal);

BASE_DECLARE_FEATURE(kSkipUndecryptablePasswords);

BASE_DECLARE_FEATURE(kUseExtensionListForPSLMatching);

}  // namespace password_manager::features

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FEATURES_PASSWORD_FEATURES_H_
