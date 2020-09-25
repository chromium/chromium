// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection_delegate.h"

#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "components/autofill/core/common/save_password_progress_logger.h"
#include "components/password_manager/core/browser/compromised_credentials_table.h"
#include "components/password_manager/core/browser/form_parsing/form_parser.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check_factory_impl.h"
#include "components/password_manager/core/browser/leak_detection_delegate_helper.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace password_manager {
namespace {

using Logger = autofill::SavePasswordProgressLogger;

void LogString(const PasswordManagerClient* client,
               Logger::StringID string_id) {
  if (client && password_manager_util::IsLoggingActive(client)) {
    BrowserSavePasswordProgressLogger logger(client->GetLogManager());
    logger.LogMessage(string_id);
  }
}

}  // namespace

LeakDetectionDelegate::LeakDetectionDelegate(PasswordManagerClient* client)
    : client_(client),
      leak_factory_(std::make_unique<LeakDetectionCheckFactoryImpl>()) {}

LeakDetectionDelegate::~LeakDetectionDelegate() = default;

void LeakDetectionDelegate::StartLeakCheck(const PasswordForm& form) {
  if (client_->IsIncognito())
    return;

  if (!CanStartLeakCheck(*client_->GetPrefs(), client_))
    return;

  if (form.username_value.empty())
    return;

  DCHECK(!form.password_value.empty());
  leak_check_ = leak_factory_->TryCreateLeakCheck(
      this, client_->GetIdentityManager(), client_->GetURLLoaderFactory());
  // Reset the helper to avoid notifications from the currently running check.
  helper_.reset();
  if (leak_check_) {
    is_leaked_timer_ = std::make_unique<base::ElapsedTimer>();
    leak_check_->Start(form.url, form.username_value, form.password_value);
  }
}

void LeakDetectionDelegate::OnLeakDetectionDone(bool is_leaked,
                                                GURL url,
                                                base::string16 username,
                                                base::string16 password) {
  leak_check_.reset();
  if (password_manager_util::IsLoggingActive(client_)) {
    BrowserSavePasswordProgressLogger logger(client_->GetLogManager());
    logger.LogBoolean(Logger::STRING_LEAK_DETECTION_FINISHED, is_leaked);
  }

  bool force_dialog_for_testing = base::GetFieldTrialParamByFeatureAsBool(
      password_manager::features::kPasswordChange,
      password_manager::features::
          kPasswordChangeWithForcedDialogAfterEverySuccessfulSubmission,
      false);
  if (is_leaked || force_dialog_for_testing) {
    // Otherwise query the helper to asynchronously determine the
    // |CredentialLeakType|.
    helper_ = std::make_unique<LeakDetectionDelegateHelper>(
        client_->GetProfilePasswordStore(), client_->GetAccountPasswordStore(),
        base::BindOnce(&LeakDetectionDelegate::OnShowLeakDetectionNotification,
                       base::Unretained(this)));
    helper_->ProcessLeakedPassword(std::move(url), std::move(username),
                                   std::move(password));
  }
}

void LeakDetectionDelegate::OnShowLeakDetectionNotification(
    IsSaved is_saved,
    IsReused is_reused,
    GURL url,
    base::string16 username,
    CompromisedSitesCount saved_sites) {
  bool force_dialog_for_testing = base::GetFieldTrialParamByFeatureAsBool(
      password_manager::features::kPasswordChange,
      password_manager::features::
          kPasswordChangeWithForcedDialogAfterEverySuccessfulSubmission,
      false);
  if (force_dialog_for_testing) {
    helper_.reset();
    // Correct leak_type to offer change password.
    CredentialLeakType leak_type =
        CreateLeakType(is_saved, IsReused(false),
                       IsSyncing(client_->GetPasswordSyncState() ==
                                 SYNCING_NORMAL_ENCRYPTION));
    client_->NotifyUserCredentialsWereLeaked(leak_type, saved_sites, url,
                                             username);
    return;
  }

  DCHECK(is_leaked_timer_);
  base::UmaHistogramTimes("PasswordManager.LeakDetection.NotifyIsLeakedTime",
                          std::exchange(is_leaked_timer_, nullptr)->Elapsed());
  helper_.reset();
  CredentialLeakType leak_type = CreateLeakType(
      is_saved, is_reused,
      IsSyncing(client_->GetPasswordSyncState() == SYNCING_NORMAL_ENCRYPTION));
  base::UmaHistogramBoolean("PasswordManager.LeakDetection.IsPasswordSaved",
                            IsPasswordSaved(leak_type));
  base::UmaHistogramBoolean("PasswordManager.LeakDetection.IsPasswordReused",
                            IsPasswordUsedOnOtherSites(leak_type));
  base::UmaHistogramBoolean("PasswordManager.LeakDetection.IsSyncing",
                            IsSyncingPasswordsNormally(leak_type));
  client_->NotifyUserCredentialsWereLeaked(leak_type, saved_sites, url,
                                           username);
}

void LeakDetectionDelegate::OnError(LeakDetectionError error) {
  leak_check_.reset();

  base::UmaHistogramEnumeration("PasswordManager.LeakDetection.Error", error);
  if (password_manager_util::IsLoggingActive(client_)) {
    BrowserSavePasswordProgressLogger logger(client_->GetLogManager());
    switch (error) {
      case LeakDetectionError::kNotSignIn:
        logger.LogMessage(Logger::STRING_LEAK_DETECTION_SIGNED_OUT_ERROR);
        break;
      case LeakDetectionError::kTokenRequestFailure:
        logger.LogMessage(Logger::STRING_LEAK_DETECTION_TOKEN_REQUEST_ERROR);
        break;
      case LeakDetectionError::kHashingFailure:
        logger.LogMessage(Logger::STRING_LEAK_DETECTION_HASH_ERROR);
        break;
      case LeakDetectionError::kInvalidServerResponse:
        logger.LogMessage(
            Logger::STRING_LEAK_DETECTION_INVALID_SERVER_RESPONSE_ERROR);
        break;
      case LeakDetectionError::kNetworkError:
        logger.LogMessage(Logger::STRING_LEAK_DETECTION_NETWORK_ERROR);
        break;
      case LeakDetectionError::kQuotaLimit:
        logger.LogMessage(Logger::STRING_LEAK_DETECTION_QUOTA_LIMIT);
        break;
    }
  }
}

bool CanStartLeakCheck(const PrefService& prefs,
                       const PasswordManagerClient* client) {
  const bool is_leak_protection_on =
      prefs.GetBoolean(password_manager::prefs::kPasswordLeakDetectionEnabled);

  // Leak detection can only start if:
  // 1. The user has not opted out and Safe Browsing is turned on, or
  // 2. The user is an enhanced protection user
  safe_browsing::SafeBrowsingState sb_state =
      safe_browsing::GetSafeBrowsingState(prefs);
  switch (sb_state) {
    case safe_browsing::NO_SAFE_BROWSING:
      LogString(client, Logger::STRING_LEAK_DETECTION_DISABLED_SAFE_BROWSING);
      return false;
    case safe_browsing::STANDARD_PROTECTION:
      if (!is_leak_protection_on)
        LogString(client, Logger::STRING_LEAK_DETECTION_DISABLED_FEATURE);
      return is_leak_protection_on;
    case safe_browsing::ENHANCED_PROTECTION:
      // feature is on.
      break;
  }

  return true;
}

}  // namespace password_manager
