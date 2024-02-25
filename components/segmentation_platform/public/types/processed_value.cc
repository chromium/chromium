// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/types/processed_value.h"

#include "base/i18n/time_formatting.h"
#include "base/json/values_util.h"
#include "base/values.h"

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
ProcessedValue::ProcessedValue(const GURL& val)
    : type(Type::URL), url(std::make_unique<GURL>(val)) {}

ProcessedValue::ProcessedValue(const ProcessedValue& other) {
  *this = other;
}
ProcessedValue::ProcessedValue(ProcessedValue&& other) = default;

ProcessedValue& ProcessedValue::operator=(const ProcessedValue& other) {
  type = other.type;
  switch (type) {
    case Type::UNKNOWN:
      return *this;
    case Type::BOOL:
      bool_val = other.bool_val;
      return *this;
    case Type::INT:
      int_val = other.int_val;
      return *this;
    case Type::FLOAT:
      float_val = other.float_val;
      return *this;
    case Type::DOUBLE:
      double_val = other.double_val;
      return *this;
    case Type::STRING:
      str_val = other.str_val;
      return *this;
    case Type::TIME:
      time_val = other.time_val;
      return *this;
    case Type::INT64:
      int64_val = other.int64_val;
      return *this;
    case Type::URL:
      url = std::make_unique<GURL>(*other.url);
      return *this;
  }
}

ProcessedValue::~ProcessedValue() = default;

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
    case Type::URL:
      return *url == *rhs.url;
  }
}

base::Value ProcessedValue::ToDebugValue() const {
  base::Value::Dict dict;
  switch (type) {
    case Type::UNKNOWN:
      dict.Set("type", "UNKNOWN");
      break;
    case Type::BOOL:
      dict.Set("type", "BOOL");
      dict.Set("value", bool_val);
      break;
    case Type::INT:
      dict.Set("type", "INT");
      dict.Set("value", int_val);
      break;
    case Type::FLOAT:
      dict.Set("type", "FLOAT");
      dict.Set("value", float_val);
      break;
    case Type::DOUBLE:
      dict.Set("type", "DOUBLE");
      dict.Set("value", double_val);
      break;
    case Type::STRING:
      dict.Set("type", "STRING");
      dict.Set("value", str_val);
      break;
    case Type::TIME:
      dict.Set("type", "TIME");
      dict.Set("value", base::TimeFormatHTTP(time_val));
      break;
    case Type::INT64:
      dict.Set("type", "INT64");
      dict.Set("value", base::Int64ToValue(int64_val));
      break;
    case Type::URL:
      dict.Set("type", "URL");
      if (!url) {
        dict.Set("value", "(not set)");
      } else if (url->is_empty()) {
        dict.Set("value", "(empty)");
      } else if (!url->is_valid()) {
        dict.Set("value", "(invalid)");
      } else {
        dict.Set("value", url->spec());
      }
      break;
  }
  return base::Value(std::move(dict));
}

// static
ProcessedValue ProcessedValue::FromFloat(float val) {
  return ProcessedValue(val);
}

std::ostream& operator<<(std::ostream& out, const ProcessedValue& value) {
  return out << value.ToDebugValue();
}

}  // namespace segmentation_platform::processing
