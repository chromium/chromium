// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_V8_MEMORY_V8_PER_FRAME_MEMORY_DECORATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_V8_MEMORY_V8_PER_FRAME_MEMORY_DECORATOR_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/util/type_safety/pass_key.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/graph/node_data_describer.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/render_process_host_id.h"
#include "content/public/browser/global_routing_id.h"
#include "third_party/blink/public/mojom/performance_manager/v8_detailed_memory_reporter.mojom.h"

namespace performance_manager {

namespace v8_memory {

// A decorator that queries each renderer process for the amount of memory used
// by V8 in each frame.
//
// To start sampling create a V8PerFrameMemoryRequest object that specifies how
// often to request a memory measurement. Delete the object when you no longer
// need measurements. Measurement involves some overhead so choose the lowest
// sampling frequency your use case needs. The decorator will use the highest
// sampling frequency that any caller requests, and stop measurements entirely
// when no more V8PerFrameMemoryRequest objects exist.
//
// When measurements are available the decorator attaches them to
// V8PerFrameMemoryFrameData and V8PerFrameMemoryProcessData objects that can
// be retrieved with V8PerFrameMemoryFrameData::ForFrameNode and
// V8PerFrameMemoryProcessData::ForProcessNode. V8PerFrameMemoryProcessData
// objects can be cleaned up when V8PerFrameMemoryRequest objects are deleted
// so callers must save the measurements they are interested in before
// releasing their V8PerFrameMemoryRequest.
//
// Callers can be notified when a request is available by implementing
// V8PerFrameMemoryObserver.
//
// V8PerFrameMemoryRequest, V8PerFrameMemoryFrameData and
// V8PerFrameMemoryProcessData must all be accessed on the graph sequence, and
// V8PerFrameMemoryObserver::OnV8MemoryMeasurementAvailable will be called on
// this sequence. To request memory measurements from another sequence use the
// V8PerFrameMemoryRequestAnySeq and V8PerFrameMemoryObserverAnySeq wrappers.
//
// Usage:
//
// Take a memory measurement every 30 seconds and poll for updates:
//
//   class MemoryPoller {
//    public:
//     MemoryPoller() {
//       PerformanceManager::CallOnGraph(FROM_HERE,
//           base::BindOnce(&Start, base::Unretained(this)));
//     }
//
//     void Start(Graph* graph) {
//       DCHECK_ON_GRAPH_SEQUENCE(graph);
//       request_ = std::make_unique<V8PerFrameMemoryRequest>(
//           base::TimeDelta::FromSeconds(30));
//       request_->StartMeasurement(graph);
//
//       // Periodically check Process and Frame nodes for the latest results.
//       timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(30),
//           base::BindRepeating(&GetResults, base::Unretained(this),
//                               base::Unretained(graph)));
//     }
//
//     void GetResults(Graph* graph) {
//       DCHECK_ON_GRAPH_SEQUENCE(graph);
//       for (auto* node : graph->GetAllProcessNodes()) {
//         auto* process_data = V8PerFrameMemoryProcessData::ForProcess(node);
//         if (process_data) {
//           LOG(INFO) << "Process " << node->GetProcessId() <<
//               " reported " << process_data->unassociated_v8_bytes_used() <<
//               " bytes of V8 memory that wasn't associated with a frame.";
//         }
//         for (auto* frame_node : node->GetFrameNodes()) {
//           auto* frame_data = V8PerFrameMemoryFrameData::ForFrame(frame_node);
//           if (frame_data) {
//             LOG(INFO) << "Frame " << frame_node->GetFrameToken().value() <<
//                 " reported " << frame_data->v8_bytes_used() <<
//                 " bytes of V8 memory in its main world.";
//           }
//         }
//     }
//
//     void Stop(Graph* graph) {
//       DCHECK_ON_GRAPH_SEQUENCE(graph);
//       // Measurements stop when |request_| is deleted.
//       request_.reset();
//       timer_.Stop();
//     }
//
//    private:
//     std::unique_ptr<V8PerFrameMemoryRequest> request_;
//     base::RepeatingTimer timer_;
//   };
//
// Take a memory measurement every 2 minutes and register an observer for the
// results:
//
//   class Observer : public V8PerFrameMemoryObserver {
//    public:
//     // Called on the PM sequence for each process.
//     void OnV8MemoryMeasurementAvailable(
//         const ProcessNode* process_node,
//         const V8PerFrameMemoryProcessData* data) override {
//       DCHECK(data);
//       LOG(INFO) << "Process " << process_node->GetProcessId() <<
//           " reported " << data->unassociated_v8_bytes_used() <<
//           " bytes of V8 memory that wasn't associated with a frame.";
//       for (auto* frame_node : process_node->GetFrameNodes()) {
//         auto* frame_data = V8PerFrameMemoryFrameData::ForFrame(frame_node);
//         if (frame_data) {
//           LOG(INFO) << "Frame " << frame_node->GetFrameToken().value() <<
//               " reported " << frame_data->v8_bytes_used() <<
//               " bytes of V8 memory in its main world.";
//         }
//       }
//      }
//   };
//
//   class MemoryMonitor {
//    public:
//     MemoryMonitor() {
//       PerformanceManager::CallOnGraph(FROM_HERE,
//           base::BindOnce(&Start, base::Unretained(this)));
//     }
//
//     void Start(Graph* graph) {
//       DCHECK_ON_GRAPH_SEQUENCE(graph);
//
//       // Creating a V8PerFrameMemoryRequest with the |graph| parameter
//       // automatically starts measurements.
//       request_ = std::make_unique<V8PerFrameMemoryRequest>(
//           base::TimeDelta::FromSeconds(30), graph);
//       observer_ = std::make_unique<Observer>();
//       request_->AddObserver(observer_.get());
//     }
//
//     void Stop(Graph* graph) {
//       DCHECK_ON_GRAPH_SEQUENCE(graph);
//
//       // |observer_| must be removed from |request_| before deleting it.
//       // Afterwards they can be deleted in any order.
//       request_->RemoveObserver(observer_.get());
//       observer_.reset();
//
//       // Measurements stop when |request_| is deleted.
//       request_.reset();
//     }
//
//    private:
//     std::unique_ptr<V8PerFrameMemoryRequest> request_;
//     std::unique_ptr<Observer> observer_;
//   };
//
// Same, but from the another thread:
//
//   class Observer : public V8PerFrameMemoryObserverAnySeq {
//    public:
//     // Called on the same sequence for each process.
//     void OnV8MemoryMeasurementAvailable(
//         RenderProcessHostId process_id,
//         const V8PerFrameMemoryProcessData& process_data,
//         const V8PerFrameMemoryObserverAnySeq::FrameDataMap& frame_data)
//         override {
//       const auto* process = RenderProcessHost::FromID(process_id.value());
//       if (!process) {
//         // Process was deleted after measurement arrived on the PM sequence.
//         return;
//       }
//       LOG(INFO) << "Process " << process->GetID() <<
//           " reported " << process_data.unassociated_v8_bytes_used() <<
//           " bytes of V8 memory that wasn't associated with a frame.";
//       for (std::pair<
//             content::GlobalFrameRoutingId,
//             V8PerFrameMemoryFrameData
//           > frame_and_data : frame_data) {
//         const auto* frame = RenderFrameHost::FromID(frame_and_data.first);
//         if (!frame) {
//           // Frame was deleted after measurement arrived on the PM sequence.
//           continue;
//         }
//         LOG(INFO) << "Frame " << frame->GetFrameToken() <<
//             " using " << token_and_data.second.v8_bytes_used() <<
//             " bytes of V8 memory in its main world.";
//       }
//     }
//   };
//
//  class MemoryMonitor {
//    public:
//     MemoryMonitor() {
//       Start();
//     }
//
//     void Start() {
//       DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
//
//       // Creating a V8PerFrameMemoryRequest with the |graph| parameter
//       // automatically starts measurements.
//       request_ = std::make_unique<V8PerFrameMemoryRequestAnySeq>(
//           base::TimeDelta::FromMinutes(2));
//       observer_ = std::make_unique<Observer>();
//       request_->AddObserver(observer_.get());
//     }
//
//     void Stop() {
//       DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
//
//       // |observer_| must be removed from |request_| before deleting it.
//       // Afterwards they can be deleted in any order.
//       request_->RemoveObserver(observer_.get());
//       observer_.reset();
//
//       // Measurements stop when |request_| is deleted.
//       request_.reset();
//     }
//
//    private:
//     std::unique_ptr<V8PerFrameMemoryRequestAnySeq> request_;
//     std::unique_ptr<Observer> observer_;
//
//     SEQUENCE_CHECKER(sequence_checker_);
//   };

class V8PerFrameMemoryObserver;
class V8PerFrameMemoryRequest;
class V8PerFrameMemoryRequestAnySeq;

class V8PerFrameMemoryDecorator
    : public GraphOwned,
      public GraphRegisteredImpl<V8PerFrameMemoryDecorator>,
      public ProcessNode::ObserverDefaultImpl,
      public NodeDataDescriberDefaultImpl {
 public:
  // A priority queue of memory requests. The decorator will hold a global
  // queue of requests that measure every process, and each ProcessNode will
  // have a queue of requests that measure only that process.
  class MeasurementRequestQueue;

  V8PerFrameMemoryDecorator();
  ~V8PerFrameMemoryDecorator() override;

  V8PerFrameMemoryDecorator(const V8PerFrameMemoryDecorator&) = delete;
  V8PerFrameMemoryDecorator& operator=(const V8PerFrameMemoryDecorator&) =
      delete;

  // GraphOwned implementation.
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // ProcessNodeObserver overrides.
  void OnProcessNodeAdded(const ProcessNode* process_node) override;
  void OnBeforeProcessNodeRemoved(const ProcessNode* process_node) override;

  // NodeDataDescriber overrides.
  base::Value DescribeFrameNodeData(const FrameNode* node) const override;
  base::Value DescribeProcessNodeData(const ProcessNode* node) const override;

  // Returns the next measurement request that should be scheduled.
  const V8PerFrameMemoryRequest* GetNextRequest() const;

  // Returns the next measurement request with mode kBounded or
  // kEagerForTesting that should be scheduled.
  const V8PerFrameMemoryRequest* GetNextBoundedRequest() const;

  // Implementation details below this point.

  // V8PerFrameMemoryRequest objects register themselves with the decorator.
  // If |process_node| is null, the request will be sent to every process,
  // otherwise it will be sent only to |process_node|.
  void AddMeasurementRequest(util::PassKey<V8PerFrameMemoryRequest>,
                             V8PerFrameMemoryRequest* request,
                             const ProcessNode* process_node = nullptr);
  void RemoveMeasurementRequest(util::PassKey<V8PerFrameMemoryRequest>,
                                V8PerFrameMemoryRequest* request);

  // Internal helper class that can call NotifyObserversOnMeasurementAvailable
  // when a measurement is received.
  class ObserverNotifier;
  void NotifyObserversOnMeasurementAvailable(
      util::PassKey<ObserverNotifier>,
      const ProcessNode* process_node) const;

 private:
  using RequestQueueCallback =
      base::RepeatingCallback<void(MeasurementRequestQueue*)>;

  // Runs the given |callback| for every MeasurementRequestQueue (global and
  // per-process).
  void ApplyToAllRequestQueues(RequestQueueCallback callback) const;

  void UpdateProcessMeasurementSchedules() const;

  Graph* graph_ = nullptr;

  std::unique_ptr<MeasurementRequestQueue> measurement_requests_;

  SEQUENCE_CHECKER(sequence_checker_);
};

class V8PerFrameMemoryRequest {
 public:
  enum class MeasurementMode {
    // Measurements will be taken at the next GC after a request is received.
    // If no GC happens within a bounded time an extra GC will be scheduled.
    kBounded,

    // Measurements will only be taken at the next scheduled GC after a request
    // is received.
    kLazy,

    // Measurements will be taken immediately when a request is received. This
    // causes an extra GC so should only be done in tests. Attempts to use this
    // mode will DCHECK if SetEagerMemoryMeasurementEnabledForTesting was not
    // called.
    kEagerForTesting,

    kDefault = kBounded,
  };

  // Creates a request but does not start the measurements. Call
  // StartMeasurement to add it to the request list.
  //
  // Measurement requests will be sent repeatedly to each process, with at
  // least |min_time_between_requests| (which must be greater than 0) between
  // each repetition. The next GC after each request is received will be
  // instrumented, which adds some overhead. |mode| determines whether extra
  // GC's can be scheduled, which would add even more overhead.
  explicit V8PerFrameMemoryRequest(
      const base::TimeDelta& min_time_between_requests,
      MeasurementMode mode = MeasurementMode::kDefault);

  // Creates a request and calls StartMeasurement with the given |graph| and
  // |min_time_between_requests|, using the default measurement mode.
  V8PerFrameMemoryRequest(const base::TimeDelta& min_time_between_requests,
                          Graph* graph);

  // Creates a request and calls StartMeasurement with the given |graph|,
  // |min_time_between_requests|, and |mode|.
  V8PerFrameMemoryRequest(const base::TimeDelta& min_time_between_requests,
                          MeasurementMode mode,
                          Graph* graph);

  ~V8PerFrameMemoryRequest();

  V8PerFrameMemoryRequest(const V8PerFrameMemoryRequest&) = delete;
  V8PerFrameMemoryRequest& operator=(const V8PerFrameMemoryRequest&) = delete;

  const base::TimeDelta& min_time_between_requests() const {
    return min_time_between_requests_;
  }

  MeasurementMode mode() const { return mode_; }

  // Requests measurements for all ProcessNode's in |graph|. There must be at
  // most one call to this or StartMeasurementForProcess for each
  // V8PerFrameMemoryRequest.
  void StartMeasurement(Graph* graph);

  // Requests measurements only for the given |process_node|, which must be a
  // renderer process. There must be at most one call to this or
  // StartMeasurement for each V8PerFrameMemoryRequest.
  void StartMeasurementForProcess(const ProcessNode* process_node);

  // Adds/removes an observer.
  void AddObserver(V8PerFrameMemoryObserver* observer);
  void RemoveObserver(V8PerFrameMemoryObserver* observer);

  // Implementation details below this point.

  // Private constructor for V8PerFrameMemoryRequestAnySeq. Saves
  // |off_sequence_request| as a pointer to the off-sequence object that
  // triggered the request and starts measurements with frequency
  // |min_time_between_requests|.
  V8PerFrameMemoryRequest(
      util::PassKey<V8PerFrameMemoryRequestAnySeq>,
      const base::TimeDelta& min_time_between_requests,
      MeasurementMode mode,
      base::WeakPtr<V8PerFrameMemoryRequestAnySeq> off_sequence_request);

  // V8PerFrameMemoryDecorator::MeasurementRequestQueue calls
  // OnOwnerUnregistered for all requests in the queue when the owning
  // decorator or process node is removed from the graph.
  void OnOwnerUnregistered(
      util::PassKey<V8PerFrameMemoryDecorator::MeasurementRequestQueue>);

  // V8PerFrameMemoryDecorator::MeasurementRequestQueue calls
  // NotifyObserversOnMeasurementAvailable when a measurement is received.
  void NotifyObserversOnMeasurementAvailable(
      util::PassKey<V8PerFrameMemoryDecorator::MeasurementRequestQueue>,
      const ProcessNode* process_node) const;

 private:
  void StartMeasurementImpl(Graph* graph, const ProcessNode* process_node);

  base::TimeDelta min_time_between_requests_;
  MeasurementMode mode_;
  V8PerFrameMemoryDecorator* decorator_ = nullptr;
  base::ObserverList<V8PerFrameMemoryObserver, /*check_empty=*/true> observers_;

  // Pointer back to the off-sequence V8PerFrameMemoryRequestAnySeq that
  // created this, if any.
  base::WeakPtr<V8PerFrameMemoryRequestAnySeq> off_sequence_request_;

  // Sequence that |off_sequence_request_| lives on.
  scoped_refptr<base::SequencedTaskRunner> off_sequence_request_sequence_;

  SEQUENCE_CHECKER(sequence_checker_);
};

class V8PerFrameMemoryFrameData {
 public:
  V8PerFrameMemoryFrameData() = default;
  virtual ~V8PerFrameMemoryFrameData() = default;

  bool operator==(const V8PerFrameMemoryFrameData& other) const {
    return v8_bytes_used_ == other.v8_bytes_used_;
  }

  // Returns the number of bytes used by V8 for this frame at the last
  // measurement.
  uint64_t v8_bytes_used() const { return v8_bytes_used_; }

  void set_v8_bytes_used(uint64_t v8_bytes_used) {
    v8_bytes_used_ = v8_bytes_used;
  }

  // Returns frame data for the given node, or nullptr if no measurement has
  // been taken. The returned pointer must only be accessed on the graph
  // sequence and may go invalid at any time after leaving the calling scope.
  static const V8PerFrameMemoryFrameData* ForFrameNode(const FrameNode* node);

 private:
  uint64_t v8_bytes_used_ = 0;
};

class V8PerFrameMemoryProcessData {
 public:
  V8PerFrameMemoryProcessData() = default;
  virtual ~V8PerFrameMemoryProcessData() = default;

  bool operator==(const V8PerFrameMemoryProcessData& other) const {
    return unassociated_v8_bytes_used_ == other.unassociated_v8_bytes_used_;
  }

  // Returns the number of bytes used by V8 at the last measurement in this
  // process that could not be attributed to a frame.
  uint64_t unassociated_v8_bytes_used() const {
    return unassociated_v8_bytes_used_;
  }

  void set_unassociated_v8_bytes_used(uint64_t unassociated_v8_bytes_used) {
    unassociated_v8_bytes_used_ = unassociated_v8_bytes_used;
  }

  // Returns process data for the given node, or nullptr if no measurement has
  // been taken. The returned pointer must only be accessed on the graph
  // sequence and may go invalid at any time after leaving the calling scope.
  static const V8PerFrameMemoryProcessData* ForProcessNode(
      const ProcessNode* node);

 private:
  uint64_t unassociated_v8_bytes_used_ = 0;
};

class V8PerFrameMemoryObserver : public base::CheckedObserver {
 public:
  // Called on the PM sequence when a measurement is available for
  // |process_node|. |process_data| contains the process-level measurements for
  // the process, and can go invalid at any time after returning from this
  // method. Per-frame measurements can be read by walking the graph from
  // |process_node| to find frame nodes, and calling
  // V8PerFrameMemoryFrameData::ForFrameNode to retrieve the measurement data.
  virtual void OnV8MemoryMeasurementAvailable(
      const ProcessNode* process_node,
      const V8PerFrameMemoryProcessData* process_data) = 0;
};

// Observer that can be created on any sequence, and will be notified on that
// sequence when measurements are available. Register the observer through
// V8PerFrameMemoryRequestAnySeq::AddObserver. The
// V8PerFrameMemoryRequestAnySeq must live on the same sequence as the
// observer.
class V8PerFrameMemoryObserverAnySeq : public base::CheckedObserver {
 public:
  // TODO(crbug.com/1096617): Should use FrameToken here instead of routing id.
  using FrameDataMap =
      base::flat_map<content::GlobalFrameRoutingId, V8PerFrameMemoryFrameData>;

  // Called on the observer's sequence when a measurement is available for the
  // process with ID |render_process_host_id|. The notification includes the
  // measurement data for the process and each frame that had a result in that
  // process at the time of the measurement, so that the implementer doesn't
  // need to return to the PM sequence to read it.
  virtual void OnV8MemoryMeasurementAvailable(
      RenderProcessHostId render_process_host_id,
      const V8PerFrameMemoryProcessData& process_data,
      const FrameDataMap& frame_data) = 0;
};

// Wrapper that can instantiate a V8PerFrameMemoryRequest from any sequence.
class V8PerFrameMemoryRequestAnySeq {
 public:
  using MeasurementMode = V8PerFrameMemoryRequest::MeasurementMode;

  explicit V8PerFrameMemoryRequestAnySeq(
      const base::TimeDelta& min_time_between_requests,
      MeasurementMode mode = MeasurementMode::kDefault);
  ~V8PerFrameMemoryRequestAnySeq();

  V8PerFrameMemoryRequestAnySeq(const V8PerFrameMemoryRequestAnySeq&) = delete;
  V8PerFrameMemoryRequestAnySeq& operator=(
      const V8PerFrameMemoryRequestAnySeq&) = delete;

  // Returns whether |observer| is in |observers_|.
  bool HasObserver(V8PerFrameMemoryObserverAnySeq* observer);

  // Adds an observer that was created on the same sequence as the
  // V8PerFrameMemoryRequestAnySeq.
  void AddObserver(V8PerFrameMemoryObserverAnySeq* observer);

  // Removes an observer that was added with AddObserver.
  void RemoveObserver(V8PerFrameMemoryObserverAnySeq* observer);

  // Implementation details below this point.

  // V8PerFrameMemoryRequest calls NotifyObserversOnMeasurementAvailable when
  // a measurement is received.
  void NotifyObserversOnMeasurementAvailable(
      util::PassKey<V8PerFrameMemoryRequest>,
      RenderProcessHostId render_process_host_id,
      const V8PerFrameMemoryProcessData& process_data,
      const V8PerFrameMemoryObserverAnySeq::FrameDataMap& frame_data) const;

 private:
  std::unique_ptr<V8PerFrameMemoryRequest> request_;
  base::ObserverList<V8PerFrameMemoryObserverAnySeq, /*check_empty=*/true>
      observers_;

  // This object can live on any sequence but all methods and the destructor
  // must be called from that sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<V8PerFrameMemoryRequestAnySeq> weak_factory_{this};
};

namespace internal {

// A callback that will bind a V8DetailedMemoryReporter interface to
// communicate with the given process. Exposed so that it can be overridden to
// implement the interface with a test fake.
using BindV8DetailedMemoryReporterCallback = base::RepeatingCallback<void(
    mojo::PendingReceiver<blink::mojom::V8DetailedMemoryReporter>,
    RenderProcessHostProxy)>;

// Sets a callback that will be used to bind the V8PerFrameMemoryReporter
// interface. The callback is owned by the caller and must live until this
// function is called again with nullptr.
void SetBindV8DetailedMemoryReporterCallbackForTesting(
    BindV8DetailedMemoryReporterCallback* callback);

// Enables or disables MeasurementMode::kEagerModeForTesting. Creating eager
// measurement requests can have a high performance penalty so this should only
// be enabled in tests.
void SetEagerMemoryMeasurementEnabledForTesting(bool enable);

}  // namespace internal

}  // namespace v8_memory

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_V8_MEMORY_V8_PER_FRAME_MEMORY_DECORATOR_H_
