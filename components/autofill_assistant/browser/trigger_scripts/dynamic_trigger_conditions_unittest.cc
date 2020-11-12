// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/trigger_scripts/trigger_script_coordinator.h"

#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_callback.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

class DynamicTriggerConditionsTest : public testing::Test {
 public:
  DynamicTriggerConditionsTest() = default;
  ~DynamicTriggerConditionsTest() override = default;

 protected:
  std::set<Selector>* GetSelectorsForTest() {
    return &dynamic_trigger_conditions_.selectors_;
  }

  base::MockCallback<base::OnceCallback<void(void)>> mock_callback_;
  DynamicTriggerConditions dynamic_trigger_conditions_;
  NiceMock<MockWebController> mock_web_controller_;
};

TEST_F(DynamicTriggerConditionsTest, UpdateWithoutSelectorsDoesNothing) {
  EXPECT_CALL(mock_web_controller_, OnFindElement).Times(0);
  EXPECT_CALL(mock_callback_, Run).Times(1);
  dynamic_trigger_conditions_.Update(&mock_web_controller_,
                                     mock_callback_.Get());
}

TEST_F(DynamicTriggerConditionsTest, LookupInvalidSelectorsFails) {
  EXPECT_EQ(dynamic_trigger_conditions_.GetSelectorMatches(
                Selector(ToSelectorProto("not_evaluated"))),
            base::nullopt);
}

TEST_F(DynamicTriggerConditionsTest, AddSelectorsFromTriggerScript) {
  TriggerScriptProto proto_1;
  auto* all_of = proto_1.mutable_trigger_condition()->mutable_all_of();
  *all_of->add_conditions()->mutable_selector() = ToSelectorProto("a");
  auto* any_of = all_of->add_conditions()->mutable_any_of();
  *any_of->add_conditions()->mutable_selector() = ToSelectorProto("b");
  auto* none_of = any_of->add_conditions()->mutable_none_of();
  *none_of->add_conditions()->mutable_selector() = ToSelectorProto("c");

  TriggerScriptProto proto_2;
  *proto_2.mutable_trigger_condition()->mutable_selector() =
      ToSelectorProto("d");

  TriggerScriptProto proto_3;
  all_of = proto_3.mutable_trigger_condition()->mutable_all_of();
  *all_of->add_conditions()->mutable_selector() = ToSelectorProto("a");
  *all_of->add_conditions()->mutable_selector() = ToSelectorProto("e");
  *all_of->add_conditions()->mutable_selector() = ToSelectorProto("f");

  dynamic_trigger_conditions_.AddSelectorsFromTriggerScript(proto_1);
  dynamic_trigger_conditions_.AddSelectorsFromTriggerScript(proto_2);
  dynamic_trigger_conditions_.AddSelectorsFromTriggerScript(proto_3);
  EXPECT_THAT(
      *GetSelectorsForTest(),
      UnorderedElementsAre(
          Selector(ToSelectorProto("a")), Selector(ToSelectorProto("b")),
          Selector(ToSelectorProto("c")), Selector(ToSelectorProto("d")),
          Selector(ToSelectorProto("e")), Selector(ToSelectorProto("f"))));
}

TEST_F(DynamicTriggerConditionsTest, Update) {
  TriggerScriptProto proto;
  auto* all_of = proto.mutable_trigger_condition()->mutable_all_of();
  *all_of->add_conditions()->mutable_selector() = ToSelectorProto("a");
  *all_of->add_conditions()->mutable_selector() = ToSelectorProto("b");
  *all_of->add_conditions()->mutable_selector() = ToSelectorProto("c");

  EXPECT_CALL(mock_web_controller_,
              OnFindElement(Selector(ToSelectorProto("a")), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), nullptr));
  EXPECT_CALL(mock_web_controller_,
              OnFindElement(Selector(ToSelectorProto("b")), _))
      .WillOnce(
          RunOnceCallback<1>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));
  EXPECT_CALL(mock_web_controller_,
              OnFindElement(Selector(ToSelectorProto("c")), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), nullptr));

  EXPECT_CALL(mock_callback_, Run).Times(1);
  dynamic_trigger_conditions_.AddSelectorsFromTriggerScript(proto);
  dynamic_trigger_conditions_.Update(&mock_web_controller_,
                                     mock_callback_.Get());

  EXPECT_EQ(dynamic_trigger_conditions_.GetSelectorMatches(
                Selector(ToSelectorProto("a"))),
            base::make_optional(true));
  EXPECT_EQ(dynamic_trigger_conditions_.GetSelectorMatches(
                Selector(ToSelectorProto("b"))),
            base::make_optional(false));
  EXPECT_EQ(dynamic_trigger_conditions_.GetSelectorMatches(
                Selector(ToSelectorProto("c"))),
            base::make_optional(true));
}

TEST_F(DynamicTriggerConditionsTest, ClearSelectors) {
  TriggerScriptProto proto;
  *proto.mutable_trigger_condition()->mutable_selector() = ToSelectorProto("a");
  dynamic_trigger_conditions_.AddSelectorsFromTriggerScript(proto);
  EXPECT_EQ(GetSelectorsForTest()->size(), 1u);
  dynamic_trigger_conditions_.ClearSelectors();
  EXPECT_EQ(GetSelectorsForTest()->size(), 0u);
}

TEST_F(DynamicTriggerConditionsTest, HasResults) {
  // Since no selectors were added to the evaluation, the result is valid.
  EXPECT_TRUE(dynamic_trigger_conditions_.HasResults());

  TriggerScriptProto proto;
  *proto.mutable_trigger_condition()->mutable_selector() = ToSelectorProto("a");
  dynamic_trigger_conditions_.AddSelectorsFromTriggerScript(proto);
  EXPECT_FALSE(dynamic_trigger_conditions_.HasResults());

  EXPECT_CALL(mock_web_controller_,
              OnFindElement(Selector(ToSelectorProto("a")), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), nullptr));
  dynamic_trigger_conditions_.Update(&mock_web_controller_,
                                     mock_callback_.Get());
  EXPECT_TRUE(dynamic_trigger_conditions_.HasResults());

  EXPECT_CALL(mock_web_controller_,
              OnFindElement(Selector(ToSelectorProto("a")), _))
      .WillOnce(
          [&](const Selector& selector, ElementFinder::Callback& callback) {
            // While Update is running, GetSelectorMatches should return the
            // previous results.
            EXPECT_EQ(dynamic_trigger_conditions_.GetSelectorMatches(
                          Selector(ToSelectorProto("a"))),
                      base::make_optional(true));
            std::move(callback).Run(ClientStatus(ELEMENT_RESOLUTION_FAILED),
                                    nullptr);
          });
  dynamic_trigger_conditions_.Update(&mock_web_controller_,
                                     mock_callback_.Get());

  // After the update, the new result is returned.
  EXPECT_EQ(dynamic_trigger_conditions_.GetSelectorMatches(
                Selector(ToSelectorProto("a"))),
            base::make_optional(false));
}

}  // namespace autofill_assistant
