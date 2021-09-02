// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INPUT_OVERLAY_RESOURCES_INPUT_OVERLAY_RESOURCES_UTIL_H_
#define COMPONENTS_ARC_INPUT_OVERLAY_RESOURCES_INPUT_OVERLAY_RESOURCES_UTIL_H_

#include "base/strings/string_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace arc {

// Get the resource ID of the input overlay JSON file by the associated package
// name.
absl::optional<int> GetInputOverlayResourceId(const std::string& package_name);

}  // namespace arc

#endif  // COMPONENTS_ARC_INPUT_OVERLAY_RESOURCES_INPUT_OVERLAY_RESOURCES_UTIL_H_
