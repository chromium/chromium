// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PROCESS_NODE_SOURCE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PROCESS_NODE_SOURCE_H_

namespace performance_manager {

class ProcessNodeImpl;

// Represents a source of existing process nodes that lives on the main thread.
// In practice, this is used by the worker watchers as an abstraction over the
// peformance_manager::RenderProcessUserData to make testing easier.
class ProcessNodeSource {
 public:
  virtual ~ProcessNodeSource() = default;

  // Retrieves the process node associated with the |render_process_id|.
  virtual ProcessNodeImpl* GetProcessNode(int render_process_id);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PROCESS_NODE_SOURCE_H_
