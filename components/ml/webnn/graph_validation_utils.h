// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ML_WEBNN_GRAPH_VALIDATION_UTILS_H_
#define COMPONENTS_ML_WEBNN_GRAPH_VALIDATION_UTILS_H_

#include <vector>

#include "base/containers/span.h"
#include "base/types/expected.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace webnn {

base::expected<size_t, std::string> ValidateAndCalculateElementsNumber(
    base::span<const uint32_t> dimensions);

base::expected<size_t, std::string> ValidateAndCalculateByteLength(
    size_t type_bytes,
    base::span<const uint32_t> dimensions);

// Broadcast the input shapes and return the output shape.
// If bidirectional is true, its behavior follows the numpy-broadcasting-rule:
// https://numpy.org/doc/stable/user/basics.broadcasting.html#general-broadcasting-rules.
// Otherwise, it unidirectionally broadcasts the lhs to the rhs.
absl::optional<std::vector<uint32_t>> BroadcastShapes(
    base::span<const uint32_t> dims_lhs,
    base::span<const uint32_t> dims_rhs,
    bool bidirectional = true);

}  // namespace webnn

#endif  // COMPONENTS_ML_WEBNN_GRAPH_VALIDATION_UTILS_H_
