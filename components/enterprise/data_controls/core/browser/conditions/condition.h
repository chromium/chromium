// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_CONDITIONS_CONDITION_H_
#define COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_CONDITIONS_CONDITION_H_

#include "components/enterprise/data_controls/core/browser/action_context.h"

namespace data_controls {

// Interface for a generic condition to be evaluated by a Data Controls policy.
class Condition {
 public:
  // Returns true if `action_context` contains the necessary fields to properly
  // evaluate the condition.
  virtual bool CanBeEvaluated(const ActionContext& action_context) const = 0;

  // Returns true if the condition is triggered by `action_context`. This should
  // be called after `CanBeEvaluated()` as not every `ActionContext` has enough
  // information to know if it should trigger for a given rule.
  virtual bool IsTriggered(const ActionContext& action_context) const = 0;

  virtual ~Condition() = default;
};

}  // namespace data_controls

#endif  // COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_CONDITIONS_CONDITION_H_
