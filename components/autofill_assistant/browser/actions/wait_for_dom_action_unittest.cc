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

class WaitForDomActionTest : public testing::Test {
 public:
  WaitForDomActionTest() {}

  void SetUp() override {
    ON_CALL(mock_web_controller_, OnElementCheck(_, _))
        .WillByDefault(
            RunOnceCallback<1>(ClientStatus(ELEMENT_RESOLUTION_FAILED)));

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
      base::OnceCallback<void(const ClientStatus&)>& callback) {
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
      base::OnceCallback<void(const ClientStatus&)> callback) {
    ASSERT_TRUE(
        has_check_elements_result_);  // OnCheckElementsDone() not called
    std::move(callback).Run(check_elements_result_);
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
};

TEST_F(WaitForDomActionTest, NoSelectors) {
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  Run();
}

TEST_F(WaitForDomActionTest, MatchOneElementFound) {
  EXPECT_CALL(mock_web_controller_, OnElementCheck(Selector({"#element"}), _))
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus()));

  proto_.mutable_wait_until()->add_selectors("#element");
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(WaitForDomActionTest, MatchOneElementNotFound) {
  proto_.mutable_wait_until()->add_selectors("#element");
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              ELEMENT_RESOLUTION_FAILED))));
  Run();
}

TEST_F(WaitForDomActionTest, NoMatchOneElementFound) {
  EXPECT_CALL(mock_web_controller_, OnElementCheck(Selector({"#element"}), _))
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus()));

  proto_.mutable_wait_while()->add_selectors("#element");
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              ELEMENT_RESOLUTION_FAILED))));
  Run();
}

TEST_F(WaitForDomActionTest, NoMatchOneElementNotFound) {
  proto_.mutable_wait_while()->add_selectors("#element");
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(WaitForDomActionTest, AllConditionsMetWantAll) {
  EXPECT_CALL(mock_web_controller_, OnElementCheck(Selector({"#element1"}), _))
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_, OnElementCheck(Selector({"#element2"}), _))
      .WillRepeatedly(RunOnceCallback<1>(ClientStatus()));
  proto_.mutable_wait_for_all()
      ->add_conditions()
      ->mutable_must_match()
      ->add_selectors("#element1");
  proto_.mutable_wait_for_all()
      ->add_conditions()
      ->mutable_must_not_match()
      ->add_selectors("#element2");
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(WaitForDomActionTest, AllConditionsMetWantSome) {
  EXPECT_CALL(mock_web_controller_, OnElementCheck(Selector({"#element1"}), _))
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_, OnElementCheck(Selector({"#element2"}), _))
      .WillRepeatedly(RunOnceCallback<1>(ClientStatus()));
  proto_.mutable_wait_for_any()
      ->add_conditions()
      ->mutable_must_match()
      ->add_selectors("#element1");
  proto_.mutable_wait_for_any()
      ->add_conditions()
      ->mutable_must_not_match()
      ->add_selectors("#element2");
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(WaitForDomActionTest, NoConditionsMetWantAll) {
  proto_.mutable_wait_for_all()
      ->add_conditions()
      ->mutable_must_match()
      ->add_selectors("#element1");
  proto_.mutable_wait_for_all()
      ->add_conditions()
      ->mutable_must_match()
      ->add_selectors("#element2");
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              ELEMENT_RESOLUTION_FAILED))));
  Run();
}

TEST_F(WaitForDomActionTest, NoConditionsMetWantSome) {
  proto_.mutable_wait_for_any()
      ->add_conditions()
      ->mutable_must_match()
      ->add_selectors("#element1");
  proto_.mutable_wait_for_any()
      ->add_conditions()
      ->mutable_must_match()
      ->add_selectors("#element2");
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              ELEMENT_RESOLUTION_FAILED))));
  Run();
}

TEST_F(WaitForDomActionTest, OnConditionMetWantAll) {
  // element1 should be there, but isn't, so this doesn't satisfy wait_for_all
  proto_.mutable_wait_for_all()
      ->add_conditions()
      ->mutable_must_match()
      ->add_selectors("#element1");
  proto_.mutable_wait_for_all()
      ->add_conditions()
      ->mutable_must_not_match()
      ->add_selectors("#element2");
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              ELEMENT_RESOLUTION_FAILED))));
  Run();
}

TEST_F(WaitForDomActionTest, OnConditionMetWantSome) {
  // element1 should be there, but isn't. element2 is not there, as expected.
  // This satisfies wait_for_one.
  proto_.mutable_wait_for_any()
      ->add_conditions()
      ->mutable_must_match()
      ->add_selectors("#element1");
  proto_.mutable_wait_for_any()
      ->add_conditions()
      ->mutable_must_not_match()
      ->add_selectors("#element2");
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(WaitForDomActionTest, ReportMatchesToServer) {
  EXPECT_CALL(mock_web_controller_, OnElementCheck(Selector({"#element1"}), _))
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_, OnElementCheck(Selector({"#element2"}), _))
      .WillRepeatedly(RunOnceCallback<1>(ClientStatus()));
  EXPECT_CALL(mock_web_controller_, OnElementCheck(Selector({"#element3"}), _))
      .WillRepeatedly(RunOnceCallback<1>(ClientStatus()));
  EXPECT_CALL(mock_web_controller_, OnElementCheck(Selector({"#element4"}), _))
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus()));

  auto* condition1 = proto_.mutable_wait_for_any()->add_conditions();
  condition1->mutable_must_match()->add_selectors("#element1");
  condition1->set_server_payload("1");

  auto* condition2 = proto_.mutable_wait_for_any()->add_conditions();
  condition2->mutable_must_not_match()->add_selectors("#element2");
  condition2->set_server_payload("2");

  auto* condition3 = proto_.mutable_wait_for_any()->add_conditions();
  condition3->mutable_must_match()->add_selectors("#element3");
  condition3->set_server_payload("3");

  auto* condition4 = proto_.mutable_wait_for_any()->add_conditions();
  condition4->mutable_must_not_match()->add_selectors("#element4");
  condition4->set_server_payload("4");

  // Condition 1 and 2 are met, conditions 3 and 4 are not.

  ProcessedActionProto capture;
  EXPECT_CALL(callback_, Run(_)).WillOnce(testing::SaveArgPointee<0>(&capture));
  Run();

  EXPECT_THAT(capture.wait_for_dom_result().matching_condition_payloads(),
              ElementsAre("1", "2"));
}

}  // namespace
}  // namespace autofill_assistant
