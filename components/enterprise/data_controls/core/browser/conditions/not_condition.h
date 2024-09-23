// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_CONDITIONS_NOT_CONDITION_H_
#define COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_CONDITIONS_NOT_CONDITION_H_

#include "components/enterprise/data_controls/core/browser/conditions/condition.h"

namespace data_controls {

// Implementation of an abstract "not" condition, which evaluates to true if its
// sub-condition is false.
class NotCondition : public Condition {
 public:
  // Returns nullptr if `condition` is null.
  static std::unique_ptr<const Condition> Create(
      std::unique_ptr<const Condition> condition);

  ~NotCondition() override;

  // Condition:
  bool CanBeEvaluated(const ActionContext& action_context) const override;
  bool IsTriggered(const ActionContext& action_context) const override;

 private:
  explicit NotCondition(std::unique_ptr<const Condition> condition);

  const std::unique_ptr<const Condition> condition_;
};

}  // namespace data_controls

#endif  // COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_CONDITIONS_NOT_CONDITION_H_
