// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_CONTROLLER_OBSERVER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_CONTROLLER_OBSERVER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "components/autofill_assistant/browser/controller_observer.h"
#include "components/autofill_assistant/browser/execution_delegate.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/script.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class MockControllerObserver : public ControllerObserver {
 public:
  MockControllerObserver();
  ~MockControllerObserver() override;

  MOCK_METHOD1(OnStateChanged, void(AutofillAssistantState));
  MOCK_METHOD1(OnKeyboardSuppressionStateChanged, void(bool));
  MOCK_METHOD0(CloseCustomTab, void());
  MOCK_METHOD2(OnError,
               void(const std::string& error_message,
                    Metrics::DropOutReason reason));
  MOCK_METHOD2(OnUserDataChanged,
               void(const UserData& user_data,
                    UserDataFieldChange field_change));
  MOCK_METHOD3(OnTouchableAreaChanged,
               void(const RectF& visual_viewport,
                    const std::vector<RectF>& touchable_areas,
                    const std::vector<RectF>& restricted_areas));
  MOCK_METHOD1(OnViewportModeChanged, void(ViewportMode mode));
  MOCK_METHOD1(OnOverlayColorsChanged,
               void(const ExecutionDelegate::OverlayColors& colors));
  MOCK_METHOD1(OnClientSettingsChanged, void(const ClientSettings& settings));
  MOCK_METHOD1(OnShouldShowOverlayChanged, void(bool should_show));
  MOCK_METHOD1(OnExecuteScript, void(const std::string& start_message));
  MOCK_METHOD1(OnStart, void(const TriggerContext& trigger_context));
  MOCK_METHOD0(OnStop, void());
  MOCK_METHOD0(OnResetState, void());
  MOCK_METHOD1(OnUiShownChanged, void(bool shown));
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_CONTROLLER_OBSERVER_H_
