// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_HEURISTIC_CONFIGS_FINCH_STARTER_HEURISTIC_CONFIG_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_HEURISTIC_CONFIGS_FINCH_STARTER_HEURISTIC_CONFIG_H_

#include "base/metrics/field_trial_params.h"
#include "components/autofill_assistant/browser/starter_heuristic_configs/starter_heuristic_config.h"

namespace autofill_assistant {

// Dictionary key for the list of denylisted domains.
const char kDenylistedDomainsKey[] = "denylistedDomains";
// Dictionary key for the list of heuristics.
const char kHeuristicsKey[] = "heuristics";
// Dictionary key for the intent.
const char kIntentKey[] = "intent";
// Dictionary keys for filters that can't be directly enforced via finch. If not
// specified, these default to false, so at least some of them must be set. In
// addition to the conditions here, supervised accounts are never supported, and
// the proactive setting must be enabled as well.
// Note that only custom tabs created by GSA are supported.
const char kEnabledInCustomTabsKey[] = "enabledInCustomTabs";
const char kEnabledInRegularTabsKey[] = "enabledInRegularTabs";
const char kEnabledInWeblayerKey[] = "enabledInWeblayer";
// Note: signed-in users default to true and need not be configured.
const char kEnabledForSignedOutUsers[] = "enabledForSignedOutUsers";
// Whether 'make searches and browsing better' is required or not. By default,
// MSBB must be enabled.
const char kEnabledWithoutMsbb[] = "enabledWithoutMsbb";

// A heuristic config that is originating from a finch feature parameter.
// The trial parameter must be a JSON object of the following format:
/*
  {
    "intent": "SOME_INTENT",
    "denylistedDomains": ["example.com", ...],
    "heuristics": [
      {"conditionSet": {...}},
      {"conditionSet": {...}},
      ...
    ],
    "enabledInCustomTabs":true,
    "enabledInRegularTabs":false,
    "enabledInWeblayer":false,
    "enabledForSignedOutUsers":true
    "enabledWithoutMsbb":false
  }
*/
// The 'intent' parameter is mandatory. All other parameters are optional, but
// at least one conditionSet and one enabled* flag must be set for the config
// to be meaningful.
class FinchStarterHeuristicConfig : public StarterHeuristicConfig {
 public:
  explicit FinchStarterHeuristicConfig(
      const base::FeatureParam<std::string>& trial_parameter);
  ~FinchStarterHeuristicConfig() override;

  // Overrides HeuristicConfig:
  const std::string& GetIntent() const override;
  const base::Value::List& GetConditionSetsForClientState(
      StarterPlatformDelegate* platform_delegate,
      content::BrowserContext* browser_context) const override;
  const base::flat_set<std::string>& GetDenylistedDomains() const override;

 protected:
  // Default constructor only accessible by subclasses.
  FinchStarterHeuristicConfig();
  void InitFromString(const std::string& parameters);

 private:
  void InitFromTrialParams(
      const base::FeatureParam<std::string>& trial_parameter);

  // Returns the list of denylisted domains in |dict|. Returns the empty list
  // if the relevant key does not exist in |dict|. Returns absl::nullopt if the
  // format of the encountered denylist was invalid.
  absl::optional<base::flat_set<std::string>> ReadDenylistedDomains(
      const base::Value::Dict& dict) const;

  bool enabled_in_custom_tabs_ = false;
  bool enabled_in_regular_tabs_ = false;
  bool enabled_in_weblayer_ = false;
  bool enabled_for_signed_out_users_ = false;
  bool enabled_without_msbb_ = false;
  std::string intent_;
  base::Value condition_sets_ = base::Value(base::Value::Type::LIST);
  base::flat_set<std::string> denylisted_domains_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_HEURISTIC_CONFIGS_FINCH_STARTER_HEURISTIC_CONFIG_H_
