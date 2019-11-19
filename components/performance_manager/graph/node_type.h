// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_NODE_TYPE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_NODE_TYPE_H_

#include <stdint.h>

namespace performance_manager {

enum class NodeTypeEnum : uint8_t {
  kInvalidType,
  kFrame,
  kPage,
  kProcess,
  kSystem,
  kWorker,
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_NODE_TYPE_H_
