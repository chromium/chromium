// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_DRIVER_SWITCHES_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_DRIVER_SWITCHES_H_

#include "base/feature_list.h"

namespace switches {

// Defines all the command-line switches used by sync driver. All switches in
// alphabetical order. The switches should be documented alongside the
// definition of their values in the .cc file.
extern const char kSyncDeferredStartupTimeoutSeconds[];
extern const char kSyncDisableDeferredStartup[];
extern const char kSyncEnableGetUpdateAvoidance[];
extern const char kSyncIncludeSpecificsInProtocolLog[];
extern const char kSyncServiceURL[];
extern const char kSyncShortInitialRetryOverride[];
extern const char kSyncShortNudgeDelayForTest[];

extern const base::Feature
    kSyncAllowWalletDataInTransportModeWithCustomPassphrase;
extern const base::Feature kSyncClearDataOnPassphraseEncryption;
extern const base::Feature kSyncPseudoUSSAppList;
extern const base::Feature kSyncPseudoUSSApps;
extern const base::Feature kSyncPseudoUSSDictionary;
extern const base::Feature kSyncPseudoUSSExtensionSettings;
extern const base::Feature kSyncPseudoUSSExtensions;
extern const base::Feature kSyncPseudoUSSFavicons;
extern const base::Feature kSyncPseudoUSSHistoryDeleteDirectives;
extern const base::Feature kSyncPseudoUSSPasswords;
extern const base::Feature kSyncPseudoUSSPreferences;
extern const base::Feature kSyncPseudoUSSPriorityPreferences;
extern const base::Feature kSyncPseudoUSSSearchEngines;
extern const base::Feature kSyncPseudoUSSSupervisedUsers;
extern const base::Feature kSyncPseudoUSSThemes;
extern const base::Feature kSyncStandaloneTransport;
extern const base::Feature kSyncSupportSecondaryAccount;
extern const base::Feature kSyncUserEvents;
extern const base::Feature kSyncUserFieldTrialEvents;
extern const base::Feature kSyncUserConsentEvents;
extern const base::Feature kSyncUserConsentSeparateType;
extern const base::Feature kSyncUserLanguageDetectionEvents;
extern const base::Feature kSyncUserTranslationEvents;
extern const base::Feature kSyncUSSBookmarks;
extern const base::Feature kSyncUSSSessions;
extern const base::Feature kSyncUSSAutofillProfile;
extern const base::Feature kSyncUSSAutofillWalletData;
extern const base::Feature kSyncUSSAutofillWalletMetadata;

}  // namespace switches

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_DRIVER_SWITCHES_H_
