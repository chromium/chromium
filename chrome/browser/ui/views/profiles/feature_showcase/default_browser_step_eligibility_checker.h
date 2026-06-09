// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_FEATURE_SHOWCASE_DEFAULT_BROWSER_STEP_ELIGIBILITY_CHECKER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_FEATURE_SHOWCASE_DEFAULT_BROWSER_STEP_ELIGIBILITY_CHECKER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/views/profiles/feature_showcase/feature_showcase_step_eligibility_checker.h"

inline constexpr char kFeatureShowcaseDefaultBrowserStepIdentifier[] =
    "default-browser";

class DefaultBrowserStepEligibilityChecker
    : public FeatureShowcaseStepEligibilityChecker {
 public:
  DefaultBrowserStepEligibilityChecker();
  ~DefaultBrowserStepEligibilityChecker() override;

  void CheckEligibility(Profile& profile,
                        base::OnceCallback<void(bool)> callback) override;

  std::string GetStepIdentifier() const override;

 protected:
  // Virtual for testing.
  virtual void StartCheckIsDefault(
      shell_integration::DefaultWebClientWorkerCallback callback);

 private:
  void OnCheckFinished(base::OnceCallback<void(bool)> callback,
                       shell_integration::DefaultWebClientState state);

  base::WeakPtrFactory<DefaultBrowserStepEligibilityChecker> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_FEATURE_SHOWCASE_DEFAULT_BROWSER_STEP_ELIGIBILITY_CHECKER_H_
