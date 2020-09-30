// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/generic_ui_replace_placeholders.h"
#include "base/logging.h"
#include "components/autofill_assistant/browser/field_formatter.h"

namespace autofill_assistant {
// Forward declaration to allow recursive calls.
void ReplacePlaceholdersInGenericUi(
    GenericUserInterfaceProto* in_out_proto,
    const std::map<std::string, std::string>& placeholders);

namespace {

void ReplaceInPlace(std::string* in_out,
                    const std::map<std::string, std::string>& placeholders) {
  auto formatted_string = field_formatter::FormatString(*in_out, placeholders,
                                                        /*strict = */ false);
  if (!formatted_string.has_value()) {
    VLOG(1) << "lenient placeholder replacement failed";
    return;
  }
  in_out->assign(*formatted_string);
}

void ReplacePlaceholdersInView(
    ViewProto* in_out_proto,
    const std::map<std::string, std::string>& placeholders) {
  if (in_out_proto->has_identifier()) {
    ReplaceInPlace(in_out_proto->mutable_identifier(), placeholders);
  }

  switch (in_out_proto->view_case()) {
    case ViewProto::kViewContainer:
      for (auto& child :
           *in_out_proto->mutable_view_container()->mutable_views()) {
        ReplacePlaceholdersInView(&child, placeholders);
      }
      return;
    case ViewProto::kTextView:
      switch (in_out_proto->text_view().kind_case()) {
        case TextViewProto::kModelIdentifier:
          ReplaceInPlace(
              in_out_proto->mutable_text_view()->mutable_model_identifier(),
              placeholders);
          return;
        case TextViewProto::kText:
        case TextViewProto::KIND_NOT_SET:
          return;
      }
      return;
    case ViewProto::kVerticalExpanderView: {
      if (in_out_proto->vertical_expander_view().has_title_view()) {
        ReplacePlaceholdersInView(in_out_proto->mutable_vertical_expander_view()
                                      ->mutable_title_view(),
                                  placeholders);
      }
      if (in_out_proto->vertical_expander_view().has_collapsed_view()) {
        ReplacePlaceholdersInView(in_out_proto->mutable_vertical_expander_view()
                                      ->mutable_collapsed_view(),
                                  placeholders);
      }
      if (in_out_proto->vertical_expander_view().has_expanded_view()) {
        ReplacePlaceholdersInView(in_out_proto->mutable_vertical_expander_view()
                                      ->mutable_expanded_view(),
                                  placeholders);
      }
      return;
    }
    case ViewProto::kTextInputView: {
      if (in_out_proto->text_input_view().has_model_identifier()) {
        ReplaceInPlace(
            in_out_proto->mutable_text_input_view()->mutable_model_identifier(),
            placeholders);
      }
      return;
    }
    case ViewProto::kToggleButtonView:
      if (in_out_proto->toggle_button_view().has_model_identifier()) {
        ReplaceInPlace(in_out_proto->mutable_toggle_button_view()
                           ->mutable_model_identifier(),
                       placeholders);
      }
      if (in_out_proto->toggle_button_view().has_left_content_view()) {
        ReplacePlaceholdersInView(in_out_proto->mutable_toggle_button_view()
                                      ->mutable_left_content_view(),
                                  placeholders);
      }
      if (in_out_proto->toggle_button_view().has_right_content_view()) {
        ReplacePlaceholdersInView(in_out_proto->mutable_toggle_button_view()
                                      ->mutable_right_content_view(),
                                  placeholders);
      }
      return;
    case ViewProto::kDividerView:
    case ViewProto::kImageView:
    case ViewProto::VIEW_NOT_SET:
      return;
  }
}

void ReplacePlaceholdersInEvent(
    EventProto* in_out_proto,
    const std::map<std::string, std::string>& placeholders) {
  switch (in_out_proto->kind_case()) {
    case EventProto::kOnValueChanged:
      if (in_out_proto->on_value_changed().has_model_identifier()) {
        ReplaceInPlace(in_out_proto->mutable_on_value_changed()
                           ->mutable_model_identifier(),
                       placeholders);
      }
      return;
    case EventProto::kOnViewClicked:
      if (in_out_proto->on_view_clicked().has_view_identifier()) {
        ReplaceInPlace(
            in_out_proto->mutable_on_view_clicked()->mutable_view_identifier(),
            placeholders);
      }
      return;
    case EventProto::kOnViewContainerCleared:
      if (in_out_proto->on_view_container_cleared().has_view_identifier()) {
        ReplaceInPlace(in_out_proto->mutable_on_view_container_cleared()
                           ->mutable_view_identifier(),
                       placeholders);
      }
      return;
    case EventProto::kOnPopupDismissed:
      if (in_out_proto->on_popup_dismissed().has_popup_identifier()) {
        ReplaceInPlace(in_out_proto->mutable_on_popup_dismissed()
                           ->mutable_popup_identifier(),
                       placeholders);
      }
      return;
    case EventProto::kOnUserActionCalled:
    case EventProto::kOnTextLinkClicked:
    case EventProto::KIND_NOT_SET:
      return;
  }
  return;
}

void ReplacePlaceholdersInValue(
    ValueReferenceProto* in_out_proto,
    const std::map<std::string, std::string>& placeholders) {
  switch (in_out_proto->kind_case()) {
    case ValueReferenceProto::kModelIdentifier:
      ReplaceInPlace(in_out_proto->mutable_model_identifier(), placeholders);
      return;
    case ValueReferenceProto::kValue:
    case ValueReferenceProto::KIND_NOT_SET:
      return;
  }
}

void ReplacePlaceholdersInInteraction(
    InteractionProto* in_out_proto,
    const std::map<std::string, std::string>& placeholders) {
  for (auto& trigger_event : *in_out_proto->mutable_trigger_event()) {
    ReplacePlaceholdersInEvent(&trigger_event, placeholders);
  }

  for (auto& callback : *in_out_proto->mutable_callbacks()) {
    ReplacePlaceholdersInCallback(&callback, placeholders);
  }
}

void ReplacePlaceholdersInModel(
    ModelProto* in_out_proto,
    const std::map<std::string, std::string>& placeholders) {
  for (auto& value : *in_out_proto->mutable_values()) {
    if (value.has_identifier()) {
      ReplaceInPlace(value.mutable_identifier(), placeholders);
    }
  }
}

}  // namespace

void ReplacePlaceholdersInGenericUi(
    GenericUserInterfaceProto* in_out_proto,
    const std::map<std::string, std::string>& placeholders) {
  if (placeholders.empty()) {
    return;
  }

  if (in_out_proto->has_root_view()) {
    ReplacePlaceholdersInView(in_out_proto->mutable_root_view(), placeholders);
  }

  for (auto& interaction :
       *in_out_proto->mutable_interactions()->mutable_interactions()) {
    ReplacePlaceholdersInInteraction(&interaction, placeholders);
  }

  if (in_out_proto->has_model()) {
    ReplacePlaceholdersInModel(in_out_proto->mutable_model(), placeholders);
  }
}

void ReplacePlaceholdersInCallback(
    CallbackProto* in_out_proto,
    const std::map<std::string, std::string>& placeholders) {
  if (in_out_proto->has_condition_model_identifier()) {
    ReplaceInPlace(in_out_proto->mutable_condition_model_identifier(),
                   placeholders);
  }

  switch (in_out_proto->kind_case()) {
    case CallbackProto::kSetValue:
      if (in_out_proto->set_value().has_model_identifier()) {
        ReplaceInPlace(
            in_out_proto->mutable_set_value()->mutable_model_identifier(),
            placeholders);
      }
      if (in_out_proto->set_value().has_value()) {
        ReplacePlaceholdersInValue(
            in_out_proto->mutable_set_value()->mutable_value(), placeholders);
      }
      return;
    case CallbackProto::kShowListPopup:
      if (in_out_proto->show_list_popup().has_item_names()) {
        ReplacePlaceholdersInValue(
            in_out_proto->mutable_show_list_popup()->mutable_item_names(),
            placeholders);
      }
      if (in_out_proto->show_list_popup().has_item_types()) {
        ReplacePlaceholdersInValue(
            in_out_proto->mutable_show_list_popup()->mutable_item_types(),
            placeholders);
      }
      if (in_out_proto->show_list_popup()
              .has_selected_item_indices_model_identifier()) {
        ReplaceInPlace(in_out_proto->mutable_show_list_popup()
                           ->mutable_selected_item_indices_model_identifier(),
                       placeholders);
      }
      if (in_out_proto->show_list_popup()
              .has_selected_item_names_model_identifier()) {
        ReplaceInPlace(in_out_proto->mutable_show_list_popup()
                           ->mutable_selected_item_names_model_identifier(),
                       placeholders);
      }
      return;
    case CallbackProto::kComputeValue:
      if (in_out_proto->compute_value().has_result_model_identifier()) {
        ReplaceInPlace(in_out_proto->mutable_compute_value()
                           ->mutable_result_model_identifier(),
                       placeholders);
      }
      switch (in_out_proto->compute_value().kind_case()) {
        case ComputeValueProto::kBooleanAnd:
          for (auto& value : *in_out_proto->mutable_compute_value()
                                  ->mutable_boolean_and()
                                  ->mutable_values()) {
            ReplacePlaceholdersInValue(&value, placeholders);
          }
          return;
        case ComputeValueProto::kBooleanOr:
          for (auto& value : *in_out_proto->mutable_compute_value()
                                  ->mutable_boolean_or()
                                  ->mutable_values()) {
            ReplacePlaceholdersInValue(&value, placeholders);
          }
          return;
        case ComputeValueProto::kBooleanNot:
          if (in_out_proto->compute_value().boolean_not().has_value()) {
            ReplacePlaceholdersInValue(in_out_proto->mutable_compute_value()
                                           ->mutable_boolean_not()
                                           ->mutable_value(),
                                       placeholders);
          }
          return;
        case ComputeValueProto::kToString:
          if (in_out_proto->compute_value().to_string().has_value()) {
            ReplacePlaceholdersInValue(in_out_proto->mutable_compute_value()
                                           ->mutable_to_string()
                                           ->mutable_value(),
                                       placeholders);
          }
          return;
        case ComputeValueProto::kComparison:
          if (in_out_proto->compute_value().comparison().has_value_a()) {
            ReplacePlaceholdersInValue(in_out_proto->mutable_compute_value()
                                           ->mutable_comparison()
                                           ->mutable_value_a(),
                                       placeholders);
          }
          if (in_out_proto->compute_value().comparison().has_value_b()) {
            ReplacePlaceholdersInValue(in_out_proto->mutable_compute_value()
                                           ->mutable_comparison()
                                           ->mutable_value_b(),
                                       placeholders);
          }
          return;
        case ComputeValueProto::kIntegerSum:
          for (auto& value : *in_out_proto->mutable_compute_value()
                                  ->mutable_integer_sum()
                                  ->mutable_values()) {
            ReplacePlaceholdersInValue(&value, placeholders);
          }
          return;
        case ComputeValueProto::kCreateCreditCardResponse:
          if (in_out_proto->compute_value()
                  .create_credit_card_response()
                  .has_value()) {
            ReplacePlaceholdersInValue(
                in_out_proto->mutable_compute_value()
                    ->mutable_create_credit_card_response()
                    ->mutable_value(),
                placeholders);
          }
          return;
        case ComputeValueProto::kCreateLoginOptionResponse:
          if (in_out_proto->compute_value()
                  .create_login_option_response()
                  .has_value()) {
            ReplacePlaceholdersInValue(
                in_out_proto->mutable_compute_value()
                    ->mutable_create_login_option_response()
                    ->mutable_value(),
                placeholders);
          }
          return;
        case ComputeValueProto::KIND_NOT_SET:
          return;
      }
    case CallbackProto::kSetUserActions:
      if (in_out_proto->set_user_actions().has_user_actions()) {
        ReplacePlaceholdersInValue(
            in_out_proto->mutable_set_user_actions()->mutable_user_actions(),
            placeholders);
      }
      return;
    case CallbackProto::kShowCalendarPopup:
      if (in_out_proto->show_calendar_popup().has_date_model_identifier()) {
        ReplaceInPlace(in_out_proto->mutable_show_calendar_popup()
                           ->mutable_date_model_identifier(),
                       placeholders);
      }
      if (in_out_proto->show_calendar_popup().has_min_date()) {
        ReplacePlaceholdersInValue(
            in_out_proto->mutable_show_calendar_popup()->mutable_min_date(),
            placeholders);
      }
      if (in_out_proto->show_calendar_popup().has_max_date()) {
        ReplacePlaceholdersInValue(
            in_out_proto->mutable_show_calendar_popup()->mutable_max_date(),
            placeholders);
      }
      return;
    case CallbackProto::kSetText:
      if (in_out_proto->set_text().has_text()) {
        ReplacePlaceholdersInValue(
            in_out_proto->mutable_set_text()->mutable_text(), placeholders);
      }
      if (in_out_proto->set_text().has_view_identifier()) {
        ReplaceInPlace(
            in_out_proto->mutable_set_text()->mutable_view_identifier(),
            placeholders);
      }
      return;
    case CallbackProto::kToggleUserAction:
      if (in_out_proto->toggle_user_action()
              .has_user_actions_model_identifier()) {
        ReplaceInPlace(in_out_proto->mutable_toggle_user_action()
                           ->mutable_user_actions_model_identifier(),
                       placeholders);
      }
      if (in_out_proto->toggle_user_action().has_enabled()) {
        ReplacePlaceholdersInValue(
            in_out_proto->mutable_toggle_user_action()->mutable_enabled(),
            placeholders);
      }
      return;
    case CallbackProto::kSetViewVisibility:
      if (in_out_proto->set_view_visibility().has_view_identifier()) {
        ReplaceInPlace(in_out_proto->mutable_set_view_visibility()
                           ->mutable_view_identifier(),
                       placeholders);
      }
      if (in_out_proto->set_view_visibility().has_visible()) {
        ReplacePlaceholdersInValue(
            in_out_proto->mutable_set_view_visibility()->mutable_visible(),
            placeholders);
      }
      return;
    case CallbackProto::kSetViewEnabled:
      if (in_out_proto->set_view_enabled().has_view_identifier()) {
        ReplaceInPlace(
            in_out_proto->mutable_set_view_enabled()->mutable_view_identifier(),
            placeholders);
      }
      if (in_out_proto->set_view_enabled().has_enabled()) {
        ReplacePlaceholdersInValue(
            in_out_proto->mutable_set_view_enabled()->mutable_enabled(),
            placeholders);
      }
      return;
    case CallbackProto::kShowGenericPopup:
      if (in_out_proto->show_generic_popup().has_popup_identifier()) {
        ReplaceInPlace(in_out_proto->mutable_show_generic_popup()
                           ->mutable_popup_identifier(),
                       placeholders);
      }
      if (in_out_proto->show_generic_popup().has_generic_ui()) {
        ReplacePlaceholdersInGenericUi(
            in_out_proto->mutable_show_generic_popup()->mutable_generic_ui(),
            placeholders);
      }
      return;
    case CallbackProto::kCreateNestedUi:
      if (in_out_proto->create_nested_ui().has_generic_ui_identifier()) {
        ReplaceInPlace(in_out_proto->mutable_create_nested_ui()
                           ->mutable_generic_ui_identifier(),
                       placeholders);
      }
      if (in_out_proto->create_nested_ui().has_generic_ui()) {
        ReplacePlaceholdersInGenericUi(
            in_out_proto->mutable_create_nested_ui()->mutable_generic_ui(),
            placeholders);
      }
      if (in_out_proto->create_nested_ui().has_parent_view_identifier()) {
        ReplaceInPlace(in_out_proto->mutable_create_nested_ui()
                           ->mutable_parent_view_identifier(),
                       placeholders);
      }
      return;
    case CallbackProto::kClearViewContainer:
      if (in_out_proto->clear_view_container().has_view_identifier()) {
        ReplaceInPlace(in_out_proto->mutable_clear_view_container()
                           ->mutable_view_identifier(),
                       placeholders);
      }
      return;
    case CallbackProto::kForEach:
      if (in_out_proto->for_each().has_loop_value_model_identifier()) {
        ReplaceInPlace(in_out_proto->mutable_for_each()
                           ->mutable_loop_value_model_identifier(),
                       placeholders);
      }
      for (auto& callback :
           *in_out_proto->mutable_for_each()->mutable_callbacks()) {
        ReplacePlaceholdersInCallback(&callback, placeholders);
      }
      return;
    case CallbackProto::kShowInfoPopup:
    case CallbackProto::kEndAction:
    case CallbackProto::KIND_NOT_SET:
      return;
  }
}

}  // namespace autofill_assistant
