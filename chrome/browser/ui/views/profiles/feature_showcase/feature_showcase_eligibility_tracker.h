// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_FEATURE_SHOWCASE_FEATURE_SHOWCASE_ELIGIBILITY_TRACKER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_FEATURE_SHOWCASE_FEATURE_SHOWCASE_ELIGIBILITY_TRACKER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"

class Profile;
class FeatureShowcaseStepEligibilityChecker;

// Tracks and evaluates the eligibility of various Feature Showcase steps for a
// given profile.
class FeatureShowcaseEligibilityTracker {
 public:
  explicit FeatureShowcaseEligibilityTracker(
      std::vector<std::unique_ptr<FeatureShowcaseStepEligibilityChecker>>
          checkers);
  FeatureShowcaseEligibilityTracker(const FeatureShowcaseEligibilityTracker&) =
      delete;
  FeatureShowcaseEligibilityTracker& operator=(
      const FeatureShowcaseEligibilityTracker&) = delete;
  ~FeatureShowcaseEligibilityTracker();

  // Asynchronously evaluates the eligibility of all showcase steps (via passed
  // checkers).
  //
  // The results are returned via `callback` as a vector of step identifiers.
  // The returned vector is guaranteed to:
  //   1. Contain only identifiers for steps that the profile is eligible for.
  //   2. Be ordered by priority (based on the order checkers were provided).
  //   3. Be capped at a maximum number of steps.
  //
  // If a step checker does not return a result within the timeout period,
  // it is silently ignored and treated as ineligible.
  //
  // Note: Calling this method while an evaluation is already in progress will
  // cancel the ongoing evaluation and invoke its callback with an empty vector.
  void EvaluateEligibleSteps(
      Profile& profile,
      base::OnceCallback<void(const std::vector<std::string>&)> callback);

 private:
  void OnStepEligibilityDetermined(size_t index, bool is_eligible);
  void FinishEvaluation();

  std::vector<std::unique_ptr<FeatureShowcaseStepEligibilityChecker>> checkers_;
  base::OnceCallback<void(const std::vector<std::string>&)>
      on_eligibility_evaluated_callback_;

  std::vector<std::optional<bool>> results_;
  size_t completed_checkers_ = 0;

  base::OneShotTimer timeout_timer_;
  base::WeakPtrFactory<FeatureShowcaseEligibilityTracker> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_FEATURE_SHOWCASE_FEATURE_SHOWCASE_ELIGIBILITY_TRACKER_H_
