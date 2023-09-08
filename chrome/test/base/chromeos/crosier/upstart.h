// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROMEOS_CROSIER_UPSTART_H_
#define CHROME_TEST_BASE_CHROMEOS_CROSIER_UPSTART_H_

#include <string>
#include <vector>

#include "base/time/time.h"

// This file is a wrapper around the "initctl" command-line program that is used
// to control the system "upstart" daemon manager.

namespace upstart {

enum class Goal {
  kInvalid,  // Goal could not be queried or is unavailable.
  kStart,
  kStop
};

Goal GoalFromString(std::string_view s);
std::string_view GoalToString(Goal g);

enum class State {
  kInvalid,  // State could not be queried or is unavailable.
  kWaiting,
  kStarting,
  kSecurity,
  kTmpfiles,
  kPreStart,
  kSpawned,
  kPostStart,
  kRunning,
  kPreStop,
  kStopping,
  kKilled,
  kPostStop
};

State StateFromString(std::string_view s);
std::string_view StateFromString(State s);

// Dumps the current job list to stdout. Returns true on success.
bool DumpJobs();

struct JobStatus {
  bool is_valid = false;  // Set when the status was successfully queried.
  Goal goal = Goal::kInvalid;
  State state = State::kInvalid;
  int pid = 0;
};

// The extra_args is passed to the job as extra parameters, see StartJob().
JobStatus GetJobStatus(const std::string& job,
                       const std::vector<std::string>& extra_args = {});

// Returns true if the supplied job exists (i.e. it has a config file known by
// Upstart).
bool JobExists(const std::string& job);

// Starts job. If it is already running, this returns an error.
//
// The extra_args is passed to the job as extra parameters. For example,
// multiple-instance jobs can use it to specify an instance. The format of each
// extra_args string is "<key>=<value>" so:
//   StartJob("cryptohomed, {"LOG_LEVEL=-2"});
bool StartJob(const std::string& job,
              const std::vector<std::string>& extra_args = {});

// Restarts the job (single-instance) or the specified instance of the job
// (multiple-instance). If the job (instance) is currently stopped, it will be
// started. Note that the job is reloaded if it is already running; this differs
// from the "initctl restart" behavior as described in Section 10.1.2,
// "restart", in the Upstart Cookbook.
//
// The extra_args is passed to the job as extra parameters, see StartJob().
bool RestartJob(const std::string& job,
                const std::vector<std::string>& extra_args = {});

// StopJob stops job or the specified job instance. If it is not currently
// running, this is a no-op.
//
// The extra_args is passed to the job as extra parameters, see StartJob().
bool StopJob(const std::string& job,
             const std::vector<std::string>& extra_args = {});

// Starts the job if it isn't currently running. If it is already running, this
// is a no-op.
//
// The extra_args is passed to the job as extra parameters, see StartJob().
bool EnsureJobRunning(const std::string& job,
                      const std::vector<std::string>& extra_args = {});

enum class WrongGoalPolicy {
  // Indicates that it's acceptable for the job to initially have a goal that
  // doesn't match the requested one. WaitForJobStatus will continue waiting for
  // the requested goal.
  kTolerate,

  // RejectWrongGoal indicates that an error should be returned immediately if
  // the job doesn't have the requested goal.
  kReject
};

// Waits for job to have the status described by goal/state. The |policy|
// controls the function's behavior if the job's goal doesn't match the
// requested one. Returns true if the status was matched, false if it went to an
// invalid state or timed out.
//
// Automatically times out according to the process' TestTimeouts.
//
// The extra_args is passed to the job as extra parameters, see StartJob().
bool WaitForJobStatus(const std::string& job,
                      Goal goal,
                      State state,
                      WrongGoalPolicy policy,
                      const std::vector<std::string>& extra_args = {});

namespace internal {

// Parses the result of the "initctl status" for the expected job name. Exposed
// for testing.
JobStatus ParseStatus(std::string_view job_name, std::string_view s);

// Like WaitForJobStatus() but exposes the timeout for internal testing. Normal
// tests should always use WaitForJobStatus() which will use the default testing
// timeout.
bool WaitForJobStatusWithTimeout(
    const std::string& job,
    Goal goal,
    State state,
    WrongGoalPolicy policy,
    base::TimeDelta timeout,
    const std::vector<std::string>& extra_args = {});

}  // namespace internal

}  // namespace upstart

#endif  // CHROME_TEST_BASE_CHROMEOS_CROSIER_UPSTART_H_
