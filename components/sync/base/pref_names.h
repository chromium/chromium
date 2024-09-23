// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_PREF_NAMES_H_
#define COMPONENTS_SYNC_BASE_PREF_NAMES_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace syncer::prefs {

// Enabled the local sync backend implemented by the LoopbackServer.
inline constexpr char kEnableLocalSyncBackend[] =
    "sync.enable_local_sync_backend";

// Specifies the local sync backend directory. The name is chosen to mimic
// user-data-dir etc. This flag only matters if the enable-local-sync-backend
// flag is present.
inline constexpr char kLocalSyncBackendDir[] = "sync.local_sync_backend_dir";

// NOTE: All the "internal" prefs should not be used directly by non-sync code,
// but should rather always be accessed via SyncUserSettings.
// TODO(crbug.com/40265119): Clean up/replace any existing references to these
// prefs from outside components/sync/.
namespace internal {

// Boolean specifying whether the user finished setting up sync at least once.
// On ChromeOS-Ash, the concept of initial-sync-setup doesn't exist.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
inline constexpr char kSyncInitialSyncFeatureSetupComplete[] =
    "sync.has_setup_completed";
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Boolean specifying whether to automatically sync all data types (including
// future ones, as they're added).  If this is true, the following preferences
// (kSyncBookmarks, kSyncPasswords, etc.) can all be ignored.
inline constexpr char kSyncKeepEverythingSynced[] =
    "sync.keep_everything_synced";

// Dict specifying the selected types per account for signed-in, non-syncing
// users only.
inline constexpr char kSelectedTypesPerAccount[] =
    "sync.selected_types_per_account";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Boolean specifying whether sync was disabled due to a dashboard reset event.
inline constexpr char kSyncDisabledViaDashboard[] =
    "sync.disabled_via_dashboard";

// Boolean specifying whether to automatically sync all Chrome OS specific data
// types (including future ones). This includes types like printers, OS-only
// settings, etc. If set, the individual type preferences can be ignored.
inline constexpr char kSyncAllOsTypes[] = "sync.all_os_types";

// Booleans specifying whether the user has selected to sync the following
// OS user selectable types.
inline constexpr char kSyncOsApps[] = "sync.os_apps";
inline constexpr char kSyncOsPreferences[] = "sync.os_preferences";
inline constexpr char kSyncWifiConfigurations[] = "sync.wifi_configurations";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// On Lacros, apps sync for primary profile is controlled by the OS. This
// preference caches the last known value.
inline constexpr char kSyncAppsEnabledByOs[] = "sync.apps_enabled_by_os";
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

// Booleans specifying whether the user has selected to sync the following
// user selectable types. Which are also used as keys within
// "sync.selected_types_per_account".
inline constexpr char kSyncApps[] = "sync.apps";
inline constexpr char kSyncAutofill[] = "sync.autofill";
inline constexpr char kSyncBookmarks[] = "sync.bookmarks";
inline constexpr char kSyncCookies[] = "sync.cookies";
inline constexpr char kSyncExtensions[] = "sync.extensions";
// Note: The pref for history is called "typed_urls" for historic reasons - not
// worth the hassle of renaming.
inline constexpr char kSyncHistory[] = "sync.typed_urls";
inline constexpr char kSyncPasswords[] = "sync.passwords";
inline constexpr char kSyncPayments[] = "sync.payments";
inline constexpr char kSyncPreferences[] = "sync.preferences";
inline constexpr char kSyncProductComparison[] = "sync.product_comparison";
inline constexpr char kSyncReadingList[] = "sync.reading_list";
inline constexpr char kSyncSavedTabGroups[] = "sync.saved_tab_groups";
inline constexpr char kSyncSharedTabGroupData[] = "sync.shared_tab_group_data";
inline constexpr char kSyncTabs[] = "sync.tabs";
inline constexpr char kSyncThemes[] = "sync.themes";

// Boolean used by enterprise configuration management in order to lock down
// sync.
inline constexpr char kSyncManaged[] = "sync.managed";

// The type of encryption passphrase used. Determined and set the first time the
// engine is successfully initialized.
// Note that the actual values correspond to the proto version of
// PassphraseType, see ProtoPassphraseInt32ToEnum() etc.
inline constexpr char kSyncCachedPassphraseType[] =
    "sync.cached_passphrase_type";

// The user's TrustedVaultAutoUpgradeExperimentGroup, determined the first time
// the engine is successfully initialized.
inline constexpr char kSyncCachedTrustedVaultAutoUpgradeExperimentGroup[] =
    "sync.cached_trusted_vault_auto_upgrade_experiment_group";

// A string that can be used to restore sync encryption infrastructure on
// startup so that the user doesn't need to provide credentials on each start.
inline constexpr char kSyncEncryptionBootstrapToken[] =
    "sync.encryption_bootstrap_token";

// A dict that can be used to restore per-account the sync encryption
// infrastructure on startup so that the user doesn't need to provide
// credentials on each start.
inline constexpr char kSyncEncryptionBootstrapTokenPerAccount[] =
    "sync.encryption_bootstrap_token_per_account";

// Stores whether a platform specific passphrase error prompt has been muted by
// the user (e.g. an Android system notification). Specifically, it stores which
// major product version was used to mute this error.
inline constexpr char kSyncPassphrasePromptMutedProductVersion[] =
    "sync.passphrase_prompt_muted_product_version";

// Overall status of Sync-the-feature for the Sync-to-Signin migration,
// expressed as SyncFeatureStatusForSyncToSigninMigration.
inline constexpr char kSyncFeatureStatusForSyncToSigninMigration[] =
    "sync.feature_status_for_sync_to_signin";
// Prefix for boolean per-data-type statuses, to be suffixed with "." plus
// DataTypeToStableLowerCaseString().
inline constexpr char kSyncDataTypeStatusForSyncToSigninMigrationPrefix[] =
    "sync.data_type_status_for_sync_to_signin";

inline constexpr char kMigrateReadingListFromLocalToAccount[] =
    "sync.migrate_reading_list_from_local_to_account";

// State of SyncPrefs::MaybeMigratePrefsForSyncToSigninPart1() and
// MaybeMigratePrefsForSyncToSigninPart2(). Should be cleaned up after those
// migration methods are gone.
inline constexpr char kSyncToSigninMigrationState[] =
    "sync.sync_to_signin_migration_state";

// Set only if the kMigrateSyncingUserToSignedIn flag is enabled, reset if the
// flag is disabled.
// If the user has sync-the-feature enabled but TransportState::PAUSED, this
// records the first time MaybeMigrateSyncingUserToSignedIn() is called.
// This is not written for users who are in TransportState::PAUSED with
// sync-the-feature disabled.
inline constexpr char kFirstTimeTriedToMigrateSyncFeaturePausedToSignin[] =
    "sync.first_time_tried_to_migrate_sync_feature_paused_to_signin";

#if BUILDFLAG(IS_ANDROID)
// Name of a boolean pref recording whether the WEB_APK data went through a
// one-off wipe to fix crbug.com/361771496.
inline constexpr char kWipedWebAPkDataForMigration[] =
    "sync.wiped_web_apk_data_for_migration";
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace internal
}  // namespace syncer::prefs

#endif  // COMPONENTS_SYNC_BASE_PREF_NAMES_H_
