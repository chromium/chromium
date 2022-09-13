// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_MOCK_EXTERNAL_ACTION_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_MOCK_EXTERNAL_ACTION_DELEGATE_H_

#include "base/callback_helpers.h"
#include "components/autofill_assistant/browser/public/external_action_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class MockExternalActionDelegate : public ExternalActionDelegate {
 public:
  MockExternalActionDelegate();
  ~MockExternalActionDelegate() override;

  MOCK_METHOD(
      void,
      OnActionRequested,
      (const external::Action& action_info,
       bool is_interrupt,
       base::OnceCallback<void(DomUpdateCallback)> start_dom_checks_callback,
       base::OnceCallback<void(const external::Result&)> end_action_callback),
      (override));
  MOCK_METHOD(void, OnInterruptStarted, (), (override));
  MOCK_METHOD(void, OnInterruptFinished, (), (override));
  MOCK_METHOD(void,
              OnTouchableAreaChanged,
              (const RectF& visual_viewport,
               const std::vector<RectF>& touchable_areas,
               const std::vector<RectF>& restricted_areas),
              (override));
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_MOCK_EXTERNAL_ACTION_DELEGATE_H_
