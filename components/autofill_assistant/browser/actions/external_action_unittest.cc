// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/external_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/actions/wait_for_dom_test_base.h"
#include "components/autofill_assistant/browser/external_action_extension_test.pb.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::UnorderedElementsAre;
using ::testing::WithArgs;

class ExternalActionTest : public WaitForDomTestBase {
 public:
  ExternalActionTest() = default;

 protected:
  void Run() {
    ON_CALL(mock_action_delegate_, SupportsExternalActions)
        .WillByDefault(Return(true));

    ActionProto action_proto;
    *action_proto.mutable_external_action() = proto_;
    action_ =
        std::make_unique<ExternalAction>(&mock_action_delegate_, action_proto);
    action_->ProcessAction(callback_.Get());
  }

  base::MockCallback<Action::ProcessActionCallback> callback_;
  ExternalActionProto proto_;
  std::unique_ptr<ExternalAction> action_;
};

external::Result MakeResult(bool success) {
  external::Result result;
  result.set_success(success);
  testing::TestResultExtension test_extension_proto;
  test_extension_proto.set_text("test text");

  *result.mutable_result_info()->MutableExtension(
      testing::test_result_extension) = std::move(test_extension_proto);
  return result;
}

TEST_F(ExternalActionTest, Success) {
  proto_.mutable_info();

  EXPECT_CALL(mock_action_delegate_, RequestExternalAction)
      .WillOnce(RunOnceCallback<2>(MakeResult(/* success= */ true)));

  std::unique_ptr<ProcessedActionProto> returned_processed_action_proto;
  EXPECT_CALL(callback_, Run)
      .WillOnce(
          [&returned_processed_action_proto](
              std::unique_ptr<ProcessedActionProto> processed_action_proto) {
            returned_processed_action_proto = std::move(processed_action_proto);
          });
  Run();
  EXPECT_THAT(returned_processed_action_proto->status(), Eq(ACTION_APPLIED));
  EXPECT_TRUE(returned_processed_action_proto->has_external_action_result());
  EXPECT_THAT(returned_processed_action_proto->external_action_result()
                  .result_info()
                  .GetExtension(testing::test_result_extension)
                  .text(),
              Eq("test text"));
}

TEST_F(ExternalActionTest, ExternalFailure) {
  proto_.mutable_info();

  std::unique_ptr<ProcessedActionProto> returned_processed_action_proto;
  EXPECT_CALL(callback_, Run)
      .WillOnce(
          [&returned_processed_action_proto](
              std::unique_ptr<ProcessedActionProto> processed_action_proto) {
            returned_processed_action_proto = std::move(processed_action_proto);
          });
  EXPECT_CALL(mock_action_delegate_, RequestExternalAction)
      .WillOnce(RunOnceCallback<2>(MakeResult(/* success= */ false)));
  Run();
  EXPECT_THAT(returned_processed_action_proto->status(),
              Eq(UNKNOWN_ACTION_STATUS));
  EXPECT_TRUE(returned_processed_action_proto->has_external_action_result());
  EXPECT_THAT(returned_processed_action_proto->external_action_result()
                  .result_info()
                  .GetExtension(testing::test_result_extension)
                  .text(),
              Eq("test text"));
}

TEST_F(ExternalActionTest, FailsIfProtoExtensionInfoNotSet) {
  EXPECT_CALL(mock_action_delegate_, RequestExternalAction).Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  Run();
}

TEST_F(ExternalActionTest, FailsIfDelegateDoesNotSupportExternalActions) {
  proto_.mutable_info();
  EXPECT_CALL(mock_action_delegate_, SupportsExternalActions())
      .WillOnce(Return(false));
  EXPECT_CALL(mock_action_delegate_, RequestExternalAction).Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  Run();
}

TEST_F(ExternalActionTest, ExternalActionWithInterrupts) {
  proto_.mutable_info();
  proto_.set_allow_interrupt(true);

  EXPECT_CALL(mock_action_delegate_, RequestExternalAction)
      .WillOnce(
          [](const ExternalActionProto& external_action,
             base::OnceCallback<void(ExternalActionDelegate::DomUpdateCallback)>
                 start_dom_checks_callback,
             base::OnceCallback<void(const external::Result& result)>
                 end_action_callback) {
            std::move(start_dom_checks_callback).Run(base::DoNothing());
            std::move(end_action_callback).Run(MakeResult(/* success= */ true));
          });
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
  // The action should end at the next WaitForDom notification.
  task_env_.FastForwardBy(base::Seconds(1));
}

TEST_F(ExternalActionTest, ExternalActionWithoutInterrupts) {
  proto_.mutable_info();
  proto_.set_allow_interrupt(false);

  EXPECT_CALL(mock_action_delegate_, RequestExternalAction)
      .WillOnce(
          [](const ExternalActionProto& external_action,
             base::OnceCallback<void(ExternalActionDelegate::DomUpdateCallback)>
                 start_dom_checks_callback,
             base::OnceCallback<void(const external::Result& result)>
                 end_action_callback) {
            std::move(start_dom_checks_callback).Run(base::DoNothing());
            std::move(end_action_callback).Run(MakeResult(/* success= */ true));
          });
  EXPECT_CALL(mock_action_delegate_, WaitForDom).Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(ExternalActionTest, DoesNotStartWaitForDomIfDomChecksAreNotRequested) {
  proto_.mutable_info();
  proto_.set_allow_interrupt(true);

  EXPECT_CALL(mock_action_delegate_, RequestExternalAction)
      .WillOnce(
          [](const ExternalActionProto& external_action,
             base::OnceCallback<void(ExternalActionDelegate::DomUpdateCallback)>
                 start_dom_checks_callback,
             base::OnceCallback<void(const external::Result& result)>
                 end_action_callback) {
            // We call the |end_action_callback| without calling
            // |start_dom_checks_callback|.
            std::move(end_action_callback).Run(MakeResult(/* success= */ true));
          });
  EXPECT_CALL(mock_action_delegate_, WaitForDom).Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(ExternalActionTest, ExternalActionWithDomChecks) {
  proto_.mutable_info();
  ExternalActionProto::ExternalCondition condition;
  condition.set_id(55);
  *condition.mutable_element_condition()->mutable_match() =
      ToSelectorProto("element");
  *proto_.add_conditions() = condition;

  base::MockCallback<ExternalActionDelegate::DomUpdateCallback>
      dom_update_callback;

  EXPECT_CALL(mock_action_delegate_, RequestExternalAction)
      .WillOnce([&dom_update_callback](
                    const ExternalActionProto& external_action,
                    base::OnceCallback<void(
                        ExternalActionDelegate::DomUpdateCallback)>
                        start_dom_checks_callback,
                    base::OnceCallback<void(const external::Result& result)>
                        end_action_callback) {
        std::move(start_dom_checks_callback).Run(dom_update_callback.Get());
        std::move(end_action_callback).Run(MakeResult(/* success= */ true));
      });

  EXPECT_CALL(
      dom_update_callback,
      Run(Property(
          &external::ElementConditionsUpdate::results,
          ElementsAre(AllOf(
              Property(&external::ElementConditionsUpdate::ConditionResult::id,
                       55),
              Property(&external::ElementConditionsUpdate::ConditionResult::
                           satisfied,
                       false))))));
  Run();

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  // The action should end at the next WaitForDom notification.
  task_env_.FastForwardBy(base::Seconds(1));
}

TEST_F(ExternalActionTest, DomChecksOnlyUpdateOnChange) {
  proto_.mutable_info();
  ExternalActionProto::ExternalCondition changing_condition;
  changing_condition.set_id(55);
  *changing_condition.mutable_element_condition()->mutable_match() =
      ToSelectorProto("changing_condition");
  ExternalActionProto::ExternalCondition unchanging_condition;
  unchanging_condition.set_id(9);
  *unchanging_condition.mutable_element_condition()->mutable_match() =
      ToSelectorProto("unchanging_condition");
  *proto_.add_conditions() = changing_condition;
  *proto_.add_conditions() = unchanging_condition;

  base::MockCallback<ExternalActionDelegate::DomUpdateCallback>
      dom_update_callback;

  EXPECT_CALL(mock_action_delegate_, RequestExternalAction)
      .WillOnce([&dom_update_callback](
                    const ExternalActionProto& external_action,
                    base::OnceCallback<void(
                        ExternalActionDelegate::DomUpdateCallback)>
                        start_dom_checks_callback,
                    base::OnceCallback<void(const external::Result& result)>
                        end_action_callback) {
        std::move(start_dom_checks_callback).Run(dom_update_callback.Get());
      });

  // For the first rounds of checks, all elements should be in the notification.
  // Note that the |mock_web_controller_| reports an element as missing by
  // default in the fixture.
  EXPECT_CALL(
      dom_update_callback,
      Run(Property(
          &external::ElementConditionsUpdate::results,
          UnorderedElementsAre(
              AllOf(Property(
                        &external::ElementConditionsUpdate::ConditionResult::id,
                        55),
                    Property(&external::ElementConditionsUpdate::
                                 ConditionResult::satisfied,
                             false)),
              AllOf(Property(
                        &external::ElementConditionsUpdate::ConditionResult::id,
                        9),
                    Property(&external::ElementConditionsUpdate::
                                 ConditionResult::satisfied,
                             false))))));

  Run();

  // For the second rounds of checks, we simulate the |changing_condition|
  // changing to being satisfied and |unchanging_condition| remaining
  // unsatisfied.
  EXPECT_CALL(mock_web_controller_,
              FindElement(Selector({"changing_condition"}), _, _))
      .WillOnce(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinderResult>());
      }));
  EXPECT_CALL(mock_web_controller_,
              FindElement(Selector({"unchanging_condition"}), _, _))
      .WillOnce(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(ClientStatus(ELEMENT_RESOLUTION_FAILED),
                                std::make_unique<ElementFinderResult>());
      }));

  // The notification should now only contain an entry for |changed_condition|.
  EXPECT_CALL(
      dom_update_callback,
      Run(Property(
          &external::ElementConditionsUpdate::results,
          UnorderedElementsAre(AllOf(
              Property(&external::ElementConditionsUpdate::ConditionResult::id,
                       55),
              Property(&external::ElementConditionsUpdate::ConditionResult::
                           satisfied,
                       true))))));
  task_env_.FastForwardBy(base::Seconds(1));

  // We keep the same state as the last roundtrip.
  EXPECT_CALL(mock_web_controller_,
              FindElement(Selector({"changing_condition"}), _, _))
      .WillOnce(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinderResult>());
      }));
  EXPECT_CALL(mock_web_controller_,
              FindElement(Selector({"unchanging_condition"}), _, _))
      .WillOnce(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(ClientStatus(ELEMENT_RESOLUTION_FAILED),
                                std::make_unique<ElementFinderResult>());
      }));
  // Since there were no changes, no notification is sent.
  EXPECT_CALL(dom_update_callback, Run(_)).Times(0);
  task_env_.FastForwardBy(base::Seconds(1));
}

TEST_F(ExternalActionTest, WaitForDomFailure) {
  proto_.mutable_info();
  proto_.set_allow_interrupt(true);

  EXPECT_CALL(mock_action_delegate_, RequestExternalAction)
      .WillOnce(
          [](const ExternalActionProto& external_action,
             base::OnceCallback<void(ExternalActionDelegate::DomUpdateCallback)>
                 start_dom_checks_callback,
             base::OnceCallback<void(const external::Result& result)>
                 end_action_callback) {
            std::move(start_dom_checks_callback).Run(base::DoNothing());
            std::move(end_action_callback).Run(MakeResult(/* success= */ true));
          });

  // Even if the external action ended in a success, if the WaitForDom ends in
  // an error we expect the error to be reported.
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INTERRUPT_FAILED))));
  Run();
  wait_for_dom_status_ = ClientStatus(INTERRUPT_FAILED);
  // The action should end at the next WaitForDom notification.
  task_env_.FastForwardBy(base::Seconds(1));
}

}  // namespace
}  // namespace autofill_assistant
