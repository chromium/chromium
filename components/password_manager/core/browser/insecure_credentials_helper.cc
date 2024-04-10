// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/insecure_credentials_helper.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"

namespace password_manager {

namespace {

// Helper object which is used to update `PasswordForm::password_issues` for the
// affected credentials.
class InsecureCredentialsHelper : public PasswordStoreConsumer {
 public:
  explicit InsecureCredentialsHelper(PasswordStoreInterface* store);
  ~InsecureCredentialsHelper() override;

  void AddPhishedCredentials(const MatchingReusedCredential& credential);

  void RemovePhishedCredentials(const MatchingReusedCredential& credential);

 private:
  using LoginsResult = std::vector<std::unique_ptr<PasswordForm>>;

  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(LoginsResult results) override;

  void AddPhishedCredentialsInternal(const MatchingReusedCredential& credential,
                                     LoginsResult results);

  void RemovePhishedCredentialsInternal(
      const MatchingReusedCredential& credential,
      LoginsResult results);

  base::OnceCallback<void(LoginsResult)> operation_;

  raw_ptr<PasswordStoreInterface> store_;

  base::WeakPtrFactory<InsecureCredentialsHelper> weak_ptr_factory_{this};
};

InsecureCredentialsHelper::InsecureCredentialsHelper(
    PasswordStoreInterface* store)
    : store_(store) {}

InsecureCredentialsHelper::~InsecureCredentialsHelper() = default;

void InsecureCredentialsHelper::AddPhishedCredentials(
    const MatchingReusedCredential& credential) {
  PasswordFormDigest digest = {PasswordForm::Scheme::kHtml,
                               credential.signon_realm,
                               GURL(credential.signon_realm)};
  operation_ =
      base::BindOnce(&InsecureCredentialsHelper::AddPhishedCredentialsInternal,
                     base::Owned(this), credential);
  store_->GetLogins(digest, weak_ptr_factory_.GetWeakPtr());
}

void InsecureCredentialsHelper::RemovePhishedCredentials(
    const MatchingReusedCredential& credential) {
  PasswordFormDigest digest = {PasswordForm::Scheme::kHtml,
                               credential.signon_realm,
                               GURL(credential.signon_realm)};
  operation_ = base::BindOnce(
      &InsecureCredentialsHelper::RemovePhishedCredentialsInternal,
      base::Owned(this), credential);
  store_->GetLogins(digest, weak_ptr_factory_.GetWeakPtr());
}

void InsecureCredentialsHelper::OnGetPasswordStoreResults(
    LoginsResult results) {
  std::move(operation_).Run(std::move(results));
}

void InsecureCredentialsHelper::AddPhishedCredentialsInternal(
    const MatchingReusedCredential& credential,
    LoginsResult results) {
  for (auto& form : results) {
    if (form->signon_realm == credential.signon_realm &&
        form->username_value == credential.username) {
      if (form->password_issues.find(InsecureType::kPhished) ==
          form->password_issues.end()) {
        form->password_issues.insert(
            {InsecureType::kPhished,
             InsecurityMetadata(base::Time::Now(), IsMuted(false),
                                TriggerBackendNotification(false))});
        store_->UpdateLogin(*form);
      }
    }
  }
}

void InsecureCredentialsHelper::RemovePhishedCredentialsInternal(
    const MatchingReusedCredential& credential,
    LoginsResult results) {
  for (auto& form : results) {
    if (form->signon_realm == credential.signon_realm &&
        form->username_value == credential.username) {
      if (form->password_issues.find(InsecureType::kPhished) !=
          form->password_issues.end()) {
        form->password_issues.erase(InsecureType::kPhished);
        store_->UpdateLogin(*form);
      }
    }
  }
}

}  // namespace

void AddPhishedCredentials(PasswordStoreInterface* store,
                           const MatchingReusedCredential& credential) {
  InsecureCredentialsHelper* helper = new InsecureCredentialsHelper(store);
  helper->AddPhishedCredentials(credential);
}

void RemovePhishedCredentials(PasswordStoreInterface* store,
                              const MatchingReusedCredential& credential) {
  InsecureCredentialsHelper* helper = new InsecureCredentialsHelper(store);
  helper->RemovePhishedCredentials(credential);
}

}  // namespace password_manager
