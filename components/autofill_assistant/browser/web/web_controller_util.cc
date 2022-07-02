// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/web_controller_util.h"

#include "base/logging.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_runtime.h"
#include "components/autofill_assistant/browser/js_flow_util.h"
#include "components/autofill_assistant/browser/service.pb.h"

// Necessary to avoid a type collision while building for Windows.
#if defined(GetClassName)
#undef GetClassName
#endif  // defined(GetClassName)

namespace autofill_assistant {

namespace {

template <typename S>
void AddStackEntry(const S& s,
                   const std::string& devtools_source_url,
                   const JsLineOffsets& js_line_offsets,
                   UnexpectedErrorInfoProto* info) {
  int line_number = s.GetLineNumber();
  if (js_line_offsets.contains(devtools_source_url)) {
    const int line_offset = js_line_offsets.at(devtools_source_url);
    line_number -= line_offset;
    DCHECK(line_number >= 0)
        << "Line number (" << s.GetLineNumber()
        << ") pointing into the offset (" << line_offset
        << ") for devtools source url (" << devtools_source_url
        << ") included in the stack.";
  }

  info->add_js_exception_locations(
      js_flow_util::GetExceptionLocation(devtools_source_url));
  info->add_js_exception_line_numbers(line_number);
  info->add_js_exception_column_numbers(s.GetColumnNumber());
}

void AddStackEntries(const runtime::ExceptionDetails* exception,
                     const JsLineOffsets& js_line_offsets,
                     const int num_stack_entries_to_drop,
                     UnexpectedErrorInfoProto* info) {
  if (!exception->HasStackTrace()) {
    AddStackEntry(*exception, exception->HasUrl() ? exception->GetUrl() : "",
                  js_line_offsets, info);
    return;
  }
  const std::vector<std::unique_ptr<runtime::CallFrame>>& frames =
      *exception->GetStackTrace()->GetCallFrames();
  const int num_stack_entries = static_cast<int>(frames.size());

  DCHECK(num_stack_entries > num_stack_entries_to_drop)
      << "Trying to drop too many stack entries.";
  const int num_frames_to_use =
      std::max(num_stack_entries - num_stack_entries_to_drop, 1);

  for (int i = 0; i < num_frames_to_use; i++) {
    const auto& frame = *frames[i];
    AddStackEntry(frame, frame.GetUrl(), js_line_offsets, info);
  }
}
}  // namespace

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
    const int line,
    const runtime::ExceptionDetails* exception,
    const JsLineOffsets& js_line_offsets,
    const int num_stack_entries_to_drop) {
  ClientStatus status = UnexpectedDevtoolsErrorStatus(reply_status, file, line);
  status.set_proto_status(UNEXPECTED_JS_ERROR);
  if (!exception) {
    return status;
  }

  auto* info = status.mutable_details()->mutable_unexpected_error_info();
  if (exception->HasException() && exception->GetException()->HasClassName()) {
    info->set_js_exception_classname(exception->GetException()->GetClassName());
  }
  AddStackEntries(exception, js_line_offsets, num_stack_entries_to_drop, info);
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
