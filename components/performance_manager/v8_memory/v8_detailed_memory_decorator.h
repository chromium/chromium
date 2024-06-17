// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_V8_DETAILED_MEMORY_DECORATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_V8_DETAILED_MEMORY_DECORATOR_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/functional/function_ref.h"
#include "base/sequence_checker.h"
#include "base/types/pass_key.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/graph/node_data_describer.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/v8_memory/v8_detailed_memory.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/performance_manager/v8_detailed_memory_reporter.mojom.h"

namespace performance_manager {

class FrameNode;
class Graph;

namespace v8_memory {

class V8DetailedMemoryRequestQueue;

// A decorator that queries each renderer process for the amount of memory used
// by V8 in each frame. The public interface to create requests for this
// decorator is in
// //components/performance_manager/public/v8_detailed_memory.h.
class V8DetailedMemoryDecorator
    : public ProcessNode::ObserverDefaultImpl,
      public GraphOwnedAndRegistered<V8DetailedMemoryDecorator>,
      public NodeDataDescriberDefaultImpl {
 public:
  V8DetailedMemoryDecorator();
  ~V8DetailedMemoryDecorator() override;

  V8DetailedMemoryDecorator(const V8DetailedMemoryDecorator&) = delete;
  V8DetailedMemoryDecorator& operator=(const V8DetailedMemoryDecorator&) =
      delete;

  // GraphOwned implementation.
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // ProcessNodeObserver overrides.
  void OnProcessNodeAdded(const ProcessNode* process_node) override;
  void OnBeforeProcessNodeRemoved(const ProcessNode* process_node) override;

  // NodeDataDescriber overrides.
  base::Value::Dict DescribeFrameNodeData(const FrameNode* node) const override;
  base::Value::Dict DescribeProcessNodeData(
      const ProcessNode* node) const override;

  // Returns the next measurement request that should be scheduled.
  const V8DetailedMemoryRequest* GetNextRequest() const;

  // Returns the next measurement request with mode kBounded or
  // kEagerForTesting that should be scheduled.
  const V8DetailedMemoryRequest* GetNextBoundedRequest() const;

  // Implementation details below this point.

  // V8DetailedMemoryRequest objects register themselves with the decorator.
  // If |process_node| is null, the request will be sent to every renderer
  // process, otherwise it will be sent only to |process_node|.
  void AddMeasurementRequest(base::PassKey<V8DetailedMemoryRequest>,
                             V8DetailedMemoryRequest* request,
                             const ProcessNode* process_node = nullptr);
  void RemoveMeasurementRequest(base::PassKey<V8DetailedMemoryRequest>,
                                V8DetailedMemoryRequest* request);

  // Internal helper class that can call NotifyObserversOnMeasurementAvailable
  // when a measurement is received.
  class ObserverNotifier;
  void NotifyObserversOnMeasurementAvailable(
      base::PassKey<ObserverNotifier>,
      const ProcessNode* process_node) const;

  // The following functions retrieve data maintained by the decorator.
  static const V8DetailedMemoryExecutionContextData* GetExecutionContextData(
      const FrameNode* node);
  static const V8DetailedMemoryExecutionContextData* GetExecutionContextData(
      const WorkerNode* node);
  static const V8DetailedMemoryExecutionContextData* GetExecutionContextData(
      const execution_context::ExecutionContext* ec);
  static V8DetailedMemoryExecutionContextData*
  CreateExecutionContextDataForTesting(const FrameNode* node);
  static V8DetailedMemoryExecutionContextData*
  CreateExecutionContextDataForTesting(const WorkerNode* node);
  static const V8DetailedMemoryProcessData* GetProcessData(
      const ProcessNode* node);
  static V8DetailedMemoryProcessData* CreateProcessDataForTesting(
      const ProcessNode* node);

 private:
  // Runs the given |callback| for every V8DetailedMemoryRequestQueue (global
  // and per-process).
  void ApplyToAllRequestQueues(
      base::FunctionRef<void(V8DetailedMemoryRequestQueue*)> func) const;

  void UpdateProcessMeasurementSchedules() const;

  std::unique_ptr<V8DetailedMemoryRequestQueue> measurement_requests_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

//////////////////////////////////////////////////////////////////////////////
// The following internal functions are exposed in the header for testing.

namespace internal {

// A callback that will bind a V8DetailedMemoryReporter interface to
// communicate with the given process. Exposed so that it can be overridden to
// implement the interface with a test fake.
using BindV8DetailedMemoryReporterCallback = base::RepeatingCallback<void(
    mojo::PendingReceiver<blink::mojom::V8DetailedMemoryReporter>,
    RenderProcessHostProxy)>;

// Sets a callback that will be used to bind the V8DetailedMemoryReporter
// interface. The callback is owned by the caller and must live until this
// function is called again with nullptr.
void SetBindV8DetailedMemoryReporterCallbackForTesting(
    BindV8DetailedMemoryReporterCallback* callback);

// Destroys the V8DetailedMemoryDecorator. Exposed for testing.
void DestroyV8DetailedMemoryDecoratorForTesting(Graph* graph);

}  // namespace internal

}  // namespace v8_memory

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_V8_DETAILED_MEMORY_DECORATOR_H_
