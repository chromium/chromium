// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_RULES_SERVICE_BASE_H_
#define COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_RULES_SERVICE_BASE_H_

#include "components/enterprise/data_controls/core/browser/rule.h"
#include "components/enterprise/data_controls/core/browser/verdict.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace data_controls {

// Platform-agnostic keyed service that provides an interface to check what
// restrictions should be applied from the DataControlsRules policy, as well as
// internal logic to track updates made to that policy.
class RulesServiceBase : public KeyedService {
 public:
  explicit RulesServiceBase(PrefService* pref_service);
  ~RulesServiceBase() override;

  // Returns a `Verdict` corresponding to all triggered Data Control rules given
  // the provided context.
  Verdict GetVerdict(Rule::Restriction restriction,
                     const ActionContext& context) const;

 private:
  // Parse the "DataControlsRules" policy if the corresponding experiment is
  // enabled, and populate `rules_`.
  void OnDataControlsRulesUpdate();

  // Watches changes to the "DataControlsRules" policy.
  PrefChangeRegistrar pref_registrar_;

  // List of rules created from the "DataControlsRules" policy.
  std::vector<Rule> rules_;
};

}  // namespace data_controls

#endif  // COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_RULES_SERVICE_BASE_H_
