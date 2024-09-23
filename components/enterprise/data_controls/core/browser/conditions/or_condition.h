// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_CONDITIONS_OR_CONDITION_H_
#define COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_CONDITIONS_OR_CONDITION_H_

#include "components/enterprise/data_controls/core/browser/conditions/condition.h"

namespace data_controls {

// Implementation of an abstract "and" condition, which evaluates to true if one
// of its sub-conditions are true.
class OrCondition : public Condition {
 public:
  // Returns nullptr if the passed vector is empty.
  static std::unique_ptr<Condition> Create(
      std::vector<std::unique_ptr<const Condition>> conditions);

  ~OrCondition() override;

  // Condition:
  bool CanBeEvaluated(const ActionContext& action_context) const override;
  bool IsTriggered(const ActionContext& action_context) const override;

 private:
  explicit OrCondition(
      std::vector<std::unique_ptr<const Condition>> conditions);

  const std::vector<std::unique_ptr<const Condition>> conditions_;
};

}  // namespace data_controls

#endif  // COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_CONDITIONS_OR_CONDITION_H_
