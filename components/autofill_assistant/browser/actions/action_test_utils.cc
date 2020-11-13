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
                                std::make_unique<ElementFinder::Result>());
      }));
}

ElementFinder::Result MockFindElement(MockActionDelegate& delegate,
                                      const Selector& selector,
                                      int times) {
  EXPECT_CALL(delegate, FindElement(selector, _))
      .Times(times)
      .WillRepeatedly(WithArgs<1>([&selector](auto&& callback) {
        auto element_result = std::make_unique<ElementFinder::Result>();
        element_result->dom_object.object_data.object_id =
            selector.proto.filters(0).css_selector();
        std::move(callback).Run(OkClientStatus(), std::move(element_result));
      }));

  ElementFinder::Result expected_result;
  expected_result.dom_object.object_data.object_id =
      selector.proto.filters(0).css_selector();
  return expected_result;
}

void MockFindAnyElement(MockWebController& web_controller) {
  ON_CALL(web_controller, OnFindElement(_, _))
      .WillByDefault(WithArgs<1>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinder::Result>());
      }));
}

ElementFinder::Result MockFindElement(MockWebController& web_controller,
                                      const Selector& selector,
                                      int times) {
  EXPECT_CALL(web_controller, OnFindElement(selector, _))
      .Times(times)
      .WillRepeatedly(WithArgs<1>([&selector](auto&& callback) {
        auto element_result = std::make_unique<ElementFinder::Result>();
        element_result->dom_object.object_data.object_id =
            selector.proto.filters(0).css_selector();
        std::move(callback).Run(OkClientStatus(), std::move(element_result));
      }));

  ElementFinder::Result expected_result;
  expected_result.dom_object.object_data.object_id =
      selector.proto.filters(0).css_selector();
  return expected_result;
}

}  // namespace test_util
}  // namespace autofill_assistant
