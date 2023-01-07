// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/element_action_util.h"

#include "base/guid.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "components/autofill_assistant/browser/action_value.pb.h"
#include "components/autofill_assistant/browser/actions/action_test_utils.h"
#include "components/autofill_assistant/browser/web/element_finder_result.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace autofill_assistant {
namespace element_action_util {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::InSequence;

class ElementActionUtilTest : public testing::Test {
 public:
  ElementActionUtilTest() {}

  void SetUp() override { element_.SetObjectId("element"); }

  MOCK_METHOD2(MockAction,
               void(const ElementFinderResult& element,
                    base::OnceCallback<void(const ClientStatus&)> done));

  MOCK_METHOD3(MockIndexedAction,
               void(int index,
                    const ElementFinderResult&,
                    base::OnceCallback<void(const ClientStatus&)> done));

  MOCK_METHOD1(MockDone, void(const ClientStatus& status));

  MOCK_METHOD2(MockGetAction,
               void(const ElementFinderResult& element,
                    base::OnceCallback<void(const ClientStatus&,
                                            const std::string&)> done));

  MOCK_METHOD2(MockDoneGet,
               void(const ClientStatus& status, const std::string& value));

 protected:
  MockWebController mock_web_controller_;
  ElementFinderResult element_;
};

TEST_F(ElementActionUtilTest, ExecuteSingleAction) {
  EXPECT_CALL(*this, MockAction(EqualsElement(element_), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus()));
  EXPECT_CALL(*this, MockDone(EqualsStatus(OkClientStatus())));

  auto actions = std::make_unique<ElementActionVector>();
  actions->emplace_back(base::BindOnce(&ElementActionUtilTest::MockAction,
                                       base::Unretained(this)));
  PerformAll(
      std::move(actions), element_,
      base::BindOnce(&ElementActionUtilTest::MockDone, base::Unretained(this)));
}

TEST_F(ElementActionUtilTest, ExecuteMultipleActions) {
  InSequence sequence;

  EXPECT_CALL(*this, MockIndexedAction(1, EqualsElement(element_), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(*this, MockIndexedAction(2, EqualsElement(element_), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(*this, MockIndexedAction(3, EqualsElement(element_), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(*this, MockDone(EqualsStatus(OkClientStatus())));

  auto actions = std::make_unique<ElementActionVector>();
  actions->emplace_back(base::BindOnce(
      &ElementActionUtilTest::MockIndexedAction, base::Unretained(this), 1));
  actions->emplace_back(base::BindOnce(
      &ElementActionUtilTest::MockIndexedAction, base::Unretained(this), 2));
  actions->emplace_back(base::BindOnce(
      &ElementActionUtilTest::MockIndexedAction, base::Unretained(this), 3));

  PerformAll(
      std::move(actions), element_,
      base::BindOnce(&ElementActionUtilTest::MockDone, base::Unretained(this)));
}

TEST_F(ElementActionUtilTest, ExecuteActionsAbortOnError) {
  InSequence sequence;

  EXPECT_CALL(*this, MockIndexedAction(1, EqualsElement(element_), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(*this, MockIndexedAction(2, EqualsElement(element_), _))
      .WillOnce(RunOnceCallback<2>(ClientStatus(UNEXPECTED_JS_ERROR)));
  EXPECT_CALL(*this, MockIndexedAction(3, EqualsElement(element_), _)).Times(0);
  EXPECT_CALL(*this, MockDone(EqualsStatus(ClientStatus(UNEXPECTED_JS_ERROR))));

  auto actions = std::make_unique<ElementActionVector>();
  actions->emplace_back(base::BindOnce(
      &ElementActionUtilTest::MockIndexedAction, base::Unretained(this), 1));
  actions->emplace_back(base::BindOnce(
      &ElementActionUtilTest::MockIndexedAction, base::Unretained(this), 2));
  actions->emplace_back(base::BindOnce(
      &ElementActionUtilTest::MockIndexedAction, base::Unretained(this), 3));

  PerformAll(
      std::move(actions), element_,
      base::BindOnce(&ElementActionUtilTest::MockDone, base::Unretained(this)));
}

TEST_F(ElementActionUtilTest, TakeElementAndPerform) {
  auto expected_element = std::make_unique<ElementFinderResult>();

  EXPECT_CALL(*this, MockAction(EqualsElement(*expected_element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus()));
  EXPECT_CALL(*this, MockDone(EqualsStatus(OkClientStatus())));

  TakeElementAndPerform(
      base::BindOnce(&ElementActionUtilTest::MockAction,
                     base::Unretained(this)),
      base::BindOnce(&ElementActionUtilTest::MockDone, base::Unretained(this)),
      OkClientStatus(), std::move(expected_element));
}

TEST_F(ElementActionUtilTest, TakeElementAndPerformWithFailedStatus) {
  auto expected_element = std::make_unique<ElementFinderResult>();

  EXPECT_CALL(*this, MockAction(_, _)).Times(0);
  EXPECT_CALL(*this,
              MockDone(EqualsStatus(ClientStatus(ELEMENT_RESOLUTION_FAILED))));

  TakeElementAndPerform(
      base::BindOnce(&ElementActionUtilTest::MockAction,
                     base::Unretained(this)),
      base::BindOnce(&ElementActionUtilTest::MockDone, base::Unretained(this)),
      ClientStatus(ELEMENT_RESOLUTION_FAILED), std::move(expected_element));
}

TEST_F(ElementActionUtilTest, TakeElementAndGetProperty) {
  auto expected_element = std::make_unique<ElementFinderResult>();

  EXPECT_CALL(*this, MockGetAction(EqualsElement(*expected_element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "value"));
  EXPECT_CALL(*this, MockDoneGet(EqualsStatus(OkClientStatus()), "value"));

  TakeElementAndGetProperty<const std::string&>(
      base::BindOnce(&ElementActionUtilTest::MockGetAction,
                     base::Unretained(this)),
      std::string(),
      base::BindOnce(&ElementActionUtilTest::MockDoneGet,
                     base::Unretained(this)),
      OkClientStatus(), std::move(expected_element));
}

TEST_F(ElementActionUtilTest, TakeElementAndGetPropertyWithFailedStatus) {
  auto expected_element = std::make_unique<ElementFinderResult>();

  EXPECT_CALL(*this, MockGetAction(_, _)).Times(0);
  EXPECT_CALL(*this,
              MockDoneGet(EqualsStatus(ClientStatus(ELEMENT_RESOLUTION_FAILED)),
                          std::string()));

  TakeElementAndGetProperty<const std::string&>(
      base::BindOnce(&ElementActionUtilTest::MockGetAction,
                     base::Unretained(this)),
      std::string(),
      base::BindOnce(&ElementActionUtilTest::MockDoneGet,
                     base::Unretained(this)),
      ClientStatus(ELEMENT_RESOLUTION_FAILED), std::move(expected_element));
}

}  // namespace
}  // namespace element_action_util
}  // namespace autofill_assistant
