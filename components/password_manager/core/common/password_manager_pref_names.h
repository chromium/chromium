// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_PREF_NAMES_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_PREF_NAMES_H_

#include "build/build_config.h"

namespace password_manager::prefs {

// Alphabetical list of preference names specific to the PasswordManager
// component.

// Boolean controlling whether the password manager allows automatic signing in
// through Credential Management API.
//
// IMPORTANT: This pref is neither querried nor updated on Android if the
// unified password manager is enabled.
// Use `password_manager_util::IsAutoSignInEnabled` to check
// the value of this setting instead.
inline constexpr char kCredentialsEnableAutosignin[] =
    "credentials_enable_autosignin";

// The value of this preference controls whether the Password Manager will save
// credentials. When it is false, it doesn't ask if you want to save passwords
// but will continue to fill passwords.
//
// IMPORTANT: This pref is neither querried nor updated on Android if the
// unified password manager is enabled.
// Use `password_manager_util::IsSavingPasswordsEnabled` to check the value of
// this setting instead.
inline constexpr char kCredentialsEnableService[] =
    "credentials_enable_service";

#if BUILDFLAG(IS_IOS)
// The value of this preference determines whether the user had enabled the
// credential provider in their iOS settings at startup.
inline constexpr char kCredentialProviderEnabledOnStartup[] =
    "credential_provider_enabled_on_startup";
#endif

#if BUILDFLAG(IS_ANDROID)
// Boolean controlling whether the password manager allows automatic signing in
// through Credential Management API. This pref is not synced. Its value is set
// by fetching the latest value from Google Mobile Services. Except for
// migration steps, it should not be modified in Chrome.
inline constexpr char kAutoSignInEnabledGMS[] =
    "profile.auto_sign_in_enabled_gms";

// A cache of whether the profile LoginDatabase is empty, so that can be checked
// early on startup.
inline constexpr char kEmptyProfileStoreLoginDatabase[] =
    "password_manager.empty_profile_store_login_database";

// Boolean controlling whether the password manager offers to save passwords.
// If false, the password manager will not save credentials, but it will still
// fill previously saved ones. This pref is not synced. Its value is set
// by fetching the latest value from Google Mobile Services. Except for
// migration steps, it should not be modified in Chrome.
//
// This pref doesn't have a policy mapped to it directly, instead, the policy
// mapped to `kCredentialEnableService` will be applied.
inline constexpr char kOfferToSavePasswordsEnabledGMS[] =
    "profile.save_passwords_enabed_gms";

// Boolean that disables saving by overriding kOfferToSavePasswordsEnabledGMS.
// If there are errors that prevent successful saves, this pref will be true and
// users should act as if kOfferToSavePasswordsEnabledGMS was disabled. If this
// pref is false, the value of kOfferToSavePasswordsEnabledGMS applies. This
// pref is not synced since errors presumably affect only the local client. Its
// value is set automatically whenever communication with GMS succeeds or fails.
//
// This pref doesn't have a policy mapped to it. It is temporary in nature and
// can only be stricter than any policy applied
inline constexpr char kSavePasswordsSuspendedByError[] =
    "profile.save_passwords_suspended_by_error";

// Boolean value indicating whether the regular prefs that apply to the local
// password store were migrated to UPM settings. It will be set to true
// automatically if there is nothing to migrate.
inline constexpr char kSettingsMigratedToUPMLocal[] =
    "profile.settings_migrated_to_upm_local";

// Integer value which indicates the version used to migrate passwords from
// built in storage to Google Mobile Services.
inline constexpr char kCurrentMigrationVersionToGoogleMobileServices[] =
    "current_migration_version_to_google_mobile_services";

// Timestamps of when credentials from the GMS Core to the built in storage were
// last time migrated, in microseconds since Windows epoch.
inline constexpr char kTimeOfLastMigrationAttempt[] =
    "time_of_last_migration_attempt";

// Integer pref indicating whether the client is ready to use UPM for local
// passwords and settings and split password stores for syncing users.
// The preconditions for the pref to be set to true:
// - M2: For users syncing passwords, the profile store contents have been
// moved to the account store. For the users who are not syncing passwords, the
// login database is empty and prefs are default.
// - M3: For the users who are not syncing passwords, the passwords have been
// successfully copied to GMS Core. The settings will be migrated as well, but
// their migration doesn't impact this pref.
//
// Do not renumber UseUpmLocalAndSeparateStoresState, values are persisted.
enum class UseUpmLocalAndSeparateStoresState {
  kOff = 0,
  kOffAndMigrationPending = 1,
  kOn = 2,
};
inline constexpr char kPasswordsUseUPMLocalAndSeparateStores[] =
    "passwords_use_upm_local_and_separate_stores";

// Boolean value that indicated the need of data migration between the two
// backends due to sync settings change.
inline constexpr char kRequiresMigrationAfterSyncStatusChange[] =
    "requires_migration_after_sync_status_change";

// Boolean value indicating if the user should not get UPM experience because
// of user-unresolvable errors received on communication with Google Mobile
// Services.
inline constexpr char kUnenrolledFromGoogleMobileServicesDueToErrors[] =
    "unenrolled_from_google_mobile_services_due_to_errors";

// Integer value indicating the Google Mobile Services API error code that
// caused the last unenrollment from the UPM experience. Only set if
// |kUnenrolledFromGoogleMobileServicesDueToErrors| is true.
inline constexpr char kUnenrolledFromGoogleMobileServicesAfterApiErrorCode[] =
    "unenrolled_from_google_mobile_services_after_api_error_code";

// Integer value indicating the version of the ignored/retriable error list
// during the last unenrollment from the UPM experience. User will not be
// re-enrolled if this value is set and is not less than the in the current
// error list version.
inline constexpr char
    kUnenrolledFromGoogleMobileServicesWithErrorListVersion[] =
        "unenrolled_from_google_mobile_services_with_error_list_version";

// Timestamp at which the last UPM error message was shown to the user in
// milliseconds since UNIX epoch (used in Java).
// This is needed to ensure that the UI is prompted only once per given
// time interval (currently 24h).
inline constexpr char kUPMErrorUIShownTimestamp[] =
    "profile.upm_error_ui_shown_timestamp";

// Integer value indicating the number of times the client was reenrolled into
// the UPM experiment after experiencing user-unresolvable errors in
// communication with Google Mobile Services.
inline constexpr char kTimesReenrolledToGoogleMobileServices[] =
    "times_reenrolled_to_google_mobile_services";

// Integer value indicating the number of times the client has attempted a
// migration in an attempt to reenroll into the UPM experiment. Reset to zero
// after a successful reenrollment.
inline constexpr char kTimesAttemptedToReenrollToGoogleMobileServices[] =
    "times_attempted_to_reenroll_to_google_mobile_services";

// Boolean value meant to record in the prefs if the user clicked "Got it" in
// the UPM local passwords migration warning. When set to true, the warning
// should not be displayed again.
inline constexpr char kUserAcknowledgedLocalPasswordsMigrationWarning[] =
    "user_acknowledged_local_passwords_migration_warning";

// The timestamp at which the last UPM local passwords migration warning was
// shown to the user in microseconds since Windows epoch. This is needed to
// ensure that the UI is prompted only once per given time interval (currently
// one month).
inline constexpr char kLocalPasswordsMigrationWarningShownTimestamp[] =
    "local_passwords_migration_warning_shown_timestamp";

// Whether the local password migration warning was already shown at startup.
inline constexpr char kLocalPasswordMigrationWarningShownAtStartup[] =
    "local_passwords_migration_warning_shown_at_startup";

// The version of the password migration warning prefs.
inline constexpr char kLocalPasswordMigrationWarningPrefsVersion[] =
    "local_passwords_migration_warning_reset_count";

// How many times the password generation bottom sheet was dismissed by the user
// in a row. The counter resets when the user applies password generation.
inline constexpr char kPasswordGenerationBottomSheetDismissCount[] =
    "password_generation_bottom_sheet_dismiss_count";
#endif

#if BUILDFLAG(IS_WIN)
// Whether the password was blank, only valid if OS password was last changed
// on or before the value contained in kOsPasswordLastChanged.
inline constexpr char kOsPasswordBlank[] = "password_manager.os_password_blank";

// The number of seconds since epoch that the OS password was last changed.
inline constexpr char kOsPasswordLastChanged[] =
    "password_manager.os_password_last_changed";

// Whether biometric authentication is available on this device.
inline constexpr char kIsBiometricAvailable[] =
    "password_manager.is_biometric_avaliable";
#endif

#if BUILDFLAG(IS_APPLE)
// The current status of migrating the passwords from the Keychain to the
// database. Stores a value from MigrationStatus.
inline constexpr char kKeychainMigrationStatus[] =
    "password_manager.keychain_migration";
#endif

// Boolean that indicated whether first run experience for the auto sign-in
// prompt was shown or not.
inline constexpr char kWasAutoSignInFirstRunExperienceShown[] =
    "profile.was_auto_sign_in_first_run_experience_shown";

// Boolean that indicated whether one time removal of old google.com logins was
// performed.
inline constexpr char kWereOldGoogleLoginsRemoved[] =
    "profile.were_old_google_logins_removed";

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
// A dictionary of account-storage-related settings that exist per Gaia account
// (e.g. whether that user has opted in). It maps from hash of Gaia ID to
// dictionary of key-value pairs.
inline constexpr char kAccountStoragePerAccountSettings[] =
    "profile.password_account_storage_settings";
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

// String that represents the sync password hash.
inline constexpr char kSyncPasswordHash[] = "profile.sync_password_hash";

// String that represents the sync password length and salt. Its format is
// encrypted and converted to base64 string "<password length, as ascii
// int>.<16 char salt>".
inline constexpr char kSyncPasswordLengthAndHashSalt[] =
    "profile.sync_password_length_and_hash_salt";

// Indicates the time (in seconds) when last cleaning of obsolete HTTP
// credentials was performed.
inline constexpr char kLastTimeObsoleteHttpCredentialsRemoved[] =
    "profile.last_time_obsolete_http_credentials_removed";

// The last time the password check has run to completion.
inline constexpr char kLastTimePasswordCheckCompleted[] =
    "profile.last_time_password_check_completed";

// Timestamps of when password store metrics where last reported, in
// microseconds since Windows epoch.
inline constexpr char kLastTimePasswordStoreMetricsReported[] =
    "profile.last_time_password_store_metrics_reported";

// List that contains captured password hashes.
inline constexpr char kPasswordHashDataList[] =
    "profile.password_hash_data_list";

// Boolean indicating whether Chrome should check whether the credentials
// submitted by the user were part of a leak.
inline constexpr char kPasswordLeakDetectionEnabled[] =
    "profile.password_manager_leak_detection";

// Boolean indicating whether users can mute (aka dismiss) alerts resulting from
// compromised credentials that were submitted by the user.
inline constexpr char kPasswordDismissCompromisedAlertEnabled[] =
    "profile.password_dismiss_compromised_alert";

// Boolean value indicating if the user has clicked on the "Password Manager"
// item in settings after switching to the Unified Password Manager. A "New"
// label is shown for the users who have not clicked on this item yet.
// TODO(crbug.com/1217070): Remove this on Android once the feature is rolled
// out.
// TODO(crbug.com/1420597): Remove this for desktop once the feature is rolled
// out.
inline constexpr char kPasswordsPrefWithNewLabelUsed[] =
    "passwords_pref_with_new_label_used";

// Timestamps of when credentials from the profile / account store were last
// used to fill a form, in microseconds since Windows epoch.
inline constexpr char kProfileStoreDateLastUsedForFilling[] =
    "password_manager.profile_store_date_last_used_for_filling";
inline constexpr char kAccountStoreDateLastUsedForFilling[] =
    "password_manager.account_store_date_last_used_for_filling";

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
// Integer indicating how many times user saw biometric authentication before
// filling promo.
inline constexpr char kBiometricAuthBeforeFillingPromoShownCounter[] =
    "password_manager.biometric_authentication_filling_promo_counter";
// Boolean indicating whether user interacted with biometric authentication
// before filling promo.
inline constexpr char kHasUserInteractedWithBiometricAuthPromo[] =
    "password_manager.has_user_interacted_with_biometric_authentication_promo";
// Boolean indicating whether user enabled biometric authentication before
// filling.
inline constexpr char kBiometricAuthenticationBeforeFilling[] =
    "password_manager.biometric_authentication_filling";
// Boolean indicating whether user had ever biometrics available on their
// device.
inline constexpr char kHadBiometricsAvailable[] =
    "password_manager.had_biometrics_available";
#endif

#if BUILDFLAG(IS_IOS)
// Boolean pref indicating if the one-time notice for account storage was shown.
// The notice informs passwords will start being saved to the signed-in account.
inline constexpr char kAccountStorageNoticeShown[] =
    "password_manager.account_storage_notice_shown";

// Integer value indicating the number of times the "new feature icon" was
// displayed with the account storage opt-out toggle.
inline constexpr char kAccountStorageNewFeatureIconImpressions[] =
    "password_manager.account_storage_new_feature_icon_impressions";
#endif  // BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
// How many times in a row the password generation popup in `kNudgePassword`
// experiment was dismissed by the user. The counter resets when the user
// accepts password generation.
inline constexpr char kPasswordGenerationNudgePasswordDismissCount[] =
    "password_generation_nudge_password_dismiss_count";

// A list of available promo cards with related information which are displayed
// in the Password Manager UI.
inline constexpr char kPasswordManagerPromoCardsList[] =
    "password_manager.password_promo_cards_list";
#endif

// Boolean pref indicating whether password sharing is enabled. Enables both
// sending and receiving passwords.
inline constexpr char kPasswordSharingEnabled[] =
    "password_manager.password_sharing_enabled";

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Integer pref indicating how many times relaunch Chrome bubble was dismissed.
inline constexpr char kRelaunchChromeBubbleDismissedCounter[] =
    "password_manager.relaunch_chrome_bubble_dismissed_counter";
#endif

}  // namespace password_manager::prefs

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_PREF_NAMES_H_
