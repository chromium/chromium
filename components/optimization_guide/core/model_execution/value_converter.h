// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_VALUE_CONVERTER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_VALUE_CONVERTER_H_

#include <cstdint>
#include <string_view>

#include "base/types/expected.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_status.h"

namespace optimization_guide {

// This file defines a family of ValueConverter types providing utility
// functions for use by the generated
// on_device_model_execution_proto_descriptors.cc file in converting
// string-encoded values to proto field values of the appropriate type.

template <typename T>
struct ValueConverter {
  static base::expected<T, ProtoStatus> TryConvertFromString(
      std::string_view string) {
    static_assert(false, "No specialization is defined for type T");
  }
};

template <>
struct ValueConverter<bool> {
  static base::expected<bool, ProtoStatus> TryConvertFromString(
      std::string_view string);
};

template <>
struct ValueConverter<float> {
  static base::expected<float, ProtoStatus> TryConvertFromString(
      std::string_view string);
};

template <>
struct ValueConverter<double> {
  static base::expected<double, ProtoStatus> TryConvertFromString(
      std::string_view string);
};

template <>
struct ValueConverter<int32_t> {
  static base::expected<int32_t, ProtoStatus> TryConvertFromString(
      std::string_view string);
};

template <>
struct ValueConverter<uint32_t> {
  static base::expected<uint32_t, ProtoStatus> TryConvertFromString(
      std::string_view string);
};

template <>
struct ValueConverter<int64_t> {
  static base::expected<int64_t, ProtoStatus> TryConvertFromString(
      std::string_view string);
};

template <>
struct ValueConverter<uint64_t> {
  static base::expected<uint64_t, ProtoStatus> TryConvertFromString(
      std::string_view string);
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_VALUE_CONVERTER_H_
