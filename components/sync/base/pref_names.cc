// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/pref_names.h"

#include "build/chromeos_buildflags.h"

namespace syncer {

namespace prefs {

// Boolean specifying whether the user finished setting up sync at least once.
const char kSyncFirstSetupComplete[] = "sync.has_setup_completed";

// Boolean specifying whether to automatically sync all data types (including
// future ones, as they're added).  If this is true, the following preferences
// (kSyncBookmarks, kSyncPasswords, etc.) can all be ignored.
const char kSyncKeepEverythingSynced[] = "sync.keep_everything_synced";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Boolean pref that records whether OS sync preferences were migrated due to
// SyncSettingsCategorization rollout.
const char kOsSyncPrefsMigrated[] = "sync.os_sync_prefs_migrated";

// Boolean specifying whether to automatically sync all Chrome OS specific data
// types (including future ones). This includes types like printers, OS-only
// settings, etc. If set, the individual type preferences can be ignored.
const char kSyncAllOsTypes[] = "sync.all_os_types";

// Booleans specifying whether the user has selected to sync the following
// OS user selectable types.
const char kSyncOsApps[] = "sync.os_apps";
const char kSyncOsPreferences[] = "sync.os_preferences";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Booleans specifying whether the user has selected to sync the following
// user selectable types.
const char kSyncApps[] = "sync.apps";
const char kSyncAutofill[] = "sync.autofill";
const char kSyncBookmarks[] = "sync.bookmarks";
const char kSyncExtensions[] = "sync.extensions";
const char kSyncPasswords[] = "sync.passwords";
const char kSyncPreferences[] = "sync.preferences";
const char kSyncReadingList[] = "sync.reading_list";
const char kSyncTabs[] = "sync.tabs";
const char kSyncThemes[] = "sync.themes";
const char kSyncTypedUrls[] = "sync.typed_urls";
const char kSyncWifiConfigurations[] = "sync.wifi_configurations";

// Boolean used by enterprise configuration management in order to lock down
// sync.
const char kSyncManaged[] = "sync.managed";

// Boolean whether has requested sync to be enabled. This is set early in the
// sync setup flow, after the user has pressed "turn on sync" but before they
// have accepted the confirmation dialog (that maps to kSyncFirstSetupComplete).
// This is also set to false when sync is disabled by the user in sync settings,
// or when sync was reset from the dashboard.
const char kSyncRequested[] = "sync.requested";

// A string that can be used to restore sync encryption infrastructure on
// startup so that the user doesn't need to provide credentials on each start.
const char kSyncEncryptionBootstrapToken[] = "sync.encryption_bootstrap_token";

// Stores whether a platform specific passphrase error prompt has been muted by
// the user (e.g. an Android system notification). Specifically, it stores which
// major product version was used to mute this error.
const char kSyncPassphrasePromptMutedProductVersion[] =
    "sync.passphrase_prompt_muted_product_version";

// Enabled the local sync backend implemented by the LoopbackServer.
const char kEnableLocalSyncBackend[] = "sync.enable_local_sync_backend";

// Specifies the local sync backend directory. The name is chosen to mimic
// user-data-dir etc. This flag only matters if the enable-local-sync-backend
// flag is present.
const char kLocalSyncBackendDir[] = "sync.local_sync_backend_dir";

#if defined(OS_ANDROID)
// Stores whether sync should no longer respect the state of master toggle for
// this user.
// TODO(crbug.com/1107904): Clean pref when the decoupling logic is removed.
const char kSyncDecoupledFromAndroidMasterSync[] =
    "sync.decoupled_from_master_sync";
#endif  // defined(OS_ANDROID)

}  // namespace prefs

}  // namespace syncer
