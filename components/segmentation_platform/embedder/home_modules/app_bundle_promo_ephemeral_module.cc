// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/app_bundle_promo_ephemeral_module.h"

#include "base/feature_list.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/ephemeral_module_utils.h"
#include "components/segmentation_platform/internal/metadata/feature_query.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {
namespace home_modules {

namespace {

// Impression counter for the App Bundle promo ephemeral module.
const char kAppBundlePromoEphemeralModuleImpressionCounterPref[] =
    "ephemeral_pref_counter.app_bundle_promo_ephemeral_module_counter";

}  // namespace

const char kAppBundleAppsInstalledCountSignalKey[] =
    "app_bundle_apps_installed_count";

AppBundlePromoEphemeralModule::AppBundlePromoEphemeralModule()
    : CardSelectionInfo(kAppBundlePromoEphemeralModule) {}

// static
void AppBundlePromoEphemeralModule::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      kAppBundlePromoEphemeralModuleImpressionCounterPref, 0);
}

// static
bool AppBundlePromoEphemeralModule::IsModuleLabel(std::string_view label) {
  return label == kAppBundlePromoEphemeralModule;
}

// static
bool AppBundlePromoEphemeralModule::IsEnabled(PrefService* local_state) {
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
  int impression_count = local_state->GetInteger(
      kAppBundlePromoEphemeralModuleImpressionCounterPref);

  return impression_count < features::kMaxAppBundlePromoImpressions.Get();
}

void AppBundlePromoEphemeralModule::OnShow(PrefService* profile_prefs,
                                           PrefService* local_state) {
  int freshness_impression_count = local_state->GetInteger(
      kAppBundlePromoEphemeralModuleImpressionCounterPref);

  local_state->SetInteger(kAppBundlePromoEphemeralModuleImpressionCounterPref,
                          freshness_impression_count + 1);
}

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

}  // namespace home_modules
}  // namespace segmentation_platform
