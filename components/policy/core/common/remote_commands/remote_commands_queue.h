// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_REMOTE_COMMANDS_QUEUE_H_
#define COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_REMOTE_COMMANDS_QUEUE_H_

#include <memory>

#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "components/policy/policy_export.h"

namespace base {
class Clock;
class TickClock;
}  // namespace base

namespace policy {

class RemoteCommandJob;

// This class manages the execution of multiple instances of RemoteCommandJob.
// It runs all commands one by one in order from the same thread, i.e. there
// will be at most one running command at any given time.
// Although the queue lives on a single thread and manages the command execution
// from the same thread, the actual command processing happens asynchronously in
// the background.
class POLICY_EXPORT RemoteCommandsQueue {
 public:
  // Interface for classes who would like to monitor remote command execution.
  class POLICY_EXPORT Observer {
   public:
    // Called when a remote command starts running.
    virtual void OnJobStarted(RemoteCommandJob* command) = 0;

    // Called when a remote command finishes execution, fails or is being
    // terminated. After this method has run, |command| is destroyed and must
    // not be accessed anymore.
    virtual void OnJobFinished(RemoteCommandJob* command) = 0;

   protected:
    virtual ~Observer() = default;
  };

  RemoteCommandsQueue();
  RemoteCommandsQueue(const RemoteCommandsQueue&) = delete;
  RemoteCommandsQueue& operator=(const RemoteCommandsQueue&) = delete;
  ~RemoteCommandsQueue();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Add a |job| to the queue.
  void AddJob(std::unique_ptr<RemoteCommandJob> job);

  // Set alternative clocks for testing.
  void SetClocksForTesting(const base::Clock* clock,
                           const base::TickClock* tick_clock);

  // Helper function to get the current time.
  base::TimeTicks GetNowTicks();

 private:
  // Callback function for the timer, used to terminate the running command
  // after certain time.
  void OnCommandTimeout();

  // Called whenever the command currently scheduled to run finishes, regardless
  // of reasons.
  void CurrentJobFinished();

  // Attempts to start a new command.
  void ScheduleNextJob();

  base::queue<std::unique_ptr<RemoteCommandJob>> incoming_commands_;

  std::unique_ptr<RemoteCommandJob> running_command_;

  raw_ptr<const base::Clock> clock_;
  raw_ptr<const base::TickClock> tick_clock_;
  base::OneShotTimer execution_timeout_timer_;

  base::ObserverList<Observer, true>::Unchecked observer_list_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_REMOTE_COMMANDS_QUEUE_H_
