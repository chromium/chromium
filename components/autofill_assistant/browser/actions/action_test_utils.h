// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_TEST_UTILS_H_

#include "components/autofill/core/browser/field_types.h"
#include "components/autofill_assistant/browser/action_value.pb.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/web/element_finder_result.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

MATCHER_P(EqualsElement, element, "") {
  return arg.object_id() == element.object_id();
}

MATCHER_P(EqualsStatus, status, "") {
  return arg.proto_status() == status.proto_status();
}

namespace test_util {

// Mock |ActionDelegate::FindElement| an unspecified amount of times for any
// selector.
void MockFindAnyElement(MockActionDelegate& delegate);

// Expect |ActionDelegate::FindElement| being called a specified amount of
// times for the given |Selector|.
ElementFinderResult MockFindElement(MockActionDelegate& delegate,
                                    const Selector& selector,
                                    int times = 1);

// Mock |WebController::FindElement| an unspecified amount of times for any
// selector.
void MockFindAnyElement(MockWebController& web_controller);

// Expect |WebController::FindElement| being called a specified amount of times
// for the given |Selector|.
ElementFinderResult MockFindElement(MockWebController& web_controller,
                                    const Selector& selector,
                                    int times = 1);

struct ValueExpressionBuilder {
 public:
  ValueExpressionBuilder();

  ValueExpressionBuilder(const ValueExpressionBuilder&) = delete;
  ValueExpressionBuilder& operator=(const ValueExpressionBuilder&) = delete;

  ValueExpressionBuilder& addChunk(const std::string& text);
  ValueExpressionBuilder& addChunk(int key);
  ValueExpressionBuilder& addChunk(autofill::ServerFieldType field);

  ValueExpression toProto();

 private:
  ValueExpression value_expression;
};

}  // namespace test_util
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_TEST_UTILS_H_
