// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_WEB_CONTROLLER_UTIL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_WEB_CONTROLLER_UTIL_H_

#include <string>
#include "base/values.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_runtime.h"
#include "components/autofill_assistant/browser/devtools/devtools_client.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

// Builds a ClientStatus appropriate for an unexpected error.
//
// This should only be used in situations where getting an error cannot be
// anything but a bug in the client and no devtools ReplyStatus is available.
ClientStatus UnexpectedErrorStatus(const std::string& file, int line);

// Builds a ClientStatus appropriate for an unexpected error in a devtools
// response.
//
// This should only be used in situations where getting an error cannot be
// anything but a bug in the client.
ClientStatus UnexpectedDevtoolsErrorStatus(
    const DevtoolsClient::ReplyStatus& reply_status,
    const std::string& file,
    int line);

// Builds a ClientStatus appropriate for a JavaScript error.
ClientStatus JavaScriptErrorStatus(
    const DevtoolsClient::ReplyStatus& reply_status,
    const std::string& file,
    int line,
    const runtime::ExceptionDetails* exception,
    int js_line_offset = 0,
    int num_stack_entries_to_drop = 0);

// Makes sure that the given EvaluateResult exists, is successful and contains a
// result.
template <typename T>
ClientStatus CheckJavaScriptResult(
    const DevtoolsClient::ReplyStatus& reply_status,
    T* result,
    const char* file,
    int line,
    int js_line_offset = 0,
    int num_stack_entries_to_drop = 0) {
  if (!result)
    return JavaScriptErrorStatus(reply_status, file, line, nullptr,
                                 js_line_offset, num_stack_entries_to_drop);
  if (result->HasExceptionDetails())
    return JavaScriptErrorStatus(reply_status, file, line,
                                 result->GetExceptionDetails(), js_line_offset,
                                 num_stack_entries_to_drop);
  if (!result->GetResult())
    return JavaScriptErrorStatus(reply_status, file, line, nullptr,
                                 js_line_offset, num_stack_entries_to_drop);
  return OkClientStatus();
}

// Fills a ClientStatus with appropriate details from the
void FillWebControllerErrorInfo(
    WebControllerErrorInfoProto::WebAction failed_web_action,
    ClientStatus* status);

// Safely gets an object id from a RemoteObject
bool SafeGetObjectId(const runtime::RemoteObject* result, std::string* out);

// Safely gets a string value from a RemoteObject
bool SafeGetStringValue(const runtime::RemoteObject* result, std::string* out);

// Safely gets a int value from a RemoteObject.
bool SafeGetIntValue(const runtime::RemoteObject* result, int* out);

// Safely gets a boolean value from a RemoteObject
bool SafeGetBool(const runtime::RemoteObject* result, bool* out);

// Add a new runtime::CallArgument to the list.
template <typename T>
void AddRuntimeCallArgument(
    const T& value,
    std::vector<std::unique_ptr<runtime::CallArgument>>* arguments) {
  arguments->emplace_back(
      runtime::CallArgument::Builder()
          .SetValue(base::Value::ToUniquePtrValue(base::Value(value)))
          .Build());
}

// Add a new runtime::CallArgument from the object_id.
void AddRuntimeCallArgumentObjectId(
    const std::string& object_id,
    std::vector<std::unique_ptr<runtime::CallArgument>>* arguments);

}  //  namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_WEB_CONTROLLER_UTIL_H_
