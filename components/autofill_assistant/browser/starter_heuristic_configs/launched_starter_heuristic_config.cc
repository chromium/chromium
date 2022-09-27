// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/starter_heuristic_configs/launched_starter_heuristic_config.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "components/autofill_assistant/browser/starter_platform_delegate.h"

namespace autofill_assistant {

LaunchedStarterHeuristicConfig::LaunchedStarterHeuristicConfig(
    const base::Feature& launched_feature,
    const std::string& parameters,
    const base::flat_set<std::string>& countries)
    : countries_(countries) {
#ifndef NDEBUG
  for (const auto& country : countries_) {
    CHECK(country == base::ToLowerASCII(country))
        << "countries must be specified in lowercase ISO 3166-1 alpha-2, e.g., "
           "'us'";
  }
#endif

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

  // Prefer the permanent country, but fallback to latest country if not
  // available. This is mostly to allow integration tests to pass, since
  // injecting the country via --variations-override-country seems to only
  // affect the latest country.
  std::string country =
      base::ToLowerASCII(platform_delegate->GetCommonDependencies()
                             ->GetStoredPermanentCountryCode());
  if (country == "zz" || country.empty()) {
    country = base::ToLowerASCII(
        platform_delegate->GetCommonDependencies()->GetLatestCountryCode());
  }
  if (!countries_.contains(country)) {
    return empty_list->GetList();
  }

  return FinchStarterHeuristicConfig::GetConditionSetsForClientState(
      platform_delegate, browser_context);
}

}  // namespace autofill_assistant
