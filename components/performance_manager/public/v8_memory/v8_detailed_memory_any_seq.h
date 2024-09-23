// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_V8_MEMORY_V8_DETAILED_MEMORY_ANY_SEQ_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_V8_MEMORY_V8_DETAILED_MEMORY_ANY_SEQ_H_

#include <memory>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "components/performance_manager/public/render_process_host_id.h"
#include "components/performance_manager/public/v8_memory/v8_detailed_memory.h"
#include "content/public/browser/global_routing_id.h"

namespace performance_manager {

class ProcessNode;

namespace v8_memory {

// Classes that work similarly to V8DetailedMemoryRequest and
// V8DetailedMemoryRequestOneShot, but can be called from any sequence.
//
// To start sampling create a V8DetailedMemoryRequestAnySeq object that
// specifies how often to request a memory measurement. Delete the object when
// you no longer need measurements. Measurement involves some overhead so
// choose the lowest sampling frequency your use case needs. Performance
// Manager will use the highest sampling frequency that any caller requests,
// and stop measurements entirely when no more request objects exist.
//
// When measurements are available each V8DetailedMemoryObserverAnySeq
// object you supply will be invoked with a copy of the results.
//
// Usage:
//
// Take a memory measurement every 30 seconds and register an observer for
// the results:
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
//           " reported " << process_data.detached_v8_bytes_used() <<
//           " bytes of V8 memory that wasn't associated with a frame.";
//       LOG(INFO) << "Process " << process_node->GetProcessId() <<
//           " reported " << data->shared_v8_bytes_used() <<
//           " bytes of V8 memory that are shared between all frames";
//       for (std::pair<
//             content::GlobalRenderFrameHostId,
//             V8DetailedMemoryExecutionContextData
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
//           base::Minutes(2));
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

// Observer that can be created on any sequence, and will be notified on
// that sequence when measurements are available. Register the observer
// through V8DetailedMemoryRequestAnySeq::AddObserver. The
// V8DetailedMemoryRequestAnySeq must live on the same sequence as the
// observer.
class V8DetailedMemoryObserverAnySeq : public base::CheckedObserver {
 public:
  // TODO(crbug.com/40136290): Should use ExecutionContext tokens here. We
  // should potentially also split "main" content from "isolated world" content,
  // and have a detailed V8ContextToken map for those who care about all the
  // details.
  using FrameDataMap = base::flat_map<content::GlobalRenderFrameHostId,
                                      V8DetailedMemoryExecutionContextData>;

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
      std::optional<RenderProcessHostId> process_to_measure = std::nullopt);
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
      base::PassKey<V8DetailedMemoryRequest>,
      RenderProcessHostId render_process_host_id,
      const V8DetailedMemoryProcessData& process_data,
      const V8DetailedMemoryObserverAnySeq::FrameDataMap& frame_data) const;

 private:
  void InitializeWrappedRequest(
      const base::TimeDelta& min_time_between_requests,
      MeasurementMode mode,
      std::optional<base::WeakPtr<ProcessNode>> process_to_measure);

  std::unique_ptr<V8DetailedMemoryRequest> request_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::ObserverList<V8DetailedMemoryObserverAnySeq, /*check_empty=*/true>
      observers_ GUARDED_BY_CONTEXT(sequence_checker_);

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

  MeasurementMode mode_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The wrapped request. Must only be accessed from the PM sequence.
  std::unique_ptr<V8DetailedMemoryRequestOneShot> request_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // This object can live on any sequence but all methods and the destructor
  // must be called from that sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<V8DetailedMemoryRequestOneShotAnySeq> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace v8_memory

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_V8_MEMORY_V8_DETAILED_MEMORY_ANY_SEQ_H_
