// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_HEURISTIC_CONFIGS_STARTER_HEURISTIC_CONFIG_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_HEURISTIC_CONFIGS_STARTER_HEURISTIC_CONFIG_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/values.h"
#include "components/autofill_assistant/browser/script_parameters.h"
#include "components/autofill_assistant/browser/starter_platform_delegate.h"

namespace autofill_assistant {

// Base class for starter heuristic configs. Instances of this class define
// heurists that are used by the starter to determine when to start. Configs
// are usually either hard-coded into Chrome, or supplied via finch parameters.
class StarterHeuristicConfig {
 public:
  StarterHeuristicConfig() = default;
  virtual ~StarterHeuristicConfig() = default;
  StarterHeuristicConfig(const StarterHeuristicConfig&) = delete;
  StarterHeuristicConfig& operator=(const StarterHeuristicConfig&) = delete;

  // Returns the intent script parameter for this starter heuristic.
  virtual const std::string& GetIntent() const = 0;

  // Returns a list containing the condition sets to use for the
  // current client state (can be empty). Each conditionSet is a
  // URLMatcherConditionSet dictionary as defined by the URLMatcherFactory
  // (components/url_matcher/url_matcher_factory.h).
  virtual const base::Value::List& GetConditionSetsForClientState(
      StarterPlatformDelegate* platform_delegate) const = 0;

  // Returns the list of denylisted domains for this config.
  virtual const base::flat_set<std::string>& GetDenylistedDomains() const = 0;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_HEURISTIC_CONFIGS_STARTER_HEURISTIC_CONFIG_H_
