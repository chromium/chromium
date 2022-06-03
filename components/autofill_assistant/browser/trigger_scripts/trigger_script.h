// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_TRIGGER_SCRIPT_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_TRIGGER_SCRIPT_H_

#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/trigger_scripts/dynamic_trigger_conditions.h"
#include "components/autofill_assistant/browser/trigger_scripts/static_trigger_conditions.h"

namespace autofill_assistant {

// C++ class for a particular proto instance of |TriggerScriptProto|.
class TriggerScript {
 public:
  TriggerScript(const TriggerScriptProto& proto);
  ~TriggerScript();
  TriggerScript(const TriggerScript&) = delete;
  TriggerScript& operator=(const TriggerScript&) = delete;

  // Evaluates the trigger conditions of this trigger script. Expects all
  // trigger conditions to already be evaluated and cached in
  // |static_trigger_conditions| and |dynamic_trigger_conditions|.
  bool EvaluateTriggerConditions(
      const StaticTriggerConditions& static_trigger_conditions,
      const DynamicTriggerConditions& dynamic_trigger_conditions) const;

  TriggerScriptProto AsProto() const;

  // Whether the trigger script is currently waiting for its precondition to be
  // fulfilled or for its precondition to stop being fulfilled.
  bool waiting_for_precondition_no_longer_true() const;
  void waiting_for_precondition_no_longer_true(bool waiting);

  TriggerScriptProto::TriggerUIType trigger_ui_type() const {
    return proto_.trigger_ui_type();
  }

 private:
  friend class TriggerScriptTest;

  TriggerScriptProto proto_;
  bool waiting_for_precondition_no_longer_true_ = false;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_TRIGGER_SCRIPT_H_
