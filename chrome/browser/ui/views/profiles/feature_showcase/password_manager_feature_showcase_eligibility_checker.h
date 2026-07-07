// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_FEATURE_SHOWCASE_PASSWORD_MANAGER_FEATURE_SHOWCASE_ELIGIBILITY_CHECKER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_FEATURE_SHOWCASE_PASSWORD_MANAGER_FEATURE_SHOWCASE_ELIGIBILITY_CHECKER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/profiles/feature_showcase/feature_showcase_step_eligibility_checker.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"

class PasswordManagerFeatureShowcaseEligibilityChecker
    : public FeatureShowcaseStepEligibilityChecker,
      public syncer::SyncServiceObserver {
 public:
  PasswordManagerFeatureShowcaseEligibilityChecker();
  PasswordManagerFeatureShowcaseEligibilityChecker(
      const PasswordManagerFeatureShowcaseEligibilityChecker&) = delete;
  PasswordManagerFeatureShowcaseEligibilityChecker& operator=(
      const PasswordManagerFeatureShowcaseEligibilityChecker&) = delete;
  ~PasswordManagerFeatureShowcaseEligibilityChecker() override;

  // FeatureShowcaseStepEligibilityChecker:
  void CheckEligibility(Profile& profile,
                        base::OnceCallback<void(bool)> callback) override;
  std::string GetStepIdentifier() const override;
  bool OnTimeout() override;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

 private:
  void RunCallbackAndStopObserving();

  raw_ptr<Profile> profile_ = nullptr;
  base::OnceCallback<void(bool)> callback_;
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_FEATURE_SHOWCASE_PASSWORD_MANAGER_FEATURE_SHOWCASE_ELIGIBILITY_CHECKER_H_
