// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/upstart.h"

#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/test/base/chromeos/crosier/helper/test_sudo_helper_client.h"

namespace upstart {

namespace {

struct GoalMapping {
  const char* str = nullptr;
  Goal goal = Goal::kInvalid;
};
const GoalMapping kAllGoals[] = {{"start", Goal::kStart},
                                 {"stop", Goal::kStop}};

struct StateMapping {
  const char* str = nullptr;
  State state = State::kInvalid;
};
const StateMapping kAllStates[] = {
    {"waiting", State::kWaiting},      {"starting", State::kStarting},
    {"security", State::kSecurity},    {"tmpfiles", State::kTmpfiles},
    {"pre-start", State::kPreStart},   {"spawned", State::kSpawned},
    {"post-start", State::kPostStart}, {"running", State::kRunning},
    {"pre-stop", State::kPreStop},     {"stopping", State::kStopping},
    {"killed", State::kKilled},        {"post-stop", State::kPostStop}};

// Special-cased in StopJob due to its respawning behavior.
const char kUiJob[] = "ui";

// Runs the given command vector as sudo using the test_sudo_helper that should
// be set up on the system.
//
// test_sudo_helper runs the string through the shell so it should be escaped
// accordingly. Currently we have no need for escaping so just assert for some
// simple cases to catch if somebody starts doing this. If things like spaces
// and quotes are needed in the future, we should update the test sudo helper to
// take an array rather than trying to escape here.
TestSudoHelperClient::Result RunSudoHelper(
    const std::vector<std::string>& args) {
  std::string cmd;
  for (const std::string& cur : args) {
    // The string should not need escaping (see above).
    CHECK(cur.find_first_of(" \\*\"?$&();<>[]^`") == std::string::npos);
    if (!cmd.empty()) {
      cmd.push_back(' ');
    }
    cmd.append(cur);
  }

  return TestSudoHelperClient::ConnectAndRunCommand(cmd);
}

}  // namespace

namespace internal {

// Examples:
//    "temp_logger stop/waiting"
//    "log-rotate start/running, process 1779"
//    "ml-service (mojo_service) start/running, process 6820"
//    "ui start/tmpfiles, (tmpfiles) process 19419"
JobStatus ParseStatus(std::string_view job_name, std::string_view s) {
  s = base::TrimWhitespaceASCII(s, base::TRIM_ALL);

  // Expect a prefix of the job name, trim it.
  if (!base::StartsWith(s, job_name)) {
    return JobStatus();
  }
  s = s.substr(job_name.size());

  // Spaces.
  s = base::TrimString(s, " ", base::TRIM_LEADING);

  // Optional instance status in parens.
  if (s.empty()) {
    return JobStatus();
  }
  if (s[0] == '(') {
    // Eat until closing.
    if (auto close = s.find(')'); close != std::string_view::npos) {
      s = base::TrimString(s.substr(close + 1), " ", base::TRIM_LEADING);
    } else {
      return JobStatus();  // No closing paren.
    }
  }

  // Spaces.
  s = base::TrimString(s, " ", base::TRIM_LEADING);

  // Goal string until '/'.
  Goal goal = Goal::kInvalid;
  if (auto slash = s.find('/'); slash != std::string_view::npos) {
    goal = GoalFromString(s.substr(0, slash));
    if (goal == Goal::kInvalid) {
      return JobStatus();
    }
    s = s.substr(slash + 1);  // Eat the goal plus the slash.
  } else {
    return JobStatus();  // No slash.
  }

  // State until comma or end of string.
  State state = State::kInvalid;
  if (auto comma = s.find(','); comma != std::string_view::npos) {
    state = StateFromString(s.substr(0, comma));
    s = s.substr(comma + 1);  // Eat the status and comma.
  } else {
    // No status terminator, everything to end-of-string is the status.
    state = StateFromString(s);
  }
  if (state == State::kInvalid) {
    return JobStatus();
  }

  // Spaces.
  s = base::TrimString(s, " ", base::TRIM_LEADING);

  // Optional "(tmpfiles)" annotation.
  std::string_view tmpfiles_annotation("(tmpfiles)");
  if (base::StartsWith(s, tmpfiles_annotation)) {
    s = s.substr(tmpfiles_annotation.size());
    s = base::TrimString(s, " ", base::TRIM_LEADING);
  }

  // Optional "process" annotation with the PID.
  std::string_view process_annotation("process");
  int pid = 0;
  if (base::StartsWith(s, process_annotation)) {
    s = s.substr(process_annotation.size());
    s = base::TrimString(s, " ", base::TRIM_LEADING);

    // Everything to the end or whitespace (could have more lines of other
    // instances following) is the PID.
    std::string_view pid_str;
    if (auto pid_end = s.find_first_of(base::kWhitespaceASCII);
        pid_end != std::string_view::npos) {
      pid_str = s.substr(0, pid_end);
    } else {
      pid_str = s;
    }
    if (!base::StringToInt(pid_str, &pid)) {
      return JobStatus();
    }
  }

  return JobStatus{true, goal, state, pid};
}

bool WaitForJobStatusWithTimeout(const std::string& job,
                                 Goal goal,
                                 State state,
                                 WrongGoalPolicy policy,
                                 base::TimeDelta timeout,
                                 const std::vector<std::string>& extra_args) {
  base::TimeTicks deadline = base::TimeTicks::Now() + timeout;

  while (true) {
    JobStatus status = GetJobStatus(job, extra_args);
    if (!status.is_valid) {
      return false;  // Failure checking.
    }
    if (status.goal == goal && status.state == state) {
      return true;  // Done.
    }

    if (status.goal != goal && policy == WrongGoalPolicy::kReject) {
      return false;  // Wrong goal.
    }

    if (base::TimeTicks::Now() > deadline) {
      return false;  // Timeout.
    }

    // Uses 50ms from base/test/spin_wait.h (which we can't use because of our
    // extra error handling).
    base::PlatformThread::Sleep(base::Milliseconds(50));
  }
}

}  // namespace internal

Goal GoalFromString(std::string_view s) {
  for (const GoalMapping& map : kAllGoals) {
    if (map.str == s) {
      return map.goal;
    }
  }
  return Goal::kInvalid;
}

std::string_view GoalToString(Goal g) {
  for (const GoalMapping& map : kAllGoals) {
    if (map.goal == g) {
      return map.str;
    }
  }
  return "<invalid>";
}

State StateFromString(std::string_view s) {
  for (const StateMapping& map : kAllStates) {
    if (map.str == s) {
      return map.state;
    }
  }
  return State::kInvalid;
}

std::string_view StateFromString(State s) {
  for (const StateMapping& map : kAllStates) {
    if (map.state == s) {
      return map.str;
    }
  }
  return "<invalid>";
}

bool DumpJobs() {
  std::vector<std::string> args{"initctl", "list"};
  TestSudoHelperClient::Result result = RunSudoHelper(args);
  if (result.return_code != 0) {
    return false;
  }

  printf("%s", result.output.c_str());
  return true;
}

JobStatus GetJobStatus(const std::string& job,
                       const std::vector<std::string>& extra_args) {
  // This does not need to use the sudo helper because it's read-only.
  std::vector<std::string> args{"initctl", "status", job};
  args.insert(args.end(), extra_args.begin(), extra_args.end());

  TestSudoHelperClient::Result result = RunSudoHelper(args);

  // Tolerates an error if stderr starts with "initctl: Unknown instance". This
  // happens when the job is multiple-instance and the specific instance is
  // treated as stop/waiting.
  if (base::StartsWith(result.output, "initctl: Unknown instance")) {
    return JobStatus{true, Goal::kStop, State::kWaiting, 0};
  }

  // Any other error is a failure.
  if (result.return_code != 0) {
    LOG(ERROR) << "Running initctl failed: " << result.output;
    return JobStatus();
  }

  return internal::ParseStatus(job, result.output);
}

bool JobExists(const std::string& job) {
  return GetJobStatus(job).is_valid;
}

bool StartJob(const std::string& job,
              const std::vector<std::string>& extra_args) {
  std::vector<std::string> args{"initctl", "start", job};
  args.insert(args.end(), extra_args.begin(), extra_args.end());
  return RunSudoHelper(args).return_code == 0;
}

bool RestartJob(const std::string& job,
                const std::vector<std::string>& extra_args) {
  if (!StopJob(job, extra_args)) {
    return false;
  }
  return StartJob(job, extra_args);
}

bool StopJob(const std::string& job,
             const std::vector<std::string>& extra_args) {
  if (job == kUiJob) {
    // To Go version of this function does some special-casing for the ui job.
    // The ui job needs special handling because it is restarted out-of-band by
    // the ui-respawn job when session_manager exits. To work around this, when
    // job is "ui", we would first need to waits for the job to reach a stable
    // state. See https://crbug.com/891594.
    //
    // If this is needed, the necessary logic should be added here.
    CHECK(false) << "StopJob unimplemented for the 'ui' job.";
    return false;
  }

  std::vector<std::string> args{"initctl", "stop", job};
  args.insert(args.end(), extra_args.begin(), extra_args.end());

  // Issue the stop request, ignoring errors from initctl (these will be thrown
  // if the job is already stopped).
  RunSudoHelper(args);

  // Check the actual status.
  return WaitForJobStatus(job, Goal::kStop, State::kWaiting,
                          WrongGoalPolicy::kReject, extra_args);
}

bool WaitForJobStatus(const std::string& job,
                      Goal goal,
                      State state,
                      WrongGoalPolicy policy,
                      const std::vector<std::string>& extra_args) {
  return internal::WaitForJobStatusWithTimeout(
      job, goal, state, policy, TestTimeouts::action_timeout(), extra_args);
}

bool EnsureJobRunning(const std::string& job,
                      const std::vector<std::string>& extra_args) {
  // If the job already has a "start" goal, wait for it to enter the "running"
  // state. This will return nil immediately if it's already start/running, and
  // will return an error immediately if the job has a "stop" goal.
  if (WaitForJobStatus(job, Goal::kStart, State::kRunning,
                       WrongGoalPolicy::kReject, extra_args)) {
    return true;
  }

  // Otherwise it needs starting.
  return StartJob(job, extra_args);
}

}  // namespace upstart
