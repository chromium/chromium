// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_JS_FLOW_UTIL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_JS_FLOW_UTIL_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/devtools/devtools_client.h"
#include "components/autofill_assistant/browser/web/web_controller_util.h"

namespace autofill_assistant::js_flow_util {

constexpr char kMainFrame[] = "";

// Returns true if |value| contains only allowed value types, which are INT,
// BOOL, DOUBLE, and NONE. Dictionaries and lists are allowed, so long as they
// only contain allowed value types too. |out_error_message| will contain
// details if this returns false.
bool ContainsOnlyAllowedValues(const base::Value& value,
                               std::string& out_error_message);

// Converts an incoming |devtools_result| into a value that is allowed to leave
// the JS sandbox as a flow return value, see |ContainsOnlyAllowedValues|.
// - If |devtools_reply_status| is not ok, this will return OTHER_ACTION_STATUS,
// indicating that this is most likely a client bug.
// - If |devtools_result| does not contain a result, this will return an OK
// client status.
// - If an exception was thrown, this will return UNEXPECTED_JS_ERROR
// containing a sanitized stack trace (i.e., line and column numbers only, no
// error messages).
// - If |devtools_result| contains an unsupported value (e.g., a string, or an
// unserializable value such as a function), this will return INVALID_ACTION.
// Additional information may be available in the status details.
ClientStatus ExtractFlowReturnValue(
    const DevtoolsClient::ReplyStatus& devtools_reply_status,
    runtime::EvaluateResult* devtools_result,
    std::unique_ptr<base::Value>& out_flow_result,
    const JsLineOffsets& js_line_offsets);

// Extracts client status and optionally return value from |value|. Expects
// status and result to be in specific fields (see .cc) Other fields are
// ignored.
//
// This returns one the following statuses. In case of error, a source line
// number is provided in the status details to allow disambiguating.
//
// <value.status>, [out_result_value]:
//   - value is a dictionary, value.status exists, is an int and a valid
//     ProcessedActionStatusProto enum. It need not be ACTION_APPLIED.
//     If value.result exists, it will be assigned to |out_result_value|.
// INVALID_ACTION:
//   - |value| is not a dictionary and not NONE
//   - |value| does not contain a "status" integer field containing a valid
//     ProcessedActionStatusProto.
// ACTION_APPLIED:
//   - [value] is NONE
ClientStatus ExtractJsFlowActionReturnValue(
    const base::Value& value,
    std::unique_ptr<base::Value>& out_result_value);

// Converts the processed action result from runNativeAction to the Value that
// will be returned to the JS sandbox.
std::unique_ptr<base::Value> NativeActionResultToResultValue(
    const ProcessedActionProto& processed_action);

// Serializes the proto as base64.
std::string SerializeToBase64(const google::protobuf::MessageLite* proto);

// Returns the devtools source url comment to append to js code before
// evaluating by devtools.
//
// For example by appending //# sourceUrl=some_name.js to a js snippet the
// snippet can be identified in devtools by url = some_name.js (for example in
// exceptions).
std::string GetDevtoolsSourceUrlCommentToAppend(
    UnexpectedErrorInfoProto::JsExceptionLocation js_exception_location);

// Returns the devtools source url for the js exception location.
std::string GetDevtoolsSourceUrl(
    UnexpectedErrorInfoProto::JsExceptionLocation js_exception_location);

// Returns the js exception location for the devtools source url.
UnexpectedErrorInfoProto::JsExceptionLocation GetExceptionLocation(
    const std::string& devtools_source_url);

}  // namespace autofill_assistant::js_flow_util

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_JS_FLOW_UTIL_H_
