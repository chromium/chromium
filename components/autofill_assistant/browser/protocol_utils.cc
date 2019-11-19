// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/protocol_utils.h"

#include <utility>

#include "base/feature_list.h"
#include "base/logging.h"
#include "components/autofill_assistant/browser/actions/click_action.h"
#include "components/autofill_assistant/browser/actions/collect_user_data_action.h"
#include "components/autofill_assistant/browser/actions/configure_bottom_sheet_action.h"
#include "components/autofill_assistant/browser/actions/expect_navigation_action.h"
#include "components/autofill_assistant/browser/actions/focus_element_action.h"
#include "components/autofill_assistant/browser/actions/highlight_element_action.h"
#include "components/autofill_assistant/browser/actions/navigate_action.h"
#include "components/autofill_assistant/browser/actions/popup_message_action.h"
#include "components/autofill_assistant/browser/actions/prompt_action.h"
#include "components/autofill_assistant/browser/actions/reset_action.h"
#include "components/autofill_assistant/browser/actions/select_option_action.h"
#include "components/autofill_assistant/browser/actions/set_attribute_action.h"
#include "components/autofill_assistant/browser/actions/set_form_field_value_action.h"
#include "components/autofill_assistant/browser/actions/show_details_action.h"
#include "components/autofill_assistant/browser/actions/show_form_action.h"
#include "components/autofill_assistant/browser/actions/show_info_box_action.h"
#include "components/autofill_assistant/browser/actions/show_progress_bar_action.h"
#include "components/autofill_assistant/browser/actions/stop_action.h"
#include "components/autofill_assistant/browser/actions/tell_action.h"
#include "components/autofill_assistant/browser/actions/unsupported_action.h"
#include "components/autofill_assistant/browser/actions/upload_dom_action.h"
#include "components/autofill_assistant/browser/actions/use_address_action.h"
#include "components/autofill_assistant/browser/actions/use_credit_card_action.h"
#include "components/autofill_assistant/browser/actions/wait_for_document_action.h"
#include "components/autofill_assistant/browser/actions/wait_for_dom_action.h"
#include "components/autofill_assistant/browser/actions/wait_for_navigation_action.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "url/gurl.h"

namespace autofill_assistant {

namespace {

void FillClientContext(const ClientContextProto& client_context,
                       const TriggerContext& trigger_context,
                       ClientContextProto* proto) {
  proto->CopyFrom(client_context);
  std::string experiment_ids = trigger_context.experiment_ids();
  if (!experiment_ids.empty()) {
    proto->set_experiment_ids(experiment_ids);
  }
  if (trigger_context.is_cct()) {
    proto->set_is_cct(true);
  }
  if (trigger_context.is_onboarding_shown()) {
    proto->set_is_onboarding_shown(true);
  }
  if (trigger_context.is_direct_action()) {
    proto->set_is_direct_action(true);
  }
}

}  // namespace

// static
std::string ProtocolUtils::CreateGetScriptsRequest(
    const GURL& url,
    const TriggerContext& trigger_context,
    const ClientContextProto& client_context) {
  DCHECK(!url.is_empty());

  SupportsScriptRequestProto script_proto;
  script_proto.set_url(url.spec());
  FillClientContext(client_context, trigger_context,
                    script_proto.mutable_client_context());
  trigger_context.AddParameters(script_proto.mutable_script_parameters());
  std::string serialized_script_proto;
  bool success = script_proto.SerializeToString(&serialized_script_proto);
  DCHECK(success);
  return serialized_script_proto;
}

// static
void ProtocolUtils::AddScript(const SupportedScriptProto& script_proto,
                              std::vector<std::unique_ptr<Script>>* scripts) {
  auto script = std::make_unique<Script>();
  script->handle.path = script_proto.path();
  if (script->handle.path.empty())
    return;

  const auto& presentation = script_proto.presentation();
  script->precondition = ScriptPrecondition::FromProto(
      script_proto.path(), presentation.precondition());
  if (!script->precondition)
    return;

  script->priority = presentation.priority();
  if (presentation.interrupt()) {
    script->handle.interrupt = true;
  } else {
    script->handle.autostart = presentation.autostart();
  }
  if (script->handle.autostart) {
    // Autostartable scripts without chip text must be skipped,
    // but these chips must never be shown.
    if (presentation.chip().text().empty()) {
      return;
    }
  } else {
    script->handle.initial_prompt = presentation.initial_prompt();
    script->handle.chip = Chip(presentation.chip());
  }
  script->handle.direct_action = DirectAction(presentation.direct_action());
  script->handle.start_message = presentation.start_message();
  script->handle.needs_ui = presentation.needs_ui();
  scripts->emplace_back(std::move(script));
}

// static
std::string ProtocolUtils::CreateInitialScriptActionsRequest(
    const std::string& script_path,
    const GURL& url,
    const TriggerContext& trigger_context,
    const std::string& global_payload,
    const std::string& script_payload,
    const ClientContextProto& client_context) {
  ScriptActionRequestProto request_proto;
  InitialScriptActionsRequestProto* initial_request_proto =
      request_proto.mutable_initial_request();
  InitialScriptActionsRequestProto::QueryProto* query =
      initial_request_proto->mutable_query();
  query->add_script_path(script_path);
  query->set_url(url.spec());
  query->set_policy(PolicyType::SCRIPT);
  FillClientContext(client_context, trigger_context,
                    request_proto.mutable_client_context());
  trigger_context.AddParameters(
      initial_request_proto->mutable_script_parameters());
  if (!global_payload.empty()) {
    request_proto.set_global_payload(global_payload);
  }
  if (!script_payload.empty()) {
    request_proto.set_script_payload(script_payload);
  }

  std::string serialized_initial_request_proto;
  bool success =
      request_proto.SerializeToString(&serialized_initial_request_proto);
  DCHECK(success);
  return serialized_initial_request_proto;
}

// static
std::string ProtocolUtils::CreateNextScriptActionsRequest(
    const TriggerContext& trigger_context,
    const std::string& global_payload,
    const std::string& script_payload,
    const std::vector<ProcessedActionProto>& processed_actions,
    const ClientContextProto& client_context) {
  ScriptActionRequestProto request_proto;
  request_proto.set_global_payload(global_payload);
  request_proto.set_script_payload(script_payload);
  NextScriptActionsRequestProto* next_request =
      request_proto.mutable_next_request();
  for (const auto& processed_action : processed_actions) {
    next_request->add_processed_actions()->MergeFrom(processed_action);
  }
  FillClientContext(client_context, trigger_context,
                    request_proto.mutable_client_context());
  std::string serialized_request_proto;
  bool success = request_proto.SerializeToString(&serialized_request_proto);
  DCHECK(success);
  return serialized_request_proto;
}

// static
bool ProtocolUtils::ParseActions(ActionDelegate* delegate,
                                 const std::string& response,
                                 std::string* return_global_payload,
                                 std::string* return_script_payload,
                                 std::vector<std::unique_ptr<Action>>* actions,
                                 std::vector<std::unique_ptr<Script>>* scripts,
                                 bool* should_update_scripts) {
  DCHECK(actions);
  DCHECK(scripts);

  ActionsResponseProto response_proto;
  if (!response_proto.ParseFromString(response)) {
    LOG(ERROR) << "Failed to parse assistant actions response.";
    return false;
  }

  if (return_global_payload) {
    *return_global_payload = response_proto.global_payload();
  }
  if (return_script_payload) {
    *return_script_payload = response_proto.script_payload();
  }

  for (const auto& action : response_proto.actions()) {
    std::unique_ptr<Action> client_action;
    switch (action.action_info_case()) {
      case ActionProto::ActionInfoCase::kClick: {
        client_action = std::make_unique<ClickAction>(delegate, action);
        break;
      }
      case ActionProto::ActionInfoCase::kTell: {
        client_action = std::make_unique<TellAction>(delegate, action);
        break;
      }
      case ActionProto::ActionInfoCase::kFocusElement: {
        client_action = std::make_unique<FocusElementAction>(delegate, action);
        break;
      }
      case ActionProto::ActionInfoCase::kUseAddress:
        client_action = std::make_unique<UseAddressAction>(delegate, action);
        break;
      case ActionProto::ActionInfoCase::kUseCard: {
        client_action = std::make_unique<UseCreditCardAction>(delegate, action);
        break;
      }
      case ActionProto::ActionInfoCase::kWaitForDom: {
        client_action = std::make_unique<WaitForDomAction>(delegate, action);
        break;
      }
      case ActionProto::ActionInfoCase::kSelectOption: {
        client_action = std::make_unique<SelectOptionAction>(delegate, action);
        break;
      }
      case ActionProto::ActionInfoCase::kNavigate: {
        client_action = std::make_unique<NavigateAction>(delegate, action);
        break;
      }
      case ActionProto::ActionInfoCase::kPrompt: {
        client_action = std::make_unique<PromptAction>(delegate, action);
        break;
      }
      case ActionProto::ActionInfoCase::kStop: {
        client_action = std::make_unique<StopAction>(delegate, action);
        break;
      }
      case ActionProto::ActionInfoCase::kReset: {
        client_action = std::make_unique<ResetAction>(delegate, action);
        break;
      }
      case ActionProto::ActionInfoCase::kHighlightElement: {
        client_action =
            std::make_unique<HighlightElementAction>(delegate, action);
        break;
      }
      case ActionProto::ActionInfoCase::kUploadDom: {
        client_action = std::make_unique<UploadDomAction>(delegate, action);
        break;
      }
      case ActionProto::ActionInfoCase::kShowDetails: {
        client_action = std::make_unique<ShowDetailsAction>(delegate, action);
        break;
      }
      case ActionProto::ActionInfoCase::kCollectUserData: {
        client_action =
            std::make_unique<CollectUserDataAction>(delegate, action);
        break;
      }
      case ActionProto::ActionInfoCase::kSetFormValue: {
        client_action =
            std::make_unique<SetFormFieldValueAction>(delegate, action);
        break;
      }
      case ActionProto::ActionInfoCase::kShowProgressBar: {
        client_action =
            std::make_unique<ShowProgressBarAction>(delegate, action);
        break;
      }
      case ActionProto::ActionInfoCase::kSetAttribute: {
        client_action = std::make_unique<SetAttributeAction>(delegate, action);
        break;
      }
      case ActionProto::ActionInfoCase::kShowInfoBox: {
        client_action = std::make_unique<ShowInfoBoxAction>(delegate, action);
        break;
      }
      case ActionProto::ActionInfoCase::kExpectNavigation: {
        client_action =
            std::make_unique<ExpectNavigationAction>(delegate, action);
        break;
      }
      case ActionProto::ActionInfoCase::kWaitForNavigation: {
        client_action =
            std::make_unique<WaitForNavigationAction>(delegate, action);
        break;
      }
      case ActionProto::ActionInfoCase::kConfigureBottomSheet: {
        client_action =
            std::make_unique<ConfigureBottomSheetAction>(delegate, action);
        break;
      }
      case ActionProto::ActionInfoCase::kShowForm: {
        client_action = std::make_unique<ShowFormAction>(delegate, action);
        break;
      }
      case ActionProto::ActionInfoCase::kPopupMessage: {
        client_action = std::make_unique<PopupMessageAction>(delegate, action);
        break;
      }
      case ActionProto::ActionInfoCase::kWaitForDocument: {
        client_action =
            std::make_unique<WaitForDocumentAction>(delegate, action);
        break;
      }
      case ActionProto::ActionInfoCase::ACTION_INFO_NOT_SET: {
        DVLOG(1) << "Encountered action with ACTION_INFO_NOT_SET";
        client_action = std::make_unique<UnsupportedAction>(delegate, action);
        break;
      }
        // Intentionally no default case to ensure a compilation error for new
        // cases added to the proto.
    }
    if (client_action == nullptr) {
      DVLOG(1) << "Encountered action with Unknown or unsupported action with "
                  "action_case="
               << action.action_info_case();
      client_action = std::make_unique<UnsupportedAction>(delegate, action);
    }
    actions->emplace_back(std::move(client_action));
  }

  *should_update_scripts = response_proto.has_update_script_list();
  for (const auto& script_proto :
       response_proto.update_script_list().scripts()) {
    ProtocolUtils::AddScript(script_proto, scripts);
  }

  return true;
}

}  // namespace autofill_assistant
