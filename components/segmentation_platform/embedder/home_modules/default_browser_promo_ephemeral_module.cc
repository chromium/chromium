// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/default_browser_promo_ephemeral_module.h"

#include "base/feature_list.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/ephemeral_module_utils.h"
#include "components/segmentation_platform/public/features.h"

namespace segmentation_platform::home_modules {

const char kIsDefaultBrowserSignalKey[] = "is_default_browser_chrome_ios";

DefaultBrowserPromoEphemeralModule::DefaultBrowserPromoEphemeralModule()
    : CardSelectionInfo(kDefaultBrowserPromoEphemeralModule) {}

std::map<SignalKey, FeatureQuery>
DefaultBrowserPromoEphemeralModule::GetInputs() {
  return {{kIsDefaultBrowserSignalKey,
           CreateFeatureQueryFromCustomInputName(kIsDefaultBrowserChromeIos)}};
}

CardSelectionInfo::ShowResult
DefaultBrowserPromoEphemeralModule::ComputeCardResult(
    const CardSelectionSignals& signals) const {
  // Check for a forced `ShowResult`.
  std::optional<CardSelectionInfo::ShowResult> forced_result =
      GetForcedEphemeralModuleShowResult();

  // If forced to show/hide and the module label matches the current module,
  // return true/false accordingly.
  if (forced_result.has_value() &&
      forced_result.value().result_label.has_value() &&
      IsModuleLabel(forced_result.value().result_label.value())) {
    return forced_result.value();
  }

  CardSelectionInfo::ShowResult result;
  result.result_label = kDefaultBrowserPromoEphemeralModule;

  std::optional<bool> resultForIsDefaultBrowserChromeIos =
      signals.GetSignal(kIsDefaultBrowserSignalKey);

  // Show the card if the user does not have Chrome set as their default
  // browser.
  if (resultForIsDefaultBrowserChromeIos.has_value() &&
      resultForIsDefaultBrowserChromeIos.value() == false) {
    result.position = EphemeralHomeModuleRank::kTop;
    return result;
  }

  result.position = EphemeralHomeModuleRank::kNotShown;
  return result;
}

bool DefaultBrowserPromoEphemeralModule::IsModuleLabel(std::string_view label) {
  return label == kDefaultBrowserPromoEphemeralModule;
}

bool DefaultBrowserPromoEphemeralModule::IsEnabled(int impression_count) {
  if (!base::FeatureList::IsEnabled(features::kDefaultBrowserMagicStackIos)) {
    return false;
  }

  std::optional<CardSelectionInfo::ShowResult> forced_result =
      GetForcedEphemeralModuleShowResult();

  if (forced_result.has_value() &&
      forced_result.value().result_label.has_value() &&
      IsModuleLabel(forced_result.value().result_label.value())) {
    return forced_result.value().position == EphemeralHomeModuleRank::kTop;
  }

  return impression_count <
         features::kMaxDefaultBrowserMagicStackIosImpressions.Get();
}

}  // namespace segmentation_platform::home_modules
