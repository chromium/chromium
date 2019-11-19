// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEAP_PROFILING_SUPERVISOR_H_
#define COMPONENTS_HEAP_PROFILING_SUPERVISOR_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "components/services/heap_profiling/public/mojom/heap_profiling_client.mojom.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"

namespace heap_profiling {

class ClientConnectionManager;
class Controller;
enum class Mode;

// This class presents a single interface for both tests and embedders to use
// the HeapProfilingService. This class is intended to be used from the
// browser/privileged process of the embedder.
//
// This class must be accessed from the UI thread.
//
// Internally, this class:
//   * Starts the HeapProfilingService.
//   * Hooks up all the connections so that the appropriate processes get
//     profiled.
class Supervisor {
 public:
  static Supervisor* GetInstance();

  // When this returns |false|, no method other than Start() or
  // SetClientConnectionManagerConstructor() can be called.
  bool HasStarted();

  // Embedders can use this method to force the Supervisor to instantiate a
  // ClientConnectionManager subclass during Start(). The function will be
  // called on the UI thread.
  using ClientConnectionManagerConstructor =
      std::unique_ptr<ClientConnectionManager> (*)(
          base::WeakPtr<Controller> controller_weak_ptr,
          Mode mode);
  void SetClientConnectionManagerConstructor(
      ClientConnectionManagerConstructor constructor);

  // Must be called at most once.
  // The first method is a convenience method that calls the second with
  // default parameters.
  // Start is an asynchronous operation that must hop to the IO thread and then
  // back to the UI thread. |callback| will be invoked on the UI thread after
  // this is finished.
  //
  // This is a brief period of time when this object is in a semi-initialized
  // state - when Start has been called, but the thread hops haven't finished.
  // We avoid this side case by:
  //   * Providing a |callback| for callers to use, if they need to do anything
  //     shortly after Start().
  //   * Relying on the assumption that in all other cases, the object is either
  //     fully initialized or not initialized. There are DCHECKs to enforce this
  //     assumption.
  void Start(base::OnceClosure callback);
  void Start(Mode mode,
             mojom::StackMode stack_mode,
             uint32_t sampling_rate,
             base::OnceClosure callback);

  Mode GetMode();

  // Starts profiling the process with the given id.
  void StartManualProfiling(base::ProcessId pid);

  // Returns the pids of all profiled processes. The callback is posted on the
  // UI thread.
  using GetProfiledPidsCallback =
      base::OnceCallback<void(std::vector<base::ProcessId> pids)>;
  void GetProfiledPids(GetProfiledPidsCallback callback);

  uint32_t GetSamplingRate();

  using TraceFinishedCallback =
      base::OnceCallback<void(bool success, std::string trace_json)>;

  // This method must be called from the UI thread. |callback| will be called
  // asynchronously on the UI thread.
  //
  // This function does the following:
  //   1. Starts tracing with no categories enabled.
  //   2. Requests and waits for memory_instrumentation service to dump to
  //   trace.
  //   3. Stops tracing.
  void RequestTraceWithHeapDump(TraceFinishedCallback callback, bool anonymize);

 private:
  friend class base::NoDestructor<Supervisor>;

  Supervisor();
  ~Supervisor();

  // Initialization stage 1: Start the Service on the IO thread.
  void StartServiceOnIOThread(
      mojo::PendingReceiver<memory_instrumentation::mojom::HeapProfiler>
          receiver,
      mojo::PendingRemote<memory_instrumentation::mojom::HeapProfilerHelper>
          remote_helper,
      Mode mode,
      mojom::StackMode stack_mode,
      uint32_t sampling_rate,
      base::OnceClosure callback);

  // Initialization stage 2: Start the ClientConnectManager on the UI thread.
  void FinishInitializationOnUIhread(
      Mode mode,
      base::OnceClosure closure,
      base::WeakPtr<Controller> controller_weak_ptr);

  void GetProfiledPidsOnIOThread(GetProfiledPidsCallback callback);

  // Bound to the IO thread.
  std::unique_ptr<Controller> controller_;

  // Bound to the UI thread.
  std::unique_ptr<ClientConnectionManager> client_connection_manager_;

  ClientConnectionManagerConstructor constructor_ = nullptr;

  bool started_ = false;

  DISALLOW_COPY_AND_ASSIGN(Supervisor);
};

}  // namespace heap_profiling

#endif  // COMPONENTS_HEAP_PROFILING_SUPERVISOR_H_
