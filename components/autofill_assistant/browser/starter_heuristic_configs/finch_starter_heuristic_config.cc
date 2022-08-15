// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/starter_heuristic_configs/finch_starter_heuristic_config.h"
#include "base/json/json_reader.h"
#include "base/no_destructor.h"

namespace autofill_assistant {

FinchStarterHeuristicConfig::FinchStarterHeuristicConfig(
    const base::FeatureParam<std::string>& trial_parameter) {
  InitFromTrialParams(trial_parameter);
}

FinchStarterHeuristicConfig::~FinchStarterHeuristicConfig() = default;

const std::string& FinchStarterHeuristicConfig::GetIntent() const {
  return intent_;
}

const base::Value::List&
FinchStarterHeuristicConfig::GetConditionSetsForClientState(
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

  if (platform_delegate->GetIsCustomTab() &&
      (!platform_delegate->GetIsTabCreatedByGSA() ||
       !enabled_in_custom_tabs_)) {
    return empty_list->GetList();
  }

  if (!platform_delegate->GetIsCustomTab() &&
      !platform_delegate->GetIsWebLayer() && !enabled_in_regular_tabs_) {
    return empty_list->GetList();
  }

  if (platform_delegate->GetIsWebLayer() && !enabled_in_weblayer_) {
    return empty_list->GetList();
  }

  if (!platform_delegate->GetIsLoggedIn() && !enabled_for_signed_out_users_) {
    return empty_list->GetList();
  }

  if (!platform_delegate->GetCommonDependencies()
           ->GetMakeSearchesAndBrowsingBetterEnabled(browser_context) &&
      !enabled_without_msbb_) {
    return empty_list->GetList();
  }

  return condition_sets_.GetList();
}

const base::flat_set<std::string>&
FinchStarterHeuristicConfig::GetDenylistedDomains() const {
  return denylisted_domains_;
}

absl::optional<base::flat_set<std::string>>
FinchStarterHeuristicConfig::ReadDenylistedDomains(
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

void FinchStarterHeuristicConfig::InitFromTrialParams(
    const base::FeatureParam<std::string>& trial_parameter) {
  std::string parameters = trial_parameter.Get();
  if (parameters.empty()) {
    VLOG(2) << "Field trial parameter not set";
    return;
  }
  auto dict = base::JSONReader::ReadAndReturnValueWithError(parameters);
  if (!dict.has_value() || !dict->is_dict()) {
    VLOG(1) << "Failed to parse field trial params as JSON object: "
            << parameters;
    if (VLOG_IS_ON(1)) {
      if (dict.has_value())
        VLOG(1) << "Expecting a dictionary";
      else
        VLOG(1) << dict.error().message << ", line: " << dict.error().line
                << ", col: " << dict.error().column;
    }
    return;
  }

  // Read the mandatory intent.
  auto* intent = dict->GetDict().FindString(kIntentKey);
  if (!intent) {
    VLOG(1) << "Dictionary did not contain the intent parameter";
    return;
  }

  // Read optional list of denylisted domains.
  absl::optional<base::flat_set<std::string>> denylisted_domains =
      ReadDenylistedDomains(dict->GetDict());
  if (!denylisted_domains) {
    return;
  }

  // Read condition sets.
  base::Value::List* heuristics = dict->GetDict().FindList(kHeuristicsKey);
  if (heuristics == nullptr) {
    VLOG(1) << "Field trial params did not contain heuristics";
    return;
  }

  // Read optional additional filters.
  enabled_in_custom_tabs_ =
      dict->GetDict().FindBool(kEnabledInCustomTabsKey).value_or(false);
  enabled_in_regular_tabs_ =
      dict->GetDict().FindBool(kEnabledInRegularTabsKey).value_or(false);
  enabled_in_weblayer_ =
      dict->GetDict().FindBool(kEnabledInWeblayerKey).value_or(false);
  enabled_for_signed_out_users_ =
      dict->GetDict().FindBool(kEnabledForSignedOutUsers).value_or(false);
  enabled_without_msbb_ =
      dict->GetDict().FindBool(kEnabledWithoutMsbb).value_or(false);

  denylisted_domains_ = std::move(*denylisted_domains);
  condition_sets_ = base::Value(std::move(*heuristics));
  intent_.assign(*intent);
}

}  // namespace autofill_assistant
