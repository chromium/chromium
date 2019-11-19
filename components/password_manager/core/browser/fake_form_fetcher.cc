// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/fake_form_fetcher.h"

#include <memory>

#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/statistics_table.h"

using autofill::PasswordForm;

namespace password_manager {

FakeFormFetcher::FakeFormFetcher() = default;

FakeFormFetcher::~FakeFormFetcher() = default;

void FakeFormFetcher::AddConsumer(Consumer* consumer) {
  consumers_.AddObserver(consumer);
}

void FakeFormFetcher::RemoveConsumer(Consumer* consumer) {
  consumers_.RemoveObserver(consumer);
}

FormFetcher::State FakeFormFetcher::GetState() const {
  return state_;
}

const std::vector<InteractionsStats>& FakeFormFetcher::GetInteractionsStats()
    const {
  return stats_;
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

void FakeFormFetcher::SetNonFederated(
    const std::vector<const PasswordForm*>& non_federated) {
  non_federated_ = non_federated;
  password_manager_util::FindBestMatches(
      non_federated_, scheme_,
      /*sort_matches_by_date_last_used=*/false, &non_federated_same_scheme_,
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

void FakeFormFetcher::Fetch() {
  state_ = State::WAITING;
}

std::unique_ptr<FormFetcher> FakeFormFetcher::Clone() {
  return std::make_unique<FakeFormFetcher>();
}

}  // namespace password_manager
