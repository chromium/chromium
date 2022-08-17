// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/js_flow_util.h"
#include "base/base64.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "components/autofill_assistant/browser/model.pb.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web/web_controller_util.h"

namespace autofill_assistant {

namespace {

// The field names of the object that a JS flow action expects flows to return.
// DO NOT CHANGE THIS without changing the backend at the same time.
// This belongs to |js_flow_action| - it has been moved here to make testing
// more convenient.
const char kStatusKey[] = "status";
const char kResultKey[] = "result";

// The key for the action specific result (e.g. WaitForDomProto.Result for
// WaitForDom) in the result object returned by runNativeAction().
// DO NOT CHANGE
const char kActionSpecificResultKey[] = "actionSpecificResult";
// The key for the the navigation started boolean in the result object returned
// by runNativeAction().
// DO NOT CHANGE
const char kNavigationStartedKey[] = "navigationStarted";
// The key for the autofill error info in the result object returned by
// runNativeAction().
// DO NOT CHANGE
const char kAutofillErrorInfo[] = "autofillErrorInfo";
// By appending //# sourceUrl=some_name.js to a js snippet the snippet can be
// identified in devtools by url = some_name.js (for example in exceptions).
constexpr char kSourceUrlCommentPrefix[] = "\n//# sourceURL=";

// Returns true for remote object types that flows are allowed to return. This
// is mostly used to filter types like FUNCTION which would otherwise slip
// through.
bool IsAllowedRemoteType(runtime::RemoteObjectType type) {
  switch (type) {
    case runtime::RemoteObjectType::OBJECT:
    case runtime::RemoteObjectType::NUMBER:
    case runtime::RemoteObjectType::BOOLEAN:
      return true;
    default:
      return false;
  }
}

ClientStatus ClientStatusWithSourceLocation(
    ProcessedActionStatusProto proto_status,
    const std::string& file,
    int line) {
  ClientStatus status(proto_status);
  auto* error_info = status.mutable_details()->mutable_unexpected_error_info();
  error_info->set_source_file(file);
  error_info->set_source_line_number(line);
  return status;
}

}  // namespace

namespace js_flow_util {

ClientStatus ExtractFlowReturnValue(
    const DevtoolsClient::ReplyStatus& devtools_reply_status,
    runtime::EvaluateResult* devtools_result,
    std::unique_ptr<base::Value>& out_flow_result,
    const JsLineOffsets& js_line_offsets) {
  ClientStatus status =
      CheckJavaScriptResult(devtools_reply_status, devtools_result, __FILE__,
                            __LINE__, js_line_offsets);
  if (!status.ok()) {
    return status;
  }

  const runtime::RemoteObject* remote_object = devtools_result->GetResult();
  if (!remote_object->HasValue() &&
      remote_object->GetType() == runtime::RemoteObjectType::UNDEFINED) {
    // Special case: flows are allowed to return nothing.
    return status;
  }

  if (!remote_object->HasValue() ||
      !IsAllowedRemoteType(remote_object->GetType())) {
    status.set_proto_status(INVALID_ACTION);
    status.mutable_details()
        ->mutable_unexpected_error_info()
        ->set_devtools_error_message(
            base::StrCat({"Invalid return value: only primitive non-string "
                          "values are allowed, but got RemoteObject of type ",
                          base::NumberToString(
                              static_cast<int>(remote_object->GetType()))}));
    return status;
  }

  out_flow_result =
      base::Value::ToUniquePtrValue(remote_object->GetValue()->Clone());
  return OkClientStatus();
}

ClientStatus ExtractJsFlowActionReturnValue(
    const base::Value& value,
    std::unique_ptr<base::Value>& out_result_value) {
  if (value.is_none()) {
    return ClientStatus(ACTION_APPLIED);
  }

  if (!value.is_dict()) {
    VLOG(1) << "JS flow did not return a dictionary.";
    return ClientStatusWithSourceLocation(INVALID_ACTION, __FILE__, __LINE__);
  }

  const base::Value::Dict* dict = value.GetIfDict();
  absl::optional<int> flow_status = dict->FindInt(kStatusKey);
  const base::Value* flow_return_value = dict->Find(kResultKey);
  if (!flow_status || !ProcessedActionStatusProto_IsValid(*flow_status)) {
    VLOG(1) << "JS flow did not return a valid ActionStatus in " << kStatusKey;
    return ClientStatusWithSourceLocation(INVALID_ACTION, __FILE__, __LINE__);
  }

  if (flow_return_value) {
    out_result_value =
        std::make_unique<base::Value>(flow_return_value->Clone());
  }
  return ClientStatus(static_cast<ProcessedActionStatusProto>(*flow_status));
}

std::string SerializeToBase64(const google::protobuf::MessageLite* proto) {
  std::string serialized_result_base64;
  base::Base64Encode(proto->SerializeAsString(), &serialized_result_base64);
  return serialized_result_base64;
}

namespace {

absl::optional<std::string> SerializeActionResult(
    const ProcessedActionProto& processed_action) {
  const google::protobuf::MessageLite* proto;
  switch (processed_action.result_data_case()) {
    case ProcessedActionProto::kPromptChoice:
      proto = &processed_action.prompt_choice();
      break;
    case ProcessedActionProto::kCollectUserDataResult:
      proto = &processed_action.collect_user_data_result();
      break;
    case ProcessedActionProto::kWaitForDomResult:
      proto = &processed_action.wait_for_dom_result();
      break;
    case ProcessedActionProto::kFormResult:
      proto = &processed_action.form_result();
      break;
    case ProcessedActionProto::kWaitForDocumentResult:
      proto = &processed_action.wait_for_document_result();
      break;
    case ProcessedActionProto::kShowGenericUiResult:
      proto = &processed_action.show_generic_ui_result();
      break;
    case ProcessedActionProto::kGetElementStatusResult:
      proto = &processed_action.get_element_status_result();
      break;
    case ProcessedActionProto::kUploadDomResult:
      proto = &processed_action.upload_dom_result();
      break;
    case ProcessedActionProto::kCheckOptionElementResult:
      proto = &processed_action.check_option_element_result();
      break;
    case ProcessedActionProto::kSendKeyStrokeEventsResult:
      proto = &processed_action.send_key_stroke_events_result();
      break;
    case ProcessedActionProto::kJsFlowResult:
      proto = &processed_action.js_flow_result();
      break;
    case ProcessedActionProto::kSaveSubmittedPasswordResult:
      proto = &processed_action.save_submitted_password_result();
      break;
    case ProcessedActionProto::kExternalActionResult:
      proto = &processed_action.external_action_result();
      break;
    case ProcessedActionProto::RESULT_DATA_NOT_SET:
      return absl::nullopt;
  }

  return SerializeToBase64(proto);
}

}  // namespace

std::unique_ptr<base::Value> NativeActionResultToResultValue(
    const ProcessedActionProto& processed_action) {
  base::Value::Dict result_value;
  result_value.Set({kNavigationStartedKey},
                   processed_action.navigation_info().started());

  const absl::optional<std::string> serialized_result =
      SerializeActionResult(processed_action);
  if (serialized_result.has_value()) {
    result_value.Set(kActionSpecificResultKey, *serialized_result);
  }

  if (processed_action.status_details().has_autofill_error_info()) {
    result_value.Set(
        kAutofillErrorInfo,
        SerializeToBase64(
            &processed_action.status_details().autofill_error_info()));
  }

  return std::make_unique<base::Value>(std::move(result_value));
}

std::string GetDevtoolsSourceUrl(
    UnexpectedErrorInfoProto::JsExceptionLocation js_exception_location) {
  return UnexpectedErrorInfoProto::JsExceptionLocation_Name(
      js_exception_location);
}

UnexpectedErrorInfoProto::JsExceptionLocation GetExceptionLocation(
    const std::string& devtools_source_url) {
  UnexpectedErrorInfoProto::JsExceptionLocation js_exception_location;
  return UnexpectedErrorInfoProto::JsExceptionLocation_Parse(
             devtools_source_url, &js_exception_location)
             ? js_exception_location
             : UnexpectedErrorInfoProto::UNKNOWN;
}

std::string GetDevtoolsSourceUrlCommentToAppend(
    UnexpectedErrorInfoProto::JsExceptionLocation js_exception_location) {
  if (js_exception_location == UnexpectedErrorInfoProto::UNKNOWN) {
    return "";
  }

  return base::StrCat(
      {kSourceUrlCommentPrefix, GetDevtoolsSourceUrl(js_exception_location)});
}

}  // namespace js_flow_util
}  // namespace autofill_assistant
