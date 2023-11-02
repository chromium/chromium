// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/protocol_utils.h"

#include <utility>

#include <google/protobuf/message_lite.h>
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "components/autofill_assistant/browser/actions/action_delegate_util.h"
#include "components/autofill_assistant/browser/actions/check_element_tag_action.h"
#include "components/autofill_assistant/browser/actions/check_option_element_action.h"
#include "components/autofill_assistant/browser/actions/clear_persistent_ui_action.h"
#include "components/autofill_assistant/browser/actions/collect_user_data_action.h"
#include "components/autofill_assistant/browser/actions/configure_bottom_sheet_action.h"
#include "components/autofill_assistant/browser/actions/configure_ui_state_action.h"
#include "components/autofill_assistant/browser/actions/dispatch_js_event_action.h"
#include "components/autofill_assistant/browser/actions/execute_js_action.h"
#include "components/autofill_assistant/browser/actions/expect_navigation_action.h"
#include "components/autofill_assistant/browser/actions/external_action.h"
#include "components/autofill_assistant/browser/actions/generate_password_for_form_field_action.h"
#include "components/autofill_assistant/browser/actions/get_element_status_action.h"
#include "components/autofill_assistant/browser/actions/js_flow_action.h"
#include "components/autofill_assistant/browser/actions/navigate_action.h"
#include "components/autofill_assistant/browser/actions/parse_single_tag_xml_action.h"
#include "components/autofill_assistant/browser/actions/perform_on_single_element_action.h"
#include "components/autofill_assistant/browser/actions/popup_message_action.h"
#include "components/autofill_assistant/browser/actions/presave_generated_password_action.h"
#include "components/autofill_assistant/browser/actions/prompt_action.h"
#include "components/autofill_assistant/browser/actions/prompt_qr_code_scan_action.h"
#include "components/autofill_assistant/browser/actions/register_password_reset_request_action.h"
#include "components/autofill_assistant/browser/actions/release_elements_action.h"
#include "components/autofill_assistant/browser/actions/report_progress_action.h"
#include "components/autofill_assistant/browser/actions/reset_pending_credentials_action.h"
#include "components/autofill_assistant/browser/actions/save_generated_password_action.h"
#include "components/autofill_assistant/browser/actions/save_submitted_password_action.h"
#include "components/autofill_assistant/browser/actions/select_option_action.h"
#include "components/autofill_assistant/browser/actions/send_keystroke_events_action.h"
#include "components/autofill_assistant/browser/actions/set_attribute_action.h"
#include "components/autofill_assistant/browser/actions/set_persistent_ui_action.h"
#include "components/autofill_assistant/browser/actions/set_touchable_area_action.h"
#include "components/autofill_assistant/browser/actions/show_cast_action.h"
#include "components/autofill_assistant/browser/actions/show_details_action.h"
#include "components/autofill_assistant/browser/actions/show_form_action.h"
#include "components/autofill_assistant/browser/actions/show_generic_ui_action.h"
#include "components/autofill_assistant/browser/actions/show_info_box_action.h"
#include "components/autofill_assistant/browser/actions/show_progress_bar_action.h"
#include "components/autofill_assistant/browser/actions/stop_action.h"
#include "components/autofill_assistant/browser/actions/tell_action.h"
#include "components/autofill_assistant/browser/actions/unsupported_action.h"
#include "components/autofill_assistant/browser/actions/update_client_settings_action.h"
#include "components/autofill_assistant/browser/actions/upload_dom_action.h"
#include "components/autofill_assistant/browser/actions/use_address_action.h"
#include "components/autofill_assistant/browser/actions/use_credit_card_action.h"
#include "components/autofill_assistant/browser/actions/wait_for_document_action.h"
#include "components/autofill_assistant/browser/actions/wait_for_dom_action.h"
#include "components/autofill_assistant/browser/actions/wait_for_navigation_action.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web/web_controller.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

namespace autofill_assistant {
namespace {
// Parses |bytes| into |out|.
//
// On error, returns false and puts an error message into |error_message|.
bool ParseActionFromString(int32_t action_id,
                           const std::string& bytes,
                           std::string* error_message,
                           google::protobuf::MessageLite* out) {
  if (out->ParseFromString(bytes)) {
    return true;
  }
  if (error_message != nullptr) {
    *error_message = base::StrCat({"Message not parseable for action id ",
                                   base::NumberToString(action_id)});
  }
  return false;
}
}  // namespace

// static
std::string ProtocolUtils::CreateGetScriptsRequest(
    const GURL& url,
    const ClientContextProto& client_context,
    const ScriptParameters& script_parameters) {
  DCHECK(!url.is_empty());

  SupportsScriptRequestProto script_proto;
  script_proto.set_url(url.spec());
  *script_proto.mutable_client_context() = client_context;
  *script_proto.mutable_script_parameters() =
      script_parameters.ToProto(/* only_non_sensitive_allowlisted = */ false);
  std::string serialized_script_proto;
  bool success = script_proto.SerializeToString(&serialized_script_proto);
  DCHECK(success);
  return serialized_script_proto;
}

// static
ClientContextProto ProtocolUtils::CreateNonSensitiveContext(
    const ClientContextProto& client_context) {
  ClientContextProto non_sensitive_context;
  if (client_context.has_locale()) {
    non_sensitive_context.set_locale(client_context.locale());
  }
  if (client_context.has_country()) {
    non_sensitive_context.set_country(client_context.country());
  }
  if (client_context.chrome().has_chrome_version()) {
    non_sensitive_context.mutable_chrome()->set_chrome_version(
        client_context.chrome().chrome_version());
  }
  if (client_context.has_platform_type()) {
    non_sensitive_context.set_platform_type(client_context.platform_type());
  }
  return non_sensitive_context;
}

// static
std::string ProtocolUtils::CreateCapabilitiesByHashRequest(
    uint32_t hash_prefix_length,
    const std::vector<uint64_t>& hash_prefix,
    const ClientContextProto& client_context,
    const ScriptParameters& script_parameters) {
  GetCapabilitiesByHashPrefixRequestProto request;
  request.set_hash_prefix_length(hash_prefix_length);
  for (uint64_t prefix : hash_prefix) {
    request.add_hash_prefix(prefix);
  }
  *request.mutable_script_parameters() =
      script_parameters.ToProto(/* only_non_sensitive_allowlisted = */ true);
  *request.mutable_client_context() = CreateNonSensitiveContext(client_context);

  std::string serialized_request;
  bool success = request.SerializeToString(&serialized_request);
  DCHECK(success);
  return serialized_request;
}

// static
std::string ProtocolUtils::CreateTriggerScriptsByHashRequest(
    uint32_t hash_prefix_length,
    const std::vector<uint64_t>& hash_prefix,
    const ClientContextProto& client_context,
    const ScriptParameters& script_parameters) {
  GetTriggerScriptsByHashPrefixRequestProto request;
  request.set_hash_prefix_length(hash_prefix_length);
  for (uint64_t prefix : hash_prefix) {
    request.add_hash_prefix(prefix);
  }
  *request.mutable_script_parameters() =
      script_parameters.ToProto(/* only_non_sensitive_allowlisted */ true);
  *request.mutable_client_context() = CreateNonSensitiveContext(client_context);

  std::string serialized_request;
  bool success = request.SerializeToString(&serialized_request);
  DCHECK(success);
  return serialized_request;
}

// static
std::string ProtocolUtils::CreateGetNoRoundTripScriptsByHashRequest(
    const uint32_t hash_prefix_length,
    const uint64_t hash_prefix,
    const ClientContextProto& client_context,
    const ScriptParameters& script_parameters) {
  GetNoRoundTripScriptsByHashPrefixRequestProto request;
  request.set_hash_prefix_length(hash_prefix_length);
  request.set_hash_prefix(hash_prefix);
  *request.mutable_script_parameters() =
      script_parameters.ToProto(/* only_non_sensitive_allowlisted = */ true);

  ClientContextProto non_sensitive_context;
  if (client_context.has_locale()) {
    non_sensitive_context.set_locale(client_context.locale());
  }
  if (client_context.has_country()) {
    non_sensitive_context.set_country(client_context.country());
  }
  if (client_context.chrome().has_chrome_version()) {
    non_sensitive_context.mutable_chrome()->set_chrome_version(
        client_context.chrome().chrome_version());
  }
  if (client_context.has_platform_type()) {
    non_sensitive_context.set_platform_type(client_context.platform_type());
  }
  *request.mutable_client_context() = non_sensitive_context;

  std::string serialized_request;
  bool success = request.SerializeToString(&serialized_request);
  DCHECK(success);
  return serialized_request;
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
    const ScriptParameters& script_parameters,
    const absl::optional<ScriptStoreConfig>& script_store_config) {
  ScriptActionRequestProto request_proto;
  InitialScriptActionsRequestProto* initial_request_proto =
      request_proto.mutable_initial_request();
  if (script_store_config.has_value()) {
    *initial_request_proto->mutable_script_store_config() =
        *script_store_config;
  }
  InitialScriptActionsRequestProto::QueryProto* query =
      initial_request_proto->mutable_query();
  query->add_script_path(script_path);
  query->set_url(url.spec());
  query->set_policy(PolicyType::SCRIPT);
  *request_proto.mutable_client_context() = client_context;
  *initial_request_proto->mutable_script_parameters() =
      script_parameters.ToProto(/* only_non_sensitive_allowlisted = */ false);
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
    const RoundtripNetworkStats& network_stats,
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
  *next_request->mutable_network_stats() = network_stats;
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
    case ActionProto::ActionInfoCase::kUploadDom:
      return std::make_unique<UploadDomAction>(delegate, action);
    case ActionProto::ActionInfoCase::kShowDetails:
      return std::make_unique<ShowDetailsAction>(delegate, action);
    case ActionProto::ActionInfoCase::kCollectUserData:
      return std::make_unique<CollectUserDataAction>(delegate, action);
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
    case ActionProto::ActionInfoCase::kUpdateClientSettings:
      return std::make_unique<UpdateClientSettingsAction>(delegate, action);
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
    case ActionProto::ActionInfoCase::kScrollIntoView: {
      auto actions =
          std::make_unique<element_action_util::ElementActionVector>();
      action_delegate_util::AddStepWithoutCallback(
          base::BindOnce(&ActionDelegate::StoreScrolledToElement,
                         delegate->GetWeakPtr()),
          actions.get());
      actions->emplace_back(
          base::BindOnce(&WebController::ScrollIntoView,
                         delegate->GetWebController()->GetWeakPtr(),
                         action.scroll_into_view().animation(),
                         action.scroll_into_view().has_vertical_alignment()
                             ? action.scroll_into_view().vertical_alignment()
                             : "center",
                         action.scroll_into_view().has_horizontal_alignment()
                             ? action.scroll_into_view().horizontal_alignment()
                             : "center"));
      return PerformOnSingleElementAction::WithClientId(
          delegate, action, action.scroll_into_view().client_id(),
          base::BindOnce(&element_action_util::PerformAll, std::move(actions)));
    }
    case ActionProto::ActionInfoCase::kWaitForDocumentToBecomeInteractive:
      return PerformOnSingleElementAction::WithOptionalClientIdTimed(
          delegate, action,
          action.wait_for_document_to_become_interactive().client_id(),
          base::BindOnce(&ActionDelegate::WaitUntilDocumentIsInReadyState,
                         delegate->GetWeakPtr(),
                         base::Milliseconds(
                             action.wait_for_document_to_become_interactive()
                                 .timeout_in_ms()),
                         DOCUMENT_INTERACTIVE));
    case ActionProto::ActionInfoCase::kWaitForDocumentToBecomeComplete:
      return PerformOnSingleElementAction::WithOptionalClientIdTimed(
          delegate, action,
          action.wait_for_document_to_become_complete().client_id(),
          base::BindOnce(
              &ActionDelegate::WaitUntilDocumentIsInReadyState,
              delegate->GetWeakPtr(),
              base::Milliseconds(action.wait_for_document_to_become_complete()
                                     .timeout_in_ms()),
              DOCUMENT_COMPLETE));
    case ActionProto::ActionInfoCase::kSendClickEvent:
      return PerformOnSingleElementAction::WithClientId(
          delegate, action, action.send_click_event().client_id(),
          base::BindOnce(&WebController::ClickOrTapElement,
                         delegate->GetWebController()->GetWeakPtr(),
                         ClickType::CLICK));
    case ActionProto::ActionInfoCase::kSendTapEvent:
      return PerformOnSingleElementAction::WithClientId(
          delegate, action, action.send_tap_event().client_id(),
          base::BindOnce(&WebController::ClickOrTapElement,
                         delegate->GetWebController()->GetWeakPtr(),
                         ClickType::TAP));
    case ActionProto::ActionInfoCase::kJsClick:
      return PerformOnSingleElementAction::WithClientId(
          delegate, action, action.js_click().client_id(),
          base::BindOnce(&WebController::JsClickElement,
                         delegate->GetWebController()->GetWeakPtr()));
    case ActionProto::ActionInfoCase::kSendKeystrokeEvents:
      return std::make_unique<SendKeystrokeEventsAction>(delegate, action);
    case ActionProto::ActionInfoCase::kSendChangeEvent:
      return PerformOnSingleElementAction::WithClientId(
          delegate, action, action.send_change_event().client_id(),
          base::BindOnce(&WebController::SendChangeEvent,
                         delegate->GetWebController()->GetWeakPtr()));
    case ActionProto::ActionInfoCase::kSetElementAttribute: {
      std::vector<std::string> attributes;
      for (const auto& attribute : action.set_element_attribute().attribute()) {
        attributes.emplace_back(attribute);
      }
      return PerformOnSingleElementAction::WithClientId(
          delegate, action, action.set_element_attribute().client_id(),
          base::BindOnce(
              &action_delegate_util::PerformWithTextValue, delegate,
              action.set_element_attribute().value(),
              base::BindOnce(&WebController::SetAttribute,
                             delegate->GetWebController()->GetWeakPtr(),
                             attributes)));
    }
    case ActionProto::ActionInfoCase::kSelectFieldValue:
      return PerformOnSingleElementAction::WithClientId(
          delegate, action, action.select_field_value().client_id(),
          base::BindOnce(&WebController::SelectFieldValue,
                         delegate->GetWebController()->GetWeakPtr()));
    case ActionProto::ActionInfoCase::kFocusField:
      return PerformOnSingleElementAction::WithClientId(
          delegate, action, action.focus_field().client_id(),
          base::BindOnce(&WebController::FocusField,
                         delegate->GetWebController()->GetWeakPtr()));
    case ActionProto::ActionInfoCase::kWaitForElementToBecomeStable:
      return PerformOnSingleElementAction::WithClientIdTimed(
          delegate, action,
          action.wait_for_element_to_become_stable().client_id(),
          base::BindOnce(
              &WebController::WaitUntilElementIsStable,
              delegate->GetWebController()->GetWeakPtr(),
              action.wait_for_element_to_become_stable()
                  .stable_check_max_rounds(),
              base::Milliseconds(action.wait_for_element_to_become_stable()
                                     .stable_check_interval_ms())));
    case ActionProto::ActionInfoCase::kCheckElementIsOnTop:
      return PerformOnSingleElementAction::WithClientId(
          delegate, action, action.check_element_is_on_top().client_id(),
          base::BindOnce(&WebController::CheckOnTop,
                         delegate->GetWebController()->GetWeakPtr()));
    case ActionProto::ActionInfoCase::kReleaseElements:
      return std::make_unique<ReleaseElementsAction>(delegate, action);
    case ActionProto::ActionInfoCase::kDispatchJsEvent:
      return std::make_unique<DispatchJsEventAction>(delegate, action);
    case ActionProto::ActionInfoCase::kSendKeyEvent:
      return PerformOnSingleElementAction::WithClientId(
          delegate, action, action.send_key_event().client_id(),
          base::BindOnce(&WebController::SendKeyEvent,
                         delegate->GetWebController()->GetWeakPtr(),
                         action.send_key_event().key_event()));
    case ActionProto::ActionInfoCase::kSelectOptionElement:
      return PerformOnSingleElementAction::WithClientId(
          delegate, action, action.select_option_element().select_id(),
          base::BindOnce(
              &action_delegate_util::PerformWithElementValue, delegate,
              action.select_option_element().option_id(),
              base::BindOnce(&WebController::SelectOptionElement,
                             delegate->GetWebController()->GetWeakPtr())));
    case ActionProto::ActionInfoCase::kCheckElementTag:
      return std::make_unique<CheckElementTagAction>(delegate, action);
    case ActionProto::ActionInfoCase::kCheckOptionElement:
      return std::make_unique<CheckOptionElementAction>(delegate, action);
    case ActionProto::ActionInfoCase::kSetPersistentUi:
      return std::make_unique<SetPersistentUiAction>(delegate, action);
    case ActionProto::ActionInfoCase::kClearPersistentUi:
      return std::make_unique<ClearPersistentUiAction>(delegate, action);
    case ActionProto::ActionInfoCase::kScrollIntoViewIfNeeded:
      return PerformOnSingleElementAction::WithClientId(
          delegate, action, action.scroll_into_view_if_needed().client_id(),
          base::BindOnce(&WebController::ScrollIntoViewIfNeeded,
                         delegate->GetWebController()->GetWeakPtr(),
                         action.scroll_into_view_if_needed().has_center()
                             ? action.scroll_into_view_if_needed().center()
                             : true));
    case ActionProto::ActionInfoCase::kScrollWindow:
      return PerformOnSingleElementAction::WithOptionalClientId(
          delegate, action, action.scroll_window().optional_frame_id(),
          base::BindOnce(&WebController::ScrollWindow,
                         delegate->GetWebController()->GetWeakPtr(),
                         action.scroll_window().scroll_distance(),
                         action.scroll_window().animation()));
    case ActionProto::ActionInfoCase::kScrollContainer:
      return PerformOnSingleElementAction::WithClientId(
          delegate, action, action.scroll_container().client_id(),
          base::BindOnce(&WebController::ScrollContainer,
                         delegate->GetWebController()->GetWeakPtr(),
                         action.scroll_container().scroll_distance(),
                         action.scroll_container().animation()));
    case ActionProto::ActionInfoCase::kSetTouchableArea:
      return std::make_unique<SetTouchableAreaAction>(delegate, action);
    case ActionProto::ActionInfoCase::kBlurField:
      return PerformOnSingleElementAction::WithClientId(
          delegate, action, action.blur_field().client_id(),
          base::BindOnce(&WebController::BlurField,
                         delegate->GetWebController()->GetWeakPtr()));
    case ActionProto::ActionInfoCase::kResetPendingCredentials:
      return std::make_unique<ResetPendingCredentialsAction>(delegate, action);
    case ActionProto::ActionInfoCase::kSaveSubmittedPassword:
      return std::make_unique<SaveSubmittedPasswordAction>(delegate, action);
    case ActionProto::ActionInfoCase::kExecuteJs:
      return std::make_unique<ExecuteJsAction>(delegate, action);
    case ActionProto::ActionInfoCase::kJsFlow:
      return std::make_unique<JsFlowAction>(delegate, action);
    case ActionProto::ActionInfoCase::kExternalAction:
      return std::make_unique<ExternalAction>(delegate, action);
    case ActionProto::ActionInfoCase::kRegisterPasswordResetRequest:
      return std::make_unique<RegisterPasswordResetRequestAction>(delegate,
                                                                  action);
    case ActionProto::ActionInfoCase::kSetNativeValue:
      return PerformOnSingleElementAction::WithClientId(
          delegate, action, action.set_native_value().client_id(),
          base::BindOnce(
              &action_delegate_util::PerformWithTextValue, delegate,
              action.set_native_value().value(),
              base::BindOnce(&WebController::SetNativeValue,
                             delegate->GetWebController()->GetWeakPtr())));
    case ActionProto::ActionInfoCase::kSetNativeChecked:
      return PerformOnSingleElementAction::WithClientId(
          delegate, action, action.set_native_checked().client_id(),
          base::BindOnce(&WebController::SetNativeChecked,
                         delegate->GetWebController()->GetWeakPtr(),
                         action.set_native_checked().checked()));
    case ActionProto::ActionInfoCase::kPromptQrCodeScan:
      return std::make_unique<PromptQrCodeScanAction>(delegate, action);
    case ActionProto::ActionInfoCase::kParseSingleTagXml:
      return std::make_unique<ParseSingleTagXmlAction>(delegate, action);
    case ActionProto::ActionInfoCase::kReportProgress:
      return std::make_unique<ReportProgressAction>(delegate, action);
    case ActionProto::ActionInfoCase::ACTION_INFO_NOT_SET: {
      VLOG(1) << "Encountered action with ACTION_INFO_NOT_SET";
      return std::make_unique<UnsupportedAction>(delegate, action);
    }
      // Intentionally no default case to ensure a compilation error for new
      // cases added to the proto.
  }
}

// static
absl::optional<ActionProto> ProtocolUtils::ParseFromString(
    int32_t action_id,
    const std::string& bytes,
    std::string* error_message) {
  ActionProto proto;
  bool success = true;
  switch (static_cast<ActionProto::ActionInfoCase>(action_id)) {
    case ActionProto::ActionInfoCase::kTell:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_tell());
      break;
    case ActionProto::ActionInfoCase::kShowCast:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_show_cast());
      break;
    case ActionProto::ActionInfoCase::kUseAddress:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_use_address());
      break;
    case ActionProto::ActionInfoCase::kUseCard:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_use_card());
      break;
    case ActionProto::ActionInfoCase::kWaitForDom:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_wait_for_dom());
      break;
    case ActionProto::ActionInfoCase::kSelectOption:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_select_option());
      break;
    case ActionProto::ActionInfoCase::kNavigate:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_navigate());
      break;
    case ActionProto::ActionInfoCase::kPrompt:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_prompt());
      break;
    case ActionProto::ActionInfoCase::kStop:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_stop());
      break;
    case ActionProto::ActionInfoCase::kUploadDom:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_upload_dom());
      break;
    case ActionProto::ActionInfoCase::kShowDetails:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_show_details());
      break;
    case ActionProto::ActionInfoCase::kCollectUserData:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_collect_user_data());
      break;
    case ActionProto::ActionInfoCase::kShowProgressBar:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_show_progress_bar());
      break;
    case ActionProto::ActionInfoCase::kSetAttribute:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_set_attribute());
      break;
    case ActionProto::ActionInfoCase::kShowInfoBox:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_show_info_box());
      break;
    case ActionProto::ActionInfoCase::kExpectNavigation:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_expect_navigation());
      break;
    case ActionProto::ActionInfoCase::kWaitForNavigation:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_wait_for_navigation());
      break;
    case ActionProto::ActionInfoCase::kConfigureBottomSheet:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_configure_bottom_sheet());
      break;
    case ActionProto::ActionInfoCase::kShowForm:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_show_form());
      break;
    case ActionProto::ActionInfoCase::kUpdateClientSettings:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_update_client_settings());
      break;
    case ActionProto::ActionInfoCase::kPopupMessage:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_popup_message());
      break;
    case ActionProto::ActionInfoCase::kWaitForDocument:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_wait_for_document());
      break;
    case ActionProto::ActionInfoCase::kShowGenericUi:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_show_generic_ui());
      break;
    case ActionProto::ActionInfoCase::kGeneratePasswordForFormField:
      success = ParseActionFromString(
          action_id, bytes, error_message,
          proto.mutable_generate_password_for_form_field());
      break;
    case ActionProto::ActionInfoCase::kSaveGeneratedPassword:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_save_generated_password());
      break;
    case ActionProto::ActionInfoCase::kConfigureUiState:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_configure_ui_state());
      break;
    case ActionProto::ActionInfoCase::kPresaveGeneratedPassword:
      success =
          ParseActionFromString(action_id, bytes, error_message,
                                proto.mutable_presave_generated_password());
      break;
    case ActionProto::ActionInfoCase::kGetElementStatus:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_get_element_status());
      break;
    case ActionProto::ActionInfoCase::kScrollIntoView:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_scroll_into_view());
      break;
    case ActionProto::ActionInfoCase::kWaitForDocumentToBecomeInteractive:
      success = ParseActionFromString(
          action_id, bytes, error_message,
          proto.mutable_wait_for_document_to_become_interactive());
      break;
    case ActionProto::ActionInfoCase::kWaitForDocumentToBecomeComplete:
      success = ParseActionFromString(
          action_id, bytes, error_message,
          proto.mutable_wait_for_document_to_become_complete());
      break;
    case ActionProto::ActionInfoCase::kSendClickEvent:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_send_click_event());
      break;
    case ActionProto::ActionInfoCase::kSendTapEvent:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_send_tap_event());
      break;
    case ActionProto::ActionInfoCase::kJsClick:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_js_click());
      break;
    case ActionProto::ActionInfoCase::kSendKeystrokeEvents:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_send_keystroke_events());
      break;
    case ActionProto::ActionInfoCase::kSendChangeEvent:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_send_change_event());
      break;
    case ActionProto::ActionInfoCase::kSetElementAttribute:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_set_element_attribute());
      break;
    case ActionProto::ActionInfoCase::kSelectFieldValue:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_select_field_value());
      break;
    case ActionProto::ActionInfoCase::kFocusField:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_focus_field());
      break;
    case ActionProto::ActionInfoCase::kWaitForElementToBecomeStable:
      success = ParseActionFromString(
          action_id, bytes, error_message,
          proto.mutable_wait_for_element_to_become_stable());
      break;
    case ActionProto::ActionInfoCase::kCheckElementIsOnTop:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_check_element_is_on_top());
      break;
    case ActionProto::ActionInfoCase::kReleaseElements:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_release_elements());
      break;
    case ActionProto::ActionInfoCase::kDispatchJsEvent:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_dispatch_js_event());
      break;
    case ActionProto::ActionInfoCase::kSendKeyEvent:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_send_key_event());
      break;
    case ActionProto::ActionInfoCase::kSelectOptionElement:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_select_option_element());
      break;
    case ActionProto::ActionInfoCase::kCheckElementTag:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_check_element_tag());
      break;
    case ActionProto::ActionInfoCase::kCheckOptionElement:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_check_option_element());
      break;
    case ActionProto::ActionInfoCase::kSetPersistentUi:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_set_persistent_ui());
      break;
    case ActionProto::ActionInfoCase::kClearPersistentUi:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_clear_persistent_ui());
      break;
    case ActionProto::ActionInfoCase::kScrollIntoViewIfNeeded:
      success =
          ParseActionFromString(action_id, bytes, error_message,
                                proto.mutable_scroll_into_view_if_needed());
      break;
    case ActionProto::ActionInfoCase::kScrollWindow:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_scroll_window());
      break;
    case ActionProto::ActionInfoCase::kScrollContainer:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_scroll_container());
      break;
    case ActionProto::ActionInfoCase::kSetTouchableArea:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_set_touchable_area());
      break;
    case ActionProto::ActionInfoCase::kBlurField:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_blur_field());
      break;
    case ActionProto::ActionInfoCase::kResetPendingCredentials:
      success =
          ParseActionFromString(action_id, bytes, error_message,
                                proto.mutable_reset_pending_credentials());
      break;
    case ActionProto::ActionInfoCase::kSaveSubmittedPassword:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_save_submitted_password());
      break;
    case ActionProto::ActionInfoCase::kExecuteJs:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_execute_js());
      break;
    case ActionProto::ActionInfoCase::kJsFlow:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_js_flow());
      break;
    case ActionProto::ActionInfoCase::kExternalAction:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_external_action());
      break;
    case ActionProto::ActionInfoCase::kSetNativeValue:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_set_native_value());
      break;
    case ActionProto::ActionInfoCase::kSetNativeChecked:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_set_native_checked());
      break;
    case ActionProto::ActionInfoCase::kRegisterPasswordResetRequest:
      success = ParseActionFromString(
          action_id, bytes, error_message,
          proto.mutable_register_password_reset_request());
      break;
    case ActionProto::ActionInfoCase::kPromptQrCodeScan:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_prompt_qr_code_scan());
      break;
    case ActionProto::ActionInfoCase::kParseSingleTagXml:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_parse_single_tag_xml());
      break;
    case ActionProto::ActionInfoCase::kReportProgress:
      success = ParseActionFromString(action_id, bytes, error_message,
                                      proto.mutable_report_progress());
      break;
    case ActionProto::ActionInfoCase::ACTION_INFO_NOT_SET:
      // This is an "unknown action", handled as such in CreateAction.
      return proto;
  }
  // There's an implicit default case that ends up with success=true and an
  // empty proto if given an action_id that doesn't fit into ActionInfoCase.
  // This is the unknown action case. Doing it without an explicit default case
  // allows relying on exhaustive switch checks in the compiler.

  if (!success) {
    return absl::nullopt;
  }
  return proto;
}

// static
bool ProtocolUtils::ParseActions(ActionDelegate* delegate,
                                 const std::string& response,
                                 uint64_t* run_id,
                                 std::string* return_global_payload,
                                 std::string* return_script_payload,
                                 std::vector<std::unique_ptr<Action>>* actions,
                                 std::vector<std::unique_ptr<Script>>* scripts,
                                 bool* should_update_scripts,
                                 std::string* js_flow_library,
                                 std::string* report_token) {
  DCHECK(actions);
  DCHECK(scripts);

  ActionsResponseProto response_proto;
  if (!response_proto.ParseFromString(response)) {
    LOG(ERROR) << "Failed to parse assistant actions response.";
    return false;
  }

  if (run_id) {
    *run_id = response_proto.run_id();
  }
  if (return_global_payload) {
    *return_global_payload = response_proto.global_payload();
  }
  if (return_script_payload) {
    *return_script_payload = response_proto.script_payload();
  }
  if (js_flow_library) {
    *js_flow_library = std::move(*response_proto.mutable_js_flow_library());
  }
  // Only set the report token if it's empty; it should only be populated in the
  // initial response from GetActions beginning the script run.
  if (report_token && report_token->empty()) {
    *report_token = response_proto.report_token();
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
std::unique_ptr<Action> ProtocolUtils::ParseAction(
    ActionDelegate* delegate,
    const std::string& serialized_action) {
  ActionProto proto;
  if (!proto.ParseFromString(serialized_action)) {
    return nullptr;
  }
  return CreateAction(delegate, proto);
}

// static
std::string ProtocolUtils::CreateGetTriggerScriptsRequest(
    const GURL& url,
    const ClientContextProto& client_context,
    const ScriptParameters& script_parameters) {
  GetTriggerScriptsRequestProto request_proto;
  request_proto.set_url(url.spec());
  *request_proto.mutable_client_context() = client_context;
  *request_proto.mutable_script_parameters() =
      script_parameters.ToProto(/* only_non_sensitive_allowlisted = */ true);

  std::string serialized_request_proto;
  bool success = request_proto.SerializeToString(&serialized_request_proto);
  DCHECK(success);
  return serialized_request_proto;
}

// static
std::string ProtocolUtils::CreateReportProgressRequest(
    const std::string& token,
    const std::string& payload) {
  ReportProgressRequestProto request_proto;
  *request_proto.mutable_token() = token;
  *request_proto.mutable_payload() = payload;

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
    absl::optional<int>* trigger_condition_timeout_ms,
    absl::optional<std::unique_ptr<ScriptParameters>>* script_parameters) {
  DCHECK(trigger_scripts);
  DCHECK(additional_allowed_domains);
  DCHECK(trigger_condition_check_interval_ms);
  DCHECK(trigger_condition_timeout_ms);
  DCHECK(script_parameters);

  GetTriggerScriptsResponseProto response_proto;
  if (!response_proto.ParseFromString(response)) {
    LOG(ERROR) << "Failed to parse trigger scripts response";
    return false;
  }

  for (const auto& trigger_script_proto : response_proto.trigger_scripts()) {
    if (!ValidateTriggerCondition(trigger_script_proto.trigger_condition())) {
      return false;
    }
  }

  for (auto& trigger_script_proto : *response_proto.mutable_trigger_scripts()) {
    if (trigger_script_proto.user_interface().has_ui_timeout_ms()) {
      // Turn off scroll_to_hide if a UI timeout is set.
      trigger_script_proto.mutable_user_interface()->set_scroll_to_hide(false);
    }
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
  if (response_proto.has_trigger_condition_timeout_ms()) {
    *trigger_condition_timeout_ms =
        response_proto.trigger_condition_timeout_ms();
  }

  if (!response_proto.script_parameters().empty()) {
    std::vector<std::pair<std::string, std::string>> parameters;
    for (const auto& param : response_proto.script_parameters()) {
      parameters.emplace_back(param.name(), param.value());
    }
    *script_parameters = std::make_unique<ScriptParameters>(
        base::flat_map<std::string, std::string>(std::move(parameters)));
  }
  return true;
}

// static
bool ProtocolUtils::ParseTriggerScriptsByHashPrefix(
    const std::string& response,
    std::vector<std::pair<std::string, std::string>>* domainScripts) {
  DCHECK(domainScripts);

  GetTriggerScriptsByHashPrefixResponseProto response_proto;
  if (!response_proto.ParseFromString(response)) {
    LOG(ERROR) << "Failed to parse trigger scripts by hash prefix response";
    return false;
  }

  for (const auto& match : response_proto.match_info()) {
    domainScripts->emplace_back(
        match.domain(), match.trigger_scripts_response().SerializeAsString());
  }

  return true;
}

// static
bool ProtocolUtils::ValidateTriggerCondition(
    const TriggerScriptConditionProto& trigger_condition) {
  switch (trigger_condition.type_case()) {
    case TriggerScriptConditionProto::kAllOf:
      for (const auto& condition : trigger_condition.all_of().conditions()) {
        if (!ValidateTriggerCondition(condition)) {
          return false;
        }
      }
      return true;
    case TriggerScriptConditionProto::kAnyOf:
      for (const auto& condition : trigger_condition.any_of().conditions()) {
        if (!ValidateTriggerCondition(condition)) {
          return false;
        }
      }
      return true;
    case TriggerScriptConditionProto::kNoneOf:
      for (const auto& condition : trigger_condition.none_of().conditions()) {
        if (!ValidateTriggerCondition(condition)) {
          return false;
        }
      }
      return true;
    case TriggerScriptConditionProto::kPathPattern: {
      const re2::RE2 re(trigger_condition.path_pattern());
      if (!re.ok()) {
#ifdef NDEBUG
        VLOG(1) << "Invalid regexp in trigger condition";
#else
        VLOG(1) << "Invalid regexp in trigger condition "
                << trigger_condition.path_pattern();
#endif
        return false;
      }
      return true;
    }
    case TriggerScriptConditionProto::kDomainWithScheme: {
      const GURL domain(trigger_condition.domain_with_scheme());
      if (!domain.is_valid()) {
#ifdef NDEBUG
        VLOG(1) << "Invalid domain format in trigger condition";
#else
        VLOG(1) << "Invalid domain format in trigger condition "
                << trigger_condition.domain_with_scheme();
#endif
        return false;
      }
      return true;
    }
    case TriggerScriptConditionProto::kStoredLoginCredentials:
    case TriggerScriptConditionProto::kIsFirstTimeUser:
    case TriggerScriptConditionProto::kExperimentId:
    case TriggerScriptConditionProto::kKeyboardHidden:
    case TriggerScriptConditionProto::kScriptParameterMatch:
    case TriggerScriptConditionProto::kSelector:
    case TriggerScriptConditionProto::kDocumentReadyState:
    case TriggerScriptConditionProto::TYPE_NOT_SET:
      return true;
  }
}

// static
std::string ProtocolUtils::CreateGetUserDataRequest(
    uint64_t run_id,
    bool request_name,
    bool request_email,
    bool request_phone,
    bool request_shipping,
    const std::vector<std::string>& preexisting_address_ids,
    bool request_payment_methods,
    const std::vector<std::string>& supported_card_networks,
    const std::vector<std::string>& preexisting_payment_instrument_ids,
    const std::string& client_token) {
  GetUserDataRequestProto request_proto;
  request_proto.set_run_id(run_id);
  request_proto.set_request_name(request_name);
  request_proto.set_request_email(request_email);
  request_proto.set_request_phone(request_phone);

  if (request_shipping) {
    auto* address_request = request_proto.mutable_request_shipping_addresses();
    for (const std::string& id : preexisting_address_ids) {
      address_request->add_preexisting_ids(id);
    }
  }

  if (request_payment_methods) {
    auto* payment_methods_request =
        request_proto.mutable_request_payment_methods();
    payment_methods_request->set_client_token(client_token);
    for (const std::string& supported_card_network : supported_card_networks) {
      payment_methods_request->add_supported_card_networks(
          supported_card_network);
    }
    for (const std::string& id : preexisting_payment_instrument_ids) {
      payment_methods_request->add_preexisting_ids(id);
    }
  }

  std::string serialized_request_proto;
  bool success = request_proto.SerializeToString(&serialized_request_proto);
  DCHECK(success);
  return serialized_request_proto;
}

// static
RoundtripNetworkStats ProtocolUtils::ComputeNetworkStats(
    const std::string& response,
    const ServiceRequestSender::ResponseInfo& response_info,
    const std::vector<std::unique_ptr<Action>>& actions) {
  RoundtripNetworkStats stats;
  stats.set_num_roundtrips(1);
  stats.set_roundtrip_decoded_body_size_bytes(response.size());
  stats.set_roundtrip_encoded_body_size_bytes(
      response_info.encoded_body_length);
  for (const auto& action : actions) {
    RoundtripNetworkStats::ActionNetworkStats* action_stats =
        stats.add_action_stats();
    action_stats->set_action_info_case(
        static_cast<int>(action->proto().action_info_case()));

    std::string serialized_action;
    action->proto().SerializeToString(&serialized_action);
    action_stats->set_decoded_size_bytes(serialized_action.size());
  }
  return stats;
}

}  // namespace autofill_assistant
