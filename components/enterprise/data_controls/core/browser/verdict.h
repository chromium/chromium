// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_VERDICT_H_
#define COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_VERDICT_H_

#include "components/enterprise/data_controls/core/browser/rule.h"

namespace data_controls {

// Class representing the verdict Data Controls rules should apply in a specific
// context after evaluating all rules to be applied. Instances of this class
// should be created from combining an action's context with rules by classes
// like `ChromeDlpRulesManager`, and then be considered as the source of truth
// on what UX should be shown, what should be reported, etc.
class Verdict {
 public:
  // The key is the rule's index in the "DataControlsRules" policy list
  // representation that exists in `RulesService`.
  // Since policy updates can change the rules list and invalidate indexes of
  // previously triggered rules, this index key should only be used
  // synchronously to merge rules and not in async cases (for example after a
  // warning dialog has been shown).
  struct TriggeredRule {
    std::string rule_id;
    std::string rule_name;
  };
  using TriggeredRules = base::flat_map<size_t, TriggeredRule>;

  static Verdict NotSet();
  static Verdict Report(TriggeredRules triggered_rules);
  static Verdict Warn(TriggeredRules triggered_rules);
  static Verdict Block(TriggeredRules triggered_rules);
  static Verdict Allow();

  // Creates a combination of two `Verdict`s when both a source and destination
  // `Verdict` are obtained in a single paste. The merge `Verdict` has the
  // highest precedence between the two original verdicts, but only the
  // triggered rules of the destination one for reporting.
  static Verdict MergePasteVerdicts(Verdict source_verdict,
                                    Verdict destination_verdict);

  // Creates a combination of two `Verdict`s for a single warning copy action.
  // `source_only_verdict` represents the verdict against no specific
  // destination (aka a copy warning dialog verdict, and `os_clipboard_verdict`
  // represents the verdict deciding if data should be shared with the OS
  // clipboard. The resulting verdict should only contain triggered rules meant
  // to be reported.
  //
  // This function should only be called when a warning dialog is about to be
  // showned, so the level of the returned verdict is always `kWarn`.
  static Verdict MergeCopyWarningVerdicts(Verdict source_only_verdict,
                                          Verdict os_clipboard_verdict);

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

#endif  // COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_VERDICT_H_
