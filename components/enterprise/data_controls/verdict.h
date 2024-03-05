// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_CONTROLS_VERDICT_H_
#define COMPONENTS_ENTERPRISE_DATA_CONTROLS_VERDICT_H_

#include "components/enterprise/data_controls/rule.h"

namespace data_controls {

// Class representing the verdict Data Controls rules should apply in a specific
// context after evaluating all rules to be applied. Instances of this class
// should be created from combining an action's context with rules by classes
// like `ChromeDlpRulesManager`, and then be considered as the source of truth
// on what UX should be shown, what should be reported, etc.
class Verdict {
 public:
  // The key is the rule's ID and the value is the rule's name.
  using TriggeredRules = base::flat_map<std::string, std::string>;

  static Verdict NotSet();
  static Verdict Report(TriggeredRules triggered_rules);
  static Verdict Warn(TriggeredRules triggered_rules);
  static Verdict Block(TriggeredRules triggered_rules);
  static Verdict Allow();

  // In some circumstances multiple verdicts need to be merged, for example when
  // an action has involves two different profiles. This helper can be used to
  // simplify the logic to apply to the action for both verdicts.
  static Verdict Merge(Verdict verdict_1, Verdict verdict_2);

  ~Verdict();
  Verdict(Verdict&&);
  Verdict& operator=(Verdict&&);

  Rule::Level level() const;

  // Accessor to triggered rules corresponding to this verdict.
  // The key is the rule's ID and the value is the rule's name.
  const TriggeredRules& triggered_rules() const;

 private:
  explicit Verdict(Rule::Level level, TriggeredRules triggered_rules);

  // The highest-precedence rule level to be applied to the action potentially
  // interrupted by Data Controls.
  Rule::Level level_;

  // Rules triggered at an action's source represented by this verdict.
  // The key is the rule's ID and the value is the rule's name.
  TriggeredRules triggered_rules_;
};

}  // namespace data_controls

#endif  // COMPONENTS_ENTERPRISE_DATA_CONTROLS_VERDICT_H_
