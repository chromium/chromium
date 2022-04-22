// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/ukm_types.h"

namespace segmentation_platform::processing {

ProcessedValue::ProcessedValue(bool val) : type(Type::BOOL), bool_val(val) {}
ProcessedValue::ProcessedValue(int val) : type(Type::INT), int_val(val) {}
ProcessedValue::ProcessedValue(float val) : type(Type::FLOAT), float_val(val) {}
ProcessedValue::ProcessedValue(double val)
    : type(Type::DOUBLE), double_val(val) {}
ProcessedValue::ProcessedValue(const std::string& val)
    : type(Type::STRING), str_val(val) {}
ProcessedValue::ProcessedValue(base::Time val)
    : type(Type::TIME), time_val(val) {}
ProcessedValue::ProcessedValue(int64_t val)
    : type(Type::INT64), int64_val(val) {}

ProcessedValue::ProcessedValue(const ProcessedValue& other) = default;
ProcessedValue& ProcessedValue::operator=(const ProcessedValue& other) =
    default;

bool ProcessedValue::operator==(const ProcessedValue& rhs) const {
  if (type != rhs.type)
    return false;
  switch (type) {
    case Type::UNKNOWN:
      return false;
    case Type::BOOL:
      return bool_val == rhs.bool_val;
    case Type::INT:
      return int_val == rhs.int_val;
    case Type::FLOAT:
      return float_val == rhs.float_val;
    case Type::DOUBLE:
      return double_val == rhs.double_val;
    case Type::STRING:
      return str_val == rhs.str_val;
    case Type::TIME:
      return time_val == rhs.time_val;
    case Type::INT64:
      return int64_val == rhs.int64_val;
  }
}

}  // namespace segmentation_platform::processing
