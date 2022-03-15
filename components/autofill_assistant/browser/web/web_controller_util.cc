// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/web_controller_util.h"

#include "components/autofill_assistant/browser/devtools/devtools/domains/types_runtime.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

ClientStatus UnexpectedErrorStatus(const std::string& file, int line) {
  ClientStatus status(OTHER_ACTION_STATUS);
  auto* info = status.mutable_details()->mutable_unexpected_error_info();
  info->set_source_file(file);
  info->set_source_line_number(line);
  return status;
}

ClientStatus UnexpectedDevtoolsErrorStatus(
    const DevtoolsClient::ReplyStatus& reply_status,
    const std::string& file,
    int line) {
  ClientStatus status = UnexpectedErrorStatus(file, line);
  if (!reply_status.is_ok()) {
    auto* info = status.mutable_details()->mutable_unexpected_error_info();
    info->set_devtools_error_code(reply_status.error_code);
    info->set_devtools_error_message(reply_status.error_message);
  }
  return status;
}

ClientStatus JavaScriptErrorStatus(
    const DevtoolsClient::ReplyStatus& reply_status,
    const std::string& file,
    int line,
    const runtime::ExceptionDetails* exception) {
  ClientStatus status = UnexpectedDevtoolsErrorStatus(reply_status, file, line);
  status.set_proto_status(UNEXPECTED_JS_ERROR);
  if (exception) {
    auto* info = status.mutable_details()->mutable_unexpected_error_info();
    if (exception->HasException() &&
        exception->GetException()->HasClassName()) {
      info->set_js_exception_classname(
          exception->GetException()->GetClassName());
    }
    if (exception->HasStackTrace()) {
      for (const auto& frame : *exception->GetStackTrace()->GetCallFrames()) {
        info->add_js_exception_line_numbers(frame->GetLineNumber());
        info->add_js_exception_column_numbers(frame->GetColumnNumber());
      }
    } else {
      info->add_js_exception_line_numbers(exception->GetLineNumber());
      info->add_js_exception_column_numbers(exception->GetColumnNumber());
    }
  }
  return status;
}

void FillWebControllerErrorInfo(
    WebControllerErrorInfoProto::WebAction failed_web_action,
    ClientStatus* status) {
  status->mutable_details()
      ->mutable_web_controller_error_info()
      ->set_failed_web_action(failed_web_action);
}

bool SafeGetObjectId(const runtime::RemoteObject* result, std::string* out) {
  if (result && result->HasObjectId()) {
    *out = result->GetObjectId();
    return true;
  }
  return false;
}

bool SafeGetStringValue(const runtime::RemoteObject* result, std::string* out) {
  if (result && result->HasValue() && result->GetValue()->is_string()) {
    *out = result->GetValue()->GetString();
    return true;
  }
  return false;
}

bool SafeGetIntValue(const runtime::RemoteObject* result, int* out) {
  if (result && result->HasValue() && result->GetValue()->is_int()) {
    *out = result->GetValue()->GetInt();
    return true;
  }
  *out = 0;
  return false;
}

bool SafeGetBool(const runtime::RemoteObject* result, bool* out) {
  if (result && result->HasValue() && result->GetValue()->is_bool()) {
    *out = result->GetValue()->GetBool();
    return true;
  }
  *out = false;
  return false;
}

void AddRuntimeCallArgumentObjectId(
    const std::string& object_id,
    std::vector<std::unique_ptr<runtime::CallArgument>>* arguments) {
  arguments->emplace_back(
      runtime::CallArgument::Builder().SetObjectId(object_id).Build());
}

}  //  namespace autofill_assistant
