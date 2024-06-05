// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_TYPE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_TYPE_H_

#include <stdint.h>

namespace performance_manager {

enum class NodeTypeEnum : uint8_t {
  kFrame,
  kPage,
  kProcess,
  kSystem,
  kWorker,
  kInvalidType,
};

// Keep in sync with NodeTypeEnum above.
inline constexpr uint8_t kValidNodeTypeCount = 5;

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_TYPE_H_
