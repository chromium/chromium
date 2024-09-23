// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_EXECUTION_CONTEXT_EXECUTION_CONTEXT_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_EXECUTION_CONTEXT_EXECUTION_CONTEXT_H_

#include "base/observer_list_types.h"
#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"
#include "third_party/blink/public/common/tokens/tokens.h"

class GURL;

namespace performance_manager {

class FrameNode;
class Graph;
class ProcessNode;
class WorkerNode;

using execution_context_priority::PriorityAndReason;

namespace execution_context {

class ExecutionContextObserver;
class ExecutionContextObserverDefaultImpl;

// The types of ExecutionContexts that are defined.
enum class ExecutionContextType {
  kFrameNode,
  kWorkerNode,
};

// An ExecutionContext is a concept from Blink, which denotes a context in
// which Javascript can be executed. Roughly speaking, both FrameNodes and
// WorkerNodes correspond to ExecutionContexts in the PM world-view. This class
// allows external observers to track ExecutionContexts abstractly, without
// having to deal with multiple types at runtime, and duplicating lots of code.
//
// A decorator is responsible for augmenting each instance of a WorkerNode or a
// FrameNode with an instance of ExecutionContext which wraps it, and provides
// accessors for concepts that are common between these two node types. It also
// provides an observer which adapts FrameNodeObserver and WorkerNodeObserver
// behind the scenes, so you don't have to.
//
// Instances of this object are managed by the ExecutionContextRegistry. Their
// lifetimes are the same as those of the underlying nodes they wrap.
class ExecutionContext {
 public:
  using Observer = ExecutionContextObserver;
  using ObserverDefaultImpl = ExecutionContextObserverDefaultImpl;

  // Syntactic sugar for converting from a node to the corresponding
  // ExecutionContext.
  static const ExecutionContext* From(const ExecutionContext* ec) { return ec; }
  static const ExecutionContext* From(const FrameNode* frame_node);
  static const ExecutionContext* From(const WorkerNode* worker_node);

  virtual ~ExecutionContext() = default;

  // Returns the type of this ExecutionContext.
  virtual ExecutionContextType GetType() const = 0;

  // Returns the unique token associated with this ExecutionContext. This is a
  // constant over the lifetime of the context. Tokens are unique for all time
  // and will never be reused.
  virtual blink::ExecutionContextToken GetToken() const = 0;

  // Returns the graph to which this ExecutionContext belongs.
  virtual Graph* GetGraph() const = 0;

  // Returns the final post-redirect committed URL associated with this
  // ExecutionContext. This is the URL of the HTML document (not the javascript)
  // in the case of a FrameNode, or the URL of the worker javascript in the case
  // of a WorkerNode.
  virtual const GURL& GetUrl() const = 0;

  // Returns the ProcessNode corresponding to the process in which this
  // ExecutionContext is hosted. This will never return nullptr.
  virtual const ProcessNode* GetProcessNode() const = 0;

  // Returns the current priority of the execution context, and the reason for
  // the execution context having that particular priority.
  virtual const PriorityAndReason& GetPriorityAndReason() const = 0;

  // Returns the underlying FrameNode, if this context is a FrameNode, or
  // nullptr otherwise.
  virtual const FrameNode* GetFrameNode() const = 0;

  // Returns the underlying WorkerNode, if this context is a WorkerNode, or
  // nullptr otherwise.
  virtual const WorkerNode* GetWorkerNode() const = 0;
};

// An observer for ExecutionContexts.
class ExecutionContextObserver : public base::CheckedObserver {
 public:
  ExecutionContextObserver() = default;
  ExecutionContextObserver(const ExecutionContextObserver&) = delete;
  ExecutionContextObserver& operator=(const ExecutionContextObserver&) = delete;
  ~ExecutionContextObserver() override = default;

  // Called when an ExecutionContext is added. The provided pointer is valid
  // until a call to OnBeforeExecutionContextRemoved().
  virtual void OnExecutionContextAdded(const ExecutionContext* ec) = 0;

  // Called when an ExecutionContext is about to be removed. The pointer |ec|
  // becomes invalid immediately after this returns.
  virtual void OnBeforeExecutionContextRemoved(const ExecutionContext* ec) = 0;

  // Invoked when the execution context priority and reason changes.
  virtual void OnPriorityAndReasonChanged(
      const ExecutionContext* ec,
      const PriorityAndReason& previous_value) = 0;
};

// A default implementation of ExecutionContextObserver with empty stubs for all
// functions.
class ExecutionContextObserverDefaultImpl : public ExecutionContextObserver {
 public:
  ExecutionContextObserverDefaultImpl() = default;
  ExecutionContextObserverDefaultImpl(
      const ExecutionContextObserverDefaultImpl&) = delete;
  ExecutionContextObserverDefaultImpl& operator=(
      const ExecutionContextObserverDefaultImpl&) = delete;
  ~ExecutionContextObserverDefaultImpl() override = default;

  // ExecutionContextObserver implementation:
  void OnExecutionContextAdded(const ExecutionContext* ec) override {}
  void OnBeforeExecutionContextRemoved(const ExecutionContext* ec) override {}
  void OnPriorityAndReasonChanged(
      const ExecutionContext* ec,
      const PriorityAndReason& previous_value) override {}
};

}  // namespace execution_context
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_EXECUTION_CONTEXT_EXECUTION_CONTEXT_H_
