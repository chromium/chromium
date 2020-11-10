// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/service/lite_service_util.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/test_util.h"

#include <utility>

#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace lite_service_util {
namespace {

using ::testing::Eq;

TEST(LiteServiceUtilTest, SplitActionsAtLastBrowse) {
  ActionsResponseProto expected_first_part;
  expected_first_part.add_actions()->mutable_tell();
  expected_first_part.add_actions()->mutable_prompt()->set_browse_mode(true);
  expected_first_part.add_actions()->mutable_tell();
  expected_first_part.add_actions()->mutable_prompt()->set_browse_mode(true);

  ActionsResponseProto expected_second_part;
  expected_second_part.add_actions()->mutable_tell();
  expected_second_part.add_actions()->mutable_prompt()->set_browse_mode(false);
  expected_second_part.add_actions()->mutable_tell();
  expected_second_part.add_actions()->mutable_prompt()->set_browse_mode(false);

  ActionsResponseProto merged;
  for (const auto& action : expected_first_part.actions()) {
    *merged.add_actions() = action;
  }
  for (const auto& action : expected_second_part.actions()) {
    *merged.add_actions() = action;
  }

  EXPECT_THAT(SplitActionsAtLastBrowse(merged),
              Eq(std::make_pair(expected_first_part, expected_second_part)));
}

TEST(LiteServiceUtilTest, SplitActionsAtLastBrowseMinimumPossibleSplit) {
  ActionsResponseProto expected_first_part;
  expected_first_part.add_actions()->mutable_prompt()->set_browse_mode(true);

  ActionsResponseProto expected_second_part;
  expected_second_part.add_actions()->mutable_prompt()->set_browse_mode(false);

  ActionsResponseProto merged;
  for (const auto& action : expected_first_part.actions()) {
    *merged.add_actions() = action;
  }
  for (const auto& action : expected_second_part.actions()) {
    *merged.add_actions() = action;
  }

  EXPECT_THAT(SplitActionsAtLastBrowse(merged),
              Eq(std::make_pair(expected_first_part, expected_second_part)));
}

TEST(LiteServiceUtilTest, SplitActionsAtLastBrowseFailsIfNoBrowse) {
  ActionsResponseProto proto;
  proto.add_actions()->mutable_tell();
  proto.add_actions()->mutable_prompt()->set_browse_mode(false);
  proto.add_actions()->mutable_tell();
  proto.add_actions()->mutable_prompt()->set_browse_mode(false);

  EXPECT_THAT(SplitActionsAtLastBrowse(proto), Eq(base::nullopt));
}

TEST(LiteServiceUtilTest, SplitActionsAtLastBrowseFailsIfBrowseIsLastStep) {
  ActionsResponseProto proto;
  proto.add_actions()->mutable_tell();
  proto.add_actions()->mutable_prompt()->set_browse_mode(false);
  proto.add_actions()->mutable_tell();
  proto.add_actions()->mutable_prompt()->set_browse_mode(true);

  EXPECT_THAT(SplitActionsAtLastBrowse(proto), Eq(base::nullopt));
}

TEST(LiteServiceUtilTest, SplitActionsAtLastBrowseFailsIfPromptIsNotLastStep) {
  ActionsResponseProto proto;
  proto.add_actions()->mutable_tell();
  proto.add_actions()->mutable_prompt()->set_browse_mode(true);
  proto.add_actions()->mutable_prompt()->set_browse_mode(false);
  proto.add_actions()->mutable_tell();

  EXPECT_THAT(SplitActionsAtLastBrowse(proto), Eq(base::nullopt));
}

TEST(LiteServiceUtilTest, ContainsOnlySafeActions) {
  ActionsResponseProto safe_actions;
  safe_actions.add_actions()->mutable_tell();
  safe_actions.add_actions()->mutable_prompt();
  safe_actions.add_actions()->mutable_wait_for_dom();
  safe_actions.add_actions()->mutable_show_progress_bar();
  safe_actions.add_actions()->mutable_show_details();
  safe_actions.add_actions()->mutable_show_info_box();
  safe_actions.add_actions()->mutable_expect_navigation();
  safe_actions.add_actions()->mutable_wait_for_navigation();
  safe_actions.add_actions()->mutable_configure_bottom_sheet();
  safe_actions.add_actions()->mutable_popup_message();
  safe_actions.add_actions()->mutable_wait_for_document();

  EXPECT_TRUE(ContainsOnlySafeActions(safe_actions));

  ActionsResponseProto unsafe_actions;
  unsafe_actions.add_actions()->mutable_click();
  unsafe_actions.add_actions()->mutable_set_form_value();
  unsafe_actions.add_actions()->mutable_select_option();
  unsafe_actions.add_actions()->mutable_navigate();
  unsafe_actions.add_actions()->mutable_show_cast();
  unsafe_actions.add_actions()->mutable_use_card();
  unsafe_actions.add_actions()->mutable_use_address();
  unsafe_actions.add_actions()->mutable_upload_dom();
  unsafe_actions.add_actions()->mutable_highlight_element();
  unsafe_actions.add_actions()->mutable_stop();
  unsafe_actions.add_actions()->mutable_collect_user_data();
  unsafe_actions.add_actions()->mutable_set_attribute();
  unsafe_actions.add_actions()->mutable_show_form();
  unsafe_actions.add_actions()->mutable_show_generic_ui();
  unsafe_actions.add_actions()->mutable_generate_password_for_form_field();
  unsafe_actions.add_actions()->mutable_save_generated_password();
  unsafe_actions.add_actions()->mutable_presave_generated_password();
  unsafe_actions.add_actions()->mutable_configure_ui_state();
  for (const auto& unsafe_action : unsafe_actions.actions()) {
    ActionsResponseProto test_actions = safe_actions;
    *test_actions.add_actions() = unsafe_action;
    EXPECT_FALSE(ContainsOnlySafeActions(test_actions));
  }
}

TEST(LiteServiceUtilTest, GetActionResponseType) {
  ProcessedActionProto proto;
  proto.set_status(ACTION_APPLIED);
  EXPECT_EQ(GetActionResponseType(proto), ActionResponseType::UNKNOWN);

  proto.mutable_upload_dom_result();
  EXPECT_EQ(GetActionResponseType(proto), ActionResponseType::UNKNOWN);
  proto.mutable_collect_user_data_result();
  EXPECT_EQ(GetActionResponseType(proto), ActionResponseType::UNKNOWN);
  proto.mutable_set_form_field_value_result();
  EXPECT_EQ(GetActionResponseType(proto), ActionResponseType::UNKNOWN);
  proto.mutable_wait_for_dom_result();
  EXPECT_EQ(GetActionResponseType(proto), ActionResponseType::UNKNOWN);
  proto.mutable_form_result();
  EXPECT_EQ(GetActionResponseType(proto), ActionResponseType::UNKNOWN);
  proto.mutable_wait_for_document_result();
  EXPECT_EQ(GetActionResponseType(proto), ActionResponseType::UNKNOWN);
  proto.mutable_show_generic_ui_result();
  EXPECT_EQ(GetActionResponseType(proto), ActionResponseType::UNKNOWN);

  proto.mutable_prompt_choice();
  proto.mutable_action()->mutable_prompt();
  EXPECT_EQ(GetActionResponseType(proto), ActionResponseType::UNKNOWN);
  proto.mutable_prompt_choice()->set_navigation_ended(true);
  EXPECT_EQ(GetActionResponseType(proto), ActionResponseType::PROMPT_NAVIGATE);

  auto* auto_select_choice =
      proto.mutable_action()->mutable_prompt()->add_choices();
  auto_select_choice->mutable_auto_select_when();
  auto_select_choice->set_server_payload("auto_select_choice");

  auto* close_choice = proto.mutable_action()->mutable_prompt()->add_choices();
  close_choice->mutable_chip()->set_type(CLOSE_ACTION);
  close_choice->set_server_payload("close_choice");

  auto* done_choice = proto.mutable_action()->mutable_prompt()->add_choices();
  done_choice->mutable_chip()->set_type(DONE_ACTION);
  done_choice->set_server_payload("done_choice");

  auto* highlighted_choice =
      proto.mutable_action()->mutable_prompt()->add_choices();
  highlighted_choice->mutable_chip()->set_type(HIGHLIGHTED_ACTION);
  highlighted_choice->set_server_payload("highlighted_choice");

  auto* normal_choice = proto.mutable_action()->mutable_prompt()->add_choices();
  normal_choice->mutable_chip()->set_type(NORMAL_ACTION);
  normal_choice->set_server_payload("normal_choice");

  auto* cancel_choice = proto.mutable_action()->mutable_prompt()->add_choices();
  cancel_choice->mutable_chip()->set_type(CANCEL_ACTION);
  cancel_choice->set_server_payload("cancel_choice");

  proto.mutable_prompt_choice()->clear_navigation_ended();
  proto.mutable_prompt_choice()->set_server_payload("different");
  EXPECT_EQ(GetActionResponseType(proto), ActionResponseType::UNKNOWN);
  proto.mutable_prompt_choice()->set_server_payload("highlighted_choice");
  EXPECT_EQ(GetActionResponseType(proto), ActionResponseType::UNKNOWN);
  proto.mutable_prompt_choice()->set_server_payload("normal_choice");
  EXPECT_EQ(GetActionResponseType(proto), ActionResponseType::UNKNOWN);
  proto.mutable_prompt_choice()->set_server_payload("cancel_choice");
  EXPECT_EQ(GetActionResponseType(proto), ActionResponseType::UNKNOWN);

  proto.mutable_prompt_choice()->set_server_payload("auto_select_choice");
  EXPECT_EQ(GetActionResponseType(proto),
            ActionResponseType::PROMPT_INVISIBLE_AUTO_SELECT);
  proto.mutable_prompt_choice()->set_server_payload("close_choice");
  EXPECT_EQ(GetActionResponseType(proto), ActionResponseType::PROMPT_CLOSE);
  proto.mutable_prompt_choice()->set_server_payload("done_choice");
  EXPECT_EQ(GetActionResponseType(proto), ActionResponseType::PROMPT_DONE);
}

TEST(LiteServiceUtilTest, AssignUniquePayloadsToPrompts) {
  ActionsResponseProto proto;
  auto* prompt_1 = proto.add_actions()->mutable_prompt();
  prompt_1->add_choices();
  prompt_1->add_choices();

  auto* prompt_2 = proto.add_actions()->mutable_prompt();
  prompt_2->add_choices();

  AssignUniquePayloadsToPrompts(&proto);
  EXPECT_FALSE(proto.actions(0).prompt().choices(0).server_payload().empty());
  EXPECT_FALSE(proto.actions(0).prompt().choices(1).server_payload().empty());
  EXPECT_FALSE(proto.actions(1).prompt().choices(0).server_payload().empty());

  std::set<std::string> payloads;
  payloads.insert(proto.actions(0).prompt().choices(0).server_payload());
  payloads.insert(proto.actions(0).prompt().choices(1).server_payload());
  payloads.insert(proto.actions(1).prompt().choices(0).server_payload());
  EXPECT_TRUE(payloads.size() == 3);
}

}  // namespace
}  // namespace lite_service_util
}  // namespace autofill_assistant
