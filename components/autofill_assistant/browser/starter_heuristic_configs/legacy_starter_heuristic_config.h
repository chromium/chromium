// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_HEURISTIC_CONFIGS_LEGACY_STARTER_HEURISTIC_CONFIG_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_HEURISTIC_CONFIGS_LEGACY_STARTER_HEURISTIC_CONFIG_H_

#include <string>

#include "base/containers/flat_map.h"
#include "components/autofill_assistant/browser/script_parameters.h"
#include "components/autofill_assistant/browser/starter_heuristic_configs/starter_heuristic_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill_assistant {

// The legacy config. Some smaller changes have been made to the format of field
// trial parameters since then, so this class provides a legacy layer for the
// old trial until we can phase it out.
class LegacyStarterHeuristicConfig : public StarterHeuristicConfig {
 public:
  LegacyStarterHeuristicConfig();
  ~LegacyStarterHeuristicConfig() override;

  // Overrides HeuristicConfig:
  const std::string& GetIntent() const override;
  const base::Value::List& GetConditionSetsForClientState(
      StarterPlatformDelegate* platform_delegate,
      content::BrowserContext* browser_context) const override;
  const base::flat_set<std::string>& GetDenylistedDomains() const override;

 private:
  void InitFromTrialParams();

  // Returns the list of denylisted domains in |dict|. Returns the empty list
  // if the relevant key does not exist in |dict|. Returns absl::nullopt if the
  // format of the encountered denylist was invalid.
  absl::optional<base::flat_set<std::string>> ReadDenylistedDomains(
      const base::Value::Dict& dict) const;

  // Reads the condition sets and intent in |dict|. Returns absl::nullopt if
  // either of these parameters is invalid.
  absl::optional<std::pair<base::Value, std::string>>
  ReadConditionSetsAndIntent(const base::Value::Dict& dict) const;

  std::string intent_;
  base::Value condition_sets_ = base::Value(base::Value::Type::LIST);
  base::flat_set<std::string> denylisted_domains_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_HEURISTIC_CONFIGS_LEGACY_STARTER_HEURISTIC_CONFIG_H_
