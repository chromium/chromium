// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_GRAPH_CHANGE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_GRAPH_CHANGE_H_

#include "base/memory/raw_ptr_exclusion.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace performance_manager {
class FrameNode;
class WorkerNode;
}  // namespace performance_manager

namespace performance_manager::resource_attribution {

// Graph changes that can affect resource measurement distribution.
// These are all passed on the stack so don't need to use raw_ptr.
//
// TODO(crbug.com/1471683): This should be private, but it's referenced from the
// public cpu_measurement_monitor.h
struct NoGraphChange {};

struct GraphChangeAddFrame {
  explicit GraphChangeAddFrame(const FrameNode* node) : frame_node(node) {}

  RAW_PTR_EXCLUSION const FrameNode* frame_node;
};

struct GraphChangeRemoveFrame {
  explicit GraphChangeRemoveFrame(const FrameNode* node) : frame_node(node) {}

  RAW_PTR_EXCLUSION const FrameNode* frame_node;
};

struct GraphChangeAddWorker {
  explicit GraphChangeAddWorker(const WorkerNode* node) : worker_node(node) {}

  RAW_PTR_EXCLUSION const WorkerNode* worker_node;
};

struct GraphChangeRemoveWorker {
  explicit GraphChangeRemoveWorker(const WorkerNode* node)
      : worker_node(node) {}

  RAW_PTR_EXCLUSION const WorkerNode* worker_node;
};

struct GraphChangeAddClientFrameToWorker {
  GraphChangeAddClientFrameToWorker(const WorkerNode* node,
                                    const FrameNode* client_node)
      : worker_node(node), client_frame_node(client_node) {}

  RAW_PTR_EXCLUSION const WorkerNode* worker_node;
  RAW_PTR_EXCLUSION const FrameNode* client_frame_node;
};

struct GraphChangeRemoveClientFrameFromWorker {
  GraphChangeRemoveClientFrameFromWorker(const WorkerNode* node,
                                         const FrameNode* client_node)
      : worker_node(node), client_frame_node(client_node) {}

  RAW_PTR_EXCLUSION const WorkerNode* worker_node;
  RAW_PTR_EXCLUSION const FrameNode* client_frame_node;
};

struct GraphChangeAddClientWorkerToWorker {
  GraphChangeAddClientWorkerToWorker(const WorkerNode* node,
                                     const WorkerNode* client_node)
      : worker_node(node), client_worker_node(client_node) {}

  RAW_PTR_EXCLUSION const WorkerNode* worker_node;
  RAW_PTR_EXCLUSION const WorkerNode* client_worker_node;
};

struct GraphChangeRemoveClientWorkerFromWorker {
  GraphChangeRemoveClientWorkerFromWorker(const WorkerNode* node,
                                          const WorkerNode* client_node)
      : worker_node(node), client_worker_node(client_node) {}

  RAW_PTR_EXCLUSION const WorkerNode* worker_node;
  RAW_PTR_EXCLUSION const WorkerNode* client_worker_node;
};

using GraphChange = absl::variant<NoGraphChange,
                                  GraphChangeAddFrame,
                                  GraphChangeRemoveFrame,
                                  GraphChangeAddWorker,
                                  GraphChangeRemoveWorker,
                                  GraphChangeAddClientFrameToWorker,
                                  GraphChangeRemoveClientFrameFromWorker,
                                  GraphChangeAddClientWorkerToWorker,
                                  GraphChangeRemoveClientWorkerFromWorker>;

}  // namespace performance_manager::resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_GRAPH_CHANGE_H_
