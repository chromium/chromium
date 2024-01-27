// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_REMOTE_COMMAND_JOB_H_
#define COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_REMOTE_COMMAND_JOB_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/policy/policy_export.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

// The enum that represents the RemoteCommandJob result.
enum class ResultType {
  kSuccess,
  kFailure,
  // The remote command job has finished executing with no result to send to
  // server. This is used for the commands that has delegated the execution to
  // other components with no callback.
  kAcked,
};

// This class manages the execution of a remote command job. It's a base
// class and actual implementations are expected to inherit from this class.
class POLICY_EXPORT RemoteCommandJob {
 public:
  using UniqueIDType = int64_t;

  // Status of this job.
  // This enum is used to define the buckets for an enumerated UMA histogram.
  // Hence,
  //   (a) existing enumerated constants should never be deleted or reordered
  //   (b) new constants should only be appended at the end of the enumeration
  //       (update RemoteCommandExecutuionStatus in
  //       tools/metrics/histograms/enums.xml as well).
  enum Status {
    NOT_INITIALIZED = 0,  // The job is not initialized yet.
    INVALID = 1,          // The job was initialized from a malformed protobuf.
    EXPIRED = 2,          // The job is expired and won't be executed.
    NOT_STARTED = 3,      // The job is initialized and ready to be started.
    RUNNING = 4,          // The job was started and is running now.
    SUCCEEDED = 5,        // The job finished running successfully.
    FAILED = 6,           // The job finished running with failure.
    TERMINATED = 7,       // The job was terminated before finishing by itself.
    ACKED = 8,  // The job finished running with no immediate result to send to
                // server.
    STATUS_TYPE_SIZE  // Used by UMA histograms. Shall be the last.
  };

  using FinishedCallback = base::OnceClosure;

  RemoteCommandJob(const RemoteCommandJob&) = delete;
  RemoteCommandJob& operator=(const RemoteCommandJob&) = delete;

  virtual ~RemoteCommandJob();

  // Initialize from a RemoteCommand protobuf definition, must be called before
  // calling Run(). Returns true if the initialization is successful.
  // |now| is the current time which will be used to estimate the command issued
  // time. It must be consistent to the same parameter passed to Run() below.
  // In order to minimize the error while estimating the command issued time,
  // this method must be called immediately after the command is received from
  // the server. |signed_command| contains the entire remote command and its
  // signature, the way it was received from the server.
  bool Init(base::TimeTicks now,
            const enterprise_management::RemoteCommand& command,
            const enterprise_management::SignedData& signed_command);

  // Run the command asynchronously. |now| is the time used for marking the
  // execution start. |now_ticks| is the time which will be used for command
  // expiration checking.
  // |finished_callback| will be called once the command finishes running,
  // regardless of whether the command is successful, fails or is terminated
  // prematurely.
  // Returns true if the task is posted and the command marked as running.
  // Returns false otherwise, for example if the command is invalid or expired.
  // Subclasses should implement RunImpl() for actual work.
  bool Run(base::Time now,
           base::TimeTicks now_ticks,
           FinishedCallback finished_callback);

  // Attempts to terminate the running tasks associated with this command. Does
  // nothing if the task is already terminated or finished. It's guaranteed that
  // after calling this method, |finished_callback_| will never be called.
  // Asynchronous tasks might still be running and subclasses that wish to
  // actually terminate the tasks should implement TerminateImpl().
  // This method is also intended to be used to handle timeout of command
  // execution.
  void Terminate();

  // Returns the remote command type that this class is able to handle.
  virtual enterprise_management::RemoteCommand_Type GetType() const = 0;

  // Returns the remote command timeout. If the command takes longer than the
  // returned time interval to execute, the command queue will kill it.
  virtual base::TimeDelta GetCommandTimeout() const;

  // Helpful accessors.
  UniqueIDType unique_id() const { return unique_id_; }
  base::TimeTicks issued_time() const { return issued_time_; }
  base::Time execution_started_time() const { return execution_started_time_; }
  Status status() const { return status_; }

  // Returns result of the command job. It'll be `RESUlT_IGNORED` until the
  // command has finished running.
  std::optional<enterprise_management::RemoteCommandResult::ResultType>
  GetResult() const;

  // Returns whether execution of this command is finished.
  bool IsExecutionFinished() const;

  // Generate the result payload which will be sent back to the server.
  // This method will only be called for successfully executed commands.
  std::unique_ptr<std::string> GetResultPayload() const;

 protected:
  // Callback invoked by the job's implementation to signal the remote command
  // has been executed. `result` will indicate that if command execution has
  // ended with success or failure. The passed-in string will be uploaded to the
  // server in the `payload` field of the `RemoteCommandResult` message.
  using CallbackWithResult =
      base::OnceCallback<void(ResultType result,
                              std::optional<std::string> payload)>;

  RemoteCommandJob();

  // The server can provide additional arguments for a command,
  // serialized into a |command_payload| string. This method will be
  // called with the payload sent by the server (or an empty string
  // if there is no payload). Subclasses that expect command
  // arguments should override this method and deserialize the
  // |command_payload| string. The default implementation ignores any
  // payload.
  virtual bool ParseCommandPayload(const std::string& command_payload);

  // Subclasses may use this method for customized command expiration
  // checking. |now| is the current time obtained from a clock. Implementations
  // are usually expected to compare |now| to the issued_time(), which is the
  // timestamp when the command was issued on the server.
  virtual bool IsExpired(base::TimeTicks now);

  // Subclasses should implement this method for actual command execution logic.
  // Implementations should execute commands asynchronously, possibly on a
  // background thread. Execution should end by invoking either
  // |result_callback| on the thread that this method was called with the
  // execution result. Also see comments regarding Run().
  virtual void RunImpl(CallbackWithResult result_callback) = 0;

  // Subclasses should implement this method for actual command execution
  // termination. Be cautious that tasks might be running on another thread or
  // even already be terminated. Implementations should expect destruction of
  // the class soon. Also see comments regarding Terminate().
  // The default implementation does nothing.
  virtual void TerminateImpl();

  const enterprise_management::SignedData& signed_command() const {
    return signed_command_;
  }

 private:
  // Posted tasks are expected to call this method.
  void OnCommandExecutionFinishedWithResult(
      ResultType result,
      std::optional<std::string> result_payload);

  Status status_;

  UniqueIDType unique_id_;
  // The estimated time when the command was issued.
  base::TimeTicks issued_time_;
  // The time when the command started running.
  base::Time execution_started_time_;

  // Serialized command inside policy data proto with signature.
  enterprise_management::SignedData signed_command_;

  std::optional<std::string> result_payload_;

  FinishedCallback finished_callback_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<RemoteCommandJob> weak_factory_{this};
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_REMOTE_COMMAND_JOB_H_
