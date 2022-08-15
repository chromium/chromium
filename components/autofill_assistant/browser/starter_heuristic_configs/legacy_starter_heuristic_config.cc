// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/starter_heuristic_configs/legacy_starter_heuristic_config.h"
#include "base/json/json_reader.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/starter_heuristic_configs/finch_starter_heuristic_config.h"

namespace autofill_assistant {

const char kJsonParameterDictKey[] = "json_parameters";
constexpr base::FeatureParam<std::string> kLegacyFieldTrialParams{
    &features::kAutofillAssistantUrlHeuristics, kJsonParameterDictKey, ""};

LegacyStarterHeuristicConfig::LegacyStarterHeuristicConfig() {
  InitFromTrialParams();
}

LegacyStarterHeuristicConfig::~LegacyStarterHeuristicConfig() = default;

const std::string& LegacyStarterHeuristicConfig::GetIntent() const {
  return intent_;
}

const base::Value::List&
LegacyStarterHeuristicConfig::GetConditionSetsForClientState(
    StarterPlatformDelegate* platform_delegate,
    content::BrowserContext* browser_context) const {
  static const base::NoDestructor<base::Value> empty_list(
      base::Value::Type::LIST);
  if (platform_delegate->GetIsSupervisedUser() ||
      !platform_delegate->GetIsAllowedForMachineLearning()) {
    return empty_list->GetList();
  }

  if (!platform_delegate->GetProactiveHelpSettingEnabled()) {
    return empty_list->GetList();
  }

  if (!platform_delegate->GetCommonDependencies()
           ->GetMakeSearchesAndBrowsingBetterEnabled(browser_context)) {
    return empty_list->GetList();
  }

  if (platform_delegate->GetIsCustomTab() &&
      !platform_delegate->GetIsTabCreatedByGSA()) {
    return empty_list->GetList();
  }

  // The legacy config used a separate finch feature to gate CCT vs. non-CCT
  // support. In new configs, these can be specified directly in the params.
  if (platform_delegate->GetIsCustomTab() &&
      !base::FeatureList::IsEnabled(
          features::kAutofillAssistantInCCTTriggering)) {
    return empty_list->GetList();
  }

  if (!platform_delegate->GetIsCustomTab() &&
      !base::FeatureList::IsEnabled(
          features::kAutofillAssistantInTabTriggering)) {
    return empty_list->GetList();
  }

  // The legacy config used to only be available for signed-in users in
  // weblayer.
  if (platform_delegate->GetIsWebLayer() &&
      !platform_delegate->GetIsLoggedIn()) {
    return empty_list->GetList();
  }

  return condition_sets_.GetList();
}

const base::flat_set<std::string>&
LegacyStarterHeuristicConfig::GetDenylistedDomains() const {
  return denylisted_domains_;
}

absl::optional<base::flat_set<std::string>>
LegacyStarterHeuristicConfig::ReadDenylistedDomains(
    const base::Value::Dict& dict) const {
  const base::Value::List* denylisted_domains_value =
      dict.FindList(kDenylistedDomainsKey);
  if (!denylisted_domains_value) {
    return base::flat_set<std::string>{};
  }

  base::flat_set<std::string> denylisted_domains;
  for (const auto& domain : *denylisted_domains_value) {
    if (!domain.is_string()) {
      VLOG(1) << "Invalid type for denylisted domain";
      return absl::nullopt;
    }
    denylisted_domains.insert(*domain.GetIfString());
  }
  return denylisted_domains;
}

absl::optional<std::pair<base::Value, std::string>>
LegacyStarterHeuristicConfig::ReadConditionSetsAndIntent(
    const base::Value::Dict& dict) const {
  const base::Value::List* condition_sets_list = dict.FindList(kHeuristicsKey);
  if (!condition_sets_list) {
    VLOG(1) << "Field trial params did not contain condition sets";
    return absl::nullopt;
  }

  // In this legacy config, the INTENT script parameter was specified as part of
  // each individual heuristic entry (and not one overall). Thus, it was
  // technically possible to supply different INTENTS per heuristic. This was
  // never actually used. For legacy treatment, we simply take the first
  // specified INTENT here.
  std::string intent;
  if (!condition_sets_list->empty()) {
    auto* intent_param = condition_sets_list->front().FindKeyOfType(
        kIntentKey, base::Value::Type::STRING);
    if (!intent_param) {
      VLOG(1) << "Heuristic did not contain the intent parameter";
      return absl::nullopt;
    }
    intent.assign(*intent_param->GetIfString());
  }

  return std::make_pair(base::Value(condition_sets_list->Clone()), intent);
}

void LegacyStarterHeuristicConfig::InitFromTrialParams() {
  std::string parameters = kLegacyFieldTrialParams.Get();
  if (parameters.empty()) {
    VLOG(2) << "Field trial parameter not set";
    return;
  }
  auto dict = base::JSONReader::ReadAndReturnValueWithError(parameters);
  if (!dict.has_value() || !dict->is_dict()) {
    VLOG(1) << "Failed to parse field trial params as JSON object: "
            << parameters;
    if (VLOG_IS_ON(1)) {
      if (dict.has_value()) {
        VLOG(1) << "Expecting a dictionary";
      } else {
        VLOG(1) << dict.error().message << ", line: " << dict.error().line
                << ", col: " << dict.error().column;
      }
    }
    return;
  }

  // Read optional list of denylisted domains.
  absl::optional<base::flat_set<std::string>> denylisted_domains =
      ReadDenylistedDomains(dict->GetDict());
  if (!denylisted_domains) {
    return;
  }

  // Read condition sets and intent.
  absl::optional<std::pair<base::Value, std::string>>
      condition_sets_and_intent = ReadConditionSetsAndIntent(dict->GetDict());
  if (!condition_sets_and_intent) {
    return;
  }

  denylisted_domains_ = std::move(*denylisted_domains);
  condition_sets_ = base::Value(std::move(condition_sets_and_intent->first));
  intent_.assign(condition_sets_and_intent->second);
}

}  // namespace autofill_assistant
