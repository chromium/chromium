// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/fake_form_fetcher.h"

#include <memory>

#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/statistics_table.h"

namespace password_manager {

FakeFormFetcher::FakeFormFetcher() = default;

FakeFormFetcher::~FakeFormFetcher() = default;

void FakeFormFetcher::AddConsumer(Consumer* consumer) {
  consumers_.AddObserver(consumer);
}

void FakeFormFetcher::RemoveConsumer(Consumer* consumer) {
  consumers_.RemoveObserver(consumer);
}

void FakeFormFetcher::Fetch() {
  state_ = State::WAITING;
}

FormFetcher::State FakeFormFetcher::GetState() const {
  return state_;
}

const std::vector<InteractionsStats>& FakeFormFetcher::GetInteractionsStats()
    const {
  return stats_;
}

base::span<const CompromisedCredentials>
FakeFormFetcher::GetCompromisedCredentials() const {
  return base::make_span(compromised_);
}

std::vector<const PasswordForm*> FakeFormFetcher::GetNonFederatedMatches()
    const {
  return non_federated_;
}

std::vector<const PasswordForm*> FakeFormFetcher::GetFederatedMatches() const {
  return federated_;
}

bool FakeFormFetcher::IsBlacklisted() const {
  return is_blacklisted_;
}

bool FakeFormFetcher::IsMovingBlocked(const autofill::GaiaIdHash& destination,
                                      const base::string16& username) const {
  // This is analogous to the implementation in
  // MultiStoreFormFetcher::IsMovingBlocked().
  for (const std::vector<const PasswordForm*>& matches_vector :
       {federated_, non_federated_}) {
    for (const PasswordForm* form : matches_vector) {
      // Only local entries can be moved to the account store (though
      // account store matches should never have |moving_blocked_for_list|
      // entries anyway).
      if (form->IsUsingAccountStore())
        continue;
      // Ignore PSL matches for blocking moving.
      if (form->is_public_suffix_match)
        continue;
      if (form->username_value != username)
        continue;
      if (base::Contains(form->moving_blocked_for_list, destination))
        return true;
    }
  }
  return false;
}

const std::vector<const PasswordForm*>& FakeFormFetcher::GetAllRelevantMatches()
    const {
  return non_federated_same_scheme_;
}

const std::vector<const PasswordForm*>& FakeFormFetcher::GetBestMatches()
    const {
  return best_matches_;
}

const PasswordForm* FakeFormFetcher::GetPreferredMatch() const {
  return preferred_match_;
}

std::unique_ptr<FormFetcher> FakeFormFetcher::Clone() {
  return std::make_unique<FakeFormFetcher>();
}

void FakeFormFetcher::SetNonFederated(
    const std::vector<const PasswordForm*>& non_federated) {
  non_federated_ = non_federated;
  password_manager_util::FindBestMatches(non_federated_, scheme_,
                                         &non_federated_same_scheme_,
                                         &best_matches_, &preferred_match_);
}

void FakeFormFetcher::SetBlacklisted(bool is_blacklisted) {
  is_blacklisted_ = is_blacklisted;
}

void FakeFormFetcher::NotifyFetchCompleted() {
  state_ = State::NOT_WAITING;
  for (Consumer& consumer : consumers_)
    consumer.OnFetchCompleted();
}
}  // namespace password_manager
