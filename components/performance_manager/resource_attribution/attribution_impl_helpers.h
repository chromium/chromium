// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_ATTRIBUTION_IMPL_HELPERS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_ATTRIBUTION_IMPL_HELPERS_H_

#include "components/performance_manager/public/resource_attribution/attribution_helpers.h"

#include "base/functional/function_ref.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/resource_attribution/performance_manager_aliases.h"

namespace resource_attribution {

// Splits a resource of type T between all frames and workers hosted in
// `process_node`, by calling setter methods on the FrameNodeImpl and
// WorkerNodeImpl objects. The `frame_setter` method or `worker_setter` method
// on each node will be called with that node's fraction of `resource_value`.
template <typename T,
          typename FrameSetterMethod = void (FrameNodeImpl::*)(T),
          typename WorkerSetterMethod = void (WorkerNodeImpl::*)(T)>
void SplitResourceAmongFrameAndWorkerImpls(T resource_value,
                                           ProcessNodeImpl* process_node,
                                           FrameSetterMethod frame_setter,
                                           WorkerSetterMethod worker_setter);

// Implementation

template <typename T, typename FrameSetterMethod, typename WorkerSetterMethod>
void SplitResourceAmongFrameAndWorkerImpls(T resource_value,
                                           ProcessNodeImpl* process_node,
                                           FrameSetterMethod frame_setter,
                                           WorkerSetterMethod worker_setter) {
  using FrameSetter = base::FunctionRef<void(const FrameNode*, T)>;
  using WorkerSetter = base::FunctionRef<void(const WorkerNode*, T)>;
  SplitResourceAmongFramesAndWorkers(
      resource_value, process_node,
      FrameSetter([&frame_setter](const FrameNode* f, T value) {
        (FrameNodeImpl::FromNode(f)->*frame_setter)(value);
      }),
      WorkerSetter([&worker_setter](const WorkerNode* w, T value) {
        (WorkerNodeImpl::FromNode(w)->*worker_setter)(value);
      }));
}

}  // namespace resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_ATTRIBUTION_IMPL_HELPERS_H_
