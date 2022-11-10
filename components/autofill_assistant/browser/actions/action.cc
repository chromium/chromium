// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/wait_for_document_operation.h"
#include "components/autofill_assistant/browser/wait_for_dom_operation.h"

namespace autofill_assistant {

Action::Action(ActionDelegate* delegate, const ActionProto& proto)
    : proto_(proto), delegate_(delegate) {}

Action::~Action() {}

Action::ActionData::ActionData() = default;
Action::ActionData::~ActionData() = default;

Action::ActionData& Action::GetActionData() {
  return action_data_;
}

void Action::ProcessAction(ProcessActionCallback callback) {
  action_stopwatch_.StartActiveTime();
  delegate_->GetLogInfo().Clear();
  processed_action_proto_ = std::make_unique<ProcessedActionProto>();
  InternalProcessAction(base::BindOnce(&Action::RecordActionTimes,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       std::move(callback)));
}

void Action::RecordActionTimes(
    ProcessActionCallback callback,
    std::unique_ptr<ProcessedActionProto> processed_action_proto) {
  // Record times.
  action_stopwatch_.Stop();

  processed_action_proto->mutable_timing_stats()->set_delay_ms(
      proto_.action_delay_ms());
  processed_action_proto->mutable_timing_stats()->set_active_time_ms(
      action_stopwatch_.TotalActiveTime().InMilliseconds());
  processed_action_proto->mutable_timing_stats()->set_wait_time_ms(
      action_stopwatch_.TotalWaitTime().InMilliseconds());

  std::move(callback).Run(std::move(processed_action_proto));
}

void Action::UpdateProcessedAction(ProcessedActionStatusProto status_proto) {
  UpdateProcessedAction(ClientStatus(status_proto));
}

void Action::UpdateProcessedAction(const ClientStatus& status) {
  // Safety check in case process action is run twice.
  *processed_action_proto_->mutable_action() = proto_;
  status.FillProto(processed_action_proto_.get());

  auto& log_info = delegate_->GetLogInfo();
  processed_action_proto_->mutable_status_details()->MergeFrom(log_info);
}

void Action::OnWaitForElementTimed(
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& element_status,
    base::TimeDelta wait_time) {
  action_stopwatch_.TransferToWaitTime(wait_time);
  std::move(callback).Run(element_status);
}

// static
std::vector<std::string> Action::ExtractVector(
    const google::protobuf::RepeatedPtrField<std::string>& repeated_strings) {
  std::vector<std::string> vector;
  for (const auto& string : repeated_strings) {
    vector.emplace_back(string);
  }
  return vector;
}

std::ostream& operator<<(std::ostream& out, const Action& action) {
  out << action.proto().action_info_case();
  return out;
}

std::ostream& operator<<(std::ostream& out,
                         const ActionProto::ActionInfoCase& action_case) {
#ifdef NDEBUG
  out << static_cast<int>(action_case);
  return out;
#else
  switch (action_case) {
    case ActionProto::ActionInfoCase::kSelectOption:
      out << "SelectOption";
      break;
    case ActionProto::ActionInfoCase::kNavigate:
      out << "Navigate";
      break;
    case ActionProto::ActionInfoCase::kPrompt:
      out << "Prompt";
      break;
    case ActionProto::ActionInfoCase::kTell:
      out << "Tell";
      break;
    case ActionProto::ActionInfoCase::kUpdateClientSettings:
      out << "UpdateClientSettings";
      break;
    case ActionProto::ActionInfoCase::kShowCast:
      out << "ShowCast";
      break;
    case ActionProto::ActionInfoCase::kWaitForDom:
      out << "WaitForDom";
      break;
    case ActionProto::ActionInfoCase::kUseCard:
      out << "UseCard";
      break;
    case ActionProto::ActionInfoCase::kUseAddress:
      out << "UseAddress";
      break;
    case ActionProto::ActionInfoCase::kUploadDom:
      out << "UploadDom";
      break;
    case ActionProto::ActionInfoCase::kShowProgressBar:
      out << "ShowProgressBar";
      break;
    case ActionProto::ActionInfoCase::kShowDetails:
      out << "ShowDetails";
      break;
    case ActionProto::ActionInfoCase::kStop:
      out << "Stop";
      break;
    case ActionProto::ActionInfoCase::kCollectUserData:
      out << "CollectUserData";
      break;
    case ActionProto::ActionInfoCase::kSetAttribute:
      out << "SetAttribute";
      break;
    case ActionProto::ActionInfoCase::kShowInfoBox:
      out << "ShowInfoBox";
      break;
    case ActionProto::ActionInfoCase::kExpectNavigation:
      out << "ExpectNavigation";
      break;
    case ActionProto::ActionInfoCase::kWaitForNavigation:
      out << "WaitForNavigation";
      break;
    case ActionProto::ActionInfoCase::kConfigureBottomSheet:
      out << "ConfigureBottomSheet";
      break;
    case ActionProto::ActionInfoCase::kShowForm:
      out << "ShowForm";
      break;
    case ActionProto::ActionInfoCase::kPopupMessage:
      out << "PopupMessage";
      break;
    case ActionProto::ActionInfoCase::kWaitForDocument:
      out << "WaitForDocument";
      break;
    case ActionProto::ActionInfoCase::kShowGenericUi:
      out << "ShowGenericUi";
      break;
    case ActionProto::ActionInfoCase::kGeneratePasswordForFormField:
      out << "GeneratePasswordForFormField";
      break;
    case ActionProto::kSaveGeneratedPassword:
      out << "SaveGeneratedPassword";
      break;
    case ActionProto::ActionInfoCase::kConfigureUiState:
      out << "ConfigureUiState";
      break;
    case ActionProto::ActionInfoCase::kPresaveGeneratedPassword:
      out << "PresaveGeneratedPassword";
      break;
    case ActionProto::ActionInfoCase::kGetElementStatus:
      out << "GetElementStatus";
      break;
    case ActionProto::ActionInfoCase::kScrollIntoView:
      out << "ScrollIntoView";
      break;
    case ActionProto::ActionInfoCase::kWaitForDocumentToBecomeInteractive:
      out << "WaitForDocumentToBecomeInteractive";
      break;
    case ActionProto::ActionInfoCase::kWaitForDocumentToBecomeComplete:
      out << "WaitForDocumentToBecomeComplete";
      break;
    case ActionProto::ActionInfoCase::kSendClickEvent:
      out << "SendClickEvent";
      break;
    case ActionProto::ActionInfoCase::kSendTapEvent:
      out << "SendTapEvent";
      break;
    case ActionProto::ActionInfoCase::kJsClick:
      out << "JsClick";
      break;
    case ActionProto::ActionInfoCase::kSendKeystrokeEvents:
      out << "SendKeystrokeEvents";
      break;
    case ActionProto::ActionInfoCase::kSendChangeEvent:
      out << "SendChangeEvent";
      break;
    case ActionProto::ActionInfoCase::kSetElementAttribute:
      out << "SetElementAttribute";
      break;
    case ActionProto::ActionInfoCase::kSelectFieldValue:
      out << "SelectFieldValue";
      break;
    case ActionProto::ActionInfoCase::kFocusField:
      out << "FocusField";
      break;
    case ActionProto::ActionInfoCase::kWaitForElementToBecomeStable:
      out << "WaitForElementToBecomeStable";
      break;
    case ActionProto::ActionInfoCase::kCheckElementIsOnTop:
      out << "CheckElementIsOnTop";
      break;
    case ActionProto::ActionInfoCase::kReleaseElements:
      out << "ReleaseElements";
      break;
    case ActionProto::ActionInfoCase::kDispatchJsEvent:
      out << "DispatchJsEvent";
      break;
    case ActionProto::ActionInfoCase::kSendKeyEvent:
      out << "SendKeyEvent";
      break;
    case ActionProto::ActionInfoCase::kSelectOptionElement:
      out << "SelectOptionElement";
      break;
    case ActionProto::ActionInfoCase::kCheckElementTag:
      out << "CheckElementTag";
      break;
    case ActionProto::ActionInfoCase::kCheckOptionElement:
      out << "CheckOptionElement";
      break;
    case ActionProto::ActionInfoCase::kSetPersistentUi:
      out << "SetPersistentUi";
      break;
    case ActionProto::ActionInfoCase::kClearPersistentUi:
      out << "ClearPersistentUi";
      break;
    case ActionProto::ActionInfoCase::kScrollIntoViewIfNeeded:
      out << "ScrollIntoViewIfNeeded";
      break;
    case ActionProto::ActionInfoCase::kScrollWindow:
      out << "ScrollWindow";
      break;
    case ActionProto::ActionInfoCase::kScrollContainer:
      out << "ScrollContainer";
      break;
    case ActionProto::ActionInfoCase::kSetTouchableArea:
      out << "SetTouchableArea";
      break;
    case ActionProto::ActionInfoCase::kBlurField:
      out << "BlurField";
      break;
    case ActionProto::ActionInfoCase::kResetPendingCredentials:
      out << "ResetPendingCredentials";
      break;
    case ActionProto::ActionInfoCase::kSaveSubmittedPassword:
      out << "SaveSubmittedPassword";
      break;
    case ActionProto::ActionInfoCase::kExecuteJs:
      out << "ExecuteJs";
      break;
    case ActionProto::ActionInfoCase::kJsFlow:
      out << "JsFlow";
      break;
    case ActionProto::ActionInfoCase::kExternalAction:
      out << "ExternalAction";
      break;
    case ActionProto::ActionInfoCase::kRegisterPasswordResetRequest:
      out << "RegisterPasswordResetRequest";
      break;
    case ActionProto::ActionInfoCase::kSetNativeValue:
      out << "SetNativeValue";
      break;
    case ActionProto::ActionInfoCase::kSetNativeChecked:
      out << "SetNativeChecked";
      break;
    case ActionProto::ActionInfoCase::kParseSingleTagXml:
      out << "ParseSingleTagXml";
      break;
    case ActionProto::ActionInfoCase::kPromptQrCodeScan:
      out << "PromptQrCodeScan";
      break;
    case ActionProto::ActionInfoCase::kReportProgress:
      out << "ReportProgress";
      break;
    case ActionProto::ActionInfoCase::kRegisterInterruptScripts:
      out << "RegisterInterruptScripts";
      break;
    case ActionProto::ActionInfoCase::kRegisterJsInterruptForFlow:
      out << "RegisterJsInterruptForFlow";
      break;
    case ActionProto::ActionInfoCase::ACTION_INFO_NOT_SET:
      out << "ACTION_INFO_NOT_SET";
      break;
      // Intentionally no default case to make compilation fail if a new value
      // was added to the enum but not to this list.
  }
  return out;
#endif  // NDEBUG
}

}  // namespace autofill_assistant
