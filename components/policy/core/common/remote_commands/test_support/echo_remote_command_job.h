// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_TEST_SUPPORT_ECHO_REMOTE_COMMAND_JOB_H_
#define COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_TEST_SUPPORT_ECHO_REMOTE_COMMAND_JOB_H_

#include <string>

#include "base/time/time.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

// Remote Command Job that simply echos its payload.
// Used during unittest.
class EchoRemoteCommandJob : public RemoteCommandJob {
 public:
  EchoRemoteCommandJob(bool succeed, base::TimeDelta execution_duration);
  EchoRemoteCommandJob(const EchoRemoteCommandJob&) = delete;
  EchoRemoteCommandJob& operator=(const EchoRemoteCommandJob&) = delete;

  // RemoteCommandJob:
  enterprise_management::RemoteCommand_Type GetType() const override;

  static const char kMalformedCommandPayload[];

 private:
  // RemoteCommandJob:
  bool ParseCommandPayload(const std::string& command_payload) override;
  bool IsExpired(base::TimeTicks now) override;
  void RunImpl(CallbackWithResult result_callback) override;

  std::string command_payload_;

  const bool succeed_;
  const base::TimeDelta execution_duration_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_TEST_SUPPORT_ECHO_REMOTE_COMMAND_JOB_H_
