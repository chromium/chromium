// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_FEATURE_SHOWCASE_FEATURE_SHOWCASE_STEP_ELIGIBILITY_CHECKER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_FEATURE_SHOWCASE_FEATURE_SHOWCASE_STEP_ELIGIBILITY_CHECKER_H_

#include <string>

#include "base/functional/callback_forward.h"

class Profile;

class FeatureShowcaseStepEligibilityChecker {
 public:
  virtual ~FeatureShowcaseStepEligibilityChecker() = default;

  // Evaluates whether the step should be shown. The result is returned via
  // `callback`. If the check involves async operations, the implementation
  // should return the result as quickly as possible.
  virtual void CheckEligibility(Profile& profile,
                                base::OnceCallback<void(bool)> callback) = 0;

  // Returns a unique string identifier for the step to be passed to the WebUI.
  virtual std::string GetStepIdentifier() const = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_FEATURE_SHOWCASE_FEATURE_SHOWCASE_STEP_ELIGIBILITY_CHECKER_H_
