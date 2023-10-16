// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/features/password_features.h"

namespace password_manager::features {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
// Enables biometric authentication before form filling.
BASE_FEATURE(kBiometricAuthenticationForFilling,
             "BiometricAuthenticationForFilling",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_MAC)
// Enables biometric authentication in settings.
BASE_FEATURE(kBiometricAuthenticationInSettings,
             "BiometricAuthenticationInSettings",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Enables Biometrics for the Touch To Fill feature. This only effects Android.
BASE_FEATURE(kBiometricTouchToFill,
             "BiometricTouchToFill",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Delete undecryptable passwords from the store when Sync is active.
BASE_FEATURE(kClearUndecryptablePasswordsOnSync,
             "ClearUndecryptablePasswordsInSync",
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Disables fallback filling if the server or the autocomplete attribute says it
// is a credit card field.
BASE_FEATURE(kDisablePasswordsDropdownForCvcFields,
             "DisablePasswordsDropdownForCvcFields",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a second, Gaia-account-scoped password store for users who are signed
// in but not syncing.
BASE_FEATURE(kEnablePasswordsAccountStorage,
             "EnablePasswordsAccountStorage",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

#if BUILDFLAG(IS_ANDROID)
// Enables filling password on a website when there is saved password on
// affiliated website.
BASE_FEATURE(kFillingAcrossAffiliatedWebsitesAndroid,
             "FillingAcrossAffiliatedWebsitesAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// This flag enables password filling across grouped websites. Information about
// website groups is provided by the affiliation service.
BASE_FEATURE(kFillingAcrossGroupedSites,
             "FillingAcrossGroupedSites",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the experiment for the password manager to only fill on account
// selection, rather than autofilling on page load, with highlighting of fields.
BASE_FEATURE(kFillOnAccountSelect,
             "fill-on-account-select",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_IOS)
// Enables migration to OSCrypt with a single query to the keychain.
BASE_FEATURE(kOneReadLoginDatabaseMigration,
             "OneReadLoginDatabaseMigration",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_IOS)

// Enables the notification UI that is displayed to the user when visiting a
// website for which a stored password has been shared by another user.
BASE_FEATURE(kSharedPasswordNotificationUI,
             "SharedPasswordNotificationUI",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables password receiving service including incoming password sharing
// invitation sync data type.
BASE_FEATURE(kPasswordManagerEnableReceiverService,
             "PasswordManagerEnableReceiverService",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables password sender service including outgoing password sharing
// invitation sync data type.
BASE_FEATURE(kPasswordManagerEnableSenderService,
             "PasswordManagerEnableSenderService",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables logging the content of chrome://password-manager-internals to the
// terminal.
BASE_FEATURE(kPasswordManagerLogToTerminal,
             "PasswordManagerLogToTerminal",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Displays at least the decryptable and never saved logins in the password
// manager
BASE_FEATURE(kSkipUndecryptablePasswords,
             "SkipUndecryptablePasswords",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Improves PSL matching capabilities by utilizing PSL-extension list from
// affiliation service. It fixes problem with incorrect password suggestions on
// websites like slack.com.
BASE_FEATURE(kUseExtensionListForPSLMatching,
             "UseExtensionListForPSLMatching",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

}  // namespace password_manager::features
