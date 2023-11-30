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
const char kEmptyProfileStoreLoginDatabase[] =
    "password_manager.empty_profile_store_login_database";
const char kOfferToSavePasswordsEnabledGMS[] =
    "profile.save_passwords_enabed_gms";
const char kSavePasswordsSuspendedByError[] =
    "profile.save_passwords_suspended_by_error";
const char kSettingsMigratedToUPMLocal[] =
    "profile.settings_migrated_to_upm_local";

const char kCurrentMigrationVersionToGoogleMobileServices[] =
    "current_migration_version_to_google_mobile_services";

const char kTimeOfLastMigrationAttempt[] = "time_of_last_migration_attempt";

const char kPasswordsUseUPMLocalAndSeparateStores[] =
    "passwords_use_upm_local_and_separate_stores";

const char kRequiresMigrationAfterSyncStatusChange[] =
    "requires_migration_after_sync_status_change";

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

const char kUserAcknowledgedLocalPasswordsMigrationWarning[] =
    "user_acknowledged_local_passwords_migration_warning";
const char kLocalPasswordsMigrationWarningShownTimestamp[] =
    "local_passwords_migration_warning_shown_timestamp";
const char kLocalPasswordMigrationWarningShownAtStartup[] =
    "local_passwords_migration_warning_shown_at_startup";
const char kLocalPasswordMigrationWarningPrefsVersion[] =
    "local_passwords_migration_warning_reset_count";

const char kPasswordGenerationBottomSheetDismissCount[] =
    "password_generation_bottom_sheet_dismiss_count";
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

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
const char kAccountStoragePerAccountSettings[] =
    "profile.password_account_storage_settings";
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

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

const char kPasswordsPrefWithNewLabelUsed[] =
    "passwords_pref_with_new_label_used";

const char kProfileStoreDateLastUsedForFilling[] =
    "password_manager.profile_store_date_last_used_for_filling";
const char kAccountStoreDateLastUsedForFilling[] =
    "password_manager.account_store_date_last_used_for_filling";

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

#if BUILDFLAG(IS_IOS)
const char kAccountStorageNoticeShown[] =
    "password_manager.account_storage_notice_shown";

const char kAccountStorageNewFeatureIconImpressions[] =
    "password_manager.account_storage_new_feature_icon_impressions";
#endif  // BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
const char kPasswordGenerationNudgePasswordDismissCount[] =
    "password_generation_nudge_password_dismiss_count";

const char kPasswordManagerPromoCardsList[] =
    "password_manager.password_promo_cards_list";
#endif

const char kPasswordSharingEnabled[] =
    "password_manager.password_sharing_enabled";

#if BUILDFLAG(IS_MAC)
const char kRelaunchChromeBubbleDismissedCounter[] =
    "password_manager.relaunch_chrome_bubble_dismissed_counter";
#endif

}  // namespace prefs
}  // namespace password_manager
