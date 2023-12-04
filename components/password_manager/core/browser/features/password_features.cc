// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/features/password_features.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace password_manager::features {

// When enabled, updates to shared existing passwords from the same sender are
// auto-approved.
BASE_FEATURE(kAutoApproveSharedPasswordUpdatesFromSameSender,
             "AutoApproveSharedPasswordUpdatesFromSameSender",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Biometrics for the Touch To Fill feature. This only effects Android.
BASE_FEATURE(kBiometricTouchToFill,
             "BiometricTouchToFill",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Delete undecryptable passwords from the store when Sync is active.
BASE_FEATURE(kClearUndecryptablePasswordsOnSync,
             "ClearUndecryptablePasswordsInSync",
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_IOS)
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

// Disables eviction from UPM when error occurs and instead disables password
// manager until the error is gone.
#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kRemoveUPMUnenrollment,
             "RemoveUPMUnenrollment",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

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
             base::FEATURE_ENABLED_BY_DEFAULT);
// Enables reading credentials from SharedPreferences.
BASE_FEATURE(kFetchGaiaHashOnSignIn,
             "FetchGaiaHashOnSignIn",
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
// Enables filling for sign-in UFF on iOS.
BASE_FEATURE(kIOSPasswordSignInUff,
             "IOSPasswordSignInUff",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // IS_IOS

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Enables new confirmation bubble flow if generated password was used in a
// form.
BASE_FEATURE(kNewConfirmationBubbleForGeneratedPasswords,
             "NewConfirmationBubbleForGeneratedPasswords",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
// Enabled in M121. Remove at or after M123.
BASE_FEATURE(kPasskeysPrefetchAffiliations,
             "PasskeysPrefetchAffiliations",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Enables different experiments that modify content and behavior of the
// existing generated password suggestion dropdown.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
BASE_FEATURE(kPasswordGenerationExperiment,
             "PasswordGenerationExperiment",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

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

// Enables "Needs access to keychain, restart chrome" bubble and banner.
#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kRestartToGainAccessToKeychain,
             "RestartToGainAccessToKeychain",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Enables the notification UI that is displayed to the user when visiting a
// website for which a stored password has been shared by another user.
BASE_FEATURE(kSharedPasswordNotificationUI,
             "SharedPasswordNotificationUI",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Displays at least the decryptable and never saved logins in the password
// manager
BASE_FEATURE(kSkipUndecryptablePasswords,
             "SkipUndecryptablePasswords",
#if BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

#if BUILDFLAG(IS_ANDROID)
// Enables use of Google Mobile services for non-synced password storage that
// contains no passwords, so no migration will be necessary.
// UnifiedPasswordManagerLocalPasswordsAndroidWithMigration will replace this
// feature once UPM starts to be rolled out to users who have saved local
// passwords.
BASE_FEATURE(kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration,
             "UnifiedPasswordManagerLocalPasswordsAndroidNoMigration",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables use of Google Mobile services for non-synced password storage add for
// users who have local passwords saved.
BASE_FEATURE(kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration,
             "UnifiedPasswordManagerLocalPasswordsAndroidWithMigration",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

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

// Enables using server prediction when parsing password forms for saving.
// If disabled, password server predictions are only used when parsing forms
// for filling.
BASE_FEATURE(kUseServerPredictionsOnSaveParsing,
             "UseServerPredictionsOnSaveParsing",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables support of sending additional votes on username first flow. The votes
// are sent on single password forms and contain information about preceding
// single username forms.
// TODO(crbug.com/959776): Clean up if the main crowdsourcing is good enough and
// we don't need additional signals.
BASE_FEATURE(kUsernameFirstFlowFallbackCrowdsourcing,
             "UsernameFirstFlowFallbackCrowdsourcing",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables suggesting username in the save/update prompt in the case of
// autocomplete="username".
BASE_FEATURE(kUsernameFirstFlowHonorAutocomplete,
             "UsernameFirstFlowHonorAutocomplete",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables storing more possible username values in the LRU cache. Part of the
// `kUsernameFirstFlowWithIntermediateValues` feature.
BASE_FEATURE(kUsernameFirstFlowStoreSeveralValues,
             "UsernameFirstFlowStoreSeveralValues",
             base::FEATURE_ENABLED_BY_DEFAULT);
extern const base::FeatureParam<int> kMaxSingleUsernameFieldsToStore{
    &kUsernameFirstFlowStoreSeveralValues, /*name=*/"max_elements",
    /*default_value=*/10};

// Enables tolerating intermediate fields like OTP or CAPTCHA
// between username and password fields in Username First Flow.
BASE_FEATURE(kUsernameFirstFlowWithIntermediateValues,
             "UsernameFirstFlowWithIntermediateValues",
             base::FEATURE_DISABLED_BY_DEFAULT);
extern const base::FeatureParam<int> kSingleUsernameTimeToLive{
    &kUsernameFirstFlowWithIntermediateValues, /*name=*/"ttl",
    /*default_value=*/5};

// Enables new prediction that is based on votes from Username First Flow with
// Intermediate Values.
BASE_FEATURE(kUsernameFirstFlowWithIntermediateValuesPredictions,
             "UsernameFirstFlowWithIntermediateValuesPredictions",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables voting for more text fields outside of the password form in Username
// First Flow.
BASE_FEATURE(kUsernameFirstFlowWithIntermediateValuesVoting,
             "UsernameFirstFlowWithIntermediateValuesVoting",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// Feature enables usage of a new API to obtain all passwords with branding info
// directly from GMS Core. This feature also completely disables fetching of
// Affiliations by Chrome.
BASE_FEATURE(kUseGMSCoreForBrandingInfo,
             "UseGMSCoreForBrandingInfo",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

}  // namespace password_manager::features
