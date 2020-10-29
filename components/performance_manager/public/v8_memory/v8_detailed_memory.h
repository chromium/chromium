// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_V8_MEMORY_V8_DETAILED_MEMORY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_V8_MEMORY_V8_DETAILED_MEMORY_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
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
// To start sampling create a V8DetailedMemoryRequest object that specifies how
// often to request a memory measurement. Delete the object when you no longer
// need measurements. Measurement involves some overhead so choose the lowest
// sampling frequency your use case needs. The decorator will use the highest
// sampling frequency that any caller requests, and stop measurements entirely
// when no more V8DetailedMemoryRequest objects exist.
//
// When measurements are available the decorator attaches them to
// V8DetailedMemoryFrameData and V8DetailedMemoryProcessData objects that can
// be retrieved with V8DetailedMemoryFrameData::ForFrameNode and
// V8DetailedMemoryProcessData::ForProcessNode. V8DetailedMemoryProcessData
// objects can be cleaned up when V8DetailedMemoryRequest objects are deleted
// so callers must save the measurements they are interested in before
// releasing their V8DetailedMemoryRequest.
//
// Callers can be notified when a request is available by implementing
// V8DetailedMemoryObserver.
//
// V8DetailedMemoryRequest, V8DetailedMemoryFrameData and
// V8DetailedMemoryProcessData must all be accessed on the graph sequence, and
// V8DetailedMemoryObserver::OnV8MemoryMeasurementAvailable will be called on
// this sequence. To request memory measurements from another sequence use the
// V8DetailedMemoryRequestAnySeq and V8DetailedMemoryObserverAnySeq wrappers.
//
// Usage:
//
// Take a memory measurement every 30 seconds and register an observer for the
// results:
//
//   class Observer : public V8DetailedMemoryObserver {
//    public:
//     // Called on the PM sequence for each process.
//     void OnV8MemoryMeasurementAvailable(
//         const ProcessNode* process_node,
//         const V8DetailedMemoryProcessData* data) override {
//       DCHECK(data);
//       LOG(INFO) << "Process " << process_node->GetProcessId() <<
//           " reported " << data->unassociated_v8_bytes_used() <<
//           " bytes of V8 memory that wasn't associated with a frame.";
//       for (auto* frame_node : process_node->GetFrameNodes()) {
//         auto* frame_data = V8DetailedMemoryFrameData::ForFrame(frame_node);
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
//       // Creating a V8DetailedMemoryRequest with the |graph| parameter
//       // automatically starts measurements.
//       request_ = std::make_unique<V8DetailedMemoryRequest>(
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
//     std::unique_ptr<V8DetailedMemoryRequest> request_;
//     std::unique_ptr<Observer> observer_;
//   };
//
// Same, but from the another thread:
//
//   class Observer : public V8DetailedMemoryObserverAnySeq {
//    public:
//     // Called on the same sequence for each process.
//     void OnV8MemoryMeasurementAvailable(
//         RenderProcessHostId process_id,
//         const V8DetailedMemoryProcessData& process_data,
//         const V8DetailedMemoryObserverAnySeq::FrameDataMap& frame_data)
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
//             V8DetailedMemoryFrameData
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
//       // Creating a V8DetailedMemoryRequest with the |graph| parameter
//       // automatically starts measurements.
//       request_ = std::make_unique<V8DetailedMemoryRequestAnySeq>(
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
//     std::unique_ptr<V8DetailedMemoryRequestAnySeq> request_;
//     std::unique_ptr<Observer> observer_;
//
//     SEQUENCE_CHECKER(sequence_checker_);
//   };

class V8DetailedMemoryObserver;
class V8DetailedMemoryProcessData;
class V8DetailedMemoryRequest;
class V8DetailedMemoryRequestAnySeq;
class V8DetailedMemoryRequestOneShot;
class V8DetailedMemoryRequestOneShotAnySeq;

class V8DetailedMemoryDecorator
    : public GraphOwned,
      public GraphRegisteredImpl<V8DetailedMemoryDecorator>,
      public ProcessNode::ObserverDefaultImpl,
      public NodeDataDescriberDefaultImpl {
 public:
  // A priority queue of memory requests. The decorator will hold a global
  // queue of requests that measure every process, and each ProcessNode will
  // have a queue of requests that measure only that process.
  class MeasurementRequestQueue;

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
  base::Value DescribeFrameNodeData(const FrameNode* node) const override;
  base::Value DescribeProcessNodeData(const ProcessNode* node) const override;

  // Returns the next measurement request that should be scheduled.
  const V8DetailedMemoryRequest* GetNextRequest() const;

  // Returns the next measurement request with mode kBounded or
  // kEagerForTesting that should be scheduled.
  const V8DetailedMemoryRequest* GetNextBoundedRequest() const;

  // Implementation details below this point.

  // V8DetailedMemoryRequest objects register themselves with the decorator.
  // If |process_node| is null, the request will be sent to every renderer
  // process, otherwise it will be sent only to |process_node|.
  void AddMeasurementRequest(util::PassKey<V8DetailedMemoryRequest>,
                             V8DetailedMemoryRequest* request,
                             const ProcessNode* process_node = nullptr);
  void RemoveMeasurementRequest(util::PassKey<V8DetailedMemoryRequest>,
                                V8DetailedMemoryRequest* request);

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

//////////////////////////////////////////////////////////////////////////////
// The following classes report results from memory measurements.

class V8DetailedMemoryFrameData {
 public:
  V8DetailedMemoryFrameData() = default;
  virtual ~V8DetailedMemoryFrameData() = default;

  bool operator==(const V8DetailedMemoryFrameData& other) const {
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
  static const V8DetailedMemoryFrameData* ForFrameNode(const FrameNode* node);

 private:
  friend class WebMemoryAggregatorTest;
  // Creates frame data for the given node.
  static V8DetailedMemoryFrameData* CreateForTesting(const FrameNode* node);

  uint64_t v8_bytes_used_ = 0;
};

class V8DetailedMemoryProcessData {
 public:
  V8DetailedMemoryProcessData() = default;
  virtual ~V8DetailedMemoryProcessData() = default;

  bool operator==(const V8DetailedMemoryProcessData& other) const {
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
  static const V8DetailedMemoryProcessData* ForProcessNode(
      const ProcessNode* node);

 private:
  uint64_t unassociated_v8_bytes_used_ = 0;
};

class V8DetailedMemoryObserver : public base::CheckedObserver {
 public:
  // Called on the PM sequence when a measurement is available for
  // |process_node|. |process_data| contains the process-level measurements for
  // the process, and can go invalid at any time after returning from this
  // method. Per-frame measurements can be read by walking the graph from
  // |process_node| to find frame nodes, and calling
  // V8DetailedMemoryFrameData::ForFrameNode to retrieve the measurement data.
  virtual void OnV8MemoryMeasurementAvailable(
      const ProcessNode* process_node,
      const V8DetailedMemoryProcessData* process_data) = 0;
};

//////////////////////////////////////////////////////////////////////////////
// The following classes create requests for memory measurements.

class V8DetailedMemoryRequest {
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
  explicit V8DetailedMemoryRequest(
      const base::TimeDelta& min_time_between_requests,
      MeasurementMode mode = MeasurementMode::kDefault);

  // Creates a request and calls StartMeasurement with the given |graph| and
  // |min_time_between_requests|, using the default measurement mode.
  V8DetailedMemoryRequest(const base::TimeDelta& min_time_between_requests,
                          Graph* graph);

  // Creates a request and calls StartMeasurement with the given |graph|,
  // |min_time_between_requests|, and |mode|.
  V8DetailedMemoryRequest(const base::TimeDelta& min_time_between_requests,
                          MeasurementMode mode,
                          Graph* graph);

  ~V8DetailedMemoryRequest();

  V8DetailedMemoryRequest(const V8DetailedMemoryRequest&) = delete;
  V8DetailedMemoryRequest& operator=(const V8DetailedMemoryRequest&) = delete;

  const base::TimeDelta& min_time_between_requests() const {
    return min_time_between_requests_;
  }

  MeasurementMode mode() const { return mode_; }

  // Requests measurements for all ProcessNode's in |graph|. There must be at
  // most one call to this or StartMeasurementForProcess for each
  // V8DetailedMemoryRequest.
  void StartMeasurement(Graph* graph);

  // Requests measurements only for the given |process_node|, which must be a
  // renderer process. There must be at most one call to this or
  // StartMeasurement for each V8DetailedMemoryRequest.
  void StartMeasurementForProcess(const ProcessNode* process_node);

  // Adds/removes an observer.
  void AddObserver(V8DetailedMemoryObserver* observer);
  void RemoveObserver(V8DetailedMemoryObserver* observer);

  // Implementation details below this point.

  // Private constructor for V8DetailedMemoryRequestAnySeq. Saves
  // |off_sequence_request| as a pointer to the off-sequence object that
  // triggered the request and starts measurements with frequency
  // |min_time_between_requests|. If |process_to_measure| is nullopt, the
  // request will be sent to every renderer process, otherwise it will be sent
  // only to |process_to_measure|.
  V8DetailedMemoryRequest(
      util::PassKey<V8DetailedMemoryRequestAnySeq>,
      const base::TimeDelta& min_time_between_requests,
      MeasurementMode mode,
      base::Optional<base::WeakPtr<ProcessNode>> process_to_measure,
      base::WeakPtr<V8DetailedMemoryRequestAnySeq> off_sequence_request);

  // Private constructor for V8DetailedMemoryRequestOneShot. Sets
  // min_time_between_requests_ to 0, which is not allowed for repeating
  // requests, and registers |on_owner_unregistered_closure| to be called from
  // OnOwnerUnregistered.
  V8DetailedMemoryRequest(util::PassKey<V8DetailedMemoryRequestOneShot>,
                          MeasurementMode mode,
                          base::OnceClosure on_owner_unregistered_closure);

  // V8DetailedMemoryDecorator::MeasurementRequestQueue calls
  // OnOwnerUnregistered for all requests in the queue when the owning
  // decorator or process node is removed from the graph.
  void OnOwnerUnregistered(
      util::PassKey<V8DetailedMemoryDecorator::MeasurementRequestQueue>);

  // V8DetailedMemoryDecorator::MeasurementRequestQueue calls
  // NotifyObserversOnMeasurementAvailable when a measurement is received.
  void NotifyObserversOnMeasurementAvailable(
      util::PassKey<V8DetailedMemoryDecorator::MeasurementRequestQueue>,
      const ProcessNode* process_node) const;

 private:
  void StartMeasurementFromOffSequence(
      base::Optional<base::WeakPtr<ProcessNode>> process_to_measure,
      Graph* graph);
  void StartMeasurementImpl(Graph* graph, const ProcessNode* process_node);

  base::TimeDelta min_time_between_requests_;
  MeasurementMode mode_;
  V8DetailedMemoryDecorator* decorator_ = nullptr;
  base::ObserverList<V8DetailedMemoryObserver, /*check_empty=*/true> observers_;

  // Pointer back to the off-sequence V8DetailedMemoryRequestAnySeq that
  // created this, if any.
  base::WeakPtr<V8DetailedMemoryRequestAnySeq> off_sequence_request_;

  // Sequence that |off_sequence_request_| lives on.
  scoped_refptr<base::SequencedTaskRunner> off_sequence_request_sequence_;

  // Additional closure that will be called from OnOwnerUnregistered for
  // one-shot requests. Used to clean up resources in the
  // V8DetailedMemoryRequestOneShot wrapper.
  base::OnceClosure on_owner_unregistered_closure_;

  SEQUENCE_CHECKER(sequence_checker_);
};

class V8DetailedMemoryRequestOneShot final : public V8DetailedMemoryObserver {
 public:
  // A callback that will be passed the results of the measurement. |process|
  // will always match the value passed to the V8DetailedMemoryRequestOneShot
  // constructor.
  using MeasurementCallback =
      base::OnceCallback<void(const ProcessNode* process,
                              const V8DetailedMemoryProcessData* process_data)>;

  using MeasurementMode = V8DetailedMemoryRequest::MeasurementMode;

  // Creates a one-shot memory measurement request that will be sent when
  // StartMeasurement is called.
  explicit V8DetailedMemoryRequestOneShot(
      MeasurementMode mode = MeasurementMode::kDefault);

  // Creates a one-shot memory measurement request and calls StartMeasurement.
  V8DetailedMemoryRequestOneShot(
      const ProcessNode* process,
      MeasurementCallback callback,
      MeasurementMode mode = MeasurementMode::kDefault);

  ~V8DetailedMemoryRequestOneShot() final;

  V8DetailedMemoryRequestOneShot(const V8DetailedMemoryRequestOneShot&) =
      delete;
  V8DetailedMemoryRequestOneShot& operator=(
      const V8DetailedMemoryRequestOneShot&) = delete;

  // Sends the measurement request to |process| (which must be a renderer
  // process). The process will perform the measurement during a GC as
  // determined by the MeasurementMode, and |callback| will be called with the
  // results.
  //
  // |callback| is owned by the request object but will be destroyed after it
  // is called or once no response can be received (such as if the ProcessNode
  // is destroyed). It is safe for the callback to own resources that will be
  // freed when the callback is destroyed. It is even safe for the callback to
  // own |this|, making the V8DetailedMemoryRequestOneShot self-owning (it will
  // be deleted along with the callback).
  void StartMeasurement(const ProcessNode* process,
                        MeasurementCallback callback);

  MeasurementMode mode() const { return mode_; }

  // V8DetailedMemoryObserver implementation.

  void OnV8MemoryMeasurementAvailable(
      const ProcessNode* process_node,
      const V8DetailedMemoryProcessData* process_data) final;

  // Implementation details below this point.

  // Private constructor for V8DetailedMemoryRequestOneShotAnySeq. Will be
  // called from off-sequence.
  V8DetailedMemoryRequestOneShot(
      util::PassKey<V8DetailedMemoryRequestOneShotAnySeq>,
      base::WeakPtr<ProcessNode> process,
      MeasurementCallback callback,
      MeasurementMode mode = MeasurementMode::kDefault);

 private:
  void InitializeRequest();
  void StartMeasurementFromOffSequence(base::WeakPtr<ProcessNode>,
                                       MeasurementCallback callback);
  void DeleteRequest();
  void OnOwnerUnregistered();

#if DCHECK_IS_ON()
  const ProcessNode* process_;
#endif

  MeasurementCallback callback_;
  MeasurementMode mode_;
  std::unique_ptr<V8DetailedMemoryRequest> request_;

  SEQUENCE_CHECKER(sequence_checker_);
};

//////////////////////////////////////////////////////////////////////////////
// The following classes are wrappers that can be called from outside the PM
// sequence.

// Observer that can be created on any sequence, and will be notified on that
// sequence when measurements are available. Register the observer through
// V8DetailedMemoryRequestAnySeq::AddObserver. The
// V8DetailedMemoryRequestAnySeq must live on the same sequence as the
// observer.
class V8DetailedMemoryObserverAnySeq : public base::CheckedObserver {
 public:
  // TODO(crbug.com/1096617): Should use FrameToken here instead of routing id.
  using FrameDataMap =
      base::flat_map<content::GlobalFrameRoutingId, V8DetailedMemoryFrameData>;

  // Called on the observer's sequence when a measurement is available for the
  // process with ID |render_process_host_id|. The notification includes the
  // measurement data for the process and each frame that had a result in that
  // process at the time of the measurement, so that the implementer doesn't
  // need to return to the PM sequence to read it.
  virtual void OnV8MemoryMeasurementAvailable(
      RenderProcessHostId render_process_host_id,
      const V8DetailedMemoryProcessData& process_data,
      const FrameDataMap& frame_data) = 0;
};

// Wrapper that can instantiate a V8DetailedMemoryRequest from any sequence.
class V8DetailedMemoryRequestAnySeq {
 public:
  using MeasurementMode = V8DetailedMemoryRequest::MeasurementMode;

  // Creates a memory measurement request that will be sent repeatedly with at
  // least |min_time_between_requests| between each measurement. The request
  // will be sent to the process with ID |process_to_measure|, which must be a
  // renderer process, or to all renderer processes if |process_to_measure| is
  // nullopt. The process will perform the measurement during a GC as determined
  // by |mode|.
  explicit V8DetailedMemoryRequestAnySeq(
      const base::TimeDelta& min_time_between_requests,
      MeasurementMode mode = MeasurementMode::kDefault,
      base::Optional<RenderProcessHostId> process_to_measure = base::nullopt);
  ~V8DetailedMemoryRequestAnySeq();

  V8DetailedMemoryRequestAnySeq(const V8DetailedMemoryRequestAnySeq&) = delete;
  V8DetailedMemoryRequestAnySeq& operator=(
      const V8DetailedMemoryRequestAnySeq&) = delete;

  // Returns whether |observer| is in |observers_|.
  bool HasObserver(V8DetailedMemoryObserverAnySeq* observer);

  // Adds an observer that was created on the same sequence as the
  // V8DetailedMemoryRequestAnySeq.
  void AddObserver(V8DetailedMemoryObserverAnySeq* observer);

  // Removes an observer that was added with AddObserver.
  void RemoveObserver(V8DetailedMemoryObserverAnySeq* observer);

  // Implementation details below this point.

  // V8DetailedMemoryRequest calls NotifyObserversOnMeasurementAvailable when
  // a measurement is received.
  void NotifyObserversOnMeasurementAvailable(
      util::PassKey<V8DetailedMemoryRequest>,
      RenderProcessHostId render_process_host_id,
      const V8DetailedMemoryProcessData& process_data,
      const V8DetailedMemoryObserverAnySeq::FrameDataMap& frame_data) const;

 private:
  void InitializeWrappedRequest(
      const base::TimeDelta& min_time_between_requests,
      MeasurementMode mode,
      base::Optional<base::WeakPtr<ProcessNode>> process_to_measure);

  std::unique_ptr<V8DetailedMemoryRequest> request_;
  base::ObserverList<V8DetailedMemoryObserverAnySeq, /*check_empty=*/true>
      observers_;

  // This object can live on any sequence but all methods and the destructor
  // must be called from that sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<V8DetailedMemoryRequestAnySeq> weak_factory_{this};
};

// Wrapper that can instantiate a V8DetailedMemoryRequestOneShot from any
// sequence.
class V8DetailedMemoryRequestOneShotAnySeq {
 public:
  using MeasurementMode = V8DetailedMemoryRequest::MeasurementMode;

  using FrameDataMap = V8DetailedMemoryObserverAnySeq::FrameDataMap;

  // A callback that will be called on the request's sequence with the results
  // of the measurement. |process_id| will always match the value passed to
  // the V8DetailedMemoryRequestOneShotAnySeq constructor.
  using MeasurementCallback =
      base::OnceCallback<void(RenderProcessHostId process_id,
                              const V8DetailedMemoryProcessData& process_data,
                              const FrameDataMap& frame_data)>;

  explicit V8DetailedMemoryRequestOneShotAnySeq(
      MeasurementMode mode = MeasurementMode::kDefault);

  V8DetailedMemoryRequestOneShotAnySeq(
      RenderProcessHostId process_id,
      MeasurementCallback callback,
      MeasurementMode mode = MeasurementMode::kDefault);

  ~V8DetailedMemoryRequestOneShotAnySeq();

  V8DetailedMemoryRequestOneShotAnySeq(
      const V8DetailedMemoryRequestOneShotAnySeq&) = delete;
  V8DetailedMemoryRequestOneShotAnySeq& operator=(
      const V8DetailedMemoryRequestOneShotAnySeq&) = delete;

  void StartMeasurement(RenderProcessHostId process_id,
                        MeasurementCallback callback);

 private:
  void InitializeWrappedRequest(MeasurementCallback callback,
                                MeasurementMode mode,
                                base::WeakPtr<ProcessNode>);

  // Called on the PM sequence when a measurement is available.
  // |sequence_bound_callback| will wrap the callback passed to the
  // constructor, so it is both called and freed on the request's sequence.
  static void OnMeasurementAvailable(
      base::SequenceBound<MeasurementCallback> sequence_bound_callback,
      const ProcessNode* process_node,
      const V8DetailedMemoryProcessData* process_data);

  MeasurementMode mode_;

  // The wrapped request. Must only be accessed from the PM sequence.
  std::unique_ptr<V8DetailedMemoryRequestOneShot> request_;

  // This object can live on any sequence but all methods and the destructor
  // must be called from that sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<V8DetailedMemoryRequestOneShotAnySeq> weak_factory_{
      this};
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

// Enables or disables MeasurementMode::kEagerModeForTesting. Creating eager
// measurement requests can have a high performance penalty so this should only
// be enabled in tests.
void SetEagerMemoryMeasurementEnabledForTesting(bool enable);

// Destroys the V8DetailedMemoryDecorator. Exposed for testing.
void DestroyV8DetailedMemoryDecoratorForTesting(Graph* graph);

}  // namespace internal

}  // namespace v8_memory

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_V8_MEMORY_V8_DETAILED_MEMORY_H_
