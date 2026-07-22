// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/feature_showcase/gemini_step_eligibility_checker.h"

#include <utility>

#include "base/functional/callback.h"

GeminiStepEligibilityChecker::GeminiStepEligibilityChecker() = default;

GeminiStepEligibilityChecker::~GeminiStepEligibilityChecker() = default;

void GeminiStepEligibilityChecker::CheckEligibility(
    Profile& profile,
    base::OnceCallback<void(bool)> callback) {
  // TODO(crbug.com/506845213): Substitute with real eligibility check.
  std::move(callback).Run(false);
}

std::string GeminiStepEligibilityChecker::GetStepIdentifier() const {
  return std::string(kFeatureShowcaseGeminiStepIdentifier);
}
