// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/generic_ui_replace_placeholders.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::UnorderedElementsAreArray;

TEST(GenericUiReplacePlaceholdersTest, ReplacePlaceholdersInViews) {
  std::map<std::string, std::string> mappings{{"i", "1"}};

  GenericUserInterfaceProto input;
  auto* root_view = input.mutable_root_view();
  root_view->set_identifier("root_${i}");

  auto* text_view =
      root_view->mutable_view_container()->add_views()->mutable_text_view();
  text_view->set_model_identifier("text_${i}");

  auto* text_input_view = root_view->mutable_view_container()
                              ->add_views()
                              ->mutable_text_input_view();
  text_input_view->set_model_identifier("text_input_${i}");

  auto* vertical_expander_view = root_view->mutable_view_container()
                                     ->add_views()
                                     ->mutable_vertical_expander_view();
  vertical_expander_view->mutable_title_view()
      ->mutable_text_view()
      ->set_model_identifier("title_${i}");
  vertical_expander_view->mutable_collapsed_view()
      ->mutable_text_view()
      ->set_model_identifier("collapsed_${i}");
  vertical_expander_view->mutable_expanded_view()
      ->mutable_text_view()
      ->set_model_identifier("expanded_${i}");

  auto* toggle_button_view = root_view->mutable_view_container()
                                 ->add_views()
                                 ->mutable_toggle_button_view();
  toggle_button_view->set_model_identifier("toggle_${i}");
  toggle_button_view->mutable_left_content_view()
      ->mutable_text_view()
      ->set_model_identifier("toggle_left_${i}");
  toggle_button_view->mutable_right_content_view()
      ->mutable_text_view()
      ->set_model_identifier("toggle_right_${i}");

  ReplacePlaceholdersInGenericUi(&input, mappings);

  EXPECT_THAT(root_view->identifier(), "root_1");
  EXPECT_THAT(text_view->model_identifier(), "text_1");
  EXPECT_THAT(text_input_view->model_identifier(), "text_input_1");
  EXPECT_THAT(
      vertical_expander_view->title_view().text_view().model_identifier(),
      "title_1");
  EXPECT_THAT(
      vertical_expander_view->collapsed_view().text_view().model_identifier(),
      "collapsed_1");
  EXPECT_THAT(
      vertical_expander_view->expanded_view().text_view().model_identifier(),
      "expanded_1");
  EXPECT_THAT(toggle_button_view->model_identifier(), "toggle_1");
  EXPECT_THAT(
      toggle_button_view->left_content_view().text_view().model_identifier(),
      "toggle_left_1");
  EXPECT_THAT(
      toggle_button_view->right_content_view().text_view().model_identifier(),
      "toggle_right_1");
}

TEST(GenericUiReplacePlaceholdersTest, ReplacePlaceholdersInEvents) {
  std::map<std::string, std::string> mappings{{"i", "1"}};

  GenericUserInterfaceProto input;
  auto* on_value_changed = input.mutable_interactions()
                               ->add_interactions()
                               ->add_trigger_event()
                               ->mutable_on_value_changed();
  on_value_changed->set_model_identifier("value_${i}");

  auto* on_view_clicked = input.mutable_interactions()
                              ->add_interactions()
                              ->add_trigger_event()
                              ->mutable_on_view_clicked();
  on_view_clicked->set_view_identifier("view_${i}");

  auto* on_view_container_cleared = input.mutable_interactions()
                                        ->add_interactions()
                                        ->add_trigger_event()
                                        ->mutable_on_view_container_cleared();
  on_view_container_cleared->set_view_identifier("view_${i}");

  auto* on_popup_dismissed = input.mutable_interactions()
                                 ->add_interactions()
                                 ->add_trigger_event()
                                 ->mutable_on_popup_dismissed();
  on_popup_dismissed->set_popup_identifier("popup_${i}");

  ReplacePlaceholdersInGenericUi(&input, mappings);
  EXPECT_THAT(on_value_changed->model_identifier(), "value_1");
  EXPECT_THAT(on_view_clicked->view_identifier(), "view_1");
  EXPECT_THAT(on_view_container_cleared->view_identifier(), "view_1");
  EXPECT_THAT(on_popup_dismissed->popup_identifier(), "popup_1");
}

TEST(GenericUiReplacePlaceholdersTest, ReplacePlaceholdersInCallbacks) {
  std::map<std::string, std::string> mappings{{"i", "1"}};

  GenericUserInterfaceProto input;
  auto* callback_with_condition =
      input.mutable_interactions()->add_interactions()->add_callbacks();
  callback_with_condition->set_condition_model_identifier("condition_${i}");

  auto* set_value = input.mutable_interactions()
                        ->add_interactions()
                        ->add_callbacks()
                        ->mutable_set_value();
  set_value->set_model_identifier("output_${i}");
  set_value->mutable_value()->set_model_identifier("input_${i}");

  auto* show_list_popup = input.mutable_interactions()
                              ->add_interactions()
                              ->add_callbacks()
                              ->mutable_show_list_popup();
  show_list_popup->mutable_item_names()->set_model_identifier("names_${i}");
  show_list_popup->mutable_item_types()->set_model_identifier("types_${i}");
  show_list_popup->set_selected_item_indices_model_identifier("indices_${i}");
  show_list_popup->set_selected_item_names_model_identifier("selected_${i}");

  auto* compute_value_result_identifier = input.mutable_interactions()
                                              ->add_interactions()
                                              ->add_callbacks()
                                              ->mutable_compute_value();
  compute_value_result_identifier->set_result_model_identifier("result_${i}");

  auto* boolean_and = input.mutable_interactions()
                          ->add_interactions()
                          ->add_callbacks()
                          ->mutable_compute_value()
                          ->mutable_boolean_and();
  boolean_and->add_values()->set_model_identifier("value_${i}");
  boolean_and->add_values()->set_model_identifier("value_${i}");

  auto* boolean_or = input.mutable_interactions()
                         ->add_interactions()
                         ->add_callbacks()
                         ->mutable_compute_value()
                         ->mutable_boolean_or();
  boolean_or->add_values()->set_model_identifier("value_${i}");
  boolean_or->add_values()->set_model_identifier("value_${i}");

  auto* boolean_not = input.mutable_interactions()
                          ->add_interactions()
                          ->add_callbacks()
                          ->mutable_compute_value()
                          ->mutable_boolean_not();
  boolean_not->mutable_value()->set_model_identifier("value_${i}");

  auto* to_string = input.mutable_interactions()
                        ->add_interactions()
                        ->add_callbacks()
                        ->mutable_compute_value()
                        ->mutable_to_string();
  to_string->mutable_value()->set_model_identifier("value_${i}");

  auto* comparison = input.mutable_interactions()
                         ->add_interactions()
                         ->add_callbacks()
                         ->mutable_compute_value()
                         ->mutable_comparison();
  comparison->mutable_value_a()->set_model_identifier("value_a_${i}");
  comparison->mutable_value_b()->set_model_identifier("value_b_${i}");

  auto* integer_sum = input.mutable_interactions()
                          ->add_interactions()
                          ->add_callbacks()
                          ->mutable_compute_value()
                          ->mutable_integer_sum();
  integer_sum->add_values()->set_model_identifier("value_${i}");
  integer_sum->add_values()->set_model_identifier("value_${i}");

  auto* credit_card_response = input.mutable_interactions()
                                   ->add_interactions()
                                   ->add_callbacks()
                                   ->mutable_compute_value()
                                   ->mutable_create_credit_card_response();
  credit_card_response->mutable_value()->set_model_identifier("value_${i}");

  auto* login_option_response = input.mutable_interactions()
                                    ->add_interactions()
                                    ->add_callbacks()
                                    ->mutable_compute_value()
                                    ->mutable_create_login_option_response();
  login_option_response->mutable_value()->set_model_identifier("value_${i}");

  auto* set_user_actions = input.mutable_interactions()
                               ->add_interactions()
                               ->add_callbacks()
                               ->mutable_set_user_actions();
  set_user_actions->mutable_user_actions()->set_model_identifier(
      "actions_${i}");

  auto* show_calendar_popup = input.mutable_interactions()
                                  ->add_interactions()
                                  ->add_callbacks()
                                  ->mutable_show_calendar_popup();
  show_calendar_popup->set_date_model_identifier("date_${i}");
  show_calendar_popup->mutable_min_date()->set_model_identifier("min_${i}");
  show_calendar_popup->mutable_max_date()->set_model_identifier("max_${i}");

  auto* set_text = input.mutable_interactions()
                       ->add_interactions()
                       ->add_callbacks()
                       ->mutable_set_text();
  set_text->mutable_text()->set_model_identifier("text_${i}");
  set_text->set_view_identifier("view_${i}");

  auto* toggle_user_action = input.mutable_interactions()
                                 ->add_interactions()
                                 ->add_callbacks()
                                 ->mutable_toggle_user_action();
  toggle_user_action->set_user_actions_model_identifier("actions_${i}");
  toggle_user_action->mutable_enabled()->set_model_identifier("enabled_${i}");

  auto* set_view_visibility = input.mutable_interactions()
                                  ->add_interactions()
                                  ->add_callbacks()
                                  ->mutable_set_view_visibility();
  set_view_visibility->set_view_identifier("view_${i}");
  set_view_visibility->mutable_visible()->set_model_identifier("visible_${i}");

  auto* set_view_enabled = input.mutable_interactions()
                               ->add_interactions()
                               ->add_callbacks()
                               ->mutable_set_view_enabled();
  set_view_enabled->set_view_identifier("view_${i}");
  set_view_enabled->mutable_enabled()->set_model_identifier("enabled_${i}");

  auto* show_generic_popup = input.mutable_interactions()
                                 ->add_interactions()
                                 ->add_callbacks()
                                 ->mutable_show_generic_popup();
  show_generic_popup->set_popup_identifier("popup_${i}");
  show_generic_popup->mutable_generic_ui()->mutable_root_view()->set_identifier(
      "nested_root_${i}");

  auto* create_nested_ui = input.mutable_interactions()
                               ->add_interactions()
                               ->add_callbacks()
                               ->mutable_create_nested_ui();
  create_nested_ui->set_generic_ui_identifier("nested_${i}");
  create_nested_ui->set_parent_view_identifier("parent_${i}");
  create_nested_ui->mutable_generic_ui()->mutable_root_view()->set_identifier(
      "nested_root_${i}");

  auto* clear_view_container = input.mutable_interactions()
                                   ->add_interactions()
                                   ->add_callbacks()
                                   ->mutable_clear_view_container();
  clear_view_container->set_view_identifier("view_${i}");

  auto* for_each = input.mutable_interactions()
                       ->add_interactions()
                       ->add_callbacks()
                       ->mutable_for_each();
  for_each->set_loop_value_model_identifier("loop_${i}");
  for_each->add_callbacks()->set_condition_model_identifier("condition_${i}");

  ReplacePlaceholdersInGenericUi(&input, mappings);

  EXPECT_THAT(callback_with_condition->condition_model_identifier(),
              "condition_1");
  EXPECT_THAT(set_value->value().model_identifier(), "input_1");
  EXPECT_THAT(set_value->model_identifier(), "output_1");
  EXPECT_THAT(show_list_popup->item_names().model_identifier(), "names_1");
  EXPECT_THAT(show_list_popup->item_types().model_identifier(), "types_1");
  EXPECT_THAT(show_list_popup->selected_item_indices_model_identifier(),
              "indices_1");
  EXPECT_THAT(show_list_popup->selected_item_names_model_identifier(),
              "selected_1");
  EXPECT_THAT(compute_value_result_identifier->result_model_identifier(),
              "result_1");
  EXPECT_THAT(boolean_and->values(0).model_identifier(), "value_1");
  EXPECT_THAT(boolean_and->values(1).model_identifier(), "value_1");
  EXPECT_THAT(boolean_or->values(0).model_identifier(), "value_1");
  EXPECT_THAT(boolean_or->values(1).model_identifier(), "value_1");
  EXPECT_THAT(boolean_not->value().model_identifier(), "value_1");
  EXPECT_THAT(to_string->value().model_identifier(), "value_1");
  EXPECT_THAT(comparison->value_a().model_identifier(), "value_a_1");
  EXPECT_THAT(comparison->value_b().model_identifier(), "value_b_1");
  EXPECT_THAT(integer_sum->values(0).model_identifier(), "value_1");
  EXPECT_THAT(integer_sum->values(1).model_identifier(), "value_1");
  EXPECT_THAT(credit_card_response->value().model_identifier(), "value_1");
  EXPECT_THAT(login_option_response->value().model_identifier(), "value_1");
  EXPECT_THAT(set_user_actions->user_actions().model_identifier(), "actions_1");
  EXPECT_THAT(show_calendar_popup->date_model_identifier(), "date_1");
  EXPECT_THAT(show_calendar_popup->min_date().model_identifier(), "min_1");
  EXPECT_THAT(show_calendar_popup->max_date().model_identifier(), "max_1");
  EXPECT_THAT(set_text->text().model_identifier(), "text_1");
  EXPECT_THAT(set_text->view_identifier(), "view_1");
  EXPECT_THAT(toggle_user_action->user_actions_model_identifier(), "actions_1");
  EXPECT_THAT(toggle_user_action->enabled().model_identifier(), "enabled_1");
  EXPECT_THAT(set_view_visibility->view_identifier(), "view_1");
  EXPECT_THAT(set_view_visibility->visible().model_identifier(), "visible_1");
  EXPECT_THAT(set_view_enabled->view_identifier(), "view_1");
  EXPECT_THAT(set_view_enabled->enabled().model_identifier(), "enabled_1");
  EXPECT_THAT(show_generic_popup->popup_identifier(), "popup_1");
  EXPECT_THAT(show_generic_popup->generic_ui().root_view().identifier(),
              "nested_root_1");
  EXPECT_THAT(create_nested_ui->generic_ui_identifier(), "nested_1");
  EXPECT_THAT(create_nested_ui->parent_view_identifier(), "parent_1");
  EXPECT_THAT(create_nested_ui->generic_ui().root_view().identifier(),
              "nested_root_1");
  EXPECT_THAT(clear_view_container->view_identifier(), "view_1");
  EXPECT_THAT(for_each->loop_value_model_identifier(), "loop_1");
  EXPECT_THAT(for_each->callbacks(0).condition_model_identifier(),
              "condition_1");
}

TEST(GenericUiReplacePlaceholdersTest, ReplacePlaceholdersInModel) {
  std::map<std::string, std::string> mappings{{"i", "1"}};

  GenericUserInterfaceProto input;
  auto* value_a = input.mutable_model()->add_values();
  value_a->set_identifier("value_${i}");
  auto* value_b = input.mutable_model()->add_values();
  value_b->set_identifier("value_${j}");
  auto* value_c = input.mutable_model()->add_values();
  value_c->set_identifier("value_c");
  input.mutable_model()->add_values();

  ReplacePlaceholdersInGenericUi(&input, mappings);
  EXPECT_THAT(input.model().values(0).identifier(), "value_1");
  EXPECT_THAT(input.model().values(1).identifier(), "value_${j}");
  EXPECT_THAT(input.model().values(2).identifier(), "value_c");
  EXPECT_FALSE(input.model().values(3).has_identifier());
}
}  // namespace
}  // namespace autofill_assistant
