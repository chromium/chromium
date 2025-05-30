// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/value_converter.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"

namespace optimization_guide {

base::expected<bool, ProtoStatus> ValueConverter<bool>::TryConvertFromString(
    std::string_view string) {
  std::string lower = base::ToLowerASCII(string);
  if (lower == "true") {
    return true;
  } else if (lower == "false") {
    return false;
  }

  return base::unexpected(ProtoStatus::kError);
}

base::expected<float, ProtoStatus> ValueConverter<float>::TryConvertFromString(
    std::string_view string) {
  double value;
  if (base::StringToDouble(string, &value)) {
    return static_cast<float>(value);
  }
  return base::unexpected(ProtoStatus::kError);
}

base::expected<double, ProtoStatus>
ValueConverter<double>::TryConvertFromString(std::string_view string) {
  double value;
  if (base::StringToDouble(string, &value)) {
    return value;
  }
  return base::unexpected(ProtoStatus::kError);
}

base::expected<int32_t, ProtoStatus>
ValueConverter<int32_t>::TryConvertFromString(std::string_view string) {
  int value;
  if (base::StringToInt(string, &value)) {
    return value;
  }
  return base::unexpected(ProtoStatus::kError);
}

base::expected<uint32_t, ProtoStatus>
ValueConverter<uint32_t>::TryConvertFromString(std::string_view string) {
  unsigned int value;
  if (base::StringToUint(string, &value)) {
    return value;
  }
  return base::unexpected(ProtoStatus::kError);
}

base::expected<int64_t, ProtoStatus>
ValueConverter<int64_t>::TryConvertFromString(std::string_view string) {
  int64_t value;
  if (base::StringToInt64(string, &value)) {
    return value;
  }
  return base::unexpected(ProtoStatus::kError);
}

base::expected<uint64_t, ProtoStatus>
ValueConverter<uint64_t>::TryConvertFromString(std::string_view string) {
  uint64_t value;
  if (base::StringToUint64(string, &value)) {
    return value;
  }
  return base::unexpected(ProtoStatus::kError);
}

}  // namespace optimization_guide
