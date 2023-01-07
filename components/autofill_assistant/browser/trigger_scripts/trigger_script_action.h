// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_TRIGGER_SCRIPT_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_TRIGGER_SCRIPT_ACTION_H_

namespace autofill_assistant {

// C++ enum corresponding to the TriggerScriptProto::TriggerScriptAction enum
// defined in proto.
//
// GENERATED_JAVA_ENUM_PACKAGE: (
// org.chromium.components.autofill_assistant.trigger_scripts)
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: TriggerScriptAction
enum class TriggerScriptAction {
  UNDEFINED = 0,
  NOT_NOW = 1,
  CANCEL_SESSION = 2,
  CANCEL_FOREVER = 3,
  SHOW_CANCEL_POPUP = 4,
  ACCEPT = 5
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_TRIGGER_SCRIPT_ACTION_H_
