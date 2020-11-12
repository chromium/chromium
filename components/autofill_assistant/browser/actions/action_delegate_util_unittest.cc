// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/action_delegate_util.h"

#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/actions/action_test_utils.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/selector.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace action_delegate_util {
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

  MOCK_METHOD1(MockDone, void(const ClientStatus& status));

  MOCK_METHOD2(MockGetAction,
               void(const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&,
                                            const std::string&)> done));

  MOCK_METHOD2(MockDoneGet,
               void(const ClientStatus& status, const std::string& value));

 protected:
  MockActionDelegate mock_action_delegate_;
};

TEST_F(ActionDelegateUtilTest, FindElementFails) {
  EXPECT_CALL(mock_action_delegate_, FindElement(_, _))
      .WillOnce(
          RunOnceCallback<1>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));
  EXPECT_CALL(*this, MockAction(_, _)).Times(0);
  EXPECT_CALL(*this,
              MockDone(EqualsStatus(ClientStatus(ELEMENT_RESOLUTION_FAILED))));

  FindElementAndPerform(&mock_action_delegate_, Selector({"#nothing"}),
                        base::BindOnce(&ActionDelegateUtilTest::MockAction,
                                       base::Unretained(this)),
                        base::BindOnce(&ActionDelegateUtilTest::MockDone,
                                       base::Unretained(this)));
}

TEST_F(ActionDelegateUtilTest, FindElementAndExecuteSingleAction) {
  Selector expected_selector({"#element"});
  auto expected_element =
      test_util::MockFindElement(mock_action_delegate_, expected_selector);

  EXPECT_CALL(*this, MockAction(EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus()));
  EXPECT_CALL(*this, MockDone(EqualsStatus(OkClientStatus())));

  FindElementAndPerform(&mock_action_delegate_, expected_selector,
                        base::BindOnce(&ActionDelegateUtilTest::MockAction,
                                       base::Unretained(this)),
                        base::BindOnce(&ActionDelegateUtilTest::MockDone,
                                       base::Unretained(this)));
}

TEST_F(ActionDelegateUtilTest, FindElementAndExecuteMultipleActions) {
  InSequence sequence;

  Selector expected_selector({"#element"});
  auto expected_element =
      test_util::MockFindElement(mock_action_delegate_, expected_selector);

  EXPECT_CALL(*this, MockIndexedAction(1, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(*this, MockIndexedAction(2, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(*this, MockIndexedAction(3, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(*this, MockDone(EqualsStatus(OkClientStatus())));

  auto actions = std::make_unique<ElementActionVector>();
  actions->emplace_back(base::BindOnce(
      &ActionDelegateUtilTest::MockIndexedAction, base::Unretained(this), 1));
  actions->emplace_back(base::BindOnce(
      &ActionDelegateUtilTest::MockIndexedAction, base::Unretained(this), 2));
  actions->emplace_back(base::BindOnce(
      &ActionDelegateUtilTest::MockIndexedAction, base::Unretained(this), 3));

  FindElementAndPerform(&mock_action_delegate_, expected_selector,
                        base::BindOnce(&PerformAll, std::move(actions)),
                        base::BindOnce(&ActionDelegateUtilTest::MockDone,
                                       base::Unretained(this)));
}

TEST_F(ActionDelegateUtilTest,
       FindElementAndExecuteMultipleActionsAbortsOnError) {
  InSequence sequence;

  Selector expected_selector({"#element"});
  auto expected_element =
      test_util::MockFindElement(mock_action_delegate_, expected_selector);

  EXPECT_CALL(*this, MockIndexedAction(1, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(*this, MockIndexedAction(2, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(ClientStatus(UNEXPECTED_JS_ERROR)));
  EXPECT_CALL(*this, MockIndexedAction(3, EqualsElement(expected_element), _))
      .Times(0);
  EXPECT_CALL(*this, MockDone(EqualsStatus(ClientStatus(UNEXPECTED_JS_ERROR))));

  auto actions = std::make_unique<ElementActionVector>();
  actions->emplace_back(base::BindOnce(
      &ActionDelegateUtilTest::MockIndexedAction, base::Unretained(this), 1));
  actions->emplace_back(base::BindOnce(
      &ActionDelegateUtilTest::MockIndexedAction, base::Unretained(this), 2));
  actions->emplace_back(base::BindOnce(
      &ActionDelegateUtilTest::MockIndexedAction, base::Unretained(this), 3));

  FindElementAndPerform(&mock_action_delegate_, expected_selector,
                        base::BindOnce(&PerformAll, std::move(actions)),
                        base::BindOnce(&ActionDelegateUtilTest::MockDone,
                                       base::Unretained(this)));
}

TEST_F(ActionDelegateUtilTest, ActionDelegateDeletedDuringExecution) {
  InSequence sequence;

  auto mock_delegate = std::make_unique<MockActionDelegate>();

  Selector expected_selector({"#element"});
  auto expected_element =
      test_util::MockFindElement(*mock_delegate, expected_selector);

  EXPECT_CALL(*mock_delegate, WaitUntilElementIsStable(
                                  _, _, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus()));
  EXPECT_CALL(*mock_delegate, ScrollIntoView(_, _)).Times(0);
  EXPECT_CALL(*this, MockDone(_)).Times(0);

  auto actions = std::make_unique<ElementActionVector>();
  actions->emplace_back(base::BindOnce(
      &ActionDelegate::WaitUntilElementIsStable, mock_delegate->GetWeakPtr(), 1,
      base::TimeDelta::FromMilliseconds(0)));
  actions->emplace_back(base::BindOnce(
      [](base::OnceCallback<void()> destroy_delegate,
         const ElementFinder::Result& element,
         base::OnceCallback<void(const ClientStatus&)> done) {
        std::move(destroy_delegate).Run();
        std::move(done).Run(OkClientStatus());
      },
      base::BindLambdaForTesting([&]() { mock_delegate.reset(); })));
  actions->emplace_back(base::BindOnce(&ActionDelegate::ScrollIntoView,
                                       mock_delegate->GetWeakPtr()));

  FindElementAndPerform(mock_delegate.get(), expected_selector,
                        base::BindOnce(&PerformAll, std::move(actions)),
                        base::BindOnce(&ActionDelegateUtilTest::MockDone,
                                       base::Unretained(this)));
}

TEST_F(ActionDelegateUtilTest, TakeElementAndPerform) {
  auto expected_element = std::make_unique<ElementFinder::Result>();

  EXPECT_CALL(mock_action_delegate_, FindElement(_, _)).Times(0);

  EXPECT_CALL(*this, MockAction(EqualsElement(*expected_element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus()));
  EXPECT_CALL(*this, MockDone(EqualsStatus(OkClientStatus())));

  TakeElementAndPerform(
      base::BindOnce(&ActionDelegateUtilTest::MockAction,
                     base::Unretained(this)),
      base::BindOnce(&ActionDelegateUtilTest::MockDone, base::Unretained(this)),
      OkClientStatus(), std::move(expected_element));
}

TEST_F(ActionDelegateUtilTest, TakeElementAndPerformWithFailedStatus) {
  auto expected_element = std::make_unique<ElementFinder::Result>();

  EXPECT_CALL(mock_action_delegate_, FindElement(_, _)).Times(0);

  EXPECT_CALL(*this, MockAction(_, _)).Times(0);
  EXPECT_CALL(*this,
              MockDone(EqualsStatus(ClientStatus(ELEMENT_RESOLUTION_FAILED))));

  TakeElementAndPerform(
      base::BindOnce(&ActionDelegateUtilTest::MockAction,
                     base::Unretained(this)),
      base::BindOnce(&ActionDelegateUtilTest::MockDone, base::Unretained(this)),
      ClientStatus(ELEMENT_RESOLUTION_FAILED), std::move(expected_element));
}

TEST_F(ActionDelegateUtilTest, TakeElementAndGetProperty) {
  auto expected_element = std::make_unique<ElementFinder::Result>();

  EXPECT_CALL(mock_action_delegate_, FindElement(_, _)).Times(0);

  EXPECT_CALL(*this, MockGetAction(EqualsElement(*expected_element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "value"));
  EXPECT_CALL(*this, MockDoneGet(EqualsStatus(OkClientStatus()), "value"));

  TakeElementAndGetProperty<std::string>(
      base::BindOnce(&ActionDelegateUtilTest::MockGetAction,
                     base::Unretained(this)),
      base::BindOnce(&ActionDelegateUtilTest::MockDoneGet,
                     base::Unretained(this)),
      OkClientStatus(), std::move(expected_element));
}

TEST_F(ActionDelegateUtilTest, TakeElementAndGetPropertyWithFailedStatus) {
  auto expected_element = std::make_unique<ElementFinder::Result>();

  EXPECT_CALL(mock_action_delegate_, FindElement(_, _)).Times(0);

  EXPECT_CALL(*this, MockGetAction(_, _)).Times(0);
  EXPECT_CALL(*this,
              MockDoneGet(EqualsStatus(ClientStatus(ELEMENT_RESOLUTION_FAILED)),
                          std::string()));

  TakeElementAndGetProperty<std::string>(
      base::BindOnce(&ActionDelegateUtilTest::MockGetAction,
                     base::Unretained(this)),
      base::BindOnce(&ActionDelegateUtilTest::MockDoneGet,
                     base::Unretained(this)),
      ClientStatus(ELEMENT_RESOLUTION_FAILED), std::move(expected_element));
}

}  // namespace
}  // namespace action_delegate_util
}  // namespace autofill_assistant
