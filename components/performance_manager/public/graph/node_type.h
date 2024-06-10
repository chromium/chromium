// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_TYPE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_TYPE_H_

#include <stdint.h>

#include "base/numerics/safe_conversions.h"

namespace performance_manager {

enum class NodeTypeEnum : uint8_t {
  kFrame,
  kPage,
  kProcess,
  kSystem,
  kWorker,
  kMaxValue = kWorker
};

// Keep in sync with NodeTypeEnum above.
inline constexpr uint8_t kValidNodeTypeCount =
    base::strict_cast<uint8_t>(NodeTypeEnum::kMaxValue) + 1;

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_TYPE_H_
