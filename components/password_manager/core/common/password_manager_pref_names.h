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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_IOS)
// Boolean pref controlled by the DeletingUndecryptablePasswordsEnabled policy.
// If set to false it blocks deleting undecryptable passwords, otherwise the
// deletion can happen.
inline constexpr char kDeletingUndecryptablePasswordsEnabled[] =
    "password_manager.deleteting_undecryptable_passwords_enabled";
#endif

#if BUILDFLAG(IS_ANDROID)

// The timestamp at which the UPM password access loss warning was last
// shown to the user at the time of Chrome startup in microseconds since Windows
// epoch. This is needed to ensure that the UI is prompted only once per given
// time interval (currently seven days).
inline constexpr char kPasswordAccessLossWarningShownAtStartupTimestamp[] =
    "password_access_loss_warning_shown_at_startup_timestamp";

// The timestamp at which the UPM password access loss warning was last
// shown to the user in microseconds since Windows epoch. This is needed to
// ensure that the UI is prompted only once per given time interval (currently
// one day).
inline constexpr char kPasswordAccessLossWarningShownTimestamp[] =
    "password_access_loss_warning_shown_timestamp";

// Boolean pref indicating if the one-time notice for account storage was shown.
// The notice informs passwords will start being saved to the signed-in account.
inline constexpr char kAccountStorageNoticeShown[] =
    "password_manager.account_storage_notice_shown";

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
// last time migrated, in milliseconds since UNIX epoch.
inline constexpr char kTimeOfLastMigrationAttempt[] =
    "time_of_last_migration_attempt";
#endif

// The total amount of passwords available in Password Manager account store.
inline constexpr char kTotalPasswordsAvailableForAccount[] =
    "total_passwords_available_for_account";

// The total amount of passwords available in Password Manager profile store.
inline constexpr char kTotalPasswordsAvailableForProfile[] =
    "total_passwords_available_for_profile";

// The pref representing a bit vector that stores the reasons for password
// deletion from the Password Manager account store. It gets reset on Chrome
// startup, at most once per day.
inline constexpr char kPasswordRemovalReasonForAccount[] =
    "password_removal_reason_for_account";

// The pref representing a bit vector that stores the reasons for password
// deletion from the Password Manager profile store. It gets reset on Chrome
// startup, at most once per day.
inline constexpr char kPasswordRemovalReasonForProfile[] =
    "password_removal_reason_for_profile";

#if BUILDFLAG(IS_ANDROID)
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
// Values are also used for metrics recording.
enum class UseUpmLocalAndSeparateStoresState {
  kOff = 0,
  kOffAndMigrationPending = 1,
  kOn = 2,
  kMaxValue = kOn
};
inline constexpr char kPasswordsUseUPMLocalAndSeparateStores[] =
    "passwords_use_upm_local_and_separate_stores";

// Boolean value indicating if the user should not get UPM experience because
// of user-unresolvable errors received on communication with Google Mobile
// Services.
inline constexpr char kUnenrolledFromGoogleMobileServicesDueToErrors[] =
    "unenrolled_from_google_mobile_services_due_to_errors";

// Timestamp at which the last UPM error message was shown to the user in
// milliseconds since UNIX epoch (used in Java).
// This is needed to ensure that the UI is prompted only once per given
// time interval (currently 24h).
inline constexpr char kUPMErrorUIShownTimestamp[] =
    "profile.upm_error_ui_shown_timestamp";

// Boolean value meant to record in the prefs if the user clicked "Got it" in
// the UPM local passwords migration warning. When set to true, the warning
// should not be displayed again.
inline constexpr char kUserAcknowledgedLocalPasswordsMigrationWarning[] =
    "user_acknowledged_local_passwords_migration_warning";
#endif

// Maintains a list of password hashes of enterprise passwords. This pref
// differs from |kPasswordHashDataList| in two ways: it only stores password
// hashes for enterprise passwords and it is stored as a local state
// preference.
inline constexpr char kLocalPasswordHashDataList[] =
    "local.password_hash_data_list";

#if BUILDFLAG(IS_ANDROID)
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

// Whether the post password migration sheet ahould be shown at startup.
inline constexpr char kShouldShowPostPasswordMigrationSheetAtStartup[] =
    "should_show_post_password_migration_sheet_at_startup";
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

// List that contains captured password hashes. Only includes gaia password
// hashes.
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
// TODO(crbug.com/40185049): Remove this on Android once the feature is rolled
// out.
// TODO(crbug.com/40258836): Remove this for desktop once the feature is rolled
// out.
inline constexpr char kPasswordsPrefWithNewLabelUsed[] =
    "passwords_pref_with_new_label_used";

// Timestamps of when credentials from the profile / account store were last
// used to fill a form, in microseconds since Windows epoch.
inline constexpr char kProfileStoreDateLastUsedForFilling[] =
    "password_manager.profile_store_date_last_used_for_filling";
inline constexpr char kAccountStoreDateLastUsedForFilling[] =
    "password_manager.account_store_date_last_used_for_filling";

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
// Integer indicating how many times user saw biometric authentication before
// filling promo.
inline constexpr char kBiometricAuthBeforeFillingPromoShownCounter[] =
    "password_manager.biometric_authentication_filling_promo_counter";
// Boolean indicating whether user interacted with biometric authentication
// before filling promo.
inline constexpr char kHasUserInteractedWithBiometricAuthPromo[] =
    "password_manager.has_user_interacted_with_biometric_authentication_promo";
// Boolean indicating whether user had ever biometrics available on their
// device.
inline constexpr char kHadBiometricsAvailable[] =
    "password_manager.had_biometrics_available";
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_CHROMEOS)
// Boolean indicating whether user enabled biometric authentication before
// filling.
inline constexpr char kBiometricAuthenticationBeforeFilling[] =
    "password_manager.biometric_authentication_filling";
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
// A list of available promo cards with related information which are displayed
// in the Password Manager UI.
inline constexpr char kPasswordManagerPromoCardsList[] =
    "password_manager.password_promo_cards_list";

// A cache of whether the profile LoginDatabase has autofillable credentials.
inline constexpr char kAutofillableCredentialsProfileStoreLoginDatabase[] =
    "password_manager.autofillable_credentials_profile_store_login_database";

// A cache of whether the account LoginDatabase has autofillable credentials.
inline constexpr char kAutofillableCredentialsAccountStoreLoginDatabase[] =
    "password_manager.autofillable_credentials_account_store_login_database";
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

#if !BUILDFLAG(IS_ANDROID)
// Boolean pref indicating if the user is in one of the groups of the
// kClearUndecryptablePasswords experiment.
inline constexpr char kClearingUndecryptablePasswords[] =
    "password_manager.clearing_undecryptable_passwords";
#endif

// Boolean pref indicating if passwords were migrated to OSCryptAsync. Two for
// each store.
inline constexpr char kProfileStoreMigratedToOSCryptAsync[] =
    "password_manager.profile_store_migrated_to_os_crypt_async";
inline constexpr char kAccountStoreMigratedToOSCryptAsync[] =
    "password_manager.account_store_migrated_to_os_crypt_async";

}  // namespace password_manager::prefs

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_PREF_NAMES_H_
