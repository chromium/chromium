// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/fake_form_fetcher.h"

#include <algorithm>
#include <memory>

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_util.h"

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

base::span<const PasswordForm> FakeFormFetcher::GetInsecureCredentials() const {
  return insecure_credentials_;
}

base::span<const PasswordForm> FakeFormFetcher::GetNonFederatedMatches() const {
  return non_federated_;
}

base::span<const PasswordForm> FakeFormFetcher::GetFederatedMatches() const {
  return federated_;
}

bool FakeFormFetcher::IsBlocklisted() const {
  return is_blocklisted_;
}

bool FakeFormFetcher::IsMovingBlocked(const signin::GaiaIdHash& destination,
                                      const std::u16string& username) const {
  // This is analogous to the implementation in
  // MultiStoreFormFetcher::IsMovingBlocked().
  for (const std::vector<PasswordForm>& matches_vector :
       {federated_, non_federated_}) {
    for (const PasswordForm& form : matches_vector) {
      // Only local entries can be moved to the account store (though
      // account store matches should never have |moving_blocked_for_list|
      // entries anyway).
      if (form.IsUsingAccountStore()) {
        continue;
      }
      // Ignore non-exact matches for blocking moving.
      if (password_manager_util::GetMatchType(form) !=
          password_manager_util::GetLoginMatchType::kExact) {
        continue;
      }
      if (form.username_value != username) {
        continue;
      }
      if (base::Contains(form.moving_blocked_for_list, destination)) {
        return true;
      }
    }
  }
  return false;
}

base::span<const PasswordForm> FakeFormFetcher::GetAllRelevantMatches() const {
  return non_federated_same_scheme_;
}

base::span<const PasswordForm> FakeFormFetcher::GetBestMatches() const {
  return base::make_span(best_matches_);
}

const PasswordForm* FakeFormFetcher::GetPreferredMatch() const {
  if (best_matches_.empty()) {
    return nullptr;
  }
  return &best_matches_[0];
}

std::optional<PasswordFormMetricsRecorder::MatchedFormType>
FakeFormFetcher::GetPreferredOrPotentialMatchedFormType() const {
  return preferred_or_potential_matched_form_type_;
}

std::unique_ptr<FormFetcher> FakeFormFetcher::Clone() {
  return std::make_unique<FakeFormFetcher>();
}

void FakeFormFetcher::SetNonFederated(
    const std::vector<PasswordForm>& non_federated) {
  CHECK(base::ranges::all_of(
      non_federated, [this](auto& form) { return form.scheme == scheme_; }));
  SetNonFederated(non_federated, non_federated);
}

void FakeFormFetcher::SetNonFederated(
    const std::vector<PasswordForm>& non_federated,
    const std::vector<PasswordForm>& non_federated_same_scheme) {
  non_federated_ = non_federated;
  non_federated_same_scheme_ = non_federated_same_scheme;
}

void FakeFormFetcher::SetBestMatches(
    const std::vector<PasswordForm>& best_matches) {
  best_matches_ = best_matches;
}
void FakeFormFetcher::SetBlocklisted(bool is_blocklisted) {
  is_blocklisted_ = is_blocklisted;
}

void FakeFormFetcher::NotifyFetchCompleted() {
  state_ = State::NOT_WAITING;
  for (Consumer& consumer : consumers_) {
    consumer.OnFetchCompleted();
  }
}

std::optional<PasswordStoreBackendError>
FakeFormFetcher::GetProfileStoreBackendError() const {
  return profile_store_backend_error_;
}

std::optional<PasswordStoreBackendError>
FakeFormFetcher::GetAccountStoreBackendError() const {
  return account_store_backend_error_;
}

void FakeFormFetcher::SetProfileStoreBackendError(
    std::optional<PasswordStoreBackendError> error) {
  profile_store_backend_error_ = error;
}

void FakeFormFetcher::SetAccountStoreBackendError(
    std::optional<PasswordStoreBackendError> error) {
  account_store_backend_error_ = error;
}

}  // namespace password_manager
