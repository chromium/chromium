// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/core/browser/rules_service_base.h"

#include "components/enterprise/data_controls/core/browser/prefs.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/pref_service.h"

namespace data_controls {

RulesServiceBase::RulesServiceBase(PrefService* pref_service) {
  pref_registrar_.Init(pref_service);
  pref_registrar_.Add(
      kDataControlsRulesPref,
      base::BindRepeating(&RulesServiceBase::OnDataControlsRulesUpdate,
                          base::Unretained(this)));
  OnDataControlsRulesUpdate();
}

RulesServiceBase::~RulesServiceBase() = default;

Verdict RulesServiceBase::GetCopyRestrictedBySourceVerdict(
    const GURL& source) const {
  return GetVerdict(Rule::Restriction::kClipboard,
                    {
                        .source =
                            {
                                .url = source,
                                .incognito = incognito_profile(),
                            },
                    });
}

Verdict RulesServiceBase::GetCopyToOSClipboardVerdict(
    const GURL& source) const {
  return GetVerdict(Rule::Restriction::kClipboard,
                    {
                        .source =
                            {
                                .url = source,
                                .incognito = incognito_profile(),
                            },
                        .destination =
                            {
                                .os_clipboard = true,
                            },
                    });
}

Verdict RulesServiceBase::GetDownloadVerdict(const GURL& download_url) const {
  return GetVerdict(Rule::Restriction::kFileDownload,
                    {
                        .source =
                            {
                                .url = download_url,
                                .incognito = incognito_profile(),
                            },
                    });
}

Verdict RulesServiceBase::GetVerdict(Rule::Restriction restriction,
                                     const ActionContext& context) const {
  Rule::Level max_level = Rule::Level::kNotSet;
  Verdict::TriggeredRules triggered_rules;
  for (size_t i = 0; i < rules_.size(); ++i) {
    const auto& rule = rules_[i];
    Rule::Level level = rule.GetLevel(restriction, context);
    if (level > max_level) {
      max_level = level;
    }
    if (level != Rule::Level::kNotSet) {
      triggered_rules[Verdict::TriggeredRuleKey{
          .index = i,
          .machine_scope = MachineScopePolicy(),
      }] = {
          .rule_id = rule.rule_id(),
          .rule_name = rule.name(),
      };
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

bool RulesServiceBase::MachineScopePolicy() const {
  return pref_registrar_.prefs()->GetInteger(kDataControlsRulesScopePref) ==
         policy::POLICY_SCOPE_MACHINE;
}

void RulesServiceBase::OnDataControlsRulesUpdate() {
  DCHECK(pref_registrar_.prefs());
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
