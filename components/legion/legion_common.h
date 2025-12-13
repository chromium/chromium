// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_LEGION_COMMON_H_
#define COMPONENTS_LEGION_LEGION_COMMON_H_

#include <cstdint>
#include <vector>

namespace legion {

// Common data type for requests, likely a serialized proto.
using Request = std::vector<uint8_t>;

// Common data type for responses, likely a serialized proto.
using Response = std::vector<uint8_t>;

}  // namespace legion

#endif  // COMPONENTS_LEGION_LEGION_COMMON_H_
