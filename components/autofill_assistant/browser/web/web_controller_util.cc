// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/web_controller_util.h"

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
    info->set_js_exception_line_number(exception->GetLineNumber());
    info->set_js_exception_column_number(exception->GetColumnNumber());
  }
  return status;
}

ClientStatus FillAutofillErrorStatus(ClientStatus status) {
  status.mutable_details()
      ->mutable_autofill_error_info()
      ->set_autofill_error_status(status.proto_status());
  return status;
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

}  //  namespace autofill_assistant
