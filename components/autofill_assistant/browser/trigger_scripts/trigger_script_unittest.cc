// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/trigger_scripts/trigger_script.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/trigger_scripts/dynamic_trigger_conditions.h"
#include "components/autofill_assistant/browser/trigger_scripts/mock_dynamic_trigger_conditions.h"
#include "components/autofill_assistant/browser/trigger_scripts/mock_static_trigger_conditions.h"
#include "components/autofill_assistant/browser/trigger_scripts/static_trigger_conditions.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::testing::NiceMock;
using ::testing::Return;

class TriggerScriptTest : public testing::Test {
 public:
  TriggerScriptTest() : trigger_script_(TriggerScriptProto()) {}
  ~TriggerScriptTest() override = default;

  TriggerScriptProto* GetProtoForTest() { return &trigger_script_.proto_; }

 protected:
  TriggerScript trigger_script_;
  NiceMock<MockStaticTriggerConditions> mock_static_trigger_conditions_;
  NiceMock<MockDynamicTriggerConditions> mock_dynamic_trigger_conditions_;
};

TEST_F(TriggerScriptTest, NoTriggerConditionSucceeds) {
  EXPECT_TRUE(trigger_script_.EvaluateTriggerConditions(
      mock_static_trigger_conditions_, mock_dynamic_trigger_conditions_));
}

TEST_F(TriggerScriptTest, EmptyTriggerConditionSucceeds) {
  GetProtoForTest()->mutable_trigger_condition();
  EXPECT_TRUE(trigger_script_.EvaluateTriggerConditions(
      mock_static_trigger_conditions_, mock_dynamic_trigger_conditions_));
}

TEST_F(TriggerScriptTest, AllOfSucceedsIfEmpty) {
  GetProtoForTest()->mutable_trigger_condition()->mutable_all_of();
  EXPECT_TRUE(trigger_script_.EvaluateTriggerConditions(
      mock_static_trigger_conditions_, mock_dynamic_trigger_conditions_));
}

TEST_F(TriggerScriptTest, AllOfFailsIfOnlySomeConditionsAreTrue) {
  auto* all_of =
      GetProtoForTest()->mutable_trigger_condition()->mutable_all_of();
  *all_of->add_conditions()->mutable_selector() = ToSelectorProto("a");
  *all_of->add_conditions()->mutable_selector() = ToSelectorProto("b");
  *all_of->add_conditions()->mutable_selector() = ToSelectorProto("c");

  EXPECT_CALL(mock_dynamic_trigger_conditions_,
              GetSelectorMatches(Selector(ToSelectorProto("a"))))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_dynamic_trigger_conditions_,
              GetSelectorMatches(Selector(ToSelectorProto("b"))))
      .WillOnce(Return(false));
  // "b" already fails, "c" does not need to be evaluated.
  EXPECT_CALL(mock_dynamic_trigger_conditions_,
              GetSelectorMatches(Selector(ToSelectorProto("c"))))
      .Times(0);

  EXPECT_FALSE(trigger_script_.EvaluateTriggerConditions(
      mock_static_trigger_conditions_, mock_dynamic_trigger_conditions_));
}

TEST_F(TriggerScriptTest, AllOfSucceedsIfAllConditionsAreTrue) {
  auto* all_of =
      GetProtoForTest()->mutable_trigger_condition()->mutable_all_of();
  *all_of->add_conditions()->mutable_selector() = ToSelectorProto("a");
  *all_of->add_conditions()->mutable_selector() = ToSelectorProto("b");
  *all_of->add_conditions()->mutable_selector() = ToSelectorProto("c");
  auto* main_dom_ready_state_condition =
      all_of->add_conditions()->mutable_document_ready_state();
  main_dom_ready_state_condition->set_min_document_ready_state(
      DocumentReadyState::DOCUMENT_INTERACTIVE);
  auto* frame_dom_ready_state_condition =
      all_of->add_conditions()->mutable_document_ready_state();
  frame_dom_ready_state_condition->set_min_document_ready_state(
      DocumentReadyState::DOCUMENT_COMPLETE);
  *frame_dom_ready_state_condition->mutable_frame() = ToSelectorProto("frame");

  EXPECT_CALL(mock_dynamic_trigger_conditions_,
              GetSelectorMatches(Selector(ToSelectorProto("a"))))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_dynamic_trigger_conditions_,
              GetSelectorMatches(Selector(ToSelectorProto("b"))))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_dynamic_trigger_conditions_,
              GetSelectorMatches(Selector(ToSelectorProto("c"))))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_dynamic_trigger_conditions_,
              GetDocumentReadyState(Selector()))
      .WillOnce(Return(DocumentReadyState::DOCUMENT_COMPLETE));
  EXPECT_CALL(mock_dynamic_trigger_conditions_,
              GetDocumentReadyState(Selector(ToSelectorProto("frame"))))
      .WillOnce(Return(DocumentReadyState::DOCUMENT_COMPLETE));

  EXPECT_TRUE(trigger_script_.EvaluateTriggerConditions(
      mock_static_trigger_conditions_, mock_dynamic_trigger_conditions_));
}

TEST_F(TriggerScriptTest, AnyOfFailsIfEmpty) {
  GetProtoForTest()->mutable_trigger_condition()->mutable_any_of();
  EXPECT_FALSE(trigger_script_.EvaluateTriggerConditions(
      mock_static_trigger_conditions_, mock_dynamic_trigger_conditions_));
}

TEST_F(TriggerScriptTest, AnyOfSucceedsIfSomeConditionsAreTrue) {
  auto* any_of =
      GetProtoForTest()->mutable_trigger_condition()->mutable_any_of();
  *any_of->add_conditions()->mutable_selector() = ToSelectorProto("a");
  *any_of->add_conditions()->mutable_selector() = ToSelectorProto("b");
  *any_of->add_conditions()->mutable_selector() = ToSelectorProto("c");

  EXPECT_CALL(mock_dynamic_trigger_conditions_,
              GetSelectorMatches(Selector(ToSelectorProto("a"))))
      .WillOnce(Return(false));
  EXPECT_CALL(mock_dynamic_trigger_conditions_,
              GetSelectorMatches(Selector(ToSelectorProto("b"))))
      .WillOnce(Return(true));
  // "b" already succeeds, "c" does not need to be evaluated.
  EXPECT_CALL(mock_dynamic_trigger_conditions_,
              GetSelectorMatches(Selector(ToSelectorProto("c"))))
      .Times(0);

  EXPECT_TRUE(trigger_script_.EvaluateTriggerConditions(
      mock_static_trigger_conditions_, mock_dynamic_trigger_conditions_));
}

TEST_F(TriggerScriptTest, AnyOfFailsIfAllConditionsAreFalse) {
  auto* any_of =
      GetProtoForTest()->mutable_trigger_condition()->mutable_any_of();
  *any_of->add_conditions()->mutable_selector() = ToSelectorProto("a");
  *any_of->add_conditions()->mutable_selector() = ToSelectorProto("b");
  *any_of->add_conditions()->mutable_selector() = ToSelectorProto("c");

  EXPECT_CALL(mock_dynamic_trigger_conditions_,
              GetSelectorMatches(Selector(ToSelectorProto("a"))))
      .WillOnce(Return(false));
  EXPECT_CALL(mock_dynamic_trigger_conditions_,
              GetSelectorMatches(Selector(ToSelectorProto("b"))))
      .WillOnce(Return(false));
  EXPECT_CALL(mock_dynamic_trigger_conditions_,
              GetSelectorMatches(Selector(ToSelectorProto("c"))))
      .WillOnce(Return(false));

  EXPECT_FALSE(trigger_script_.EvaluateTriggerConditions(
      mock_static_trigger_conditions_, mock_dynamic_trigger_conditions_));
}

TEST_F(TriggerScriptTest, NoneOfSucceedsIfEmpty) {
  GetProtoForTest()->mutable_trigger_condition()->mutable_none_of();
  EXPECT_TRUE(trigger_script_.EvaluateTriggerConditions(
      mock_static_trigger_conditions_, mock_dynamic_trigger_conditions_));
}

TEST_F(TriggerScriptTest, NoneOfFailsIfSomeConditionsAreTrue) {
  auto* none_of =
      GetProtoForTest()->mutable_trigger_condition()->mutable_none_of();
  *none_of->add_conditions()->mutable_selector() = ToSelectorProto("a");
  *none_of->add_conditions()->mutable_selector() = ToSelectorProto("b");
  *none_of->add_conditions()->mutable_selector() = ToSelectorProto("c");

  EXPECT_CALL(mock_dynamic_trigger_conditions_,
              GetSelectorMatches(Selector(ToSelectorProto("a"))))
      .WillOnce(Return(false));
  EXPECT_CALL(mock_dynamic_trigger_conditions_,
              GetSelectorMatches(Selector(ToSelectorProto("b"))))
      .WillOnce(Return(true));
  // "b" already succeeds, "c" does not need to be evaluated.
  EXPECT_CALL(mock_dynamic_trigger_conditions_,
              GetSelectorMatches(Selector(ToSelectorProto("c"))))
      .Times(0);

  EXPECT_FALSE(trigger_script_.EvaluateTriggerConditions(
      mock_static_trigger_conditions_, mock_dynamic_trigger_conditions_));
}

TEST_F(TriggerScriptTest, NoneOfSucceedsIfAllConditionsAreFalse) {
  auto* none_of =
      GetProtoForTest()->mutable_trigger_condition()->mutable_none_of();
  *none_of->add_conditions()->mutable_selector() = ToSelectorProto("a");
  *none_of->add_conditions()->mutable_selector() = ToSelectorProto("b");
  *none_of->add_conditions()->mutable_selector() = ToSelectorProto("c");

  EXPECT_CALL(mock_dynamic_trigger_conditions_,
              GetSelectorMatches(Selector(ToSelectorProto("a"))))
      .WillOnce(Return(false));
  EXPECT_CALL(mock_dynamic_trigger_conditions_,
              GetSelectorMatches(Selector(ToSelectorProto("b"))))
      .WillOnce(Return(false));
  EXPECT_CALL(mock_dynamic_trigger_conditions_,
              GetSelectorMatches(Selector(ToSelectorProto("c"))))
      .WillOnce(Return(false));

  EXPECT_TRUE(trigger_script_.EvaluateTriggerConditions(
      mock_static_trigger_conditions_, mock_dynamic_trigger_conditions_));
}

TEST_F(TriggerScriptTest, ComplexConditions) {
  // Tests the condition a && (b || !(c)).
  auto* all_of =
      GetProtoForTest()->mutable_trigger_condition()->mutable_all_of();
  *all_of->add_conditions()->mutable_selector() = ToSelectorProto("a");
  auto* any_of = all_of->add_conditions()->mutable_any_of();
  *any_of->add_conditions()->mutable_selector() = ToSelectorProto("b");
  auto* none_of = any_of->add_conditions()->mutable_none_of();
  *none_of->add_conditions()->mutable_selector() = ToSelectorProto("c");

  // a == false, b == *, c == * -> false
  EXPECT_CALL(mock_dynamic_trigger_conditions_,
              GetSelectorMatches(Selector(ToSelectorProto("a"))))
      .WillOnce(Return(false));
  EXPECT_CALL(mock_dynamic_trigger_conditions_,
              GetSelectorMatches(Selector(ToSelectorProto("b"))))
      .Times(0);
  EXPECT_CALL(mock_dynamic_trigger_conditions_,
              GetSelectorMatches(Selector(ToSelectorProto("c"))))
      .Times(0);
  EXPECT_FALSE(trigger_script_.EvaluateTriggerConditions(
      mock_static_trigger_conditions_, mock_dynamic_trigger_conditions_));

  // a == true, b == true, c == * -> true
  EXPECT_CALL(mock_dynamic_trigger_conditions_,
              GetSelectorMatches(Selector(ToSelectorProto("a"))))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_dynamic_trigger_conditions_,
              GetSelectorMatches(Selector(ToSelectorProto("b"))))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_dynamic_trigger_conditions_,
              GetSelectorMatches(Selector(ToSelectorProto("c"))))
      .Times(0);
  EXPECT_TRUE(trigger_script_.EvaluateTriggerConditions(
      mock_static_trigger_conditions_, mock_dynamic_trigger_conditions_));

  // a == true, b == false, c == true -> false
  EXPECT_CALL(mock_dynamic_trigger_conditions_,
              GetSelectorMatches(Selector(ToSelectorProto("a"))))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_dynamic_trigger_conditions_,
              GetSelectorMatches(Selector(ToSelectorProto("b"))))
      .WillOnce(Return(false));
  EXPECT_CALL(mock_dynamic_trigger_conditions_,
              GetSelectorMatches(Selector(ToSelectorProto("c"))))
      .WillOnce(Return(true));
  EXPECT_FALSE(trigger_script_.EvaluateTriggerConditions(
      mock_static_trigger_conditions_, mock_dynamic_trigger_conditions_));

  // a == true, b == false, c == false -> true
  EXPECT_CALL(mock_dynamic_trigger_conditions_,
              GetSelectorMatches(Selector(ToSelectorProto("a"))))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_dynamic_trigger_conditions_,
              GetSelectorMatches(Selector(ToSelectorProto("b"))))
      .WillOnce(Return(false));
  EXPECT_CALL(mock_dynamic_trigger_conditions_,
              GetSelectorMatches(Selector(ToSelectorProto("c"))))
      .WillOnce(Return(false));
  EXPECT_TRUE(trigger_script_.EvaluateTriggerConditions(
      mock_static_trigger_conditions_, mock_dynamic_trigger_conditions_));
}

TEST_F(TriggerScriptTest, StaticTriggerConditions) {
  auto* all_of =
      GetProtoForTest()->mutable_trigger_condition()->mutable_all_of();
  all_of->add_conditions()->mutable_stored_login_credentials();
  all_of->add_conditions()->mutable_is_first_time_user();
  all_of->add_conditions()->set_experiment_id(12);

  EXPECT_CALL(mock_static_trigger_conditions_, is_first_time_user)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_static_trigger_conditions_, has_stored_login_credentials)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_static_trigger_conditions_, is_in_experiment(12))
      .WillOnce(Return(true));
  EXPECT_TRUE(trigger_script_.EvaluateTriggerConditions(
      mock_static_trigger_conditions_, mock_dynamic_trigger_conditions_));
}

}  // namespace autofill_assistant
