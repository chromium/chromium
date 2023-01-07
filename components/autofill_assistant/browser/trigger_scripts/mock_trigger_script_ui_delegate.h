// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_MOCK_TRIGGER_SCRIPT_UI_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_MOCK_TRIGGER_SCRIPT_UI_DELEGATE_H_

#include "components/autofill_assistant/browser/trigger_scripts/trigger_script_coordinator.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class MockTriggerScriptUiDelegate
    : public TriggerScriptCoordinator::UiDelegate {
 public:
  MockTriggerScriptUiDelegate();
  ~MockTriggerScriptUiDelegate() override;

  MOCK_METHOD1(ShowTriggerScript, void(const TriggerScriptUIProto&));
  MOCK_METHOD0(HideTriggerScript, void());
  MOCK_METHOD1(Attach, void(TriggerScriptCoordinator*));
  MOCK_METHOD0(Detach, void());
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_MOCK_TRIGGER_SCRIPT_UI_DELEGATE_H_
