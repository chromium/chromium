// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_V8_MEMORY_V8_DETAILED_MEMORY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_V8_MEMORY_V8_DETAILED_MEMORY_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"

namespace performance_manager {

namespace execution_context {
class ExecutionContext;
}  // namespace execution_context

class Graph;
class FrameNode;
class ProcessNode;
class WorkerNode;

namespace v8_memory {

// Classes that query each renderer process for the amount of memory used by V8
// in each frame.
//
// To start sampling create a V8DetailedMemoryRequest object that specifies how
// often to request a memory measurement. Delete the object when you no longer
// need measurements. Measurement involves some overhead so choose the lowest
// sampling frequency your use case needs. Performance Manager will use the
// highest sampling frequency that any caller requests, and stop measurements
// entirely when no more request objects exist.
//
// When measurements are available Performance Manager attaches them to
// V8DetailedMemoryExecutionContextData and V8DetailedMemoryProcessData objects
// that can be retrieved with V8DetailedMemoryExecutionContextData's
// ForFrameNode, ForWorkerNode and ForExecutionContext, and
// V8DetailedMemoryProcessData's ForProcessNode.  V8DetailedMemoryProcessData
// objects can be cleaned up when V8DetailedMemoryRequest objects are deleted
// so callers must save the measurements they are interested in before
// releasing their V8DetailedMemoryRequest.
//
// Callers can be notified when a request is available by implementing
// V8DetailedMemoryObserver.
//
// V8DetailedMemoryRequest, V8DetailedMemoryExecutionContextData and
// V8DetailedMemoryProcessData must all be accessed on the graph sequence, and
// V8DetailedMemoryObserver::OnV8MemoryMeasurementAvailable will be called on
// this sequence. To request memory measurements from another sequence use the
// wrappers in v8_detailed_memory_any_seq.h.
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
//           " reported " << data->detached_v8_bytes_used() <<
//           " bytes of V8 memory that wasn't associated with a frame.";
//       LOG(INFO) << "Process " << process_node->GetProcessId() <<
//           " reported " << data->shared_v8_bytes_used() <<
//           " bytes of V8 memory that are shared between all frames";
//       for (auto* frame_node : process_node->GetFrameNodes()) {
//         auto* frame_data =
//             V8DetailedMemoryExecutionContextData::ForFrame(frame_node);
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
//           base::Seconds(30), graph);
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

//////////////////////////////////////////////////////////////////////////////
// The following classes report results from memory measurements.

class V8DetailedMemoryExecutionContextData {
 public:
  V8DetailedMemoryExecutionContextData();
  virtual ~V8DetailedMemoryExecutionContextData();
  V8DetailedMemoryExecutionContextData(
      const V8DetailedMemoryExecutionContextData&);
  V8DetailedMemoryExecutionContextData(V8DetailedMemoryExecutionContextData&&);
  V8DetailedMemoryExecutionContextData& operator=(
      const V8DetailedMemoryExecutionContextData&);
  V8DetailedMemoryExecutionContextData& operator=(
      V8DetailedMemoryExecutionContextData&&);

  bool operator==(const V8DetailedMemoryExecutionContextData& other) const {
    return v8_bytes_used_ == other.v8_bytes_used_ && url_ == other.url_;
  }

  // Returns the number of bytes used by V8 for this frame at the last
  // measurement.
  uint64_t v8_bytes_used() const { return v8_bytes_used_; }

  void set_v8_bytes_used(uint64_t v8_bytes_used) {
    v8_bytes_used_ = v8_bytes_used;
  }

  // Returns the number of bytes used by canvas elements for this frame at the
  // last measurement. It is empty if the frame has no canvas elements.
  std::optional<uint64_t> canvas_bytes_used() const {
    return canvas_bytes_used_;
  }

  void set_canvas_bytes_used(uint64_t canvas_bytes_used) {
    canvas_bytes_used_ = canvas_bytes_used;
  }

  // TODO(crbug.com/40093136): Remove this once PlzDedicatedWorker ships. Until
  // then the browser does not know URLs of dedicated workers, so we pass them
  // together with the measurement result and store in ExecutionContext data.
  std::optional<std::string> url() const { return url_; }

  void set_url(std::optional<std::string> url) { url_ = std::move(url); }

  // Returns frame data for the given node, or nullptr if no measurement has
  // been taken. The returned pointer must only be accessed on the graph
  // sequence and may go invalid at any time after leaving the calling scope.
  static const V8DetailedMemoryExecutionContextData* ForFrameNode(
      const FrameNode* node);
  static const V8DetailedMemoryExecutionContextData* ForWorkerNode(
      const WorkerNode* node);
  static const V8DetailedMemoryExecutionContextData* ForExecutionContext(
      const execution_context::ExecutionContext* ec);

 private:
  friend class WebMemoryTestHarness;

  static V8DetailedMemoryExecutionContextData* CreateForTesting(
      const FrameNode* node);
  static V8DetailedMemoryExecutionContextData* CreateForTesting(
      const WorkerNode* node);

  uint64_t v8_bytes_used_ = 0;
  std::optional<uint64_t> canvas_bytes_used_;
  std::optional<std::string> url_;
};

class V8DetailedMemoryProcessData {
 public:
  V8DetailedMemoryProcessData() = default;
  virtual ~V8DetailedMemoryProcessData() = default;

  bool operator==(const V8DetailedMemoryProcessData& other) const {
    return detached_v8_bytes_used_ == other.detached_v8_bytes_used_ &&
           shared_v8_bytes_used_ == other.shared_v8_bytes_used_;
  }

  // Returns the number of bytes used by V8 at the last measurement in this
  // process that could not be attributed to a frame.
  uint64_t detached_v8_bytes_used() const { return detached_v8_bytes_used_; }

  void set_detached_v8_bytes_used(uint64_t detached_v8_bytes_used) {
    detached_v8_bytes_used_ = detached_v8_bytes_used;
  }

  // Returns the number of bytes used by canvas elements at the last
  // measurement in this process that could not be attributed to a frame.
  uint64_t detached_canvas_bytes_used() const {
    return detached_canvas_bytes_used_;
  }

  void set_detached_canvas_bytes_used(uint64_t detached_canvas_bytes_used) {
    detached_canvas_bytes_used_ = detached_canvas_bytes_used;
  }

  // Returns the number of bytes used by V8 at the last measurement in this
  // process that are shared between all frames.
  uint64_t shared_v8_bytes_used() const { return shared_v8_bytes_used_; }

  void set_shared_v8_bytes_used(uint64_t shared_v8_bytes_used) {
    shared_v8_bytes_used_ = shared_v8_bytes_used;
  }

  // Returns the number of bytes used by Blink heaps corresponding to V8
  // isolates at the last measurement in this process that are shared between
  // all frames.
  uint64_t blink_bytes_used() const { return blink_bytes_used_; }

  void set_blink_bytes_used(uint64_t blink_bytes_used) {
    blink_bytes_used_ = blink_bytes_used;
  }

  // Returns process data for the given node, or nullptr if no measurement has
  // been taken. The returned pointer must only be accessed on the graph
  // sequence and may go invalid at any time after leaving the calling scope.
  static const V8DetailedMemoryProcessData* ForProcessNode(
      const ProcessNode* node);

 private:
  friend class WebMemoryTestHarness;

  static V8DetailedMemoryProcessData* GetOrCreateForTesting(
      const ProcessNode* node);
  uint64_t detached_v8_bytes_used_ = 0;
  uint64_t detached_canvas_bytes_used_ = 0;
  uint64_t shared_v8_bytes_used_ = 0;
  uint64_t blink_bytes_used_ = 0;
};

class V8DetailedMemoryObserver : public base::CheckedObserver {
 public:
  // Called on the PM sequence when a measurement is available for
  // |process_node|. |process_data| contains the process-level measurements for
  // the process, and can go invalid at any time after returning from this
  // method. Per-frame measurements can be read by walking the graph from
  // |process_node| to find frame nodes, and calling
  // V8DetailedMemoryExecutionContextData::ForFrameNode to retrieve the
  // measurement data.
  virtual void OnV8MemoryMeasurementAvailable(
      const ProcessNode* process_node,
      const V8DetailedMemoryProcessData* process_data) = 0;
};

//////////////////////////////////////////////////////////////////////////////
// The following classes create requests for memory measurements.

class V8DetailedMemoryDecorator;
class V8DetailedMemoryRequestAnySeq;
class V8DetailedMemoryRequestOneShot;
class V8DetailedMemoryRequestQueue;

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

  MeasurementMode mode() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return mode_;
  }

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
      base::PassKey<V8DetailedMemoryRequestAnySeq>,
      const base::TimeDelta& min_time_between_requests,
      MeasurementMode mode,
      std::optional<base::WeakPtr<ProcessNode>> process_to_measure,
      base::WeakPtr<V8DetailedMemoryRequestAnySeq> off_sequence_request);

  // Private constructor for V8DetailedMemoryRequestOneShot. Sets
  // min_time_between_requests_ to 0, which is not allowed for repeating
  // requests, and registers |on_owner_unregistered_closure| to be called from
  // OnOwnerUnregistered.
  V8DetailedMemoryRequest(base::PassKey<V8DetailedMemoryRequestOneShot>,
                          MeasurementMode mode,
                          base::OnceClosure on_owner_unregistered_closure);

  // A V8DetailedMemoryRequestQueue calls OnOwnerUnregistered for all requests
  // in the queue when the owning decorator or process node is removed from the
  // graph.
  void OnOwnerUnregistered(base::PassKey<V8DetailedMemoryRequestQueue>);

  // A V8DetailedMemoryRequestQueue calls NotifyObserversOnMeasurementAvailable
  // when a measurement is received.
  void NotifyObserversOnMeasurementAvailable(
      base::PassKey<V8DetailedMemoryRequestQueue>,
      const ProcessNode* process_node) const;

 private:
  void StartMeasurementFromOffSequence(
      std::optional<base::WeakPtr<ProcessNode>> process_to_measure,
      Graph* graph);
  void StartMeasurementImpl(Graph* graph, const ProcessNode* process_node);

  base::TimeDelta min_time_between_requests_
      GUARDED_BY_CONTEXT(sequence_checker_);
  MeasurementMode mode_ GUARDED_BY_CONTEXT(sequence_checker_);
  raw_ptr<V8DetailedMemoryDecorator> decorator_
      GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;
  base::ObserverList<V8DetailedMemoryObserver, /*check_empty=*/true> observers_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Pointer back to the off-sequence V8DetailedMemoryRequestAnySeq that
  // created this, if any.
  base::WeakPtr<V8DetailedMemoryRequestAnySeq> off_sequence_request_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Sequence that |off_sequence_request_| lives on.
  scoped_refptr<base::SequencedTaskRunner> off_sequence_request_sequence_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Additional closure that will be called from OnOwnerUnregistered for
  // one-shot requests. Used to clean up resources in the
  // V8DetailedMemoryRequestOneShot wrapper.
  base::OnceClosure on_owner_unregistered_closure_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

class V8DetailedMemoryRequestOneShotAnySeq;

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

  MeasurementMode mode() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return mode_;
  }

  // V8DetailedMemoryObserver implementation.

  void OnV8MemoryMeasurementAvailable(
      const ProcessNode* process_node,
      const V8DetailedMemoryProcessData* process_data) final;

  // Implementation details below this point.

  // Private constructor for V8DetailedMemoryRequestOneShotAnySeq. Will be
  // called from off-sequence.
  V8DetailedMemoryRequestOneShot(
      base::PassKey<V8DetailedMemoryRequestOneShotAnySeq>,
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
  raw_ptr<const ProcessNode, AcrossTasksDanglingUntriaged> process_
      GUARDED_BY_CONTEXT(sequence_checker_);
#endif

  MeasurementCallback callback_ GUARDED_BY_CONTEXT(sequence_checker_);
  MeasurementMode mode_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<V8DetailedMemoryRequest> request_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

//////////////////////////////////////////////////////////////////////////////
// The following internal functions are exposed in the header for testing.

namespace internal {

// Enables or disables MeasurementMode::kEagerModeForTesting. Creating eager
// measurement requests can have a high performance penalty so this should only
// be enabled in tests.
void SetEagerMemoryMeasurementEnabledForTesting(bool enable);

}  // namespace internal

}  // namespace v8_memory

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_V8_MEMORY_V8_DETAILED_MEMORY_H_
