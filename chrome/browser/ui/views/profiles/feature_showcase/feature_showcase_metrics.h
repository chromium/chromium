// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_FEATURE_SHOWCASE_FEATURE_SHOWCASE_METRICS_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_FEATURE_SHOWCASE_FEATURE_SHOWCASE_METRICS_H_

#include <string_view>

// Represents the different steps available in the Feature Showcase flow.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(FeatureShowcaseStep)
enum class FeatureShowcaseStep {
  kDefaultBrowser = 0,
  kGoogleLens = 1,
  kPasswordManager = 2,
  kThemesAndCustomization = 3,
  kMaxValue = kThemesAndCustomization,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/profile/enums.xml:FeatureShowcaseStep)

// Represents the possible actions a user can take on a Feature Showcase step.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(FeatureShowcaseStepUserAction)
enum class FeatureShowcaseStepUserAction {
  kDeclined = 0,
  kAccepted = 1,
  kMaxValue = kAccepted,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/profile/enums.xml:FeatureShowcaseStepUserAction)

// Records the action taken by the user on a specific Feature Showcase step.
void RecordStepUserAction(FeatureShowcaseStep step,
                          FeatureShowcaseStepUserAction action);

// Returns the enum value for the given step identifier string.
FeatureShowcaseStep GetFeatureShowcaseStep(std::string_view step_id);

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_FEATURE_SHOWCASE_FEATURE_SHOWCASE_METRICS_H_
