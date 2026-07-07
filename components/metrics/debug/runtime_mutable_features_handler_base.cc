// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/debug/runtime_mutable_features_handler_base.h"

#include <string_view>
#include <vector>

#include "base/feature_list.h"
#include "base/feature_list_internal.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/strings/strcat.h"
#include "components/variations/service/variations_service.h"

namespace metrics {

RuntimeMutableFeaturesHandlerBase::RuntimeMutableFeaturesHandlerBase(
    Delegate* delegate,
    variations::VariationsService* variations_service)
    : delegate_(delegate), variations_service_(variations_service) {}

RuntimeMutableFeaturesHandlerBase::~RuntimeMutableFeaturesHandlerBase() =
    default;

void RuntimeMutableFeaturesHandlerBase::HandleFetchRuntimeMutableFeatures(
    const base::Value& callback_id) {
  base::ListValue features_list;

  base::FeatureList* feature_list = base::FeatureList::GetInstance();
  CHECK(feature_list);

  // Get the list of all runtime-mutable features.
  const auto& features = feature_list->GetRuntimeMutableFeatureState(
      base::PassKey<RuntimeMutableFeaturesHandlerBase>());

  for (const auto& [name, feature_state] : features) {
    const base::Feature& feature = feature_state.feature.get();
    base::FeatureList::OverrideState override_state =
        feature_list->GetOverrideStateWithoutActivation(
            feature, base::PassKey<RuntimeMutableFeaturesHandlerBase>());
    const bool enabled =
        (override_state == base::FeatureList::OVERRIDE_ENABLE_FEATURE) ||
        (override_state == base::FeatureList::OVERRIDE_USE_DEFAULT &&
         feature.default_state == base::FEATURE_ENABLED_BY_DEFAULT);

    // Determine which field trial, if any, is controlling the feature's
    // enabled/disabled state, and retrieve its group. Note, that we can
    // use string_view here because we're referencing strings within the
    // FeatureList::RuntimeMutableFeatureState object or the FieldTrial
    // object, both of which are const from the point of view of this
    // handler.
    std::string_view trial_name;
    std::string_view group_name;
    bool runtime_override = false;
    if (!feature_state.field_trial_name.empty()) {
      // The feature is being controlled by a runtime-mutable field trial.
      trial_name = feature_state.field_trial_name;
      group_name = feature_state.group_name;
      runtime_override = true;
    } else {
      // The feature is not being controlled by a runtime-mutable field trial.
      // Check if it's being controlled by a regular field trial.
      base::FieldTrial* regular_trial =
          base::FeatureList::GetFieldTrial(feature);
      if (regular_trial) {
        trial_name = regular_trial->trial_name();
        group_name = regular_trial->GetGroupNameWithoutActivation();
      }
    }
    // Prepend the trial name, if any, with "[command-line] " if the
    // command line is responsible for the feature's override state.
    std::string_view trial_prefix =
        feature_list->IsFeatureOverriddenFromCommandLine(feature.name)
            ? "[command-line] "
            : "";
    std::string_view final_trial_name = trial_name.empty() ? "-" : trial_name;
    std::string_view final_group_name = group_name.empty() ? "-" : group_name;

    // Append the feature attributes to the features_list.
    features_list.Append(
        base::DictValue()
            .Set("name", feature.name)
            .Set("enabled", enabled)
            .Set("fieldTrial", base::StrCat({trial_prefix, final_trial_name}))
            .Set("fieldTrialGroup", final_group_name)
            .Set("runtimeOverride", runtime_override));
  }

  delegate_->ResolvePageCallback(callback_id, features_list);
}

void RuntimeMutableFeaturesHandlerBase::HandleIsSeedFetchingPaused(
    const base::Value& callback_id) {
  CHECK(variations_service_);
  bool paused = variations_service_->IsSeedFetchingPaused();
  delegate_->ResolvePageCallback(callback_id, base::Value(paused));
}

void RuntimeMutableFeaturesHandlerBase::HandleSetSeedFetchingPaused(
    const base::Value& callback_id,
    bool paused) {
  DVLOG(1) << "RuntimeMutableFeaturesHandler: SetSeedFetchingPaused("
           << (paused ? "true" : "false") << ")";

  CHECK(variations_service_);
  variations_service_->SetSeedFetchingPaused(
      base::PassKey<RuntimeMutableFeaturesHandlerBase>(), paused);

  delegate_->ResolvePageCallback(callback_id, base::Value());
}

void RuntimeMutableFeaturesHandlerBase::HandleUploadSeed(
    const base::Value& callback_id,
    const base::Value& seed_value) {
  CHECK(seed_value.is_blob());
  const std::vector<uint8_t>& seed_bytes = seed_value.GetBlob();

  DVLOG(1) << "RuntimeMutableFeaturesHandler: UploadSeed (size: "
           << seed_bytes.size() << " bytes)";

  // TODO: Process seed when API is available.

  delegate_->ResolvePageCallback(callback_id, base::Value());
}

}  // namespace metrics
