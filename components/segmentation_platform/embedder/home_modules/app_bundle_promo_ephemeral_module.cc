// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/app_bundle_promo_ephemeral_module.h"

#include "base/feature_list.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/ephemeral_module_utils.h"
#include "components/segmentation_platform/internal/metadata/feature_query.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {
namespace home_modules {

const char kAppBundleAppsInstalledCountSignalKey[] =
    "app_bundle_apps_installed_count";

AppBundlePromoEphemeralModule::AppBundlePromoEphemeralModule()
    : CardSelectionInfo(kAppBundlePromoEphemeralModule) {}

std::map<SignalKey, FeatureQuery> AppBundlePromoEphemeralModule::GetInputs() {
  return {
      {kAppBundleAppsInstalledCountSignalKey,
       CreateFeatureQueryFromCustomInputName(kAppBundleAppsInstalledCount)}};
}

CardSelectionInfo::ShowResult AppBundlePromoEphemeralModule::ComputeCardResult(
    const CardSelectionSignals& signals) const {
  // Check for a forced `ShowResult`.
  std::optional<CardSelectionInfo::ShowResult> forced_result =
      GetForcedEphemeralModuleShowResult();

  if (forced_result.has_value() &&
      forced_result.value().result_label.has_value() &&
      IsModuleLabel(forced_result.value().result_label.value())) {
    return forced_result.value();
  }

  CardSelectionInfo::ShowResult result;
  result.result_label = kAppBundlePromoEphemeralModule;

  std::optional<float> apps_installed_count =
      signals.GetSignal(kAppBundleAppsInstalledCountSignalKey);

  if (apps_installed_count.has_value() &&
      apps_installed_count.value() <=
          features::kMaxAppBundleAppsInstalled.Get()) {
    result.position = EphemeralHomeModuleRank::kTop;
    return result;
  }

  result.position = EphemeralHomeModuleRank::kNotShown;
  return result;
}

bool AppBundlePromoEphemeralModule::IsModuleLabel(std::string_view label) {
  return label == kAppBundlePromoEphemeralModule;
}

bool AppBundlePromoEphemeralModule::IsEnabled(int impression_count) {
  // If the feature is not force enabled or enabled in the base feature list,
  // return `false`.
  if (!base::FeatureList::IsEnabled(features::kAppBundlePromoEphemeralCard)) {
    return false;
  }

  std::optional<CardSelectionInfo::ShowResult> forced_result =
      GetForcedEphemeralModuleShowResult();

  // Handles forcing the module to be shown or hidden in the Magic Stack.
  if (forced_result.has_value() &&
      forced_result.value().result_label.has_value() &&
      IsModuleLabel(forced_result.value().result_label.value())) {
    return forced_result.value().position == EphemeralHomeModuleRank::kTop;
  }

  // Handles showing the module based on whether its maximum impression count
  // has been met.
  return impression_count < features::kMaxAppBundlePromoImpressions.Get();
}

}  // namespace home_modules
}  // namespace segmentation_platform
