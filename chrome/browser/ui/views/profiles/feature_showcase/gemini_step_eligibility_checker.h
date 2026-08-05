// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_FEATURE_SHOWCASE_GEMINI_STEP_ELIGIBILITY_CHECKER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_FEATURE_SHOWCASE_GEMINI_STEP_ELIGIBILITY_CHECKER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/profiles/feature_showcase/feature_showcase_constants.h"
#include "chrome/browser/ui/views/profiles/feature_showcase/feature_showcase_step_eligibility_checker.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class GeminiStepEligibilityChecker
    : public FeatureShowcaseStepEligibilityChecker,
      public signin::IdentityManager::Observer {
 public:
  GeminiStepEligibilityChecker();
  GeminiStepEligibilityChecker(const GeminiStepEligibilityChecker&) = delete;
  GeminiStepEligibilityChecker& operator=(const GeminiStepEligibilityChecker&) =
      delete;
  ~GeminiStepEligibilityChecker() override;

  // FeatureShowcaseStepEligibilityChecker:
  void CheckEligibility(Profile& profile,
                        base::OnceCallback<void(bool)> callback) override;
  std::string GetStepIdentifier() const override;
  bool OnTimeout() override;

  // signin::IdentityManager::Observer:
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

 private:
  struct CountryData {
    std::string stored_permanent_country;
    std::string latest_country;
  };

  void CheckCountry();
  void CheckAccountInfo();
  void MaybeResolveEligibility();
  void StopWaiting();

  raw_ptr<Profile> profile_ = nullptr;
  base::OnceCallback<void(bool)> callback_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
  base::RepeatingTimer variations_country_timer_;

  std::optional<CountryData> country_data_;
  std::optional<AccountInfo> account_info_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_FEATURE_SHOWCASE_GEMINI_STEP_ELIGIBILITY_CHECKER_H_
