// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_FETCHER_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_FETCHER_IMPL_H_

#include <memory>
#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/http_password_store_migrator.h"
#include "components/password_manager/core/browser/password_store/password_store_backend_error.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"

namespace password_manager {

class PasswordManagerClient;

// Production implementation of FormFetcher. Fetches credentials associated with
// a particular origin from both the profile and account (if it exists) password
// stores. When adding new member fields to this class, please, update the
// Clone() method accordingly.
class FormFetcherImpl : public FormFetcher,
                        public PasswordStoreConsumer,
                        public HttpPasswordStoreMigrator::Consumer {
 public:
  // |form_digest| describes what credentials need to be retrieved and
  // |client| serves the PasswordStore, the logging information etc.
  FormFetcherImpl(PasswordFormDigest form_digest,
                  PasswordManagerClient* client,
                  bool should_migrate_http_passwords);

  FormFetcherImpl(const FormFetcherImpl&) = delete;
  FormFetcherImpl& operator=(const FormFetcherImpl&) = delete;

  ~FormFetcherImpl() override;

  // FormFetcher:
  void AddConsumer(FormFetcher::Consumer* consumer) override;
  void RemoveConsumer(FormFetcher::Consumer* consumer) override;
  void Fetch() override;
  State GetState() const override;
  const std::vector<InteractionsStats>& GetInteractionsStats() const override;
  std::vector<const PasswordForm*> GetInsecureCredentials() const override;
  std::vector<const PasswordForm*> GetNonFederatedMatches() const override;
  std::vector<const PasswordForm*> GetFederatedMatches() const override;
  bool IsBlocklisted() const override;
  bool IsMovingBlocked(const signin::GaiaIdHash& destination,
                       const std::u16string& username) const override;

  const std::vector<const PasswordForm*>& GetAllRelevantMatches()
      const override;
  const std::vector<const PasswordForm*>& GetBestMatches() const override;
  const PasswordForm* GetPreferredMatch() const override;
  std::unique_ptr<FormFetcher> Clone() override;
  std::optional<PasswordStoreBackendError> GetProfileStoreBackendError()
      const override;
  std::optional<PasswordStoreBackendError> GetAccountStoreBackendError()
      const override;

 protected:
  // Actually finds best matches and notifies consumers.
  void FindMatchesAndNotifyConsumers(
      std::vector<std::unique_ptr<PasswordForm>> results);

  // Splits |results| into |federated_|, |non_federated_|,
  // |is_blocklisted_in_profile_store_| and |is_blocklisted_in_account_store_|.
  void SplitResults(std::vector<std::unique_ptr<PasswordForm>> results);

  // PasswordStore results will be fetched for this description.
  const PasswordFormDigest form_digest_;

  // Client used to obtain a CredentialFilter.
  const raw_ptr<PasswordManagerClient> client_;

  // State of the fetcher.
  State state_ = State::NOT_WAITING;

  // False unless FetchDataFromPasswordStore has been called again without the
  // password store returning results in the meantime.
  bool need_to_refetch_ = false;

  // Results obtained from PasswordStore:
  std::vector<std::unique_ptr<PasswordForm>> non_federated_;

  // Federated credentials relevant to the observed form. They are neither
  // filled not saved by PasswordFormManager, so they are kept separately from
  // non-federated matches.
  std::vector<std::unique_ptr<PasswordForm>> federated_;

  // List of insecure credentials for the current domain.
  std::vector<std::unique_ptr<PasswordForm>> insecure_credentials_;

  // Indicates whether HTTP passwords should be migrated to HTTPS. This is
  // always false for non HTML forms.
  const bool should_migrate_http_passwords_;

 private:
  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override;
  void OnGetPasswordStoreResultsFrom(
      PasswordStoreInterface* store,
      std::vector<std::unique_ptr<PasswordForm>> results) override;
  void OnGetSiteStatistics(std::vector<InteractionsStats> stats) override;
  void OnGetPasswordStoreResultsOrErrorFrom(
      PasswordStoreInterface* store,
      LoginsResultOrError results_or_error) override;

  // HttpPasswordStoreMigrator::Consumer:
  void ProcessMigratedForms(
      std::vector<std::unique_ptr<PasswordForm>> forms) override;

  void AggregatePasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results);

  // Does the actual migration.
  base::flat_map<PasswordStoreInterface*,
                 std::unique_ptr<HttpPasswordStoreMigrator>>
      http_migrators_;

  // Non-federated credentials of the same scheme as the observed form.
  std::vector<const PasswordForm*> non_federated_same_scheme_;

  // Set of nonblocklisted PasswordForms from the password store that best match
  // the form being managed by |this|.
  std::vector<const PasswordForm*> best_matches_;

  // Whether there were any blocklisted credentials obtained from the profile
  // and account password stores respectively.
  bool is_blocklisted_in_profile_store_ = false;
  bool is_blocklisted_in_account_store_ = false;

  int wait_counter_ = 0;
  std::vector<std::unique_ptr<PasswordForm>> partial_results_;

  // Statistics for the current domain.
  std::vector<InteractionsStats> interactions_stats_;

  // Consumers of the fetcher, all are assumed to either outlive |this| or
  // remove themselves from the list during their destruction.
  base::ObserverList<FormFetcher::Consumer> consumers_;

  // Holds an error if it occurred during login retrieval from the
  // PasswordStore.
  std::optional<PasswordStoreBackendError> profile_store_backend_error_;
  std::optional<PasswordStoreBackendError> account_store_backend_error_;

  base::WeakPtrFactory<FormFetcherImpl> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_FETCHER_IMPL_H_
