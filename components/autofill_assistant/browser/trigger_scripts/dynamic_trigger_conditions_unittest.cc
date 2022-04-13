// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/trigger_scripts/trigger_script_coordinator.h"

#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_set.h"
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
  base::flat_set<Selector>* GetSelectorsForTest() {
    return &dynamic_trigger_conditions_.selectors_;
  }
  base::flat_set<Selector>* GetDomReadyStateSelectorsForTest() {
    return &dynamic_trigger_conditions_.dom_ready_state_selectors_;
  }

  base::MockCallback<base::OnceCallback<void(void)>> mock_callback_;
  DynamicTriggerConditions dynamic_trigger_conditions_;
  NiceMock<MockWebController> mock_web_controller_;
};

TEST_F(DynamicTriggerConditionsTest, UpdateWithoutSelectorsDoesNothing) {
  EXPECT_CALL(mock_web_controller_, FindElement).Times(0);
  EXPECT_CALL(mock_callback_, Run).Times(1);
  dynamic_trigger_conditions_.Update(&mock_web_controller_,
                                     mock_callback_.Get());
}

TEST_F(DynamicTriggerConditionsTest, LookupInvalidSelectorsFails) {
  EXPECT_EQ(dynamic_trigger_conditions_.GetSelectorMatches(
                Selector(ToSelectorProto("not_evaluated"))),
            absl::nullopt);
}

TEST_F(DynamicTriggerConditionsTest, AddConditionsFromTriggerScript) {
  TriggerScriptProto proto_1;
  auto* all_of = proto_1.mutable_trigger_condition()->mutable_all_of();
  *all_of->add_conditions()->mutable_selector() = ToSelectorProto("a");
  *all_of->add_conditions()->mutable_document_ready_state()->mutable_frame() =
      ToSelectorProto("frame_1");
  auto* any_of = all_of->add_conditions()->mutable_any_of();
  *any_of->add_conditions()->mutable_selector() = ToSelectorProto("b");
  *any_of->add_conditions()->mutable_document_ready_state()->mutable_frame() =
      ToSelectorProto("frame_2");
  auto* none_of = any_of->add_conditions()->mutable_none_of();
  *none_of->add_conditions()->mutable_selector() = ToSelectorProto("c");
  *none_of->add_conditions()->mutable_document_ready_state()->mutable_frame() =
      ToSelectorProto("frame_3");

  TriggerScriptProto proto_2;
  *proto_2.mutable_trigger_condition()->mutable_selector() =
      ToSelectorProto("d");

  TriggerScriptProto proto_3;
  all_of = proto_3.mutable_trigger_condition()->mutable_all_of();
  *all_of->add_conditions()->mutable_selector() = ToSelectorProto("a");
  *all_of->add_conditions()->mutable_selector() = ToSelectorProto("e");
  *all_of->add_conditions()->mutable_selector() = ToSelectorProto("f");
  *all_of->add_conditions()->mutable_document_ready_state()->mutable_frame() =
      ToSelectorProto("frame_1");

  dynamic_trigger_conditions_.AddConditionsFromTriggerScript(proto_1);
  dynamic_trigger_conditions_.AddConditionsFromTriggerScript(proto_2);
  dynamic_trigger_conditions_.AddConditionsFromTriggerScript(proto_3);
  EXPECT_THAT(
      *GetSelectorsForTest(),
      UnorderedElementsAre(
          Selector(ToSelectorProto("a")), Selector(ToSelectorProto("b")),
          Selector(ToSelectorProto("c")), Selector(ToSelectorProto("d")),
          Selector(ToSelectorProto("e")), Selector(ToSelectorProto("f"))));
  EXPECT_THAT(*GetDomReadyStateSelectorsForTest(),
              UnorderedElementsAre(Selector(ToSelectorProto("frame_1")),
                                   Selector(ToSelectorProto("frame_2")),
                                   Selector(ToSelectorProto("frame_3"))));
}

TEST_F(DynamicTriggerConditionsTest, Update) {
  TriggerScriptProto proto;
  auto* all_of = proto.mutable_trigger_condition()->mutable_all_of();
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

  std::unique_ptr<ElementFinderResult> frame_fake_element =
      std::make_unique<ElementFinderResult>();
  auto* frame_fake_element_ptr = frame_fake_element.get();

  EXPECT_CALL(mock_web_controller_,
              FindElement(Selector(ToSelectorProto("a")), _, _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus(), nullptr));
  EXPECT_CALL(mock_web_controller_,
              FindElement(Selector(ToSelectorProto("b")), _, _))
      .WillOnce(
          RunOnceCallback<2>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));
  EXPECT_CALL(mock_web_controller_,
              FindElement(Selector(ToSelectorProto("c")), _, _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus(), nullptr));
  // The empty selector is invalid and used as a stand-in for the main frame.
  EXPECT_CALL(mock_web_controller_, FindElement(Selector(), _, _)).Times(0);
  EXPECT_CALL(mock_web_controller_,
              FindElement(Selector(ToSelectorProto("frame")), _, _))
      .WillOnce(
          RunOnceCallback<2>(OkClientStatus(), std::move(frame_fake_element)));
  EXPECT_CALL(
      mock_web_controller_,
      GetDocumentReadyState(testing::Address(frame_fake_element_ptr), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(),
                                   DocumentReadyState::DOCUMENT_COMPLETE));
  EXPECT_CALL(mock_web_controller_,
              GetDocumentReadyState(
                  testing::Not(testing::Address(frame_fake_element_ptr)), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(),
                                   DocumentReadyState::DOCUMENT_INTERACTIVE));

  EXPECT_CALL(mock_callback_, Run).Times(1);
  dynamic_trigger_conditions_.AddConditionsFromTriggerScript(proto);
  dynamic_trigger_conditions_.Update(&mock_web_controller_,
                                     mock_callback_.Get());

  EXPECT_EQ(dynamic_trigger_conditions_.GetSelectorMatches(
                Selector(ToSelectorProto("a"))),
            absl::make_optional(true));
  EXPECT_EQ(dynamic_trigger_conditions_.GetSelectorMatches(
                Selector(ToSelectorProto("b"))),
            absl::make_optional(false));
  EXPECT_EQ(dynamic_trigger_conditions_.GetSelectorMatches(
                Selector(ToSelectorProto("c"))),
            absl::make_optional(true));
  EXPECT_EQ(dynamic_trigger_conditions_.GetDocumentReadyState(Selector()),
            DocumentReadyState::DOCUMENT_INTERACTIVE);
  EXPECT_EQ(dynamic_trigger_conditions_.GetDocumentReadyState(
                Selector(ToSelectorProto("frame"))),
            DocumentReadyState::DOCUMENT_COMPLETE);
  EXPECT_EQ(dynamic_trigger_conditions_.GetDocumentReadyState(
                Selector(ToSelectorProto("invalid"))),
            absl::nullopt);
}

TEST_F(DynamicTriggerConditionsTest, ClearConditions) {
  TriggerScriptProto proto;
  auto* all_of = proto.mutable_trigger_condition()->mutable_all_of();
  *all_of->add_conditions()->mutable_selector() = ToSelectorProto("a");
  all_of->add_conditions()
      ->mutable_document_ready_state()
      ->set_min_document_ready_state(DocumentReadyState::DOCUMENT_COMPLETE);
  dynamic_trigger_conditions_.AddConditionsFromTriggerScript(proto);
  EXPECT_EQ(GetSelectorsForTest()->size(), 1u);
  EXPECT_EQ(GetDomReadyStateSelectorsForTest()->size(), 1u);
  dynamic_trigger_conditions_.ClearConditions();
  EXPECT_EQ(GetSelectorsForTest()->size(), 0u);
  EXPECT_EQ(GetDomReadyStateSelectorsForTest()->size(), 0u);
}

TEST_F(DynamicTriggerConditionsTest, HasResults) {
  // Since no selectors were added to the evaluation, the result is valid.
  EXPECT_TRUE(dynamic_trigger_conditions_.HasResults());

  TriggerScriptProto proto;
  *proto.mutable_trigger_condition()->mutable_selector() = ToSelectorProto("a");
  dynamic_trigger_conditions_.AddConditionsFromTriggerScript(proto);
  EXPECT_FALSE(dynamic_trigger_conditions_.HasResults());

  EXPECT_CALL(mock_web_controller_,
              FindElement(Selector(ToSelectorProto("a")), _, _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus(), nullptr));
  dynamic_trigger_conditions_.Update(&mock_web_controller_,
                                     mock_callback_.Get());
  EXPECT_TRUE(dynamic_trigger_conditions_.HasResults());

  EXPECT_CALL(mock_web_controller_,
              FindElement(Selector(ToSelectorProto("a")), _, _))
      .WillOnce([&](const Selector& selector, bool strict,
                    ElementFinder::Callback callback) {
        // While Update is running, GetSelectorMatches should return the
        // previous results.
        EXPECT_EQ(dynamic_trigger_conditions_.GetSelectorMatches(
                      Selector(ToSelectorProto("a"))),
                  absl::make_optional(true));
        std::move(callback).Run(ClientStatus(ELEMENT_RESOLUTION_FAILED),
                                nullptr);
      });
  dynamic_trigger_conditions_.Update(&mock_web_controller_,
                                     mock_callback_.Get());

  // After the update, the new result is returned.
  EXPECT_EQ(dynamic_trigger_conditions_.GetSelectorMatches(
                Selector(ToSelectorProto("a"))),
            absl::make_optional(false));
}

TEST_F(DynamicTriggerConditionsTest, GetPathPatternMatches) {
  dynamic_trigger_conditions_.SetURL(GURL("http://example.com/match?q=m#a"));
  EXPECT_TRUE(dynamic_trigger_conditions_.GetPathPatternMatches("/match.*"));
  EXPECT_TRUE(
      dynamic_trigger_conditions_.GetPathPatternMatches("/match.*m.*a"));
  EXPECT_FALSE(dynamic_trigger_conditions_.GetPathPatternMatches("/match1"));
  EXPECT_FALSE(dynamic_trigger_conditions_.GetPathPatternMatches("match"));
  EXPECT_FALSE(dynamic_trigger_conditions_.GetPathPatternMatches(""));
}

TEST_F(DynamicTriggerConditionsTest, GetDomainAndSchemeMatches) {
  dynamic_trigger_conditions_.SetURL(GURL("http://example.com/match"));
  EXPECT_TRUE(dynamic_trigger_conditions_.GetDomainAndSchemeMatches(
      GURL("http://example.com")));

  dynamic_trigger_conditions_.SetURL(GURL("http://example.com/match?q=m#a"));
  EXPECT_TRUE(dynamic_trigger_conditions_.GetDomainAndSchemeMatches(
      GURL("http://example.com")));

  dynamic_trigger_conditions_.SetURL(GURL("http://example.com:8080"));
  EXPECT_TRUE(dynamic_trigger_conditions_.GetDomainAndSchemeMatches(
      GURL("http://example.com")));

  dynamic_trigger_conditions_.SetURL(GURL("http://example.com"));
  EXPECT_FALSE(dynamic_trigger_conditions_.GetDomainAndSchemeMatches(
      GURL("http://nomatch.com")));

  dynamic_trigger_conditions_.SetURL(GURL("http://example.com"));
  EXPECT_FALSE(dynamic_trigger_conditions_.GetDomainAndSchemeMatches(
      GURL("https://example.com")));
}

}  // namespace autofill_assistant
