// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_PREF_NAMES_H_
#define COMPONENTS_SYNC_BASE_PREF_NAMES_H_

#include "build/build_config.h"

namespace syncer {

namespace prefs {

extern const char kSyncLastSyncedTime[];
extern const char kSyncLastPollTime[];
extern const char kSyncPollIntervalSeconds[];
extern const char kSyncFirstSetupComplete[];
extern const char kSyncKeepEverythingSynced[];

#if defined(OS_CHROMEOS)
extern const char kOsSyncPrefsMigrated[];
extern const char kOsSyncFeatureEnabled[];
extern const char kSyncAllOsTypes[];
extern const char kSyncOsApps[];
extern const char kSyncOsPreferences[];
#endif  // defined(OS_CHROMEOS)

extern const char kSyncApps[];
extern const char kSyncAutofill[];
extern const char kSyncBookmarks[];
extern const char kSyncExtensions[];
extern const char kSyncPasswords[];
extern const char kSyncPreferences[];
extern const char kSyncReadingList[];
extern const char kSyncTabs[];
extern const char kSyncThemes[];
extern const char kSyncTypedUrls[];
extern const char kSyncWifiConfigurations[];

extern const char kSyncManaged[];
extern const char kSyncRequested[];

extern const char kSyncEncryptionBootstrapToken[];
extern const char kSyncKeystoreEncryptionBootstrapToken[];

extern const char kSyncGaiaId[];
extern const char kSyncCacheGuid[];
extern const char kSyncBirthday[];
extern const char kSyncBagOfChips[];

extern const char kSyncPassphrasePrompted[];

extern const char kSyncInvalidationVersions[];

extern const char kSyncLastRunVersion[];

extern const char kEnableLocalSyncBackend[];
extern const char kLocalSyncBackendDir[];

extern const char kSyncDemographics[];
extern const char kSyncDemographicsBirthYearOffset[];

#if defined(OS_ANDROID)
extern const char kSyncDecoupledFromAndroidMasterSync[];
#endif  // defined(OS_ANDROID)

// These are not prefs, they are paths inside of kSyncDemographics.
extern const char kSyncDemographics_BirthYearPath[];
extern const char kSyncDemographics_GenderPath[];

}  // namespace prefs

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_PREF_NAMES_H_
