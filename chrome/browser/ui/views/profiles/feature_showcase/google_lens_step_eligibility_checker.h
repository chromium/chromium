// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_FEATURE_SHOWCASE_GOOGLE_LENS_STEP_ELIGIBILITY_CHECKER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_FEATURE_SHOWCASE_GOOGLE_LENS_STEP_ELIGIBILITY_CHECKER_H_

#include "chrome/browser/ui/views/profiles/feature_showcase/feature_showcase_constants.h"
#include "chrome/browser/ui/views/profiles/feature_showcase/feature_showcase_step_eligibility_checker.h"

class GoogleLensStepEligibilityChecker
    : public FeatureShowcaseStepEligibilityChecker {
 public:
  GoogleLensStepEligibilityChecker();
  GoogleLensStepEligibilityChecker(const GoogleLensStepEligibilityChecker&) =
      delete;
  GoogleLensStepEligibilityChecker& operator=(
      const GoogleLensStepEligibilityChecker&) = delete;
  ~GoogleLensStepEligibilityChecker() override;

  // FeatureShowcaseStepEligibilityChecker:
  void CheckEligibility(Profile& profile,
                        base::OnceCallback<void(bool)> callback) override;
  std::string GetStepIdentifier() const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_FEATURE_SHOWCASE_GOOGLE_LENS_STEP_ELIGIBILITY_CHECKER_H_
