// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FAKE_FORM_FETCHER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FAKE_FORM_FETCHER_H_

#include <vector>

#include "base/observer_list.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/statistics_table.h"

namespace password_manager {

struct InteractionsStats;

// Test implementation of FormFetcher useful for simple fakes and as a base for
// mocks.
class FakeFormFetcher : public FormFetcher {
 public:
  FakeFormFetcher();

  FakeFormFetcher(const FakeFormFetcher&) = delete;
  FakeFormFetcher& operator=(const FakeFormFetcher&) = delete;

  ~FakeFormFetcher() override;

  // Registers consumers to be notified when results are set. Unlike the
  // production version, assumes that results have not arrived yet, i.e., one
  // has to first call AddConsumer, then setters and finally
  // NotifyFetchCompleted().
  void AddConsumer(Consumer* consumer) override;
  void RemoveConsumer(Consumer* consumer) override;

  // Only sets the internal state to WAITING, no call to PasswordStore.
  void Fetch() override;

  // Returns State::WAITING if Fetch() was called after any Set* calls, and
  // State::NOT_WAITING otherwise.
  State GetState() const override;

  const std::vector<InteractionsStats>& GetInteractionsStats() const override;
  std::vector<const PasswordForm*> GetInsecureCredentials() const override;
  std::vector<const PasswordForm*> GetNonFederatedMatches() const override;
  std::vector<const PasswordForm*> GetFederatedMatches() const override;
  bool IsBlocklisted() const override;
  bool IsMovingBlocked(const autofill::GaiaIdHash& destination,
                       const std::u16string& username) const override;
  const std::vector<const PasswordForm*>& GetAllRelevantMatches()
      const override;
  const std::vector<const PasswordForm*>& GetBestMatches() const override;
  const PasswordForm* GetPreferredMatch() const override;
  // Returns a new FakeFormFetcher.
  std::unique_ptr<FormFetcher> Clone() override;
  absl::optional<PasswordStoreBackendError> GetProfileStoreBackendError()
      const override;

  void set_stats(const std::vector<InteractionsStats>& stats) {
    state_ = State::NOT_WAITING;
    stats_ = stats;
  }

  void set_scheme(PasswordForm::Scheme scheme) { scheme_ = scheme; }

  void set_federated(const std::vector<const PasswordForm*>& federated) {
    state_ = State::NOT_WAITING;
    federated_ = federated;
  }

  void set_insecure_credentials(
      const std::vector<const PasswordForm*>& credentials) {
    insecure_credentials_ = credentials;
  }

  void SetNonFederated(const std::vector<const PasswordForm*>& non_federated);

  void SetBlocklisted(bool is_blocklisted);

  void NotifyFetchCompleted();

  void SetProfileStoreBackendError(
      absl::optional<PasswordStoreBackendError> error);

 private:
  base::ObserverList<Consumer> consumers_;
  State state_ = State::NOT_WAITING;
  PasswordForm::Scheme scheme_ = PasswordForm::Scheme::kHtml;
  std::vector<InteractionsStats> stats_;
  std::vector<const PasswordForm*> non_federated_;
  std::vector<const PasswordForm*> federated_;
  std::vector<const PasswordForm*> non_federated_same_scheme_;
  std::vector<const PasswordForm*> best_matches_;
  std::vector<const PasswordForm*> insecure_credentials_;
  const PasswordForm* preferred_match_ = nullptr;
  bool is_blocklisted_ = false;
  absl::optional<PasswordStoreBackendError> profile_store_backend_error_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FAKE_FORM_FETCHER_H_
