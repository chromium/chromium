// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/input_context.h"

#include <string_view>

#include "base/values.h"

namespace segmentation_platform {

InputContext::InputContext() = default;

InputContext::~InputContext() = default;

std::optional<processing::ProcessedValue> InputContext::GetMetadataArgument(
    std::string_view arg_name) const {
  auto it = metadata_args.find(arg_name);
  if (it == metadata_args.end()) {
    return std::nullopt;
  }
  return it->second;
}

base::Value InputContext::ToDebugValue() const {
  base::Value::Dict dict;
  for (const auto& [param_name_str, processed_value] : metadata_args) {
    dict.Set(param_name_str, processed_value.ToDebugValue());
  }
  return base::Value(std::move(dict));
}

std::ostream& operator<<(std::ostream& out, const InputContext& value) {
  return out << value.ToDebugValue();
}

}  // namespace segmentation_platform
