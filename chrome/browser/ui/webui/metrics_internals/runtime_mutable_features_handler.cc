// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/metrics_internals/runtime_mutable_features_handler.h"

#include "base/feature_list.h"
#include "base/feature_list_internal.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/types/pass_key.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/metrics_internals/metrics_internals_features.h"
#include "components/variations/service/variations_service.h"

RuntimeMutableFeaturesHandler::RuntimeMutableFeaturesHandler() = default;

RuntimeMutableFeaturesHandler::~RuntimeMutableFeaturesHandler() = default;

void RuntimeMutableFeaturesHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "fetchRuntimeMutableFeatures",
      base::BindRepeating(
          &RuntimeMutableFeaturesHandler::HandleFetchRuntimeMutableFeatures,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "isSeedFetchingPaused",
      base::BindRepeating(
          &RuntimeMutableFeaturesHandler::HandleIsSeedFetchingPaused,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setSeedFetchingPaused",
      base::BindRepeating(
          &RuntimeMutableFeaturesHandler::HandleSetSeedFetchingPaused,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "uploadSeed",
      base::BindRepeating(&RuntimeMutableFeaturesHandler::HandleUploadSeed,
                          base::Unretained(this)));
}

void RuntimeMutableFeaturesHandler::HandleFetchRuntimeMutableFeatures(
    const base::ListValue& args) {
  AllowJavascript();
  CHECK_EQ(args.size(), 1U);
  const base::Value& callback_id = args[0];

  base::ListValue features_list;

  base::FeatureList* feature_list = base::FeatureList::GetInstance();
  CHECK(feature_list);

  // Get the list of all runtime-mutable features.
  const auto& features = feature_list->GetRuntimeMutableFeatureState(
      base::PassKey<RuntimeMutableFeaturesHandler>());

  for (const auto& [name, feature_state] : features) {
    const base::Feature& feature = feature_state.feature.get();
    base::FeatureList::OverrideState override_state =
        feature_list->GetOverrideStateWithoutActivation(
            feature, base::PassKey<RuntimeMutableFeaturesHandler>());
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

  ResolveJavascriptCallback(callback_id, std::move(features_list));
}

void RuntimeMutableFeaturesHandler::HandleIsSeedFetchingPaused(
    const base::ListValue& args) {
  AllowJavascript();
  CHECK_EQ(args.size(), 1U);
  const base::Value& callback_id = args[0];
  CHECK(g_browser_process);
  auto* variations_service = g_browser_process->variations_service();
  CHECK(variations_service);
  bool paused = variations_service->IsSeedFetchingPaused();
  ResolveJavascriptCallback(callback_id, base::Value(paused));
}

void RuntimeMutableFeaturesHandler::HandleSetSeedFetchingPaused(
    const base::ListValue& args) {
  AllowJavascript();
  CHECK_EQ(args.size(), 2U);
  const base::Value& callback_id = args[0];
  bool paused = args[1].GetBool();

  DVLOG(1) << "RuntimeMutableFeaturesHandler: SetSeedFetchingPaused("
           << (paused ? "true" : "false") << ")";

  CHECK(g_browser_process);
  auto* variations_service = g_browser_process->variations_service();
  CHECK(variations_service);
  variations_service->SetSeedFetchingPaused(
      base::PassKey<RuntimeMutableFeaturesHandler>(), paused);

  ResolveJavascriptCallback(callback_id, base::Value());
}

void RuntimeMutableFeaturesHandler::HandleUploadSeed(
    const base::ListValue& args) {
  AllowJavascript();
  CHECK_EQ(args.size(), 2U);
  const base::Value& callback_id = args[0];
  const base::Value& seed_value = args[1];
  CHECK(seed_value.is_blob());
  const std::vector<uint8_t>& seed_bytes = seed_value.GetBlob();

  DVLOG(1) << "RuntimeMutableFeaturesHandler: UploadSeed (size: "
           << seed_bytes.size() << " bytes)";

  // TODO: Process seed when API is available.

  ResolveJavascriptCallback(callback_id, base::Value());
}
