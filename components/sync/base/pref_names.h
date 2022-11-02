// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_PREF_NAMES_H_
#define COMPONENTS_SYNC_BASE_PREF_NAMES_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace syncer::prefs {

// Boolean specifying whether the user finished setting up sync at least once.
inline constexpr char kSyncFirstSetupComplete[] = "sync.has_setup_completed";

// Boolean specifying whether to automatically sync all data types (including
// future ones, as they're added).  If this is true, the following preferences
// (kSyncBookmarks, kSyncPasswords, etc.) can all be ignored.
inline constexpr char kSyncKeepEverythingSynced[] =
    "sync.keep_everything_synced";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Boolean pref that records whether OS sync preferences were migrated due to
// SyncSettingsCategorization rollout.
inline constexpr char kOsSyncPrefsMigrated[] = "sync.os_sync_prefs_migrated";

// Boolean specifying whether to automatically sync all Chrome OS specific data
// types (including future ones). This includes types like printers, OS-only
// settings, etc. If set, the individual type preferences can be ignored.
inline constexpr char kSyncAllOsTypes[] = "sync.all_os_types";

// Booleans specifying whether the user has selected to sync the following
// OS user selectable types.
inline constexpr char kSyncOsApps[] = "sync.os_apps";
inline constexpr char kSyncOsPreferences[] = "sync.os_preferences";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// On Lacros, apps sync for primary profile is controlled by the OS. This
// preference caches the last known value.
inline constexpr char kSyncAppsEnabledByOs[] = "sync.apps_enabled_by_os";
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

// Booleans specifying whether the user has selected to sync the following
// user selectable types.
inline constexpr char kSyncApps[] = "sync.apps";
inline constexpr char kSyncAutofill[] = "sync.autofill";
inline constexpr char kSyncBookmarks[] = "sync.bookmarks";
inline constexpr char kSyncExtensions[] = "sync.extensions";
inline constexpr char kSyncPasswords[] = "sync.passwords";
inline constexpr char kSyncPreferences[] = "sync.preferences";
inline constexpr char kSyncReadingList[] = "sync.reading_list";
inline constexpr char kSyncTabs[] = "sync.tabs";
inline constexpr char kSyncThemes[] = "sync.themes";
inline constexpr char kSyncTypedUrls[] = "sync.typed_urls";
inline constexpr char kSyncWifiConfigurations[] = "sync.wifi_configurations";
inline constexpr char kSyncSavedTabGroups[] = "sync.saved_tab_groups";

// Boolean used by enterprise configuration management in order to lock down
// sync.
inline constexpr char kSyncManaged[] = "sync.managed";

// Boolean whether has requested sync to be enabled. This is set early in the
// sync setup flow, after the user has pressed "turn on sync" but before they
// have accepted the confirmation dialog (that maps to kSyncFirstSetupComplete).
// This is also set to false when sync is disabled by the user in sync settings,
// or when sync was reset from the dashboard.
inline constexpr char kSyncRequested[] = "sync.requested";

// A string that can be used to restore sync encryption infrastructure on
// startup so that the user doesn't need to provide credentials on each start.
inline constexpr char kSyncEncryptionBootstrapToken[] =
    "sync.encryption_bootstrap_token";

// Stores whether a platform specific passphrase error prompt has been muted by
// the user (e.g. an Android system notification). Specifically, it stores which
// major product version was used to mute this error.
inline constexpr char kSyncPassphrasePromptMutedProductVersion[] =
    "sync.passphrase_prompt_muted_product_version";

// Enabled the local sync backend implemented by the LoopbackServer.
inline constexpr char kEnableLocalSyncBackend[] =
    "sync.enable_local_sync_backend";

// Specifies the local sync backend directory. The name is chosen to mimic
// user-data-dir etc. This flag only matters if the enable-local-sync-backend
// flag is present.
inline constexpr char kLocalSyncBackendDir[] = "sync.local_sync_backend_dir";

}  // namespace syncer::prefs

#endif  // COMPONENTS_SYNC_BASE_PREF_NAMES_H_
