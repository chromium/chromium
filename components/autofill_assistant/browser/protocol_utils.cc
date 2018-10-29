// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/protocol_utils.h"

#include <utility>

#include "base/logging.h"
#include "components/autofill_assistant/browser/actions/autofill_action.h"
#include "components/autofill_assistant/browser/actions/click_action.h"
#include "components/autofill_assistant/browser/actions/focus_element_action.h"
#include "components/autofill_assistant/browser/actions/get_payment_information_action.h"
#include "components/autofill_assistant/browser/actions/highlight_element_action.h"
#include "components/autofill_assistant/browser/actions/navigate_action.h"
#include "components/autofill_assistant/browser/actions/reset_action.h"
#include "components/autofill_assistant/browser/actions/select_option_action.h"
#include "components/autofill_assistant/browser/actions/set_form_field_value_action.h"
#include "components/autofill_assistant/browser/actions/show_details_action.h"
#include "components/autofill_assistant/browser/actions/show_progress_bar_action.h"
#include "components/autofill_assistant/browser/actions/stop_action.h"
#include "components/autofill_assistant/browser/actions/tell_action.h"
#include "components/autofill_assistant/browser/actions/unsupported_action.h"
#include "components/autofill_assistant/browser/actions/upload_dom_action.h"
#include "components/autofill_assistant/browser/actions/wait_for_dom_action.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/version_info/version_info.h"
#include "url/gurl.h"

namespace autofill_assistant {

namespace {

// Fills the destination proto field with script parameters from the given
// parameter map.
void AddScriptParametersToProto(
    const std::map<std::string, std::string>& source,
    ::google::protobuf::RepeatedPtrField<ScriptParameterProto>* destination) {
  for (const auto& param_entry : source) {
    ScriptParameterProto* parameter = destination->Add();
    parameter->set_name(param_entry.first);
    parameter->set_value(param_entry.second);
  }
}

}  // namespace

// static
std::string ProtocolUtils::CreateGetScriptsRequest(
    const GURL& url,
    const std::map<std::string, std::string>& parameters) {
  DCHECK(!url.is_empty());

  SupportsScriptRequestProto script_proto;
  script_proto.set_url(url.spec());
  script_proto.mutable_client_context()->mutable_chrome()->set_chrome_version(
      version_info::GetProductNameAndVersionForUserAgent());
  AddScriptParametersToProto(parameters,
                             script_proto.mutable_script_parameters());
  std::string serialized_script_proto;
  bool success = script_proto.SerializeToString(&serialized_script_proto);
  DCHECK(success);
  return serialized_script_proto;
}

// static
bool ProtocolUtils::ParseScripts(
    const std::string& response,
    std::vector<std::unique_ptr<Script>>* scripts) {
  DCHECK(scripts);

  SupportsScriptResponseProto response_proto;
  if (!response_proto.ParseFromString(response)) {
    LOG(ERROR) << "Failed to parse getting assistant scripts response.";
    return false;
  }

  scripts->clear();
  for (const auto& script_proto : response_proto.scripts()) {
    auto script = std::make_unique<Script>();
    script->handle.path = script_proto.path();

    const auto& presentation = script_proto.presentation();
    script->handle.name = presentation.name();
    script->handle.autostart = presentation.autostart();
    script->handle.initial_prompt = presentation.initial_prompt();
    script->handle.highlight = presentation.highlight();
    script->precondition = ScriptPrecondition::FromProto(
        script_proto.path(), presentation.precondition());
    script->priority = presentation.priority();

    if (script->handle.name.empty() || script->handle.path.empty() ||
        !script->precondition) {
      LOG(ERROR) << "Ignored invalid or incomplete script '"
                 << script->handle.path << "'";
      continue;
    }
    scripts->emplace_back(std::move(script));
  }

  return true;
}

// static
std::string ProtocolUtils::CreateInitialScriptActionsRequest(
    const std::string& script_path,
    const GURL& url,
    const std::map<std::string, std::string>& parameters,
    const std::string& server_payload) {
  ScriptActionRequestProto request_proto;
  InitialScriptActionsRequestProto* initial_request_proto =
      request_proto.mutable_initial_request();
  InitialScriptActionsRequestProto::QueryProto* query =
      initial_request_proto->mutable_query();
  query->add_script_path(script_path);
  query->set_url(url.spec());
  query->set_policy(PolicyType::SCRIPT);
  AddScriptParametersToProto(
      parameters, initial_request_proto->mutable_script_parameters());
  request_proto.mutable_client_context()->mutable_chrome()->set_chrome_version(
      version_info::GetProductNameAndVersionForUserAgent());

  if (!server_payload.empty()) {
    request_proto.set_server_payload(server_payload);
  }

  std::string serialized_initial_request_proto;
  bool success =
      request_proto.SerializeToString(&serialized_initial_request_proto);
  DCHECK(success);
  return serialized_initial_request_proto;
}

// static
std::string ProtocolUtils::CreateNextScriptActionsRequest(
    const std::string& previous_server_payload,
    const std::vector<ProcessedActionProto>& processed_actions) {
  ScriptActionRequestProto request_proto;
  request_proto.set_server_payload(previous_server_payload);
  NextScriptActionsRequestProto* next_request =
      request_proto.mutable_next_request();
  for (const auto& processed_action : processed_actions) {
    next_request->add_processed_actions()->MergeFrom(processed_action);
  }
  std::string serialized_request_proto;
  bool success = request_proto.SerializeToString(&serialized_request_proto);
  DCHECK(success);
  return serialized_request_proto;
}

// static
bool ProtocolUtils::ParseActions(
    const std::string& response,
    std::string* return_server_payload,
    std::vector<std::unique_ptr<Action>>* actions) {
  DCHECK(actions);

  ActionsResponseProto response_proto;
  if (!response_proto.ParseFromString(response)) {
    LOG(ERROR) << "Failed to parse assistant actions response.";
    return false;
  }

  if (return_server_payload && response_proto.has_server_payload()) {
    *return_server_payload = response_proto.server_payload();
  }

  for (const auto& action : response_proto.actions()) {
    switch (action.action_info_case()) {
      case ActionProto::ActionInfoCase::kClick: {
        actions->emplace_back(std::make_unique<ClickAction>(action));
        break;
      }
      case ActionProto::ActionInfoCase::kTell: {
        actions->emplace_back(std::make_unique<TellAction>(action));
        break;
      }
      case ActionProto::ActionInfoCase::kFocusElement: {
        actions->emplace_back(std::make_unique<FocusElementAction>(action));
        break;
      }
      case ActionProto::ActionInfoCase::kUseAddress:
      case ActionProto::ActionInfoCase::kUseCard: {
        actions->emplace_back(std::make_unique<AutofillAction>(action));
        break;
      }
      case ActionProto::ActionInfoCase::kWaitForDom: {
        actions->emplace_back(std::make_unique<WaitForDomAction>(action));
        break;
      }
      case ActionProto::ActionInfoCase::kSelectOption: {
        actions->emplace_back(std::make_unique<SelectOptionAction>(action));
        break;
      }
      case ActionProto::ActionInfoCase::kNavigate: {
        actions->emplace_back(std::make_unique<NavigateAction>(action));
        break;
      }
      case ActionProto::ActionInfoCase::kStop: {
        actions->emplace_back(std::make_unique<StopAction>(action));
        break;
      }
      case ActionProto::ActionInfoCase::kReset: {
        actions->emplace_back(std::make_unique<ResetAction>(action));
        break;
      }
      case ActionProto::ActionInfoCase::kHighlightElement: {
        actions->emplace_back(std::make_unique<HighlightElementAction>(action));
        break;
      }
      case ActionProto::ActionInfoCase::kUploadDom: {
        actions->emplace_back(std::make_unique<UploadDomAction>(action));
        break;
      }
      case ActionProto::ActionInfoCase::kShowDetails: {
        actions->emplace_back(std::make_unique<ShowDetailsAction>(action));
        break;
      }
      case ActionProto::ActionInfoCase::kGetPaymentInformation: {
        actions->emplace_back(
            std::make_unique<GetPaymentInformationAction>(action));
        break;
      }
      case ActionProto::ActionInfoCase::kSetFormValue: {
        actions->emplace_back(
            std::make_unique<SetFormFieldValueAction>(action));
        break;
      }
      case ActionProto::ActionInfoCase::kShowProgressBar: {
        actions->emplace_back(std::make_unique<ShowProgressBarAction>(action));
        break;
      }
      default:
      case ActionProto::ActionInfoCase::ACTION_INFO_NOT_SET: {
        DLOG(ERROR) << "Unknown or unsupported action with action_case="
                    << action.action_info_case();
        actions->emplace_back(std::make_unique<UnsupportedAction>(action));
        break;
      }
    }
  }

  return true;
}

}  // namespace autofill_assistant
