// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEAP_PROFILING_IN_PROCESS_HEAP_PROFILER_CONTROLLER_H_
#define COMPONENTS_HEAP_PROFILING_IN_PROCESS_HEAP_PROFILER_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "components/sampling_profiler/process_type.h"
#include "components/version_info/channel.h"

namespace base {
class CommandLine;
}

namespace heap_profiling {

class BrowserProcessSnapshotController;
class ChildProcessSnapshotController;

// HeapProfilerController controls collection of sampled heap allocation
// snapshots for the current process.
class HeapProfilerController {
 public:
  // Returns the HeapProfilerController for this process or null if none exists.
  static HeapProfilerController* GetInstance();

  // Checks if heap profiling should be enabled for this process. If so, starts
  // sampling heap allocations immediately but does not schedule snapshots of
  // the samples until StartIfEnabled() is called. `channel` is used to
  // determine the probability that this client will be opted in to profiling.
  // `process_type` is the current process, which can be retrieved with
  // GetProfilerProcessType in base/profiler/process_type.h.
  HeapProfilerController(version_info::Channel channel,
                         sampling_profiler::ProfilerProcessType process_type);

  HeapProfilerController(const HeapProfilerController&) = delete;
  HeapProfilerController& operator=(const HeapProfilerController&) = delete;

  ~HeapProfilerController();

  // Returns true iff heap allocations are being sampled for this process. If
  // this returns true, snapshots can be scheduled by calling StartIfEnabled().
  bool IsEnabled() const { return profiling_enabled_; }

  // Starts periodic heap snapshot collection. Does nothing except record a
  // metric if heap profiling is disabled.
  // Returns true if heap profiling is enabled and was successfully started,
  // false otherwise.
  bool StartIfEnabled();

  // Get the synthetic field trial configuration. If a synthetic field trial
  // should be registered, returns true and writes the field trial details to
  // `trial_name` and `group_name`. Otherwise, returns false and does not modify
  // `trial_name` and `group_name`. Must only be called in the browser process.
  bool GetSyntheticFieldTrial(std::string& trial_name,
                              std::string& group_name) const;

  // Uses the exact parameter values for the sampling interval and time between
  // samples, instead of a distribution around those values. This must be called
  // before Start.
  void SuppressRandomnessForTesting();

  // Sets a callback that will be invoked in tests when the first snapshot is
  // scheduled after StartIfEnabled() is called. This lets tests quit a RunLoop
  // once the profiler has a chance to collect a snapshot.
  void SetFirstSnapshotCallbackForTesting(base::OnceClosure callback);

  // Appends a switch to enable or disable profiling for the given
  // `child_process_type` to `command_line`.
  void AppendCommandLineSwitchForChildProcess(
      base::CommandLine* command_line,
      sampling_profiler::ProfilerProcessType child_process_type,
      int child_process_id) const;

  // Returns the BrowserProcessSnapshotController or nullptr if none exists (if
  // heap profiling is disabled or kHeapProfilerCentralControl is disabled).
  BrowserProcessSnapshotController* GetBrowserProcessSnapshotController() const;

  // Triggers an immediate snapshot in a child process. In the browser process,
  // snapshots are scheduled internally by the HeapProfilerController.
  // `process_probability_pct` and `process_index` will be recorded in the
  // profile metadata so that samples from a single profile can be distinguished
  // and scaled to represent the full population.
  void TakeSnapshotInChildProcess(base::PassKey<ChildProcessSnapshotController>,
                                  uint32_t process_probability_pct,
                                  size_t process_index);

  // Allows unit tests to call AppendCommandLineSwitchForChildProcess without
  // creating a HeapProfilerController. `snapshot_controller` should be null if
  // profiling is disabled in the browser process.
  static void AppendCommandLineSwitchForTesting(
      base::CommandLine* command_line,
      sampling_profiler::ProfilerProcessType child_process_type,
      int child_process_id,
      BrowserProcessSnapshotController* snapshot_controller);

 private:
  using ProcessType = sampling_profiler::ProfilerProcessType;
  using StoppedFlag = base::RefCountedData<base::AtomicFlag>;

  // Parameters to control the snapshot sampling and reporting. This is
  // move-only so that it can be safely passed between threads to the static
  // snapshot functions.
  struct SnapshotParams {
    // Creates params for a repeating snapshot.
    SnapshotParams(std::optional<base::TimeDelta> mean_interval,
                   bool use_random_interval,
                   scoped_refptr<StoppedFlag> stopped,
                   ProcessType process_type,
                   base::TimeTicks profiler_creation_time,
                   base::OnceClosure on_first_snapshot_callback);

    // Creates params for a single snapshot.
    SnapshotParams(scoped_refptr<StoppedFlag> stopped,
                   ProcessType process_type,
                   base::TimeTicks profiler_creation_time,
                   uint32_t process_probability_pct,
                   size_t process_index,
                   base::OnceClosure on_first_snapshot_callback);

    ~SnapshotParams();

    // Move-only.
    SnapshotParams(const SnapshotParams& other) = delete;
    SnapshotParams& operator=(const SnapshotParams& other) = delete;
    SnapshotParams(SnapshotParams&& other);
    SnapshotParams& operator=(SnapshotParams&& other);

    // Mean interval until the next snapshot or nullopt if the params are used
    // only for a single snapshot.
    std::optional<base::TimeDelta> mean_interval;

    // If true, generate a random time centered around `mean_interval`.
    // Otherwise use `mean_interval` exactly.
    bool use_random_interval = false;

    // Atomic flag to signal that no more snapshots should be taken.
    scoped_refptr<StoppedFlag> stopped;

    // Process being sampled.
    ProcessType process_type = ProcessType::kUnknown;

    // Time the profiler was created.
    base::TimeTicks profiler_creation_time;

    // Metadata to record with the profile. The default values are correct for
    // the browser process and child processes with kHeapProfilerCentralControl
    // disabled, where one HeapProfiler always samples one process.
    uint32_t process_probability_pct = 100;
    size_t process_index = 0;

    // A callback to invoke for the first snapshot. Will be null for the
    // following snapshots. For testing.
    base::OnceClosure on_first_snapshot_callback;

    // A callback to trigger snapshots in all known child processes. Only used
    // in the browser process when kHeapProfilerCentralControl is enabled.
    base::RepeatingClosure trigger_child_process_snapshot_closure;
  };

  // Shared implementation of AppendCommandLineSwitchForChildProcess and
  // AppendCommandLineSwitchForTesting.
  static void AppendCommandLineSwitchInternal(
      base::CommandLine* command_line,
      ProcessType child_process_type,
      int child_process_id,
      BrowserProcessSnapshotController* snapshot_controller);

  // Schedules the next call to TakeSnapshot.
  static void ScheduleNextSnapshot(SnapshotParams params);

  // Takes a heap snapshot unless the `params.stopped` flag is set. If this is
  // the first call to TakeSnapshot() after StartIfEnabled(), invokes
  // `on_snapshot_callback` first.
  static void TakeSnapshot(SnapshotParams params);

  // Processes the most recent snapshot and sends it to CallStackProfileBuilder.
  static void RetrieveAndSendSnapshot(
      ProcessType process_type,
      base::TimeDelta time_since_profiler_creation,
      uint32_t process_probability_pct,
      size_t process_index);

  const ProcessType process_type_;
  bool profiling_enabled_;

  // Group name for the synthetic field trial, or nullopt for none.
  std::optional<std::string> synthetic_field_trial_group_;

  // Stores the time the HeapProfilerController was created, which will be close
  // to the process creation time. This is used instead of
  // base::Process::CreationTime() to get a TimeTicks value which won't be
  // affected by clock skew.
  const base::TimeTicks creation_time_ = base::TimeTicks::Now();

  SEQUENCE_CHECKER(sequence_checker_);

  // This flag is set when the HeapProfilerController is torn down, to stop
  // profiling. It is the only member that should be referenced by the static
  // functions, to be sure that they can run on the thread pool while
  // HeapProfilerController is deleted on the main thread.
  scoped_refptr<StoppedFlag> stopped_ GUARDED_BY_CONTEXT(sequence_checker_);

  // A task runner to trigger snapshots in the background.
  scoped_refptr<base::SequencedTaskRunner> snapshot_task_runner_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // A controller that notifies the HeapProfilerController in child processes to
  // take a snapshot at the same time as this HeapProfilerController. Created
  // only in the browser process when kHeapProfilerCentralControl is enabled.
  std::unique_ptr<BrowserProcessSnapshotController>
      browser_process_snapshot_controller_
          GUARDED_BY_CONTEXT(sequence_checker_);

  // If true, the sampling interval and time between samples won't have any
  // random variance added so that tests are repeatable.
  bool suppress_randomness_for_testing_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;

  // A callback to call before the first scheduled snapshot in tests.
  base::OnceClosure on_first_snapshot_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace heap_profiling

#endif  // COMPONENTS_HEAP_PROFILING_IN_PROCESS_HEAP_PROFILER_CONTROLLER_H_
