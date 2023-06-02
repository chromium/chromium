// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/input_context.h"

namespace segmentation_platform {

InputContext::InputContext() = default;

InputContext::~InputContext() = default;

absl::optional<processing::ProcessedValue> InputContext::GetMetadataArgument(
    base::StringPiece arg_name) const {
  auto it = metadata_args.find(arg_name);
  if (it == metadata_args.end()) {
    return absl::nullopt;
  }
  return it->second;
}

}  // namespace segmentation_platform
