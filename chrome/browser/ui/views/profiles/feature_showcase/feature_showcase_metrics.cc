// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/feature_showcase/feature_showcase_metrics.h"

#include <string_view>

#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ui/views/profiles/feature_showcase/feature_showcase_constants.h"

void RecordStepUserAction(FeatureShowcaseStep step,
                          FeatureShowcaseStepUserAction action) {
  static constexpr std::string_view kHistogramPrefix =
      "ProfilePicker.FREFlow.FeatureShowcase.StepUserAction.";

  // LINT.IfChange(FeatureShowcaseStepSuffix)
  std::string_view suffix;
  switch (step) {
    case FeatureShowcaseStep::kDefaultBrowser:
      suffix = "DefaultBrowser";
      break;
    case FeatureShowcaseStep::kGoogleLens:
      suffix = "GoogleLens";
      break;
    case FeatureShowcaseStep::kPasswordManager:
      suffix = "PasswordManager";
      break;
    case FeatureShowcaseStep::kThemesAndCustomization:
      suffix = "ThemesAndCustomization";
      break;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/profile/histograms.xml:FeatureShowcaseStepSuffix)

  base::UmaHistogramEnumeration(base::StrCat({kHistogramPrefix, suffix}),
                                action);
}

FeatureShowcaseStep GetFeatureShowcaseStep(std::string_view step_id) {
  static const base::NoDestructor<
      base::flat_map<std::string_view, FeatureShowcaseStep>>
      kStepMap({
          {kFeatureShowcaseDefaultBrowserStepIdentifier,
           FeatureShowcaseStep::kDefaultBrowser},
          {kFeatureShowcaseGoogleLensStepIdentifier,
           FeatureShowcaseStep::kGoogleLens},
          {kFeatureShowcasePasswordManagerStepIdentifier,
           FeatureShowcaseStep::kPasswordManager},
          {kFeatureShowcaseThemesAndCustomizationStepIdentifier,
           FeatureShowcaseStep::kThemesAndCustomization},
      });
  if (const auto it = kStepMap->find(step_id); it != kStepMap->end()) {
    return it->second;
  }
  NOTREACHED();
}
