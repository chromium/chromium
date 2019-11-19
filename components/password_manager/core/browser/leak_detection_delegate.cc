// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection_delegate.h"

#include "base/metrics/histogram_functions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/save_password_progress_logger.h"
#include "components/password_manager/core/browser/compromised_credentials_table.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check_factory_impl.h"
#include "components/password_manager/core/browser/leak_detection_delegate_helper.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace password_manager {

using Logger = autofill::SavePasswordProgressLogger;

LeakDetectionDelegate::LeakDetectionDelegate(PasswordManagerClient* client)
    : client_(client),
      leak_factory_(std::make_unique<LeakDetectionCheckFactoryImpl>()) {}

LeakDetectionDelegate::~LeakDetectionDelegate() = default;

void LeakDetectionDelegate::StartLeakCheck(const autofill::PasswordForm& form) {
  if (client_->IsIncognito())
    return;

  if (!client_->GetPrefs()->GetBoolean(
          password_manager::prefs::kPasswordLeakDetectionEnabled)) {
    return;
  }

  if (form.username_value.empty())
    return;

  DCHECK(!form.password_value.empty());
  leak_check_ = leak_factory_->TryCreateLeakCheck(
      this, client_->GetIdentityManager(), client_->GetURLLoaderFactory());
  if (leak_check_) {
    is_leaked_timer_ = std::make_unique<base::ElapsedTimer>();
    leak_check_->Start(form.origin, form.username_value, form.password_value);
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

  password_manager::PasswordStore* password_store =
      client_->GetProfilePasswordStore();
  if (base::FeatureList::IsEnabled(password_manager::features::kLeakHistory)) {
    if (is_leaked) {
      password_store->AddCompromisedCredentials(CompromisedCredentials(
          url, username, base::Time::Now(), CompromiseType::kLeaked));
    } else {
      // If the credentials are not saved as leaked in the database, this call
      // will just get ignored.
      password_store->RemoveCompromisedCredentials(url, username);
    }
  }

  if (is_leaked) {
    if (!client_->GetPasswordFeatureManager()
             ->ShouldCheckReuseOnLeakDetection()) {
      // If leaked password reuse should not be checked, then the
      // |CredentialLeakType| needed to show the correct notification is already
      // determined.
      OnShowLeakDetectionNotification(
          CreateLeakType(IsSaved(false), IsReused(false), IsSyncing(false)),
          std::move(url), std::move(username));
    } else {
      // Otherwise query the helper to asynchronously determine the
      // |CredentialLeakType|.
      helper_ = std::make_unique<LeakDetectionDelegateHelper>(base::BindOnce(
          &LeakDetectionDelegate::OnShowLeakDetectionNotification,
          base::Unretained(this)));
      helper_->GetCredentialLeakType(password_store, std::move(url),
                                     std::move(username), std::move(password));
    }
  }
}

void LeakDetectionDelegate::OnShowLeakDetectionNotification(
    CredentialLeakType leak_type,
    GURL url,
    base::string16 username) {
  DCHECK(is_leaked_timer_);
  base::UmaHistogramTimes("PasswordManager.LeakDetection.NotifyIsLeakedTime",
                          std::exchange(is_leaked_timer_, nullptr)->Elapsed());
  helper_.reset();
  client_->NotifyUserCredentialsWereLeaked(leak_type, url);
}

void LeakDetectionDelegate::OnError(LeakDetectionError error) {
  leak_check_.reset();

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
        logger.LogMessage(Logger::STRING_LEAK_DETECTION_TOKEN_REQUEST_ERROR);
        break;
      case LeakDetectionError::kInvalidServerResponse:
        logger.LogMessage(
            Logger::STRING_LEAK_DETECTION_INVALID_SERVER_RESPONSE_ERROR);
        break;
    }
  }
}

}  // namespace password_manager
