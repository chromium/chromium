// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_EXECUTION_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_EXECUTION_DELEGATE_H_

#include "components/autofill_assistant/browser/client_settings.h"
#include "components/autofill_assistant/browser/execution_delegate.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class MockExecutionDelegate : public ExecutionDelegate {
 public:
  MockExecutionDelegate();
  ~MockExecutionDelegate() override;

  MOCK_CONST_METHOD0(GetState, AutofillAssistantState());
  MOCK_METHOD0(GetUserData, UserData*());
  MOCK_METHOD2(OnFatalError,
               void(const std::string& error_message,
                    Metrics::DropOutReason reason));
  MOCK_METHOD2(OnStop,
               void(const std::string& message,
                    const std::string& button_label));
  MOCK_CONST_METHOD1(GetTouchableArea, void(std::vector<RectF>* rectangles));
  MOCK_CONST_METHOD1(GetRestrictedArea, void(std::vector<RectF>* rectangles));
  MOCK_CONST_METHOD1(GetVisualViewport, void(RectF* viewport));
  MOCK_METHOD0(GetViewportMode, ViewportMode());
  MOCK_METHOD0(IsTabSelected, bool());
  MOCK_METHOD1(SetTabSelected, void(bool selected));
  MOCK_CONST_METHOD1(GetOverlayColors, void(OverlayColors* colors));
  MOCK_CONST_METHOD0(GetClientSettings, const ClientSettings&());
  MOCK_METHOD0(GetTriggerContext, const TriggerContext*());
  MOCK_METHOD0(GetCurrentURL, const GURL&());
  MOCK_METHOD1(SetUiShown, void(bool shown));
  MOCK_METHOD0(GetUserModel, UserModel*());
  MOCK_CONST_METHOD0(ShouldShowOverlay, bool());
  MOCK_CONST_METHOD0(ShouldSuppressKeyboard, bool());
  MOCK_METHOD1(SuppressKeyboard, void(bool suppress));
  MOCK_METHOD0(ShutdownIfNecessary, void());
  MOCK_METHOD1(NotifyUserDataChange, void(UserDataFieldChange field_change));
  MOCK_METHOD1(AddObserver, void(ControllerObserver* observer));
  MOCK_METHOD1(RemoveObserver, void(const ControllerObserver* observer));
  MOCK_CONST_METHOD0(NeedsUI, bool());
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_EXECUTION_DELEGATE_H_
