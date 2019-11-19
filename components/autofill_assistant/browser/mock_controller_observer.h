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
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/ui_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class MockControllerObserver : public ControllerObserver {
 public:
  MockControllerObserver();
  ~MockControllerObserver() override;

  MOCK_METHOD1(OnStatusMessageChanged, void(const std::string& message));
  MOCK_METHOD1(OnBubbleMessageChanged, void(const std::string& message));
  MOCK_METHOD0(CloseCustomTab, void());
  MOCK_METHOD1(OnStateChanged, void(AutofillAssistantState));
  MOCK_METHOD1(OnUserActionsChanged,
               void(const std::vector<UserAction>& user_actions));
  MOCK_METHOD1(OnCollectUserDataOptionsChanged,
               void(const CollectUserDataOptions* options));
  MOCK_METHOD2(OnUserDataChanged,
               void(const UserData* user_data,
                    UserData::FieldChange field_change));
  MOCK_METHOD1(OnDetailsChanged, void(const Details* details));
  MOCK_METHOD1(OnInfoBoxChanged, void(const InfoBox* info_box));
  MOCK_METHOD1(OnProgressChanged, void(int progress));
  MOCK_METHOD1(OnProgressVisibilityChanged, void(bool visible));
  MOCK_METHOD3(OnTouchableAreaChanged,
               void(const RectF&,
                    const std::vector<RectF>& touchable_areas,
                    const std::vector<RectF>& restricted_areas));
  MOCK_CONST_METHOD0(Terminate, bool());
  MOCK_CONST_METHOD0(GetDropOutReason, Metrics::DropOutReason());
  MOCK_METHOD1(OnViewportModeChanged, void(ViewportMode mode));
  MOCK_METHOD1(OnPeekModeChanged,
               void(ConfigureBottomSheetProto::PeekMode peek_mode));
  MOCK_METHOD1(OnOverlayColorsChanged,
               void(const UiDelegate::OverlayColors& colors));
  MOCK_METHOD1(OnFormChanged, void(const FormProto* form));
  MOCK_METHOD1(OnClientSettingsChanged, void(const ClientSettings& settings));
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_CONTROLLER_OBSERVER_H_
