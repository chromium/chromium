// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEAP_PROFILING_IN_PROCESS_HEAP_PROFILER_CONTROLLER_H_
#define COMPONENTS_HEAP_PROFILING_IN_PROCESS_HEAP_PROFILER_CONTROLLER_H_

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/synchronization/atomic_flag.h"
#include "base/time/time.h"
#include "components/metrics/call_stacks/call_stack_profile_params.h"
#include "components/version_info/channel.h"

namespace heap_profiling {

// If this is enabled, reports with 0 samples (from clients who allocated less
// than the sampling rate threshold) will be uploaded so that they're included
// in the average as 0 bytes allocated.
BASE_DECLARE_FEATURE(kHeapProfilerIncludeZero);

// HeapProfilerController controls collection of sampled heap allocation
// snapshots for the current process.
class HeapProfilerController {
 public:
  enum class ProfilingEnabled {
    kNoController,
    kDisabled,
    kEnabled,
  };

  // Returns kEnabled if heap profiling is enabled, kDisabled if not. If no
  // HeapProfilerController exists the profiling state is indeterminate so the
  // function returns kNoController.
  static ProfilingEnabled GetProfilingEnabled();

  // Checks if heap profiling should be enabled for this process. If so, starts
  // sampling heap allocations immediately but does not schedule snapshots of
  // the samples until Start() is called. `channel` is used to determine the
  // probability that this client will be opted in to profiling. `process_type`
  // is the current process, which can be retrieved with GetProfileParamsProcess
  // in chrome/common/profiler/process_type.h.
  explicit HeapProfilerController(
      version_info::Channel channel,
      metrics::CallStackProfileParams::Process process_type);

  HeapProfilerController(const HeapProfilerController&) = delete;
  HeapProfilerController& operator=(const HeapProfilerController&) = delete;

  ~HeapProfilerController();

  // Starts periodic heap snapshot collection. Does nothing except record a
  // metric if heap profiling is disabled.
  // Returns true if heap profiling is enabled and was successfully started,
  // false otherwise.
  bool StartIfEnabled();

  // Uses the exact parameter values for the sampling interval and time between
  // samples, instead of a distribution around those values. This must be called
  // before Start.
  void SuppressRandomnessForTesting();

  // Sets a callback that will be invoked in tests after StartIfEnabled() is
  // called. The callback will be called immediately if profiling is disabled,
  // or when the first snapshot is scheduled if it's enabled. This lets tests
  // quit a RunLoop once the profiler has a chance to collect a snapshot.
  //
  // The callback parameter will be true if a snapshot is to be collected, false
  // otherwise. If the parameter is true, the test will need to wait for another
  // callback from CallStackProfileBuilder before the snapshot is actually
  // collected.
  void SetFirstSnapshotCallbackForTesting(
      base::OnceCallback<void(bool)> callback);

 private:
  using ProcessType = metrics::CallStackProfileParams::Process;
  using StoppedFlag = base::RefCountedData<base::AtomicFlag>;

  // Parameters to control the snapshot sampling and reporting. This is
  // move-only so that it can be safely passed between threads to the static
  // snapshot functions.
  struct SnapshotParams {
    SnapshotParams(base::TimeDelta mean_interval,
                   bool use_random_interval,
                   scoped_refptr<StoppedFlag> stopped,
                   ProcessType process_type,
                   base::TimeTicks profiler_creation_time,
                   base::OnceCallback<void(bool)> on_first_snapshot_callback);
    ~SnapshotParams();

    // Move-only.
    SnapshotParams(const SnapshotParams& other) = delete;
    SnapshotParams& operator=(const SnapshotParams& other) = delete;
    SnapshotParams(SnapshotParams&& other);
    SnapshotParams& operator=(SnapshotParams&& other);

    // Mean interval until the next snapshot.
    base::TimeDelta mean_interval;

    // If true, generate a random time centered around `mean_interval`.
    // Otherwise use `mean_interval` exactly.
    bool use_random_interval = false;

    // Atomic flag to signal that no more snapshots should be taken.
    scoped_refptr<StoppedFlag> stopped;

    // Process being sampled.
    ProcessType process_type = ProcessType::kUnknown;

    // Time the profiler was created.
    base::TimeTicks profiler_creation_time;

    // A callback to invoke for the first snapshot. Will be null for the
    // following snapshots.
    base::OnceCallback<void(bool)> on_first_snapshot_callback;
  };

  // Schedules the next call to TakeSnapshot.
  static void ScheduleNextSnapshot(SnapshotParams params);

  // Takes a heap snapshot unless the `params.stopped` flag is set.
  // `previous_interval` is the time since the previous snapshot, which is used
  // to log metrics about snapshot frequency.
  static void TakeSnapshot(SnapshotParams params,
                           base::TimeDelta previous_interval);

  // Processes the most recent snapshot and sends it to CallStackProfileBuilder.
  // Invokes `on_snapshot_callback` with true if a snapshot will be sent,
  // false otherwise.
  static void RetrieveAndSendSnapshot(
      ProcessType process_type,
      base::TimeDelta time_since_profiler_creation,
      base::OnceCallback<void(bool)> on_snapshot_callback);

  const ProcessType process_type_;

  // Stores the time the HeapProfilerController was created, which will be close
  // to the process creation time. This is used instead of
  // base::Process::CreationTime() to get a TimeTicks value which won't be
  // affected by clock skew.
  const base::TimeTicks creation_time_ = base::TimeTicks::Now();

  // This flag is set when the HeapProfilerController is torn down, to stop
  // profiling. It is the only member that should be referenced by the static
  // functions, to be sure that they can run on the thread pool while
  // HeapProfilerController is deleted on the main thread.
  scoped_refptr<StoppedFlag> stopped_;
  bool suppress_randomness_for_testing_ = false;

  // A callback to call before the first scheduled snapshot in tests.
  base::OnceCallback<void(bool)> on_first_snapshot_callback_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace heap_profiling

#endif  // COMPONENTS_HEAP_PROFILING_IN_PROCESS_HEAP_PROFILER_CONTROLLER_H_
