// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_FETCHER_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_FETCHER_IMPL_H_

#include <memory>
#include <set>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/http_password_store_migrator.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

namespace password_manager {

class PasswordManagerClient;

// Production implementation of FormFetcher. Fetches credentials associated with
// a particular origin. When adding new member fields to this class, please,
// update the Clone() method accordingly.
class FormFetcherImpl : public FormFetcher,
                        public PasswordStoreConsumer,
                        public HttpPasswordStoreMigrator::Consumer {
 public:
  // |form_digest| describes what credentials need to be retrieved and
  // |client| serves the PasswordStore, the logging information etc.
  FormFetcherImpl(PasswordStore::FormDigest form_digest,
                  const PasswordManagerClient* client,
                  bool should_migrate_http_passwords);

  ~FormFetcherImpl() override;

  // Returns a MultiStoreFormFetcher if  the password account storage feature is
  // enabled. Returns a FormFetcherImpl otherwise.
  static std::unique_ptr<FormFetcherImpl> CreateFormFetcherImpl(
      PasswordStore::FormDigest form_digest,
      const PasswordManagerClient* client,
      bool should_migrate_http_passwords);

  // FormFetcher:
  void AddConsumer(FormFetcher::Consumer* consumer) override;
  void RemoveConsumer(FormFetcher::Consumer* consumer) override;
  State GetState() const override;
  const std::vector<InteractionsStats>& GetInteractionsStats() const override;

  std::vector<const autofill::PasswordForm*> GetNonFederatedMatches()
      const override;
  std::vector<const autofill::PasswordForm*> GetFederatedMatches()
      const override;

  bool IsBlacklisted() const override;

  const std::vector<const autofill::PasswordForm*>& GetAllRelevantMatches()
      const override;

  const std::vector<const autofill::PasswordForm*>& GetBestMatches()
      const override;

  const autofill::PasswordForm* GetPreferredMatch() const override;

  void Fetch() override;
  std::unique_ptr<FormFetcher> Clone() override;

  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;
  void OnGetSiteStatistics(std::vector<InteractionsStats> stats) override;

  // HttpPasswordStoreMigrator::Consumer:
  void ProcessMigratedForms(
      std::vector<std::unique_ptr<autofill::PasswordForm>> forms) override;

 protected:
  // PasswordStore results will be fetched for this description.
  const PasswordStore::FormDigest form_digest_;

  // Client used to obtain a CredentialFilter.
  const PasswordManagerClient* const client_;

  // State of the fetcher.
  State state_ = State::NOT_WAITING;

  // False unless FetchDataFromPasswordStore has been called again without the
  // password store returning results in the meantime.
  bool need_to_refetch_ = false;

  // If false, matches are sorted using the "preferred" field.
  bool sort_matches_by_date_last_used_ = false;

  // Processes password form results and forwards them to the |consumers_|.
  void ProcessPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results);

 private:
  // Splits |results| into |federated_|, |non_federated_| and |blacklisted_|.
  void SplitResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results);

  // Results obtained from PasswordStore:
  std::vector<std::unique_ptr<autofill::PasswordForm>> non_federated_;

  // Federated credentials relevant to the observed form. They are neither
  // filled not saved by PasswordFormManager, so they are kept separately from
  // non-federated matches.
  std::vector<std::unique_ptr<autofill::PasswordForm>> federated_;

  // Non-federated credentials of the same scheme as the observed form.
  std::vector<const autofill::PasswordForm*> non_federated_same_scheme_;

  // Set of nonblacklisted PasswordForms from the password store that best match
  // the form being managed by |this|.
  std::vector<const autofill::PasswordForm*> best_matches_;

  // Convenience pointer to entry in |best_matches_| that is marked as
  // preferred. This is only allowed to be null if there are no best matches at
  // all, since there will always be one preferred login when there are multiple
  // matches (when first saved, a login is marked preferred).
  const autofill::PasswordForm* preferred_match_ = nullptr;

  // Whether there were any blacklisted credentials obtained from the password
  // store.
  bool is_blacklisted_ = false;

  // Statistics for the current domain.
  std::vector<InteractionsStats> interactions_stats_;

  // Consumers of the fetcher, all are assumed to either outlive |this| or
  // remove themselves from the list during their destruction.
  base::ObserverList<FormFetcher::Consumer> consumers_;

  // Indicates whether HTTP passwords should be migrated to HTTPS.
  const bool should_migrate_http_passwords_;

  // Does the actual migration.
  std::unique_ptr<HttpPasswordStoreMigrator> http_migrator_;

  DISALLOW_COPY_AND_ASSIGN(FormFetcherImpl);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_FETCHER_IMPL_H_
