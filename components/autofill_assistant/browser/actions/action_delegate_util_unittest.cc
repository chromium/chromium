// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/action_delegate_util.h"

#include "base/test/bind_test_util.h"
#include "base/test/gmock_callback_support.h"
#include "components/autofill_assistant/browser/actions/action_test_utils.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/selector.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::InSequence;

class ActionDelegateUtilTest : public testing::Test {
 public:
  ActionDelegateUtilTest() {}

  void SetUp() override {}

  MOCK_METHOD2(MockAction,
               void(const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&)> done));

  MOCK_METHOD3(MockIndexedAction,
               void(int index,
                    const ElementFinder::Result&,
                    base::OnceCallback<void(const ClientStatus&)> done));

 protected:
  MockActionDelegate mock_action_delegate_;
};

TEST_F(ActionDelegateUtilTest, FindElementFails) {
  EXPECT_CALL(mock_action_delegate_, FindElement(_, _))
      .WillOnce(
          RunOnceCallback<1>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));

  ActionDelegateUtil::FindElementAndPerform(
      &mock_action_delegate_, Selector({"#nothing"}), base::DoNothing(),
      base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), ELEMENT_RESOLUTION_FAILED);
      }));
}

TEST_F(ActionDelegateUtilTest, FindElementAndExecuteSingleAction) {
  Selector expected_selector({"#element"});
  auto expected_element =
      test_util::MockFindElement(mock_action_delegate_, expected_selector);

  EXPECT_CALL(*this, MockAction(EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<1>(ClientStatus(ACTION_APPLIED)));

  ActionDelegateUtil::FindElementAndPerform(
      &mock_action_delegate_, expected_selector,
      base::BindOnce(&ActionDelegateUtilTest::MockAction,
                     base::Unretained(this)),
      base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
      }));
}

TEST_F(ActionDelegateUtilTest, FindElementAndExecuteMultipleActions) {
  InSequence sequence;

  Selector expected_selector({"#element"});
  auto expected_element =
      test_util::MockFindElement(mock_action_delegate_, expected_selector);

  EXPECT_CALL(*this, MockIndexedAction(1, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(ClientStatus(ACTION_APPLIED)));
  EXPECT_CALL(*this, MockIndexedAction(2, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(ClientStatus(ACTION_APPLIED)));
  EXPECT_CALL(*this, MockIndexedAction(3, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(ClientStatus(ACTION_APPLIED)));

  auto actions = std::make_unique<ActionDelegateUtil::ElementActionVector>();
  actions->emplace_back(base::BindOnce(
      &ActionDelegateUtilTest::MockIndexedAction, base::Unretained(this), 1));
  actions->emplace_back(base::BindOnce(
      &ActionDelegateUtilTest::MockIndexedAction, base::Unretained(this), 2));
  actions->emplace_back(base::BindOnce(
      &ActionDelegateUtilTest::MockIndexedAction, base::Unretained(this), 3));

  ActionDelegateUtil::FindElementAndPerform(
      &mock_action_delegate_, expected_selector, std::move(actions),
      base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
      }));
}

TEST_F(ActionDelegateUtilTest, ActionDelegateDeletedDuringExecution) {
  InSequence sequence;

  auto mock_delegate = std::make_unique<MockActionDelegate>();

  Selector expected_selector({"#element"});
  auto expected_element =
      test_util::MockFindElement(*mock_delegate, expected_selector);

  EXPECT_CALL(*mock_delegate, WaitForDocumentToBecomeInteractive(
                                  EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<1>(ClientStatus(ACTION_APPLIED)));
  EXPECT_CALL(*mock_delegate, ScrollIntoView(_, _)).Times(0);

  auto actions = std::make_unique<ActionDelegateUtil::ElementActionVector>();
  actions->emplace_back(
      base::BindOnce(&ActionDelegate::WaitForDocumentToBecomeInteractive,
                     mock_delegate->GetWeakPtr()));
  actions->emplace_back(base::BindOnce(
      [](base::OnceCallback<void()> destroy_delegate,
         const ElementFinder::Result& element,
         base::OnceCallback<void(const ClientStatus&)> done) {
        std::move(destroy_delegate).Run();
        std::move(done).Run(ClientStatus(ACTION_APPLIED));
      },
      base::BindLambdaForTesting([&]() { mock_delegate.reset(); })));
  actions->emplace_back(base::BindOnce(&ActionDelegate::ScrollIntoView,
                                       mock_delegate->GetWeakPtr()));

  ActionDelegateUtil::FindElementAndPerform(
      mock_delegate.get(), expected_selector, std::move(actions),
      base::BindOnce([](const ClientStatus& status) {}));
}

}  // namespace
}  // namespace autofill_assistant
