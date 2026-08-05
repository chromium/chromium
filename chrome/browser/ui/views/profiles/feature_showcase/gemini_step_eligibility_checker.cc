// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/feature_showcase/gemini_step_eligibility_checker.h"

#include <utility>

#include "base/check_deref.h"
#include "base/functional/callback.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/variations/service/variations_service.h"

GeminiStepEligibilityChecker::GeminiStepEligibilityChecker() = default;

GeminiStepEligibilityChecker::~GeminiStepEligibilityChecker() = default;

void GeminiStepEligibilityChecker::CheckEligibility(
    Profile& profile,
    base::OnceCallback<void(bool)> callback) {
  profile_ = &profile;
  callback_ = std::move(callback);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  CHECK(identity_manager);

  // Filter out signed-out users.
  CoreAccountInfo primary_account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (primary_account.IsEmpty()) {
    std::move(callback_).Run(false);
    return;
  }

  identity_manager_observation_.Observe(identity_manager);

  // TODO(crbug.com/524959454): Remove this workaround once Finch country
  // available in the first run is ready.
  // `base::Unretained(this)` is safe here because `this` owns the timer,
  // which will be destroyed with `this`, cancelling any pending callbacks.
  variations_country_timer_.Start(
      FROM_HERE, base::Milliseconds(500),
      base::BindRepeating(&GeminiStepEligibilityChecker::CheckCountry,
                          base::Unretained(this)));

  CheckCountry();
  CheckAccountInfo();
}

std::string GeminiStepEligibilityChecker::GetStepIdentifier() const {
  return std::string(kFeatureShowcaseGeminiStepIdentifier);
}

bool GeminiStepEligibilityChecker::OnTimeout() {
  StopWaiting();
  callback_.Reset();
  return false;
}

void GeminiStepEligibilityChecker::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  CheckAccountInfo();
}

void GeminiStepEligibilityChecker::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  StopWaiting();
  if (callback_) {
    std::move(callback_).Run(false);
  }
}

void GeminiStepEligibilityChecker::StopWaiting() {
  identity_manager_observation_.Reset();
  variations_country_timer_.Stop();
}

void GeminiStepEligibilityChecker::CheckCountry() {
  variations::VariationsService& variations_service =
      CHECK_DEREF(g_browser_process->variations_service());

  const std::string latest_country = variations_service.GetLatestCountry();
  if (latest_country.empty()) {
    return;
  }

  const std::string stored_permanent_country =
      variations_service.GetStoredPermanentCountry();
  if (stored_permanent_country.empty()) {
    return;
  }

  country_data_ =
      CountryData{.stored_permanent_country = stored_permanent_country,
                  .latest_country = latest_country};
  variations_country_timer_.Stop();
  MaybeResolveEligibility();
}

void GeminiStepEligibilityChecker::CheckAccountInfo() {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  CHECK(identity_manager);

  CoreAccountInfo primary_account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

  AccountInfo account_info =
      identity_manager->FindExtendedAccountInfo(primary_account);
  if (account_info.GetAccountCapabilities().AreAllCapabilitiesKnown()) {
    account_info_ = account_info;
    identity_manager_observation_.Reset();
    MaybeResolveEligibility();
  }
}

void GeminiStepEligibilityChecker::MaybeResolveEligibility() {
  if (!callback_ || !country_data_ || !account_info_) {
    return;
  }

  StopWaiting();

  const bool is_eligible = glic::GlicEnabling::IsEnabledForFirstRunProfile(
      profile_, country_data_->stored_permanent_country,
      country_data_->latest_country, *account_info_);
  std::move(callback_).Run(is_eligible);
}
