// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/startup_util.h"

#include <functional>
#include <string>

#include "base/feature_list.h"
#include "base/logging.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/script_parameters.h"

namespace autofill_assistant {

using autofill_assistant::features::kAutofillAssistant;
using autofill_assistant::features::kAutofillAssistantChromeEntry;
using autofill_assistant::features::kAutofillAssistantDirectActions;
using autofill_assistant::features::
    kAutofillAssistantGetTriggerScriptsByHashPrefix;
using autofill_assistant::features::kAutofillAssistantLoadDFMForTriggerScripts;
using autofill_assistant::features::kAutofillAssistantProactiveHelp;

StartupUtil::StartupUtil() = default;
StartupUtil::~StartupUtil() = default;

StartupMode StartupUtil::ChooseStartupModeForIntent(
    const TriggerContext& trigger_context,
    const Options& options) const {
  const auto& script_parameters = trigger_context.GetScriptParameters();
  std::vector<std::reference_wrapper<const base::Feature>> features_to_check = {
      std::ref(kAutofillAssistant)};
  if (!trigger_context.GetCCT()) {
    features_to_check.emplace_back(kAutofillAssistantChromeEntry);
  }
  if (script_parameters.HasStartImmediately() &&
      !script_parameters.GetStartImmediately()) {
    features_to_check.emplace_back(kAutofillAssistantProactiveHelp);
    if (!options.feature_module_installed) {
      features_to_check.emplace_back(
          kAutofillAssistantLoadDFMForTriggerScripts);
    }
  }

  for (const auto& feature : features_to_check) {
    if (!base::FeatureList::IsEnabled(feature)) {
      VLOG(1) << "Invalid Autofill Assistant intent: feature "
              << feature.get().name << " required but disabled.";
      return StartupMode::FEATURE_DISABLED;
    }
  }

  if (!script_parameters.HasStartImmediately() ||
      !script_parameters.GetEnabled()) {
    VLOG(1)
        << "Invalid Autofill Assistant intent: a mandatory startup parameter "
           "(START_IMMEDIATELY or ENABLED) was missing or invalid.";
    return StartupMode::MANDATORY_PARAMETERS_MISSING;
  }

  if (!ChooseStartupUrlForIntent(trigger_context).has_value()) {
    VLOG(1)
        << "Invalid Autofill Assistant intent: could not determine the URL "
           "to start on. ORIGINAL_DEEPLINK and initial url not set or invalid.";
    return StartupMode::NO_INITIAL_URL;
  }

  if (script_parameters.GetStartImmediately()) {
    // For regular scripts we can stop checking here.
    return StartupMode::START_REGULAR;
  }

  if (!(script_parameters.GetRequestsTriggerScript())) {
    VLOG(1) << "Invalid Autofill Assistant intent: START_IMMEDIATELY=false "
               "requires REQUEST_TRIGGER_SCRIPT=true.";
    return StartupMode::MANDATORY_PARAMETERS_MISSING;
  }

  if (!options.proactive_help_setting_enabled) {
    VLOG(1) << "Invalid Autofill Assistant intent: The 'Proactive help' Chrome "
               "setting was turned off.";
    return StartupMode::SETTING_DISABLED;
  }

  DCHECK(script_parameters.GetRequestsTriggerScript());
  if (!options.msbb_setting_enabled &&
      !base::FeatureList::IsEnabled(
          kAutofillAssistantGetTriggerScriptsByHashPrefix)) {
    VLOG(1) << "Invalid Autofill Assistant intent: REQUEST_TRIGGER_SCRIPT "
               "requires MSBB, but was turned off.";
    return StartupMode::SETTING_DISABLED;
  }

  return StartupMode::START_RPC_TRIGGER_SCRIPT;
}

absl::optional<GURL> StartupUtil::ChooseStartupUrlForIntent(
    const TriggerContext& trigger_context) const {
  GURL url =
      GURL(trigger_context.GetScriptParameters().GetOriginalDeeplink().value_or(
          std::string()));
  if (url.is_valid()) {
    return url;
  }

  url = GURL(trigger_context.GetInitialUrl());
  if (url.is_valid()) {
    return url;
  }
  return absl::nullopt;
}

}  // namespace autofill_assistant
