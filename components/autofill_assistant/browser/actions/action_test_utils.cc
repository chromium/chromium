// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/action_test_utils.h"

#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/web/element_finder.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace test_util {

using ::testing::_;
using ::testing::WithArgs;

void MockFindAnyElement(MockActionDelegate& delegate) {
  ON_CALL(delegate, FindElement(_, _))
      .WillByDefault(WithArgs<1>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinderResult>());
      }));
}

ElementFinderResult MockFindElement(MockActionDelegate& delegate,
                                    const Selector& selector,
                                    int times) {
  EXPECT_CALL(delegate, FindElement(selector, _))
      .Times(times)
      .WillRepeatedly(WithArgs<1>([&selector](auto&& callback) {
        auto element_result = std::make_unique<ElementFinderResult>();
        element_result->SetObjectId(selector.proto.filters(0).css_selector());
        std::move(callback).Run(OkClientStatus(), std::move(element_result));
      }));

  ElementFinderResult expected_result;
  expected_result.SetObjectId(selector.proto.filters(0).css_selector());
  return expected_result;
}

void MockFindAnyElement(MockWebController& web_controller) {
  ON_CALL(web_controller, FindElement(_, _, _))
      .WillByDefault(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinderResult>());
      }));
}

ElementFinderResult MockFindElement(MockWebController& web_controller,
                                    const Selector& selector,
                                    int times) {
  EXPECT_CALL(web_controller, FindElement(selector, _, _))
      .Times(times)
      .WillRepeatedly(WithArgs<2>([&selector](auto&& callback) {
        auto element_result = std::make_unique<ElementFinderResult>();
        element_result->SetObjectId(selector.proto.filters(0).css_selector());
        std::move(callback).Run(OkClientStatus(), std::move(element_result));
      }));

  ElementFinderResult expected_result;
  expected_result.SetObjectId(selector.proto.filters(0).css_selector());
  return expected_result;
}

ValueExpressionBuilder::ValueExpressionBuilder() = default;

ValueExpressionBuilder& ValueExpressionBuilder::addChunk(
    const std::string& text) {
  value_expression.add_chunk()->set_text(text);
  return *this;
}

ValueExpressionBuilder& ValueExpressionBuilder::addChunk(int key) {
  value_expression.add_chunk()->set_key(key);
  return *this;
}

ValueExpressionBuilder& ValueExpressionBuilder::addChunk(
    autofill::ServerFieldType field) {
  value_expression.add_chunk()->set_key(static_cast<int>(field));
  return *this;
}

ValueExpression ValueExpressionBuilder::toProto() {
  return value_expression;
}

}  // namespace test_util
}  // namespace autofill_assistant
