// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MULTI_STORE_FORM_FETCHER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MULTI_STORE_FORM_FETCHER_H_

#include "base/macros.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"

namespace password_manager {

// Production implementation of FormFetcher that fetches credentials associated
// with a particular origin from both the account and profile password stores.
// When adding new member fields to this class, please, update the Clone()
// method accordingly.
class MultiStoreFormFetcher : public FormFetcherImpl {
 public:
  MultiStoreFormFetcher(PasswordStore::FormDigest form_digest,
                        const PasswordManagerClient* client,
                        bool should_migrate_http_passwords);
  ~MultiStoreFormFetcher() override;

  // FormFetcher overrides.
  void Fetch() override;
  bool IsBlacklisted() const override;
  bool IsMovingBlocked(const autofill::GaiaIdHash& destination,
                       const base::string16& username) const override;
  std::unique_ptr<FormFetcher> Clone() override;

  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override;
  void OnGetPasswordStoreResultsFrom(
      PasswordStore* store,
      std::vector<std::unique_ptr<PasswordForm>> results) override;

  // HttpPasswordStoreMigrator::Consumer:
  void ProcessMigratedForms(
      std::vector<std::unique_ptr<PasswordForm>> forms) override;

  // CompromisedCredentialsConsumer:
  void OnGetCompromisedCredentials(
      std::vector<CompromisedCredentials> compromised_credentials) override;

 private:
  void AggregatePasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results);

  // Splits |results| into |federated_|, |non_federated_|,
  // |is_blacklisted_in_profile_store_| and |is_blacklisted_in_account_store_|.
  void SplitResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override;

  // Whether there were any blacklisted credentials obtained from the profile
  // and account password stores respectively.
  bool is_blacklisted_in_profile_store_ = false;
  bool is_blacklisted_in_account_store_ = false;

  int wait_counter_ = 0;
  std::vector<std::unique_ptr<PasswordForm>> partial_results_;

  base::flat_map<PasswordStore*, std::unique_ptr<HttpPasswordStoreMigrator>>
      http_migrators_;

  DISALLOW_COPY_AND_ASSIGN(MultiStoreFormFetcher);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MULTI_STORE_FORM_FETCHER_H_
