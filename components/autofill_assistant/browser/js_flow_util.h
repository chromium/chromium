// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_JS_FLOW_UTIL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_JS_FLOW_UTIL_H_

#include "base/values.h"

namespace autofill_assistant {
namespace js_flow {

// Returns true if |value| contains only primitive values. Dictionaries and
// lists are allowed, so long as they only contain primitive values too.
// |out_error_message| will contain details if this returns false.
bool ContainsOnlyPrimitveValues(const base::Value& value,
                                std::string& out_error_message);

}  // namespace js_flow
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_JS_FLOW_UTIL_H_
