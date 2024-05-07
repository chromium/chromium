// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/rules_service.h"

#include "components/enterprise/data_controls/features.h"
#include "components/enterprise/data_controls/prefs.h"

namespace data_controls {

RulesService::RulesService(PrefService* pref_service) {
  if (base::FeatureList::IsEnabled(kEnableDesktopDataControls)) {
    pref_registrar_.Init(pref_service);
    pref_registrar_.Add(
        kDataControlsRulesPref,
        base::BindRepeating(&RulesService::OnDataControlsRulesUpdate,
                            base::Unretained(this)));
    OnDataControlsRulesUpdate();
  }
}

RulesService::~RulesService() = default;

Verdict RulesService::GetVerdict(Rule::Restriction restriction,
                                 const ActionContext& context) const {
  if (!base::FeatureList::IsEnabled(kEnableDesktopDataControls)) {
    return Verdict::NotSet();
  }

  Rule::Level max_level = Rule::Level::kNotSet;
  Verdict::TriggeredRules triggered_rules;
  for (const auto& rule : rules_) {
    Rule::Level level = rule.GetLevel(restriction, context);
    if (level > max_level) {
      max_level = level;
    }
    if (level != Rule::Level::kNotSet && !rule.rule_id().empty()) {
      triggered_rules[rule.rule_id()] = rule.name();
    }
  }

  switch (max_level) {
    case Rule::Level::kNotSet:
      return Verdict::NotSet();
    case Rule::Level::kReport:
      return Verdict::Report(std::move(triggered_rules));
    case Rule::Level::kWarn:
      return Verdict::Warn(std::move(triggered_rules));
    case Rule::Level::kBlock:
      return Verdict::Block(std::move(triggered_rules));
    case Rule::Level::kAllow:
      return Verdict::Allow();
  }
}

void RulesService::OnDataControlsRulesUpdate() {
  DCHECK(pref_registrar_.prefs());
  if (!base::FeatureList::IsEnabled(kEnableDesktopDataControls)) {
    return;
  }

  rules_.clear();

  const base::Value::List& rules_list =
      pref_registrar_.prefs()->GetList(kDataControlsRulesPref);

  for (const base::Value& rule_value : rules_list) {
    auto rule = Rule::Create(rule_value);

    if (!rule) {
      continue;
    }

    rules_.push_back(std::move(*rule));
  }
}

}  // namespace data_controls
