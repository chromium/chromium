// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_FEATURE_SHOWCASE_FEATURE_SHOWCASE_ELIGIBILITY_TRACKER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_FEATURE_SHOWCASE_FEATURE_SHOWCASE_ELIGIBILITY_TRACKER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"

class Profile;
class FeatureShowcaseStepEligibilityChecker;

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

  void EvaluateEligibleSteps(
      Profile& profile,
      base::OnceCallback<void(const std::vector<std::string>&)> callback);

 private:
  std::vector<std::unique_ptr<FeatureShowcaseStepEligibilityChecker>> checkers_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_FEATURE_SHOWCASE_FEATURE_SHOWCASE_ELIGIBILITY_TRACKER_H_
