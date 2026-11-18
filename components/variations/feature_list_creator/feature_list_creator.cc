// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/feature_list_creator/feature_list_creator.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/trace_event/trace_event.h"
#include "components/crash/core/common/crash_keys.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/variations/feature_list_creator/feature_list_creator_client.h"
#include "components/variations/platform_field_trials.h"
#include "components/variations/service/variations_service.h"
#include "components/webui/flags/flags_state.h"
#include "components/webui/flags/flags_storage.h"
#include "components/webui/flags/flags_ui_switches.h"

namespace variations {

void CreateFeatureList(FeatureListCreatorClient& client) {
  CHECK(!base::FeatureList::GetInstance());

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const bool no_experiments =
      command_line->HasSwitch(::switches::kNoExperiments);
  std::unique_ptr<flags_ui::FlagsStorage> flags_storage;
  flags_ui::FlagsState* flags_state = nullptr;
  if (!no_experiments) {
    TRACE_EVENT0("startup", "variations::CreateFeatureList::ConvertFlags");
    flags_storage = client.CreateFlagsStorage();
    CHECK(flags_storage);
    flags_state = &client.GetFlagsState();
    flags_state->ConvertFlagsToSwitches(
        flags_storage.get(), command_line, flags_ui::kAddSentinels,
        ::switches::kEnableFeatures, ::switches::kDisableFeatures);
  }

  // Update command-line crash keys after `about:flags` switch conversion.
  crash_keys::SetFeaturesFromCommandLine(*command_line);
  crash_keys::SetSwitchesFromCommandLine(*command_line,
                                         client.GetSwitchFilter());

  metrics_services_manager::MetricsServicesManager& metrics_services_manager =
      client.GetOrCreateMetricsServicesManager();

  // Initialize `base::FieldTrialList` to support `base::FieldTrial` instances
  // with one-time randomization.
  metrics_services_manager.InstantiateFieldTrialList();

  auto feature_list = std::make_unique<base::FeatureList>();
  std::vector<std::string> variation_ids;
  if (!no_experiments) {
    variation_ids = flags_state->RegisterAllFeatureVariationParameters(
        flags_storage.get(), feature_list.get());
  }
  client.WillSetUpFieldTrials(*feature_list);

  VariationsService& variations_service =
      CHECK_DEREF(metrics_services_manager.GetVariationsService());
  std::vector<base::FeatureList::FeatureOverrideInfo> extra_overrides =
      client.GetExtraFeatureOverrides(*command_line);
  PlatformFieldTrials& platform_field_trials = client.GetPlatformFieldTrials();
  variations_service.SetUpFieldTrials(variation_ids, extra_overrides,
                                      std::move(feature_list),
                                      &platform_field_trials);
}

}  // namespace variations
