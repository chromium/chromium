// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_UI_CONTROLLER_OBSERVER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_UI_CONTROLLER_OBSERVER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "components/autofill_assistant/browser/execution_delegate.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/ui_controller_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class MockUiControllerObserver : public UiControllerObserver {
 public:
  MockUiControllerObserver();
  ~MockUiControllerObserver() override;

  MOCK_METHOD1(OnStatusMessageChanged, void(const std::string& message));
  MOCK_METHOD1(OnBubbleMessageChanged, void(const std::string& message));
  MOCK_METHOD1(OnStateChanged, void(AutofillAssistantState));
  MOCK_METHOD1(OnUserActionsChanged,
               void(const std::vector<UserAction>& user_actions));
  MOCK_METHOD1(OnCollectUserDataOptionsChanged,
               void(const CollectUserDataOptions* options));
  MOCK_METHOD2(OnCollectUserDataUiStateChanged,
               void(bool loading, UserDataEventField event_field));
  MOCK_METHOD1(OnDetailsChanged, void(const std::vector<Details>& details));
  MOCK_METHOD1(OnInfoBoxChanged, void(const InfoBox* info_box));
  MOCK_METHOD1(OnProgressChanged, void(int progress));
  MOCK_METHOD1(OnProgressActiveStepChanged, void(int active_step));
  MOCK_METHOD1(OnProgressVisibilityChanged, void(bool visible));
  MOCK_METHOD1(OnStepProgressBarConfigurationChanged,
               void(const ShowProgressBarProto::StepProgressBarConfiguration&
                        configuration));
  MOCK_METHOD1(OnProgressBarErrorStateChanged, void(bool error));
  MOCK_METHOD1(OnPeekModeChanged,
               void(ConfigureBottomSheetProto::PeekMode peek_mode));
  MOCK_METHOD0(OnExpandBottomSheet, void());
  MOCK_METHOD0(OnCollapseBottomSheet, void());
  MOCK_METHOD2(OnFormChanged,
               void(const FormProto* form, const FormProto::Result* result));
  MOCK_METHOD1(OnGenericUserInterfaceChanged,
               void(const GenericUserInterfaceProto* generic_ui));
  MOCK_METHOD1(OnPersistentGenericUserInterfaceChanged,
               void(const GenericUserInterfaceProto* generic_ui));
  MOCK_METHOD1(OnTtsButtonVisibilityChanged, void(bool visible));
  MOCK_METHOD1(OnTtsButtonStateChanged, void(TtsButtonState state));
  MOCK_METHOD0(OnFeedbackFormRequested, void());
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_UI_CONTROLLER_OBSERVER_H_
