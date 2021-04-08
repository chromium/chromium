// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"

namespace password_manager {
namespace prefs {

const char kCredentialsEnableAutosignin[] = "credentials_enable_autosignin";
const char kCredentialsEnableService[] = "credentials_enable_service";

#if defined(OS_WIN)
const char kOsPasswordBlank[] = "password_manager.os_password_blank";
const char kOsPasswordLastChanged[] =
    "password_manager.os_password_last_changed";
#endif

#if defined(OS_APPLE)
const char kKeychainMigrationStatus[] = "password_manager.keychain_migration";
const char kPasswordRecovery[] = "password_manager.password_recovery";
#endif

const char kWasAutoSignInFirstRunExperienceShown[] =
    "profile.was_auto_sign_in_first_run_experience_shown";

const char kWasPhishedCredentialsUploadedToSync[] =
    "profile.was_phished_credentials_uploaded_to_sync";

const char kWasSignInPasswordPromoClicked[] =
    "profile.was_sign_in_password_promo_clicked";

const char kNumberSignInPasswordPromoShown[] =
    "profile.number_sign_in_password_promo_shown";

const char kSignInPasswordPromoRevive[] =
    "profile.sign_in_password_promo_revive";

const char kAccountStoragePerAccountSettings[] =
    "profile.password_account_storage_settings";

const char kAccountStorageExists[] = "profile.password_account_storage_exists";

const char kSyncPasswordHash[] = "profile.sync_password_hash";

const char kSyncPasswordLengthAndHashSalt[] =
    "profile.sync_password_length_and_hash_salt";

const char kLastTimeObsoleteHttpCredentialsRemoved[] =
    "profile.last_time_obsolete_http_credentials_removed";

const char kLastTimePasswordCheckCompleted[] =
    "profile.last_time_password_check_completed";

const char kSyncedLastTimePasswordCheckCompleted[] =
    "profile.credentials_last_password_checkup_time";

const char kPasswordHashDataList[] = "profile.password_hash_data_list";

const char kPasswordLeakDetectionEnabled[] =
    "profile.password_manager_leak_detection";

const char kProfileStoreDateLastUsedForFilling[] =
    "password_manager.profile_store_date_last_used_for_filling";
const char kAccountStoreDateLastUsedForFilling[] =
    "password_manager.account_store_date_last_used_for_filling";

}  // namespace prefs
}  // namespace password_manager
