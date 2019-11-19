// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FAKE_FORM_FETCHER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FAKE_FORM_FETCHER_H_

#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/statistics_table.h"

namespace autofill {
struct PasswordForm;
}

namespace password_manager {

struct InteractionsStats;

// Test implementation of FormFetcher useful for simple fakes and as a base for
// mocks.
class FakeFormFetcher : public FormFetcher {
 public:
  FakeFormFetcher();

  ~FakeFormFetcher() override;

  // Registers consumers to be notified when results are set. Unlike the
  // production version, assumes that results have not arrived yet, i.e., one
  // has to first call AddConsumer, then setters and finally
  // NotifyFetchCompleted().
  void AddConsumer(Consumer* consumer) override;

  void RemoveConsumer(Consumer* consumer) override;

  // Returns State::WAITING if Fetch() was called after any Set* calls, and
  // State::NOT_WAITING otherwise.
  State GetState() const override;

  // Statistics for recent password bubble usage.
  const std::vector<InteractionsStats>& GetInteractionsStats() const override;

  void set_stats(const std::vector<InteractionsStats>& stats) {
    state_ = State::NOT_WAITING;
    stats_ = stats;
  }

  void set_scheme(autofill::PasswordForm::Scheme scheme) { scheme_ = scheme; }

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

  void set_federated(
      const std::vector<const autofill::PasswordForm*>& federated) {
    state_ = State::NOT_WAITING;
    federated_ = federated;
  }

  void SetNonFederated(
      const std::vector<const autofill::PasswordForm*>& non_federated);

  void SetBlacklisted(bool is_blacklisted);

  void NotifyFetchCompleted();

  // Only sets the internal state to WAITING, no call to PasswordStore.
  void Fetch() override;

  // Returns a new FakeFormFetcher.
  std::unique_ptr<FormFetcher> Clone() override;

 private:
  base::ObserverList<Consumer> consumers_;
  State state_ = State::NOT_WAITING;
  autofill::PasswordForm::Scheme scheme_ =
      autofill::PasswordForm::Scheme::kHtml;
  std::vector<InteractionsStats> stats_;
  std::vector<const autofill::PasswordForm*> non_federated_;
  std::vector<const autofill::PasswordForm*> federated_;
  std::vector<const autofill::PasswordForm*> non_federated_same_scheme_;
  std::vector<const autofill::PasswordForm*> best_matches_;
  const autofill::PasswordForm* preferred_match_ = nullptr;
  bool is_blacklisted_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeFormFetcher);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FAKE_FORM_FETCHER_H_
