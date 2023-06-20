// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_TYPES_PROCESSED_VALUE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_TYPES_PROCESSED_VALUE_H_

#include <iosfwd>
#include <string>

#include "base/time/time.h"
#include "url/gurl.h"

namespace base {
class Value;
}

namespace segmentation_platform::processing {

// A struct that can accommodate multiple output types needed for Segmentation
// metadata's feature processing. It can only hold one value at a time with the
// corresponding type.
struct ProcessedValue {
  explicit ProcessedValue(bool val);
  explicit ProcessedValue(int val);
  explicit ProcessedValue(float val);
  explicit ProcessedValue(double val);
  explicit ProcessedValue(const std::string& val);
  explicit ProcessedValue(base::Time val);
  explicit ProcessedValue(int64_t val);
  explicit ProcessedValue(const GURL& url);

  ProcessedValue(const ProcessedValue& other);
  ProcessedValue(ProcessedValue&& other);
  ProcessedValue& operator=(const ProcessedValue& other);
  ~ProcessedValue();

  bool operator==(const ProcessedValue& rhs) const;

  base::Value ToDebugValue() const;

  static ProcessedValue FromFloat(float val);

  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.segmentation_platform
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: ProcessedValueType
  enum Type {
    UNKNOWN = 0,
    BOOL = 1,
    INT = 2,
    FLOAT = 3,
    DOUBLE = 4,
    STRING = 5,
    TIME = 6,
    INT64 = 7,
    URL = 8,
  };
  Type type{UNKNOWN};
  bool bool_val{false};
  int int_val{0};
  float float_val{0};
  double double_val{0};
  std::string str_val;
  base::Time time_val;
  int64_t int64_val{0};
  std::unique_ptr<GURL> url;
};

// Represents a set of values that can represent inputs or outputs for a model.
using Tensor = std::vector<ProcessedValue>;

// For logging and debug purposes.
std::ostream& operator<<(std::ostream& out, const ProcessedValue& value);

}  // namespace segmentation_platform::processing

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_TYPES_PROCESSED_VALUE_H_
