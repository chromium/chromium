// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_HEURISTIC_CONFIGS_LAUNCHED_STARTER_HEURISTIC_CONFIG_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_HEURISTIC_CONFIGS_LAUNCHED_STARTER_HEURISTIC_CONFIG_H_

#include <string>
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/values.h"
#include "components/autofill_assistant/browser/starter_heuristic_configs/finch_starter_heuristic_config.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace autofill_assistant {
class StarterPlatformDelegate;

// Represents a |FinchStarterHeuristicConfig| that is launched in a set of
// countries. The config is enabled by default in those countries, but retains
// the original feature flag so it can be disabled later on if necessary.
// Further country rollouts should specify and ramp their own finch config - the
// original feature flag should only be used as an off switch.
class LaunchedStarterHeuristicConfig : public FinchStarterHeuristicConfig {
 public:
  // See |FinchStarterHeuristicConfig| for details on the parameter format.
  // |country_codes| should be lowercase ISO 3166-1 alpha-2, e.g., "us".
  explicit LaunchedStarterHeuristicConfig(
      const base::Feature& launched_feature,
      const std::string& parameters,
      const base::flat_set<std::string>& country_codes);
  ~LaunchedStarterHeuristicConfig() override;

  const base::Value::List& GetConditionSetsForClientState(
      StarterPlatformDelegate* platform_delegate,
      content::BrowserContext* browser_context) const override;

 private:
  base::flat_set<std::string> countries_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_HEURISTIC_CONFIGS_LAUNCHED_STARTER_HEURISTIC_CONFIG_H_
