// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_PREF_NAMES_H_
#define COMPONENTS_SYNC_BASE_PREF_NAMES_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace syncer {

namespace prefs {

extern const char kSyncFirstSetupComplete[];
extern const char kSyncKeepEverythingSynced[];

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kOsSyncPrefsMigrated[];
extern const char kSyncAllOsTypes[];
extern const char kSyncOsApps[];
extern const char kSyncOsPreferences[];
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
extern const char kSyncPassphrasePromptMutedProductVersion[];

extern const char kEnableLocalSyncBackend[];
extern const char kLocalSyncBackendDir[];

#if defined(OS_ANDROID)
extern const char kSyncDecoupledFromAndroidMasterSync[];
#endif  // defined(OS_ANDROID)

}  // namespace prefs

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_PREF_NAMES_H_
