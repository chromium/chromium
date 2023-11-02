// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_CONTENT_COMMON_NODE_DATA_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_CONTENT_COMMON_NODE_DATA_H_

#include <stdint.h>

namespace autofill_assistant {

struct NodeData {
  NodeData();
  NodeData(const NodeData&);
  NodeData& operator=(const NodeData&);
  NodeData(NodeData&&);
  NodeData& operator=(NodeData&&);
  ~NodeData();

  int32_t backend_node_id = -1;
  bool used_override = false;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_CONTENT_COMMON_AUTOFILL_ASSISTANT_DATA_H_
