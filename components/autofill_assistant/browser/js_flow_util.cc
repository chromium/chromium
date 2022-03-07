// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/js_flow_util.h"

namespace autofill_assistant {
namespace js_flow {

bool ContainsOnlyPrimitveValues(const base::Value& value,
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
        if (!ContainsOnlyPrimitveValues(nested_value, out_error_message)) {
          return false;
        }
      }
      return true;
    }
    case base::Value::Type::LIST: {
      for (const auto& entry : *value.GetIfList()) {
        if (!ContainsOnlyPrimitveValues(entry, out_error_message)) {
          return false;
        }
      }
      return true;
    }
  }
}

}  // namespace js_flow
}  // namespace autofill_assistant
