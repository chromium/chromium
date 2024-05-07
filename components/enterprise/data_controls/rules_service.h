// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_CONTROLS_RULES_SERVICE_H_
#define COMPONENTS_ENTERPRISE_DATA_CONTROLS_RULES_SERVICE_H_

#include "components/enterprise/data_controls/rule.h"
#include "components/enterprise/data_controls/verdict.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace data_controls {

// Abstract keyed service that provides an interface to check what restrictions
// should be applied from the DataControlsRules policy.
class RulesService : public KeyedService {
 public:
  ~RulesService() override;

 protected:
  explicit RulesService(PrefService* pref_service);

  // Returns a `Verdict` corresponding to all triggered Data Control rules given
  // the provided context.
  Verdict GetVerdict(Rule::Restriction restriction,
                     const ActionContext& context) const;

 private:
  // Parse the "DataControlsRules" policy if the corresponding experiment is
  // enabled, and populate `rules_`.
  void OnDataControlsRulesUpdate();

  // Watches changes to the "DataControlsRules" policy. Does nothing if the
  // "EnableDesktopDataControls" experiment is disabled.
  PrefChangeRegistrar pref_registrar_;

  // List of rules created from the "DataControlsRules" policy. Empty if the
  // "EnableDesktopDataControls" experiment is disabled.
  std::vector<Rule> rules_;
};

}  // namespace data_controls

#endif  // COMPONENTS_ENTERPRISE_DATA_CONTROLS_RULES_SERVICE_H_
