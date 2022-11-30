// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_EXECUTION_CONTEXT_IMPL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_EXECUTION_CONTEXT_IMPL_H_

namespace performance_manager {

class FrameNode;
class WorkerNode;

namespace execution_context {

class ExecutionContext;

// Constructs ExecutionContext wrappers (implemented as NodeAttachedData) for
// FrameNodes and WorkerNodes. Once created the objects will live until the
// underlying node disappears. These should only be called from the graph
// sequence, like the underlying objects they wrap. The public interface of
// this is via ExecutionContextRegistry::GetExecutionContextFor*Node().
const ExecutionContext* GetOrCreateExecutionContextForFrameNode(
    const FrameNode* frame_node);
const ExecutionContext* GetOrCreateExecutionContextForWorkerNode(
    const WorkerNode* worker_node);

}  // namespace execution_context
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_EXECUTION_CONTEXT_IMPL_H_