// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/browser/grammatical_gender_service.h"

#include <utility>

#include "base/feature_list.h"
#include "components/language/core/common/language_experiments.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/tribool.h"

namespace language {

GrammaticalGenderService::GrammaticalGenderService(
    signin::IdentityManager* identity_manager)
    : feature_enabled_(
          base::FeatureList::IsEnabled(kGrammaticalGenderLocalization)),
      identity_manager_(identity_manager) {
  CHECK(identity_manager_);

  // The gender can only change if the feature is enabled.
  if (feature_enabled_) {
    identity_manager_observation_.Observe(identity_manager_);
  }
  UpdateGrammaticalGender();
}

GrammaticalGenderService::~GrammaticalGenderService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::optional<GrammaticalGenderService::Gender>
GrammaticalGenderService::GetGrammaticalGender() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return gender_;
}

void GrammaticalGenderService::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void GrammaticalGenderService::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void GrammaticalGenderService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  identity_manager_observation_.Reset();
  identity_manager_ = nullptr;
  observers_.Notify(&Observer::OnGrammaticalGenderServiceShutdown);
}

void GrammaticalGenderService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateGrammaticalGender();
}

void GrammaticalGenderService::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The primary account's capabilities are only available once it has a
  // refresh token.
  MaybeUpdateGrammaticalGenderForAccount(account_info.account_id);
}

void GrammaticalGenderService::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeUpdateGrammaticalGenderForAccount(info.GetAccountId());
}

void GrammaticalGenderService::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Only stop observing the IdentityManager here. Observers are notified when
  // the service itself shuts down (see Shutdown()), so that they're only
  // notified once.
  identity_manager_observation_.Reset();
  identity_manager_ = nullptr;
}

std::optional<GrammaticalGenderService::Gender>
GrammaticalGenderService::ComputeGrammaticalGender() const {
  if (!feature_enabled_) {
    return Gender::kOther;
  }

  CoreAccountInfo primary_account_info =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (primary_account_info.IsEmpty()) {
    // Signed out users have no grammatical gender preference.
    return Gender::kOther;
  }

  // Note: unless kSkipRefreshTokenCheckInIdentityManager is enabled, this is
  // empty (with unknown capabilities) while the account has no refresh token
  // (e.g. before refresh tokens are loaded).
  const AccountInfo account_info =
      identity_manager_->FindExtendedAccountInfo(primary_account_info);
  const AccountCapabilities& capabilities =
      account_info.GetAccountCapabilities();
  const std::pair<Gender, signin::Tribool> candidates[] = {
      {Gender::kFeminine,
       capabilities.should_be_addressed_in_feminine_grammatical_gender()},
      {Gender::kMasculine,
       capabilities.should_be_addressed_in_masculine_grammatical_gender()},
      {Gender::kNeuter,
       capabilities.should_be_addressed_in_neuter_grammatical_gender()},
  };

  // The first gender whose capability is true wins. All of the capabilities
  // are fetched together, so if only some of them are known, the unknown ones
  // will stay unknown (e.g. the server doesn't return them), and there's no
  // need to wait for them.
  bool all_unknown = true;
  for (const auto& [candidate_gender, value] : candidates) {
    if (value == signin::Tribool::kTrue) {
      return candidate_gender;
    }
    if (value != signin::Tribool::kUnknown) {
      all_unknown = false;
    }
  }

  if (all_unknown) {
    // The capabilities haven't been fetched yet (e.g. refresh tokens aren't
    // loaded at startup). Wait for them, so that a previously applied gender
    // isn't reset in the meantime.
    return std::nullopt;
  }
  return Gender::kOther;
}

void GrammaticalGenderService::MaybeUpdateGrammaticalGenderForAccount(
    const CoreAccountId& account_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Only the primary account's capabilities affect the gender.
  if (account_id !=
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin)) {
    return;
  }
  UpdateGrammaticalGender();
}

void GrammaticalGenderService::UpdateGrammaticalGender() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<Gender> gender = ComputeGrammaticalGender();
  if (!gender || gender == gender_) {
    return;
  }

  gender_ = gender;
  observers_.Notify(&Observer::OnGrammaticalGenderChanged, *gender_);
}

}  // namespace language
