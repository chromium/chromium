// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PARSE_JSPB_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PARSE_JSPB_H_

#include <string>

#include "base/values.h"

namespace autofill_assistant {

// Converts a message in JSPB wire encoding to the equivalent binary encoding,
// without knowing what that message is.
//
// Only works on messages that meet the following requirements:
// - |message| and all messages it references must have a JSPB message id
//   starting with |jspb_id_prefix|.
// - numbers fields are all int32, int64 or float (no unsigned, fixed or double)
// - there are no bytes fields.
//
// Messages that don't meet these requirements will still be parsed, but not
// completely, depending on the type and value that the original message
// contained.
//
// WARNING: In general, fields that cannot be parsed properly aren't rejected,
// but rather silently skipped later on, during proto parsing.
//
// As an exception to the above, bytes field are parsed completely and reliably.
// However, the final value is a base64-encoded version of the expected bytes.
// This is why they should be avoided.
//
// If parsing is successful, returns a string containing the bytes of the
// message. Note that an empty string is a valid serialized proto value.
absl::optional<std::string> ParseJspb(const std::string& jspb_id_prefix,
                                      const base::Value& message,
                                      std::string* error_message);

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PARSE_JSPB_H_
