// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection_delegate.h"

#include <optional>
#include <variant>

#include "base/barrier_callback.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/autofill/core/common/save_password_progress_logger.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/form_parsing/form_data_parser.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check_factory_impl.h"
#include "components/password_manager/core/browser/leak_detection_delegate_helper.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/password_change_service_interface.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/password_sync_util.h"
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
        client->GetCurrentLogManager());
  }
  return nullptr;
}

std::variant<LeakedPasswordDetails, GURL> PassUrl(GURL url) {
  return url;
}

LeakedPasswordDetails MergeResponses(
    std::vector<std::variant<LeakedPasswordDetails, GURL>> results) {
  CHECK_EQ(2u, results.size());

  std::optional<LeakedPasswordDetails> details;
  GURL url;

  for (auto& result : results) {
    if (std::holds_alternative<LeakedPasswordDetails>(result)) {
      details = std::move(std::get<LeakedPasswordDetails>(result));
      continue;
    }

    CHECK(std::holds_alternative<GURL>(result));
    url = std::move(std::get<GURL>(result));
  }

  CHECK(details.has_value());
  details->credentials.change_password_url = std::move(url);
  return std::move(*details);
}

}  // namespace

LeakDetectionDelegate::LeakDetectionDelegate(PasswordManagerClient* client)
    : client_(client),
      leak_factory_(std::make_unique<LeakDetectionCheckFactoryImpl>()) {}

LeakDetectionDelegate::~LeakDetectionDelegate() = default;

void LeakDetectionDelegate::StartLeakCheck(
    LeakDetectionInitiator initiator,
    const PasswordForm& credentials,
    const GURL& form_url,
    bool is_non_password_login_detected) {
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
      client_->GetIdentityManager(), client_->GetURLLoaderFactory(),
      client_->GetChannel());
  // Reset the helper to avoid notifications from the currently running check.
  helper_.reset();
  if (leak_check_) {
    leak_check_->Start(
        initiator, credentials,
        base::BindOnce(&LeakDetectionDelegate::OnLeakDetectionDone,
                       weak_ptr_factory_.GetWeakPtr(), credentials,
                       base::Time::Now(), is_non_password_login_detected));
  }
}

void LeakDetectionDelegate::OnLeakDetectionDone(
    PasswordForm credentials,
    base::Time check_start_time,
    bool is_non_password_login_detected,
    base::expected<IsLeaked, LeakDetectionError> result) {
  if (!result.has_value()) {
    OnError(result.error());
    return;
  }

  const bool is_leaked = result.value().value();

  if (password_manager_util::IsLoggingActive(client_)) {
    BrowserSavePasswordProgressLogger logger(client_->GetCurrentLogManager());
    logger.LogBoolean(Logger::STRING_LEAK_DETECTION_FINISHED, is_leaked);
  }
  if (!is_leaked) {
    return;
  }

  auto notify_callback =
      base::BindOnce(&LeakDetectionDelegate::NotifyUserCredentialsWereLeaked,
                     weak_ptr_factory_.GetWeakPtr(), check_start_time,
                     is_non_password_login_detected);

  auto barrier_callback =
      base::BarrierCallback<std::variant<LeakedPasswordDetails, GURL>>(
          /*num_callbacks=*/2,
          base::BindOnce(&MergeResponses).Then(std::move(notify_callback)));

  affiliations::AffiliationService* affiliation_service =
      client_->GetAffiliationService();
  if (affiliation_service && !is_non_password_login_detected &&
      base::FeatureList::IsEnabled(
          features::kFetchChangePasswordUrlForPasswordChange)) {
    affiliation_service->FetchChangePasswordURL(
        credentials.url, base::BindOnce(&PassUrl).Then(barrier_callback));
  } else {
    barrier_callback.Run(GURL());
  }

  if (base::FeatureList::IsEnabled(features::kMarkAllCredentialsAsLeaked)) {
    GURL url = credentials.url;
    auto leak_details = PrepareLeakDetails(
        PasswordForm::Store::kNotSet, IsReused(false), IsSavedAsBackup(false),
        password_manager::FromPasswordForm(std::move(credentials)),
        /*all_urls_with_leaked_credentials=*/{url});
    barrier_callback.Run(std::move(leak_details));
  } else {
    // Query the helper to asynchronously determine the `CredentialLeakType`.
    helper_ = std::make_unique<LeakDetectionDelegateHelper>(
        client_->GetProfilePasswordStore(), client_->GetAccountPasswordStore(),
        base::BindOnce(&LeakDetectionDelegate::PrepareLeakDetails,
                       base::Unretained(this))
            .Then(barrier_callback));
    helper_->ProcessLeakedPassword(
        password_manager::FromPasswordForm(std::move(credentials)));
  }
}

LeakedPasswordDetails LeakDetectionDelegate::PrepareLeakDetails(
    PasswordForm::Store in_stores,
    IsReused is_reused,
    IsSavedAsBackup is_saved_as_backup,
    StoredCredential credentials,
    std::vector<GURL> all_urls_with_leaked_credentials) {
  std::vector<std::pair<GURL, std::u16string>> identities;
  for (const auto& u : all_urls_with_leaked_credentials) {
    identities.emplace_back(u, credentials.username_value);
  }
  client_->MaybeReportEnterprisePasswordBreachEvent(identities);

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
#if !BUILDFLAG(IS_ANDROID)
    // TODO(crbug.com/40066949): Remove this codepath once
    // IsSyncFeatureEnabled() is fully deprecated.
    is_syncing = IsSyncing(sync_util::IsSyncFeatureEnabledIncludingPasswords(
        client_->GetSyncService()));
#endif
  }

  // Clear change password URL, to avoid reusing stale change password URL from
  // cache.
  credentials.change_password_url = GURL();

  CredentialLeakType leak_type = CreateLeakType(
      IsSaved(in_stores != PasswordForm::Store::kNotSet), is_reused, is_syncing,
      HasChangePasswordUrl(false), is_saved_as_backup);
  return LeakedPasswordDetails(
      leak_type, password_manager::ToPasswordForm(std::move(credentials)),
      in_account_store);
}

void LeakDetectionDelegate::NotifyUserCredentialsWereLeaked(
    base::Time check_start_time,
    bool is_non_password_login_detected,
    LeakedPasswordDetails details) {
  base::UmaHistogramTimes("PasswordManager.LeakDetection.NotifyIsLeakedTime",
                          base::Time::Now() - check_start_time);

  HasChangePasswordUrl has_change_url(
      !is_non_password_login_detected && client_->GetPasswordChangeService() &&
      client_->GetPasswordChangeService()->IsPasswordChangeSupported(
          details.credentials, is_non_password_login_detected));
  if (has_change_url) {
    details.leak_type |= CredentialLeakFlags::kHasChangePasswordUrl;
  }
  client_->NotifyUserCredentialsWereLeaked(std::move(details));
}

void LeakDetectionDelegate::OnError(LeakDetectionError error) {
  base::UmaHistogramEnumeration("PasswordManager.LeakDetection.Error", error);
  if (password_manager_util::IsLoggingActive(client_)) {
    BrowserSavePasswordProgressLogger logger(client_->GetCurrentLogManager());
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
