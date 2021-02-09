// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/service/lite_service_util.h"

#include <algorithm>
#include <memory>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"

namespace autofill_assistant {
namespace lite_service_util {

// The list of allowed actions.
// 'Stop' is intentionally not in this list: calling stop from a lite script
// would prevent the mini script from tansitioning into the main script.
// 'ShowForm', 'CollectUserData', and 'ShowGenericUi' are currently banned
// because we would not be able to process their response.
const ActionProto::ActionInfoCase kAllowedActions[] = {
    ActionProto::kPrompt,
    ActionProto::kTell,
    ActionProto::kWaitForDom,
    ActionProto::kShowProgressBar,
    ActionProto::kShowDetails,
    ActionProto::kShowInfoBox,
    ActionProto::kExpectNavigation,
    ActionProto::kWaitForNavigation,
    ActionProto::kConfigureBottomSheet,
    ActionProto::kPopupMessage,
    ActionProto::kWaitForDocument};

base::Optional<std::pair<ActionsResponseProto, ActionsResponseProto>>
SplitActionsAtLastBrowse(const ActionsResponseProto& proto) {
  // Note: we search for the last prompt(browse) action, to support use-cases
  // where a script needs to do multiple prompt(browse) actions.
  auto browse_action_reverse_it =
      std::find_if(proto.actions().rbegin(), proto.actions().rend(),
                   [](const ActionProto& action) {
                     return action.action_info_case() == ActionProto::kPrompt &&
                            action.prompt().browse_mode();
                   });
  if (browse_action_reverse_it == proto.actions().rend()) {
    VLOG(1) << __func__ << ": no prompt action with |browse|=true found";
    return base::nullopt;
  }

  // Last action must be regular prompt (no browse mode).
  if (browse_action_reverse_it == proto.actions().rbegin() ||
      (*proto.actions().rbegin()).action_info_case() != ActionProto::kPrompt ||
      (*proto.actions().rbegin()).prompt().browse_mode()) {
    VLOG(1) << __func__ << ": action sequence does not end with regular prompt";
    return base::nullopt;
  }

  ActionsResponseProto first_part;
  auto actions_iterator = proto.actions().begin();
  for (; actions_iterator != proto.actions().end() &&
         std::addressof(*actions_iterator) !=
             std::addressof(*browse_action_reverse_it);
       actions_iterator++) {
    *first_part.add_actions() = *actions_iterator;
  }
  // Add browse action to first part.
  *first_part.add_actions() = *actions_iterator++;

  ActionsResponseProto second_part;
  for (; actions_iterator != proto.actions().end(); actions_iterator++) {
    *second_part.add_actions() = *actions_iterator;
  }
  return std::make_pair(first_part, second_part);
}

bool ContainsOnlySafeActions(const ActionsResponseProto& proto) {
  return std::all_of(
      proto.actions().begin(), proto.actions().end(), [](const auto& action) {
        bool is_safe_action =
            std::find(std::begin(kAllowedActions), std::end(kAllowedActions),
                      action.action_info_case()) != std::end(kAllowedActions);
        return is_safe_action;
      });
}

ActionResponseType GetActionResponseType(const ProcessedActionProto& proto) {
  if (proto.status() != ACTION_APPLIED ||
      proto.action().action_info_case() != ActionProto::kPrompt) {
    return ActionResponseType::UNKNOWN;
  }

  if (proto.prompt_choice().navigation_ended()) {
    return ActionResponseType::PROMPT_NAVIGATE;
  }

  auto matching_choice_it = std::find_if(
      proto.action().prompt().choices().begin(),
      proto.action().prompt().choices().end(), [proto](const auto& choice) {
        return choice.server_payload() ==
               proto.prompt_choice().server_payload();
      });
  if (matching_choice_it == proto.action().prompt().choices().end()) {
    return ActionResponseType::UNKNOWN;
  }

  if (matching_choice_it->has_auto_select_when() &&
      (!matching_choice_it->has_chip() ||
       matching_choice_it->chip().type() == UNKNOWN_CHIP_TYPE)) {
    return ActionResponseType::PROMPT_INVISIBLE_AUTO_SELECT;
  }

  if (matching_choice_it->has_auto_select_when()) {
    return ActionResponseType::UNKNOWN;
  }

  switch (matching_choice_it->chip().type()) {
    case UNKNOWN_CHIP_TYPE:
    case HIGHLIGHTED_ACTION:
    case NORMAL_ACTION:
    case CANCEL_ACTION:
    case FEEDBACK_ACTION:
      return ActionResponseType::UNKNOWN;
    case CLOSE_ACTION:
      return ActionResponseType::PROMPT_CLOSE;
    case DONE_ACTION:
      return ActionResponseType::PROMPT_DONE;
  }
}

void AssignUniquePayloadsToPrompts(ActionsResponseProto* proto) {
  int counter = 0;
  for (auto& action : *proto->mutable_actions()) {
    if (action.action_info_case() != ActionProto::kPrompt) {
      continue;
    }

    for (auto& choice : *action.mutable_prompt()->mutable_choices()) {
      choice.set_server_payload(base::NumberToString(counter++));
    }
  }
}

}  // namespace lite_service_util
}  // namespace autofill_assistant
