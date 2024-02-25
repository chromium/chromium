// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_REMOTE_COMMANDS_SERVICE_H_
#define COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_REMOTE_COMMANDS_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/policy_invalidation_scope.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/core/common/remote_commands/remote_commands_queue.h"
#include "components/policy/policy_export.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace base {
class Clock;
class TickClock;
}  // namespace base

namespace policy {

class CloudPolicyClient;
class CloudPolicyStore;
class RemoteCommandsFactory;

// Service class which will connect to a CloudPolicyClient in order to fetch
// remote commands from DMServer and send results for executed commands
// back to the server.
class POLICY_EXPORT RemoteCommandsService
    : public RemoteCommandsQueue::Observer {
 public:
  // Represents received remote command status to be recorded.
  // This enum is used to define the buckets for an enumerated UMA histogram.
  // Hence,
  //   (a) existing enumerated constants should never be deleted or reordered
  //   (b) new constants should only be appended at the end of the enumeration
  //       (update RemoteCommandReceivedStatus in
  //       tools/metrics/histograms/enums.xml as well).
  enum class MetricReceivedRemoteCommand {
    // Invalid remote commands.
    kInvalidSignature = 0,
    kInvalid = 1,
    kUnknownType = 2,
    kDuplicated = 3,
    kInvalidScope = 4,
    // Remote commands type.
    kCommandEchoTest = 5,
    kDeviceReboot = 6,
    kDeviceScreenshot = 7,
    kDeviceSetVolume = 8,
    kDeviceFetchStatus = 9,
    kUserArcCommand = 10,
    kDeviceWipeUsers = 11,
    kDeviceStartCrdSession = 12,
    kDeviceRemotePowerwash = 13,
    kDeviceRefreshEnterpriseMachineCertificate = 14,
    kDeviceGetAvailableDiagnosticRoutines = 15,
    kDeviceRunDiagnosticRoutine = 16,
    kDeviceGetDiagnosticRoutineUpdate = 17,
    kBrowserClearBrowsingData = 18,
    kDeviceResetEuicc = 19,
    kBrowserRotateAttestationCredential = 20,
    kFetchCrdAvailabilityInfo = 21,
    kFetchSupportPacket = 22,
    // Used by UMA histograms. Shall refer to the last enumeration.
    kMaxValue = kFetchSupportPacket
  };

  // Signature type that will be used for the requests.
  static constexpr enterprise_management::PolicyFetchRequest::SignatureType
  GetSignatureType() {
    return enterprise_management::PolicyFetchRequest::SHA256_RSA;
  }

  // Returns the metric name to report received commands.
  static const char* GetMetricNameReceivedRemoteCommand(
      PolicyInvalidationScope scope);
  // Returns the metric name to report status of executed commands.
  static std::string GetMetricNameExecutedRemoteCommand(
      PolicyInvalidationScope scope,
      enterprise_management::RemoteCommand_Type command_type);

  // Returns remote command fetch request type based on the invalidation scope.
  static std::string GetRequestType(PolicyInvalidationScope scope);

  RemoteCommandsService(std::unique_ptr<RemoteCommandsFactory> factory,
                        CloudPolicyClient* client,
                        CloudPolicyStore* store,
                        PolicyInvalidationScope scope);
  RemoteCommandsService(const RemoteCommandsService&) = delete;
  RemoteCommandsService& operator=(const RemoteCommandsService&) = delete;
  ~RemoteCommandsService() override;

  // Attempts to fetch remote commands, mainly supposed to be called by
  // invalidation service. Note that there will be at most one ongoing fetch
  // request and all other fetch request will be enqueued if another fetch
  // request is in-progress. And in such a case, another request will be made
  // immediately after the current ongoing request finishes.
  // Returns true if the new request was started immediately. Returns false if
  // another request was in progress already and the new request got enqueued.
  bool FetchRemoteCommands();

  // Returns whether a command fetch request is in progress or not.
  bool IsCommandFetchInProgressForTesting() const {
    return command_fetch_in_progress_;
  }

  // Set alternative clocks for testing.
  void SetClocksForTesting(const base::Clock* clock,
                           const base::TickClock* tick_clock);

  // Sets a callback that will be invoked the next time we receive a response
  // from the server.
  virtual void SetOnCommandAckedCallback(base::OnceClosure callback);

 private:
  // Helper functions to enqueue a command which we get from server.
  // |VerifyAndEnqueueSignedCommand| is used for the case of secure remote
  // commands; it verifies the command, decodes it, and passes it onto
  // |EnqueueCommand|.  The latter one does some additional checks and then
  // creates the correct job for the particular remote command (it also takes
  // the original |signed_command| so it can pass it to the job for caching in
  // case the particular job needs to do additional signature verification).
  void VerifyAndEnqueueSignedCommand(
      const enterprise_management::SignedData& signed_command);
  void EnqueueCommand(const enterprise_management::RemoteCommand& command,
                      const enterprise_management::SignedData& signed_command);

  // RemoteCommandsQueue::Observer:
  void OnJobStarted(RemoteCommandJob* command) override;
  void OnJobFinished(RemoteCommandJob* command) override;

  // Callback to handle commands we get from the server.
  void OnRemoteCommandsFetched(
      DeviceManagementStatus status,
      const std::vector<enterprise_management::SignedData>& signed_commands);

  // Records UMA metric of received remote command.
  void RecordReceivedRemoteCommand(MetricReceivedRemoteCommand metric) const;
  // Records UMA metric of executed remote command.
  void RecordExecutedRemoteCommand(const RemoteCommandJob& command) const;

  // Whether there is a command fetch on going or not.
  bool command_fetch_in_progress_ = false;

  // Whether there is an enqueued fetch request, which indicates there were
  // additional FetchRemoteCommands() calls while a fetch request was ongoing.
  bool has_enqueued_fetch_request_ = false;

  // Command results that have not been sent back to the server yet.
  std::vector<enterprise_management::RemoteCommandResult> unsent_results_;

  // Whether at least one command has finished executing.
  bool has_finished_command_ = false;

  // ID of the latest command which has finished execution if
  // |has_finished_command_| is true. We will acknowledge this ID to the
  // server so that we can re-fetch commands that have not been executed yet
  // after a crash or browser restart.
  RemoteCommandJob::UniqueIDType lastest_finished_command_id_;

  // Collects the IDs of all fetched commands. We need this since the command ID
  // is opaque.
  // IDs will be stored in the order that they are fetched from the server,
  // and acknowledging a command will discard its ID from
  // |fetched_command_ids_|, as well as the IDs of every command before it.
  base::circular_deque<RemoteCommandJob::UniqueIDType> fetched_command_ids_;

  RemoteCommandsQueue queue_;
  std::unique_ptr<RemoteCommandsFactory> factory_;
  const raw_ptr<CloudPolicyClient> client_;
  const raw_ptr<CloudPolicyStore> store_;

  // Callback which gets called after the last command got ACK'd to the server
  // as executed.
  base::OnceClosure on_command_acked_callback_;

  // Represents remote commands scope covered by service.
  const PolicyInvalidationScope scope_;

  base::ScopedObservation<RemoteCommandsQueue, RemoteCommandsQueue::Observer>
      remote_commands_queue_observation{this};

  base::WeakPtrFactory<RemoteCommandsService> weak_factory_{this};
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_REMOTE_COMMANDS_SERVICE_H_
