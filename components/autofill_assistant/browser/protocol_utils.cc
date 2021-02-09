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
#include "components/autofill_assistant/browser/actions/configure_ui_state_action.h"
#include "components/autofill_assistant/browser/actions/expect_navigation_action.h"
#include "components/autofill_assistant/browser/actions/generate_password_for_form_field_action.h"
#include "components/autofill_assistant/browser/actions/get_element_status_action.h"
#include "components/autofill_assistant/browser/actions/highlight_element_action.h"
#include "components/autofill_assistant/browser/actions/navigate_action.h"
#include "components/autofill_assistant/browser/actions/popup_message_action.h"
#include "components/autofill_assistant/browser/actions/presave_generated_password_action.h"
#include "components/autofill_assistant/browser/actions/prompt_action.h"
#include "components/autofill_assistant/browser/actions/save_generated_password_action.h"
#include "components/autofill_assistant/browser/actions/select_option_action.h"
#include "components/autofill_assistant/browser/actions/set_attribute_action.h"
#include "components/autofill_assistant/browser/actions/set_form_field_value_action.h"
#include "components/autofill_assistant/browser/actions/show_cast_action.h"
#include "components/autofill_assistant/browser/actions/show_details_action.h"
#include "components/autofill_assistant/browser/actions/show_form_action.h"
#include "components/autofill_assistant/browser/actions/show_generic_ui_action.h"
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

void AppendScriptParametersToRepeatedField(
    const std::map<std::string, std::string>& script_parameters,
    google::protobuf::RepeatedPtrField<ScriptParameterProto>* dest) {
  for (const auto& param_entry : script_parameters) {
    ScriptParameterProto* parameter = dest->Add();
    parameter->set_name(param_entry.first);
    parameter->set_value(param_entry.second);
  }
}

}  // namespace

// static
std::string ProtocolUtils::CreateGetScriptsRequest(
    const GURL& url,
    const ClientContextProto& client_context,
    const std::map<std::string, std::string>& script_parameters) {
  DCHECK(!url.is_empty());

  SupportsScriptRequestProto script_proto;
  script_proto.set_url(url.spec());
  *script_proto.mutable_client_context() = client_context;
  AppendScriptParametersToRepeatedField(
      script_parameters, script_proto.mutable_script_parameters());
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
    const std::string& global_payload,
    const std::string& script_payload,
    const ClientContextProto& client_context,
    const std::map<std::string, std::string>& script_parameters) {
  ScriptActionRequestProto request_proto;
  InitialScriptActionsRequestProto* initial_request_proto =
      request_proto.mutable_initial_request();
  InitialScriptActionsRequestProto::QueryProto* query =
      initial_request_proto->mutable_query();
  query->add_script_path(script_path);
  query->set_url(url.spec());
  query->set_policy(PolicyType::SCRIPT);
  *request_proto.mutable_client_context() = client_context;
  AppendScriptParametersToRepeatedField(
      script_parameters, initial_request_proto->mutable_script_parameters());
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
    const std::string& global_payload,
    const std::string& script_payload,
    const std::vector<ProcessedActionProto>& processed_actions,
    const RoundtripTimingStats& timing_stats,
    const ClientContextProto& client_context) {
  ScriptActionRequestProto request_proto;
  request_proto.set_global_payload(global_payload);
  request_proto.set_script_payload(script_payload);
  NextScriptActionsRequestProto* next_request =
      request_proto.mutable_next_request();
  for (const auto& processed_action : processed_actions) {
    next_request->add_processed_actions()->MergeFrom(processed_action);
  }
  *next_request->mutable_timing_stats() = timing_stats;
  *request_proto.mutable_client_context() = client_context;
  std::string serialized_request_proto;
  bool success = request_proto.SerializeToString(&serialized_request_proto);
  DCHECK(success);
  return serialized_request_proto;
}

// static
std::unique_ptr<Action> ProtocolUtils::CreateAction(ActionDelegate* delegate,
                                                    const ActionProto& action) {
  switch (action.action_info_case()) {
    case ActionProto::ActionInfoCase::kClick:
      return std::make_unique<ClickAction>(delegate, action);
    case ActionProto::ActionInfoCase::kTell:
      return std::make_unique<TellAction>(delegate, action);
    case ActionProto::ActionInfoCase::kShowCast:
      return std::make_unique<ShowCastAction>(delegate, action);
    case ActionProto::ActionInfoCase::kUseAddress:
      return std::make_unique<UseAddressAction>(delegate, action);
    case ActionProto::ActionInfoCase::kUseCard:
      return std::make_unique<UseCreditCardAction>(delegate, action);
    case ActionProto::ActionInfoCase::kWaitForDom:
      return std::make_unique<WaitForDomAction>(delegate, action);
    case ActionProto::ActionInfoCase::kSelectOption:
      return std::make_unique<SelectOptionAction>(delegate, action);
    case ActionProto::ActionInfoCase::kNavigate:
      return std::make_unique<NavigateAction>(delegate, action);
    case ActionProto::ActionInfoCase::kPrompt:
      return std::make_unique<PromptAction>(delegate, action);
    case ActionProto::ActionInfoCase::kStop:
      return std::make_unique<StopAction>(delegate, action);
    case ActionProto::ActionInfoCase::kHighlightElement:
      return std::make_unique<HighlightElementAction>(delegate, action);
    case ActionProto::ActionInfoCase::kUploadDom:
      return std::make_unique<UploadDomAction>(delegate, action);
    case ActionProto::ActionInfoCase::kShowDetails:
      return std::make_unique<ShowDetailsAction>(delegate, action);
    case ActionProto::ActionInfoCase::kCollectUserData:
      return std::make_unique<CollectUserDataAction>(delegate, action);
    case ActionProto::ActionInfoCase::kSetFormValue:
      return std::make_unique<SetFormFieldValueAction>(delegate, action);
    case ActionProto::ActionInfoCase::kShowProgressBar:
      return std::make_unique<ShowProgressBarAction>(delegate, action);
    case ActionProto::ActionInfoCase::kSetAttribute:
      return std::make_unique<SetAttributeAction>(delegate, action);
    case ActionProto::ActionInfoCase::kShowInfoBox:
      return std::make_unique<ShowInfoBoxAction>(delegate, action);
    case ActionProto::ActionInfoCase::kExpectNavigation:
      return std::make_unique<ExpectNavigationAction>(delegate, action);
    case ActionProto::ActionInfoCase::kWaitForNavigation:
      return std::make_unique<WaitForNavigationAction>(delegate, action);
    case ActionProto::ActionInfoCase::kConfigureBottomSheet:
      return std::make_unique<ConfigureBottomSheetAction>(delegate, action);
    case ActionProto::ActionInfoCase::kShowForm:
      return std::make_unique<ShowFormAction>(delegate, action);
    case ActionProto::ActionInfoCase::kPopupMessage:
      return std::make_unique<PopupMessageAction>(delegate, action);
    case ActionProto::ActionInfoCase::kWaitForDocument:
      return std::make_unique<WaitForDocumentAction>(delegate, action);
    case ActionProto::ActionInfoCase::kShowGenericUi:
      return std::make_unique<ShowGenericUiAction>(delegate, action);
    case ActionProto::ActionInfoCase::kGeneratePasswordForFormField:
      return std::make_unique<GeneratePasswordForFormFieldAction>(delegate,
                                                                  action);
    case ActionProto::ActionInfoCase::kSaveGeneratedPassword:
      return std::make_unique<SaveGeneratedPasswordAction>(delegate, action);
    case ActionProto::ActionInfoCase::kConfigureUiState:
      return std::make_unique<ConfigureUiStateAction>(delegate, action);
    case ActionProto::ActionInfoCase::kPresaveGeneratedPassword:
      return std::make_unique<PresaveGeneratedPasswordAction>(delegate, action);
    case ActionProto::ActionInfoCase::kGetElementStatus:
      return std::make_unique<GetElementStatusAction>(delegate, action);
    case ActionProto::ActionInfoCase::ACTION_INFO_NOT_SET: {
      VLOG(1) << "Encountered action with ACTION_INFO_NOT_SET";
      return std::make_unique<UnsupportedAction>(delegate, action);
    }
      // Intentionally no default case to ensure a compilation error for new
      // cases added to the proto.
  }
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
    std::unique_ptr<Action> client_action = CreateAction(delegate, action);
    if (client_action == nullptr) {
      VLOG(1) << "Encountered action with Unknown or unsupported action with "
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

// static
std::string ProtocolUtils::CreateGetTriggerScriptsRequest(
    const GURL& url,
    const ClientContextProto& client_context,
    const std::map<std::string, std::string>& script_parameters) {
  GetTriggerScriptsRequestProto request_proto;
  request_proto.set_url(url.spec());
  *request_proto.mutable_client_context() = client_context;
  if (!script_parameters.empty()) {
    AppendScriptParametersToRepeatedField(
        script_parameters, request_proto.mutable_debug_script_parameters());
  }

  std::string serialized_request_proto;
  bool success = request_proto.SerializeToString(&serialized_request_proto);
  DCHECK(success);
  return serialized_request_proto;
}

// static
bool ProtocolUtils::ParseTriggerScripts(
    const std::string& response,
    std::vector<std::unique_ptr<TriggerScript>>* trigger_scripts,
    std::vector<std::string>* additional_allowed_domains,
    int* trigger_condition_check_interval_ms,
    base::Optional<int>* timeout_ms) {
  DCHECK(trigger_scripts);
  DCHECK(additional_allowed_domains);
  DCHECK(trigger_condition_check_interval_ms);
  DCHECK(timeout_ms);

  GetTriggerScriptsResponseProto response_proto;
  if (!response_proto.ParseFromString(response)) {
    LOG(ERROR) << "Failed to parse trigger scripts response";
    return false;
  }

  for (auto& trigger_script_proto : *response_proto.mutable_trigger_scripts()) {
    if (trigger_script_proto.user_interface().scroll_to_hide()) {
      // Turn off viewport resizing when scroll to hide is on as it causes
      // issues.
      trigger_script_proto.mutable_user_interface()->set_resize_visual_viewport(
          false);
    }
    trigger_scripts->emplace_back(
        std::make_unique<TriggerScript>(trigger_script_proto));
  }

  for (const auto& allowed_domain :
       response_proto.additional_allowed_domains()) {
    additional_allowed_domains->emplace_back(allowed_domain);
  }

  *trigger_condition_check_interval_ms =
      response_proto.trigger_condition_check_interval_ms();
  if (response_proto.has_timeout_ms()) {
    *timeout_ms = response_proto.timeout_ms();
  }
  return true;
}

}  // namespace autofill_assistant
