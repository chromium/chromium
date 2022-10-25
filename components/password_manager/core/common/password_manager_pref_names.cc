// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/password_manager_pref_names.h"

#include "build/build_config.h"

namespace password_manager {
namespace prefs {

const char kCredentialsEnableAutosignin[] = "credentials_enable_autosignin";
const char kCredentialsEnableService[] = "credentials_enable_service";

#if BUILDFLAG(IS_IOS)
const char kCredentialProviderEnabledOnStartup[] =
    "credential_provider_enabled_on_startup";
#endif

#if BUILDFLAG(IS_ANDROID)
const char kAutoSignInEnabledGMS[] = "profile.auto_sign_in_enabled_gms";
const char kOfferToSavePasswordsEnabledGMS[] =
    "profile.save_passwords_enabed_gms";
const char kSavePasswordsSuspendedByError[] =
    "profile.save_passwords_suspended_by_error";
const char kSettingsMigratedToUPM[] = "profile.settings_migrated_to_upm";

const char kCurrentMigrationVersionToGoogleMobileServices[] =
    "current_migration_version_to_google_mobile_services";

const char kTimeOfLastMigrationAttempt[] = "time_of_last_migration_attempt";

const char kRequiresMigrationAfterSyncStatusChange[] =
    "requires_migration_after_sync_status_change";

const char kPasswordsPrefWithNewLabelUsed[] =
    "passwords_pref_with_new_label_used";

const char kUnenrolledFromGoogleMobileServicesDueToErrors[] =
    "unenrolled_from_google_mobile_services_due_to_errors";
const char kUnenrolledFromGoogleMobileServicesAfterApiErrorCode[] =
    "unenrolled_from_google_mobile_services_after_api_error_code";
const char kUnenrolledFromGoogleMobileServicesWithErrorListVersion[] =
    "unenrolled_from_google_mobile_services_with_error_list_version";

const char kUPMErrorUIShownTimestamp[] = "profile.upm_error_ui_shown_timestamp";

const char kTimesReenrolledToGoogleMobileServices[] =
    "times_reenrolled_to_google_mobile_services";

const char kTimesAttemptedToReenrollToGoogleMobileServices[] =
    "times_attempted_to_reenroll_to_google_mobile_services";
const char kTimesUPMAuthErrorShown[] = "times_upm_auth_error_shown";
#endif

#if BUILDFLAG(IS_WIN)
const char kOsPasswordBlank[] = "password_manager.os_password_blank";
const char kOsPasswordLastChanged[] =
    "password_manager.os_password_last_changed";
const char kIsBiometricAvailable[] = "password_manager.is_biometric_avaliable";
#endif

#if BUILDFLAG(IS_APPLE)
const char kKeychainMigrationStatus[] = "password_manager.keychain_migration";
#endif

const char kWasAutoSignInFirstRunExperienceShown[] =
    "profile.was_auto_sign_in_first_run_experience_shown";

const char kWereOldGoogleLoginsRemoved[] =
    "profile.were_old_google_logins_removed";

const char kAccountStoragePerAccountSettings[] =
    "profile.password_account_storage_settings";

const char kSyncPasswordHash[] = "profile.sync_password_hash";

const char kSyncPasswordLengthAndHashSalt[] =
    "profile.sync_password_length_and_hash_salt";

const char kLastTimeObsoleteHttpCredentialsRemoved[] =
    "profile.last_time_obsolete_http_credentials_removed";

const char kLastTimePasswordCheckCompleted[] =
    "profile.last_time_password_check_completed";

const char kLastTimePasswordStoreMetricsReported[] =
    "profile.last_time_password_store_metrics_reported";

const char kSyncedLastTimePasswordCheckCompleted[] =
    "profile.credentials_last_password_checkup_time";

const char kPasswordHashDataList[] = "profile.password_hash_data_list";

const char kPasswordLeakDetectionEnabled[] =
    "profile.password_manager_leak_detection";

const char kPasswordDismissCompromisedAlertEnabled[] =
    "profile.password_dismiss_compromised_alert";

const char kProfileStoreDateLastUsedForFilling[] =
    "password_manager.profile_store_date_last_used_for_filling";
const char kAccountStoreDateLastUsedForFilling[] =
    "password_manager.account_store_date_last_used_for_filling";

const char kPasswordChangeSuccessTrackerFlows[] =
    "password_manager.password_change_success_tracker.flows";
const char kPasswordChangeSuccessTrackerVersion[] =
    "password_manager.password_change_success_tracker.version";

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
const char kBiometricAuthBeforeFillingPromoShownCounter[] =
    "password_manager.biometric_authentication_filling_promo_counter";
const char kHasUserInteractedWithBiometricAuthPromo[] =
    "password_manager.has_user_interacted_with_biometric_authentication_promo";
const char kBiometricAuthenticationBeforeFilling[] =
    "password_manager.biometric_authentication_filling";
const char kHadBiometricsAvailable[] =
    "password_manager.had_biometrics_available";
#endif

}  // namespace prefs
}  // namespace password_manager
