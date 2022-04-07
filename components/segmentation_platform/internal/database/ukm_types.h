// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UKM_TYPES_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UKM_TYPES_H_

#include <cstdint>

#include "base/time/time.h"
#include "base/types/id_type.h"

namespace segmentation_platform {

// Max number of days to keep UKM metrics stored in database.
constexpr base::TimeDelta kNumDaysToKeepUkm = base::Days(30);

using UkmEventHash = base::IdTypeU64<class UkmEventHashTag>;
using UkmMetricHash = base::IdTypeU64<class UkmMetricHashTag>;
using UrlId = base::IdType64<class UrlIdTag>;

// A struct that can accommodate multiple output types needed for Segmentation
// metadata's feature processing. It can only hold one value at a time with the
// corresponding type.
struct ProcessedValue {
  explicit ProcessedValue(bool val) : type(Type::BOOL), bool_val(val) {}
  explicit ProcessedValue(int val) : type(Type::INT), int_val(val) {}
  explicit ProcessedValue(float val) : type(Type::FLOAT), float_val(val) {}
  explicit ProcessedValue(double val) : type(Type::DOUBLE), double_val(val) {}
  explicit ProcessedValue(std::string val) : type(Type::STRING), str_val(val) {}
  explicit ProcessedValue(base::Time val) : type(Type::TIME), time_val(val) {}

  bool operator==(const ProcessedValue& rhs) const {
    if (type != rhs.type)
      return false;
    switch (type) {
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
      default:
        return false;
    }
  }

  enum Type {
    UNKNOWN = 0,
    BOOL = 1,
    INT = 2,
    FLOAT = 3,
    DOUBLE = 4,
    STRING = 5,
    TIME = 6,
  };
  Type type{UNKNOWN};
  bool bool_val{false};
  int int_val{0};
  float float_val{0};
  double double_val{0};
  std::string str_val;
  base::Time time_val;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UKM_TYPES_H_
