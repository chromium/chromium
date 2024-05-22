// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/http_credentials_cleaner.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "components/password_manager/core/browser/http_password_store_migrator.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "url/gurl.h"

namespace password_manager {

HttpCredentialCleaner::HttpCredentialCleaner(
    scoped_refptr<PasswordStoreInterface> store,
    base::RepeatingCallback<network::mojom::NetworkContext*()>
        network_context_getter,
    PrefService* prefs)
    : store_(std::move(store)),
      network_context_getter_(network_context_getter),
      prefs_(prefs) {}

HttpCredentialCleaner::~HttpCredentialCleaner() = default;

bool HttpCredentialCleaner::NeedsCleaning() {
  auto last = base::Time::FromSecondsSinceUnixEpoch(prefs_->GetDouble(
      password_manager::prefs::kLastTimeObsoleteHttpCredentialsRemoved));
  return ((base::Time::Now() - last).InDays() >= kCleanUpDelayInDays);
}

void HttpCredentialCleaner::StartCleaning(Observer* observer) {
  DCHECK(observer);
  DCHECK(!observer_);
  observer_ = observer;
  store_->GetAutofillableLogins(weak_ptr_factory_.GetWeakPtr());
}

void HttpCredentialCleaner::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  // Non HTTP or HTTPS credentials are ignored, in particular Android or
  // federated credentials.
  for (auto& form : RemoveNonHTTPOrHTTPSForms(std::move(results))) {
    FormKey form_key(
        {std::string(
             password_manager_util::GetSignonRealmWithProtocolExcluded(*form)),
         form->scheme, form->username_value});
    if (form->url.SchemeIs(url::kHttpScheme)) {
      auto origin = url::Origin::Create(form->url);
      PostHSTSQueryForHostAndNetworkContext(
          origin, network_context_getter_.Run(),
          base::BindOnce(&HttpCredentialCleaner::OnHSTSQueryResult,
                         weak_ptr_factory_.GetWeakPtr(), std::move(form),
                         form_key));
      ++total_http_credentials_;
    } else {  // HTTPS
      https_credentials_map_[form_key].insert(form->password_value);
    }
  }

  // This is needed in case of empty |results|.
  SetPrefIfDone();
}

void HttpCredentialCleaner::OnHSTSQueryResult(
    std::unique_ptr<PasswordForm> form,
    FormKey key,
    HSTSResult hsts_result) {
  ++processed_results_;
  absl::Cleanup report = [this] { SetPrefIfDone(); };

  if (hsts_result == HSTSResult::kError) {
    return;
  }

  bool is_hsts = (hsts_result == HSTSResult::kYes);

  auto user_it = https_credentials_map_.find(key);
  if (user_it == https_credentials_map_.end()) {
    // Credentials are not migrated yet.
    base::UmaHistogramEnumeration(
        "PasswordManager.HttpCredentials2",
        is_hsts ? HttpCredentialType::kHasNoMatchingHttpsWithHsts
                : HttpCredentialType::kHasNoMatchingHttpsWithoutHsts);
    if (is_hsts) {
      // Migrate credentials to HTTPS, by moving them.
      store_->AddLogin(
          HttpPasswordStoreMigrator::MigrateHttpFormToHttps(*form));
      store_->RemoveLogin(FROM_HERE, *form);
    }
    return;
  }

  if (base::Contains(user_it->second, form->password_value)) {
    // The password store contains the same credentials (signon_realm, scheme,
    // username and password) on HTTPS version of the form.
    base::UmaHistogramEnumeration(
        "PasswordManager.HttpCredentials2",
        is_hsts ? HttpCredentialType::kHasEquivalentHttpsWithHsts
                : HttpCredentialType::kHasEquivalentHttpsWithoutHsts);
    if (is_hsts) {
      // This HTTP credential is no more used.
      store_->RemoveLogin(FROM_HERE, *form);
    }
  } else {
    base::UmaHistogramEnumeration(
        "PasswordManager.HttpCredentials2",
        is_hsts ? HttpCredentialType::kHasConflictingHttpsWithHsts
                : HttpCredentialType::kHasConflictingHttpsWithoutHsts);
  }
}

void HttpCredentialCleaner::SetPrefIfDone() {
  if (processed_results_ != total_http_credentials_) {
    return;
  }

  prefs_->SetDouble(prefs::kLastTimeObsoleteHttpCredentialsRemoved,
                    base::Time::Now().InSecondsFSinceUnixEpoch());
  observer_->CleaningCompleted();
}

}  // namespace password_manager
