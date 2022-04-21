// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/perform_on_single_element_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/action_value.pb.h"
#include "components/autofill_assistant/browser/actions/action_test_utils.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/dom_action.pb.h"
#include "components/autofill_assistant/browser/web/element_finder_result.h"
#include "components/autofill_assistant/browser/web/element_store.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::SaveArgPointee;

const char kClientId[] = "1";

class PerformOnSingleElementActionTest : public testing::Test {
 public:
  PerformOnSingleElementActionTest() {}

  void SetUp() override { client_id_.set_identifier(kClientId); }

 protected:
  MockActionDelegate mock_action_delegate_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  base::MockCallback<PerformOnSingleElementAction::PerformAction> perform_;
  base::MockCallback<PerformOnSingleElementAction::PerformTimedAction>
      perform_timed_;
  UserData user_data_;
  ActionProto action_proto_;
  ClientIdProto client_id_;
};

TEST_F(PerformOnSingleElementActionTest, EmptyClientIdFails) {
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  EXPECT_CALL(perform_, Run).Times(0);

  ClientIdProto client_id;
  auto action = PerformOnSingleElementAction::WithClientId(
      &mock_action_delegate_, action_proto_, client_id, perform_.Get());
  action->ProcessAction(callback_.Get());
}

TEST_F(PerformOnSingleElementActionTest, OptionalEmptyClientIdDoesNotFail) {
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  EXPECT_CALL(perform_, Run).WillOnce(RunOnceCallback<1>(OkClientStatus()));

  ClientIdProto client_id;
  auto action = PerformOnSingleElementAction::WithOptionalClientId(
      &mock_action_delegate_, action_proto_, client_id, perform_.Get());
  action->ProcessAction(callback_.Get());
}

TEST_F(PerformOnSingleElementActionTest,
       OptionalEmptyClientIdDoesNotFailForTimed) {
  ProcessedActionProto capture;
  EXPECT_CALL(callback_, Run).WillOnce(testing::SaveArgPointee<0>(&capture));
  EXPECT_CALL(perform_timed_, Run)
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), base::Seconds(1)));

  ClientIdProto client_id;
  auto action = PerformOnSingleElementAction::WithOptionalClientIdTimed(
      &mock_action_delegate_, action_proto_, client_id, perform_timed_.Get());
  action->ProcessAction(callback_.Get());

  EXPECT_EQ(ACTION_APPLIED, capture.status());
  EXPECT_EQ(1000, capture.timing_stats().wait_time_ms());
}

TEST_F(PerformOnSingleElementActionTest, FailsIfElementDoesNotExist) {
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              CLIENT_ID_RESOLUTION_FAILED))));
  EXPECT_CALL(perform_, Run).Times(0);

  auto action = PerformOnSingleElementAction::WithClientId(
      &mock_action_delegate_, action_proto_, client_id_, perform_.Get());
  action->ProcessAction(callback_.Get());
}

TEST_F(PerformOnSingleElementActionTest, PerformsAndEnds) {
  ElementFinderResult element;
  element.SetObjectId("id");
  mock_action_delegate_.GetElementStore()->AddElement(kClientId,
                                                      element.dom_object());

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  EXPECT_CALL(perform_, Run(EqualsElement(element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus()));

  auto action = PerformOnSingleElementAction::WithClientId(
      &mock_action_delegate_, action_proto_, client_id_, perform_.Get());
  action->ProcessAction(callback_.Get());
}

TEST_F(PerformOnSingleElementActionTest, PerformsTimedAndEnds) {
  ElementFinderResult element;
  element.SetObjectId("id");
  mock_action_delegate_.GetElementStore()->AddElement(kClientId,
                                                      element.dom_object());

  ProcessedActionProto capture;
  EXPECT_CALL(callback_, Run).WillOnce(testing::SaveArgPointee<0>(&capture));
  EXPECT_CALL(perform_timed_, Run(EqualsElement(element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), base::Seconds(1)));

  auto action = PerformOnSingleElementAction::WithClientIdTimed(
      &mock_action_delegate_, action_proto_, client_id_, perform_timed_.Get());
  action->ProcessAction(callback_.Get());

  EXPECT_EQ(ACTION_APPLIED, capture.status());
  EXPECT_EQ(1000, capture.timing_stats().wait_time_ms());
}

}  // namespace
}  // namespace autofill_assistant
