// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/feature_showcase/feature_showcase_eligibility_tracker.h"

#include <algorithm>

#include "base/barrier_callback.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/ui/views/profiles/feature_showcase/feature_showcase_step_eligibility_checker.h"

namespace {
constexpr size_t kMaxFeatureShowcaseSteps = 3;

struct EligibilityCheckResult {
  size_t priority = 0;
  std::string identifier;
  bool is_eligible = false;
};

void OnStepEligibilityDetermined(
    std::string step_identifier,
    size_t priority,
    base::RepeatingCallback<void(EligibilityCheckResult)> barrier,
    bool is_eligible) {
  barrier.Run({.priority = priority,
               .identifier = std::move(step_identifier),
               .is_eligible = is_eligible});
}

void OnAllEligibilityChecksCompleted(
    base::OnceCallback<void(const std::vector<std::string>&)> final_callback,
    std::vector<EligibilityCheckResult> results) {
  std::ranges::sort(results, {}, &EligibilityCheckResult::priority);

  std::vector<std::string> eligible_steps;
  for (const auto& result : results) {
    if (result.is_eligible) {
      eligible_steps.push_back(result.identifier);
    }
  }

  if (eligible_steps.size() > kMaxFeatureShowcaseSteps) {
    eligible_steps.resize(kMaxFeatureShowcaseSteps);
  }

  std::move(final_callback).Run(eligible_steps);
}

}  // namespace

FeatureShowcaseEligibilityTracker::FeatureShowcaseEligibilityTracker(
    std::vector<std::unique_ptr<FeatureShowcaseStepEligibilityChecker>>
        checkers)
    : checkers_(std::move(checkers)) {}

FeatureShowcaseEligibilityTracker::~FeatureShowcaseEligibilityTracker() =
    default;

// TODO(crbug.com/507795442): Introduce a unified timeout mechanism.
void FeatureShowcaseEligibilityTracker::EvaluateEligibleSteps(
    Profile& profile,
    base::OnceCallback<void(const std::vector<std::string>&)> callback) {
  if (checkers_.empty()) {
    std::move(callback).Run({});
    return;
  }

  const size_t num_checkers = checkers_.size();
  auto barrier_callback = base::BarrierCallback<EligibilityCheckResult>(
      num_checkers,
      base::BindOnce(&OnAllEligibilityChecksCompleted, std::move(callback)));

  size_t priority = 0;
  for (const auto& checker : checkers_) {
    checker->CheckEligibility(
        profile, base::BindOnce(&OnStepEligibilityDetermined,
                                checker->GetStepIdentifier(), priority++,
                                barrier_callback));
  }
}
