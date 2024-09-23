// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_value_utils.h"

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"

namespace optimization_guide {

std::string GetStringFromValue(const proto::Value& value) {
  switch (value.type_case()) {
    case proto::Value::kStringValue:
      return value.string_value();
    case proto::Value::kBooleanValue:
      return value.boolean_value() ? "true" : "false";
    case proto::Value::kInt32Value:
      return base::NumberToString(value.int32_value());
    case proto::Value::kInt64Value:
      return base::NumberToString(value.int64_value());
    case proto::Value::kFloatValue:
      return base::NumberToString(value.float_value());
    case proto::Value::TYPE_NOT_SET:
      NOTREACHED_IN_MIGRATION();
      return std::string();
  }
}

bool AreValuesEqual(const proto::Value& a, const proto::Value& b) {
  if (a.type_case() != b.type_case()) {
    return false;
  }

  switch (a.type_case()) {
    case proto::Value::kStringValue:
      return a.string_value() == b.string_value();
    case proto::Value::kBooleanValue:
      return a.boolean_value() == b.boolean_value();
    case proto::Value::kInt32Value:
      return a.int32_value() == b.int32_value();
    case proto::Value::kInt64Value:
      return a.int64_value() == b.int64_value();
    case proto::Value::kFloatValue:
      return a.float_value() == b.float_value();
    case proto::Value::TYPE_NOT_SET:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

}  // namespace optimization_guide
