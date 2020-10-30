// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/wait_for_dom_action.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::WithArgs;

class WaitForDomActionTest : public testing::Test {
 public:
  WaitForDomActionTest() {}

  void SetUp() override {
    ON_CALL(mock_web_controller_, OnFindElement(_, _))
        .WillByDefault(RunOnceCallback<1>(
            ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));

    EXPECT_CALL(mock_action_delegate_, OnWaitForDom(_, _, _, _))
        .WillRepeatedly(Invoke(this, &WaitForDomActionTest::FakeWaitForDom));
  }

 protected:
  // Fakes ActionDelegate::WaitForDom.
  void FakeWaitForDom(
      base::TimeDelta max_wait_time,
      bool allow_interrupt,
      base::RepeatingCallback<
          void(BatchElementChecker*,
               base::OnceCallback<void(const ClientStatus&)>)>& check_elements,
      base::OnceCallback<void(const ClientStatus&, base::TimeDelta)>&
          callback) {
    checker_ = std::make_unique<BatchElementChecker>();
    has_check_elements_result_ = false;
    check_elements.Run(
        checker_.get(),
        base::BindOnce(&WaitForDomActionTest::OnCheckElementsDone,
                       base::Unretained(this)));
    checker_->AddAllDoneCallback(
        base::BindOnce(&WaitForDomActionTest::OnWaitForDomDone,
                       base::Unretained(this), std::move(callback)));
    checker_->Run(&mock_web_controller_);
  }

  // Called from the check_elements callback passed to FakeWaitForDom.
  void OnCheckElementsDone(const ClientStatus& result) {
    ASSERT_FALSE(has_check_elements_result_);  // Duplicate calls
    has_check_elements_result_ = true;
    check_elements_result_ = result;
  }

  // Called by |checker_| once it's done. This ends the call to
  // FakeWaitForDom().
  void OnWaitForDomDone(
      base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback) {
    ASSERT_TRUE(
        has_check_elements_result_);  // OnCheckElementsDone() not called
    std::move(callback).Run(check_elements_result_,
                            base::TimeDelta::FromMilliseconds(fake_wait_time_));
  }

  // Runs the action defined in |proto_| and reports the result to |callback_|.
  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_wait_for_dom() = proto_;
    WaitForDomAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  MockActionDelegate mock_action_delegate_;
  MockWebController mock_web_controller_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  WaitForDomProto proto_;
  std::unique_ptr<BatchElementChecker> checker_;
  bool has_check_elements_result_ = false;
  ClientStatus check_elements_result_;
  int fake_wait_time_ = 0;
};

TEST_F(WaitForDomActionTest, NoSelectors) {
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  Run();
}

TEST_F(WaitForDomActionTest, ConditionMet) {
  EXPECT_CALL(mock_web_controller_, OnFindElement(Selector({"#element"}), _))
      .WillRepeatedly(WithArgs<1>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinder::Result>());
      }));

  *proto_.mutable_wait_condition()->mutable_match() =
      ToSelectorProto("#element");
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(WaitForDomActionTest, TimingStatsConditionMet) {
  EXPECT_CALL(mock_web_controller_, OnFindElement(Selector({"#element"}), _))
      .WillRepeatedly(WithArgs<1>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinder::Result>());
      }));

  fake_wait_time_ = 500;
  *proto_.mutable_wait_condition()->mutable_match() =
      ToSelectorProto("#element");

  ProcessedActionProto capture;
  EXPECT_CALL(callback_, Run(_)).WillOnce(testing::SaveArgPointee<0>(&capture));
  Run();

  EXPECT_EQ(capture.timing_stats().wait_time_ms(), 500);
}

TEST_F(WaitForDomActionTest, ConditionNotMet) {
  *proto_.mutable_wait_condition()->mutable_match() =
      ToSelectorProto("#element");
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              ELEMENT_RESOLUTION_FAILED))));
  Run();
}

TEST_F(WaitForDomActionTest, ReportMatchesToServer) {
  EXPECT_CALL(mock_web_controller_, OnFindElement(Selector({"#element1"}), _))
      .WillRepeatedly(WithArgs<1>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinder::Result>());
      }));
  EXPECT_CALL(mock_web_controller_, OnFindElement(Selector({"#element2"}), _))
      .WillRepeatedly(
          RunOnceCallback<1>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));
  EXPECT_CALL(mock_web_controller_, OnFindElement(Selector({"#element3"}), _))
      .WillRepeatedly(
          RunOnceCallback<1>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));
  EXPECT_CALL(mock_web_controller_, OnFindElement(Selector({"#element4"}), _))
      .WillRepeatedly(WithArgs<1>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinder::Result>());
      }));

  auto* any_of = proto_.mutable_wait_condition()->mutable_any_of();
  auto* condition1 = any_of->add_conditions();
  *condition1->mutable_match() = ToSelectorProto("#element1");
  condition1->set_payload("1");

  auto* condition2 = any_of->add_conditions();
  *condition2->mutable_none_of()->add_conditions()->mutable_match() =
      ToSelectorProto("#element2");
  condition2->set_payload("2");

  auto* condition3 = any_of->add_conditions();
  *condition3->mutable_match() = ToSelectorProto("#element3");
  condition3->set_payload("3");

  auto* condition4 = any_of->add_conditions();
  *condition4->mutable_none_of()->add_conditions()->mutable_match() =
      ToSelectorProto("#element4");
  condition4->set_payload("4");

  // Condition 1 and 2 are met, conditions 3 and 4 are not.

  ProcessedActionProto capture;
  EXPECT_CALL(callback_, Run(_)).WillOnce(testing::SaveArgPointee<0>(&capture));
  Run();

  EXPECT_THAT(capture.wait_for_dom_result().matching_condition_payloads(),
              ElementsAre("1", "2"));
}

}  // namespace
}  // namespace autofill_assistant
