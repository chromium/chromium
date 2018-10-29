// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_driver_switches.h"

namespace switches {

// Allows overriding the deferred init fallback timeout.
const char kSyncDeferredStartupTimeoutSeconds[] =
    "sync-deferred-startup-timeout-seconds";

// Enables deferring sync backend initialization until user initiated changes
// occur.
const char kSyncDisableDeferredStartup[] = "sync-disable-deferred-startup";

// Enables feature to avoid unnecessary GetUpdate requests.
const char kSyncEnableGetUpdateAvoidance[] = "sync-enable-get-update-avoidance";

// Controls whether the initial state of the "Capture Specifics" flag on
// chrome://sync-internals is enabled.
const char kSyncIncludeSpecificsInProtocolLog[] = "sync-include-specifics";

// Overrides the default server used for profile sync.
const char kSyncServiceURL[] = "sync-url";

// This flag causes sync to retry very quickly (see polling_constants.h) the
// when it encounters an error, as the first step towards exponential backoff.
const char kSyncShortInitialRetryOverride[] =
    "sync-short-initial-retry-override";

// This flag significantly shortens the delay between nudge cycles. Its primary
// purpose is to speed up integration tests. The normal delay allows coalescing
// and prevention of server overload, so don't use this unless you're really
// sure
// that it's what you want.
const char kSyncShortNudgeDelayForTest[] = "sync-short-nudge-delay-for-test";

// Allows custom passphrase users to receive Wallet data for secondary accounts
// while in transport-only mode.
const base::Feature kSyncAllowWalletDataInTransportModeWithCustomPassphrase{
    "SyncAllowAutofillWalletDataInTransportModeWithCustomPassphrase",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables clearing of sync data when a user enables passphrase encryption.
const base::Feature kSyncClearDataOnPassphraseEncryption{
    "ClearSyncDataOnPassphraseEncryption", base::FEATURE_DISABLED_BY_DEFAULT};

// For each below, if enabled, the SyncableService implementation of the
// corresponding datatype(s) is wrapped within the USS architecture.
const base::Feature kSyncPseudoUSSAppList{"SyncPseudoUSSAppList",
                                          base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kSyncPseudoUSSApps{"SyncPseudoUSSApps",
                                       base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kSyncPseudoUSSDictionary{"SyncPseudoUSSDictionary",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kSyncPseudoUSSExtensionSettings{
    "SyncPseudoUSSExtensionSettings", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kSyncPseudoUSSExtensions{"SyncPseudoUSSExtensions",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kSyncPseudoUSSFavicons{"SyncPseudoUSSFavicons",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kSyncPseudoUSSHistoryDeleteDirectives{
    "SyncPseudoUSSHistoryDeleteDirectives", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kSyncPseudoUSSPasswords{"SyncPseudoUSSPasswords",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kSyncPseudoUSSPreferences{
    "SyncPseudoUSSPreferences", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kSyncPseudoUSSPriorityPreferences{
    "SyncPseudoUSSPriorityPreferences", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kSyncPseudoUSSSearchEngines{
    "SyncPseudoUSSSearchEngines", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kSyncPseudoUSSSupervisedUsers{
    "SyncPseudoUSSSupervisedUsers", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kSyncPseudoUSSThemes{"SyncPseudoUSSThemes",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, allows the Sync machinery ("transport layer") to start
// independently of Sync-the-feature.
const base::Feature kSyncStandaloneTransport{"SyncStandaloneTransport",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, allows the Sync machinery to start with a signed-in account that
// has *not* been chosen as Chrome's primary account (see IdentityManager). Only
// has an effect if SyncStandaloneTransport is also enabled.
const base::Feature kSyncSupportSecondaryAccount{
    "SyncSupportSecondaryAccount", base::FEATURE_DISABLED_BY_DEFAULT};

// Gates registration and construction of user events machinery. Enabled by
// default as each use case should have their own gating feature as well.
const base::Feature kSyncUserEvents{"SyncUserEvents",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// Gates emission of FieldTrial events.
const base::Feature kSyncUserFieldTrialEvents{"SyncUserFieldTrialEvents",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Gates emission of UserConsent events.
const base::Feature kSyncUserConsentEvents{"SyncUserConsentEvents",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Emit user consents through a separate sync type USER_CONSENTS instead of
// USER_EVENTS. This feature does not override kSyncUserConsentEvents.
const base::Feature kSyncUserConsentSeparateType{
    "SyncUserConsentSeparateType", base::FEATURE_ENABLED_BY_DEFAULT};

// Gates registration for user language detection events.
const base::Feature kSyncUserLanguageDetectionEvents{
    "SyncUserLanguageDetectionEvents", base::FEATURE_DISABLED_BY_DEFAULT};

// Gates registration for user translation events.
const base::Feature kSyncUserTranslationEvents{
    "SyncUserTranslationEvents", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable USS implementation of Bookmarks datatype.
const base::Feature kSyncUSSBookmarks{"SyncUSSBookmarks",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enable USS implementation of sessions.
const base::Feature kSyncUSSSessions{"SyncUSSSessions",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// Enable USS implementation of autofill profile datatype.
const base::Feature kSyncUSSAutofillProfile{"SyncUSSAutofillProfile",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Enable USS implementation of autofill wallet datatype.
const base::Feature kSyncUSSAutofillWalletData{
    "SyncUSSAutofillWalletData", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable USS implementation of autofill wallet metadata datatype.
const base::Feature kSyncUSSAutofillWalletMetadata{
    "SyncUSSAutofillWalletMetadata", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace switches
