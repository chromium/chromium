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

namespace autofill_assistant {
namespace js_flow_util {

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
    std::unique_ptr<base::Value>& out_flow_result);

}  // namespace js_flow_util
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_JS_FLOW_UTIL_H_
