// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/starter_heuristic_configs/launched_starter_heuristic_config.h"
#include "base/no_destructor.h"
#include "components/autofill_assistant/browser/starter_platform_delegate.h"

namespace autofill_assistant {

LaunchedStarterHeuristicConfig::LaunchedStarterHeuristicConfig(
    const base::Feature& launched_feature,
    const std::string& parameters,
    const base::flat_set<std::string>& countries)
    : countries_(countries) {
  if (!base::FeatureList::IsEnabled(launched_feature)) {
    return;
  }
  InitFromString(parameters);
}

LaunchedStarterHeuristicConfig::~LaunchedStarterHeuristicConfig() = default;

const base::Value::List&
LaunchedStarterHeuristicConfig::GetConditionSetsForClientState(
    StarterPlatformDelegate* platform_delegate,
    content::BrowserContext* browser_context) const {
  static const base::NoDestructor<base::Value> empty_list(
      base::Value::Type::LIST);
  if (!countries_.contains(
          platform_delegate->GetCommonDependencies()->GetCountryCode())) {
    return empty_list->GetList();
  }

  return FinchStarterHeuristicConfig::GetConditionSetsForClientState(
      platform_delegate, browser_context);
}

}  // namespace autofill_assistant
