// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection_delegate.h"

#include "build/build_config.h"
#include "components/autofill/core/common/save_password_progress_logger.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/form_parsing/form_data_parser.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check_factory_impl.h"
#include "components/password_manager/core/browser/leak_detection_delegate_helper.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/browser/split_stores_and_local_upm.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace password_manager {
namespace {

using Logger = autofill::SavePasswordProgressLogger;

std::unique_ptr<autofill::SavePasswordProgressLogger> GetLogger(
    PasswordManagerClient* client) {
  if (client && password_manager_util::IsLoggingActive(client)) {
    return std::make_unique<BrowserSavePasswordProgressLogger>(
        client->GetLogManager());
  }
  return nullptr;
}

}  // namespace

LeakDetectionDelegate::LeakDetectionDelegate(PasswordManagerClient* client)
    : client_(client),
      leak_factory_(std::make_unique<LeakDetectionCheckFactoryImpl>()) {}

LeakDetectionDelegate::~LeakDetectionDelegate() = default;

void LeakDetectionDelegate::StartLeakCheck(LeakDetectionInitiator initiator,
                                           const PasswordForm& credentials,
                                           const GURL& form_url) {
  if (client_->IsOffTheRecord()) {
    return;
  }

  if (!LeakDetectionCheck::CanStartLeakCheck(*client_->GetPrefs(), form_url,
                                             GetLogger(client_))) {
    return;
  }

  if (credentials.username_value.empty()) {
    return;
  }

  DCHECK(!credentials.password_value.empty());

  leak_check_ = leak_factory_->TryCreateLeakCheck(
      this, client_->GetIdentityManager(), client_->GetURLLoaderFactory(),
      client_->GetChannel());
  // Reset the helper to avoid notifications from the currently running check.
  helper_.reset();
  if (leak_check_) {
    is_leaked_timer_ = std::make_unique<base::ElapsedTimer>();
    leak_check_->Start(initiator, credentials.url, credentials.username_value,
                       credentials.password_value);
  }
}

void LeakDetectionDelegate::OnLeakDetectionDone(bool is_leaked,
                                                GURL url,
                                                std::u16string username,
                                                std::u16string password) {
  leak_check_.reset();
  if (password_manager_util::IsLoggingActive(client_)) {
    BrowserSavePasswordProgressLogger logger(client_->GetLogManager());
    logger.LogBoolean(Logger::STRING_LEAK_DETECTION_FINISHED, is_leaked);
  }

  if (is_leaked) {
    // Query the helper to asynchronously determine the `CredentialLeakType`.
    helper_ = std::make_unique<LeakDetectionDelegateHelper>(
        client_->GetProfilePasswordStore(), client_->GetAccountPasswordStore(),
        base::BindOnce(&LeakDetectionDelegate::OnShowLeakDetectionNotification,
                       base::Unretained(this)));
    helper_->ProcessLeakedPassword(std::move(url), std::move(username),
                                   std::move(password));
  }
}

void LeakDetectionDelegate::OnShowLeakDetectionNotification(
    PasswordForm::Store in_stores,
    IsReused is_reused,
    GURL url,
    std::u16string username,
    std::vector<GURL> all_urls_with_leaked_credentials) {
  std::vector<std::pair<GURL, std::u16string>> identities;
  for (const auto& u : all_urls_with_leaked_credentials) {
    identities.emplace_back(u, username);
  }
  client_->MaybeReportEnterprisePasswordBreachEvent(identities);

  DCHECK(is_leaked_timer_);
  base::UmaHistogramTimes("PasswordManager.LeakDetection.NotifyIsLeakedTime",
                          std::exchange(is_leaked_timer_, nullptr)->Elapsed());
  helper_.reset();

  const bool in_account_store =
      (in_stores & PasswordForm::Store::kAccountStore) ==
      PasswordForm::Store::kAccountStore;

  // A credential is marked as syncing if either the profile store is synced
  // or it is in the account store.
  IsSyncing is_syncing{false};

  if (in_account_store) {
    // Credential saved to the account store.
    is_syncing = IsSyncing{true};
  } else {
    // Credential saved to the local-or-syncable store.
#if BUILDFLAG(IS_ANDROID)
    const bool uses_split_stores_for_sync_users =
        UsesSplitStoresAndUPMForLocal(client_->GetPrefs());
#else
    const bool uses_split_stores_for_sync_users = false;
#endif  // BUILDFLAG(IS_ANDROID)

    if (!uses_split_stores_for_sync_users) {
      // TODO(crbug.com/40066949): Remove this codepath once
      // IsSyncFeatureEnabled() is fully deprecated.
      is_syncing = IsSyncing(sync_util::IsSyncFeatureEnabledIncludingPasswords(
          client_->GetSyncService()));
    }
  }

  CredentialLeakType leak_type =
      CreateLeakType(IsSaved(in_stores != PasswordForm::Store::kNotSet),
                     is_reused, is_syncing);
  client_->NotifyUserCredentialsWereLeaked(leak_type, url, username,
                                           in_account_store);
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

}  // namespace password_manager
