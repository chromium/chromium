// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_CONTROLS_VERDICT_H_
#define COMPONENTS_ENTERPRISE_DATA_CONTROLS_VERDICT_H_

#include "base/functional/callback.h"
#include "components/enterprise/data_controls/rule.h"

namespace data_controls {

// Class representing the verdict Data Controls rules should apply in a specific
// context after evaluating all rules to be applied. Instances of this class
// should be created from combining an action's context with rules by classes
// like `ChromeDlpRulesManager`, and then be considered as the source of truth
// on what UX should be shown, what should be reported, etc.
class Verdict {
 public:
  static Verdict NotSet();
  static Verdict Report(base::OnceClosure initial_report_closure);
  static Verdict Warn(base::OnceClosure initial_report_closure,
                      base::OnceClosure bypass_report_closure);
  static Verdict Block(base::OnceClosure initial_report_closure);
  static Verdict Allow();

  ~Verdict();
  Verdict(Verdict&&);
  Verdict& operator=(Verdict&&);

  Rule::Level level() const;

  // Accessors that take ownership of the underlying unique closures.
  base::OnceClosure TakeInitialReportClosure();
  base::OnceClosure TakeBypassReportClosure();

 private:
  explicit Verdict(
      Rule::Level level,
      base::OnceClosure initial_report_closure = base::OnceClosure(),
      base::OnceClosure bypass_report_closure = base::OnceClosure());

  // The highest-precedence rule level to be applied to the action potentially
  // interrupted by Data Controls.
  Rule::Level level_;

  // known at rule evaluation, so its type will need to change.
  // The callback to be called to report the initial Data Controls rule
  // triggers.
  // TODO(b/303640183): This callback will likely require more information not
  // known at rule evaluation, so its type will need to change.
  base::OnceClosure initial_report_closure_;

  // The callback to be called when `level` is `kWarn` and the user bypasses the
  // warning shown to them. This should be used to report the appropriate event,
  // and should be populated with copied data of the triggered rules since those
  // can change arbitrarily between the time of the warning being shown and the
  // user bypassing it.
  // TODO(b/303640183): This callback will likely require more information not
  // known at rule evaluation, so its type will need to change.
  base::OnceClosure bypass_report_closure_;
};

}  // namespace data_controls

#endif  // COMPONENTS_ENTERPRISE_DATA_CONTROLS_VERDICT_H_
