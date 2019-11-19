// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"

namespace autofill_assistant {

Action::Action(ActionDelegate* delegate, const ActionProto& proto)
    : proto_(proto), delegate_(delegate) {}

Action::~Action() {}

void Action::ProcessAction(ProcessActionCallback callback) {
  processed_action_proto_ = std::make_unique<ProcessedActionProto>();
  InternalProcessAction(std::move(callback));
}

void Action::UpdateProcessedAction(ProcessedActionStatusProto status_proto) {
  UpdateProcessedAction(ClientStatus(status_proto));
}

void Action::UpdateProcessedAction(const ClientStatus& status) {
  // Safety check in case process action is run twice.
  *processed_action_proto_->mutable_action() = proto_;
  status.FillProto(processed_action_proto_.get());
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
    case ActionProto::ActionInfoCase::kClick:
      out << "Click";
      break;
    case ActionProto::ActionInfoCase::kSetFormValue:
      out << "KeyboardInput";
      break;
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
    case ActionProto::ActionInfoCase::kFocusElement:
      out << "FocusElement";
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
    case ActionProto::ActionInfoCase::kHighlightElement:
      out << "HighlightElement";
      break;
    case ActionProto::ActionInfoCase::kShowDetails:
      out << "ShowDetails";
      break;
    case ActionProto::ActionInfoCase::kReset:
      out << "Reset";
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
