// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_TEST_UTILS_H_

#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/web/element_finder.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

MATCHER_P(EqualsElement, element, "") {
  return arg.object_id == element.object_id;
}

MATCHER_P(EqualsStatus, status, "") {
  return arg.proto_status() == status.proto_status();
}

namespace test_util {

void MockFindAnyElement(MockActionDelegate& delegate);

ElementFinder::Result MockFindElement(MockActionDelegate& delegate,
                                      const Selector& selector);

}  // namespace test_util
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_UNITTEST_HELPER_H_
