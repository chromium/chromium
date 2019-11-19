// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_WEB_CONTROLLER_UTIL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_WEB_CONTROLLER_UTIL_H_

#include <string>
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_runtime.h"
#include "components/autofill_assistant/browser/devtools/devtools_client.h"

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
    const runtime::ExceptionDetails* exception);

// Makes sure that the given EvaluateResult exists, is successful and contains a
// result.
template <typename T>
ClientStatus CheckJavaScriptResult(
    const DevtoolsClient::ReplyStatus& reply_status,
    T* result,
    const char* file,
    int line) {
  if (!result)
    return JavaScriptErrorStatus(reply_status, file, line, nullptr);
  if (result->HasExceptionDetails())
    return JavaScriptErrorStatus(reply_status, file, line,
                                 result->GetExceptionDetails());
  if (!result->GetResult())
    return JavaScriptErrorStatus(reply_status, file, line, nullptr);
  return OkClientStatus();
}

// Fills a ClientStatus with appropriate details for a Chrome Autofill error.
ClientStatus FillAutofillErrorStatus(ClientStatus status);

// Safely gets an object id from a RemoteObject
bool SafeGetObjectId(const runtime::RemoteObject* result, std::string* out);

// Safely gets a string value from a RemoteObject
bool SafeGetStringValue(const runtime::RemoteObject* result, std::string* out);

// Safely gets a int value from a RemoteObject.
bool SafeGetIntValue(const runtime::RemoteObject* result, int* out);

// Safely gets a boolean value from a RemoteObject
bool SafeGetBool(const runtime::RemoteObject* result, bool* out);

}  //  namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_WEB_CONTROLLER_UTIL_H_
