// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/js_flow_util.h"
#include "base/strings/strcat.h"
#include "components/autofill_assistant/browser/model.pb.h"
#include "components/autofill_assistant/browser/web/web_controller_util.h"

namespace autofill_assistant {

namespace {

// The field names of the object that a JS flow action expects flows to return.
// DO NOT CHANGE THIS without changing the backend at the same time.
// This belongs to |js_flow_action| - it has been moved here to make testing
// more convenient.
const char kStatusKey[] = "status";
const char kResultKey[] = "result";

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

bool ContainsOnlyAllowedValues(const base::Value& value,
                               std::string& out_error_message) {
  switch (value.type()) {
    case base::Value::Type::NONE:
    case base::Value::Type::BOOLEAN:
    case base::Value::Type::INTEGER:
    case base::Value::Type::DOUBLE:
      return true;
    case base::Value::Type::STRING:
      out_error_message.assign("Strings are not supported");
      return false;
    case base::Value::Type::BINARY:
      out_error_message.assign("Binary data are not supported");
      return false;
    case base::Value::Type::DICT: {
      for (const auto [key, nested_value] : *value.GetIfDict()) {
        if (!ContainsOnlyAllowedValues(nested_value, out_error_message)) {
          return false;
        }
      }
      return true;
    }
    case base::Value::Type::LIST: {
      for (const auto& entry : *value.GetIfList()) {
        if (!ContainsOnlyAllowedValues(entry, out_error_message)) {
          return false;
        }
      }
      return true;
    }
  }
}

ClientStatus ExtractFlowReturnValue(
    const DevtoolsClient::ReplyStatus& devtools_reply_status,
    runtime::EvaluateResult* devtools_result,
    std::unique_ptr<base::Value>& out_flow_result) {
  ClientStatus status = CheckJavaScriptResult(
      devtools_reply_status, devtools_result, __FILE__, __LINE__);
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

  std::string error_message;
  if (!ContainsOnlyAllowedValues(*remote_object->GetValue(), error_message)) {
    status.set_proto_status(INVALID_ACTION);
    status.mutable_details()
        ->mutable_unexpected_error_info()
        ->set_devtools_error_message(error_message);
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
    return ClientStatusWithSourceLocation(INVALID_ACTION, __FILE__, __LINE__);
  }

  const base::Value::Dict* dict = value.GetIfDict();
  absl::optional<int> flow_status = dict->FindInt(kStatusKey);
  const base::Value* flow_return_value = dict->Find(kResultKey);
  if (!flow_status || !ProcessedActionStatusProto_IsValid(*flow_status)) {
    return ClientStatusWithSourceLocation(INVALID_ACTION, __FILE__, __LINE__);
  }

  if (flow_return_value) {
    out_result_value =
        std::make_unique<base::Value>(flow_return_value->Clone());
  }
  return ClientStatus(static_cast<ProcessedActionStatusProto>(*flow_status));
}

}  // namespace js_flow_util
}  // namespace autofill_assistant
