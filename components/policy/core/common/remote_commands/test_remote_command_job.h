// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_TEST_REMOTE_COMMAND_JOB_H_
#define COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_TEST_REMOTE_COMMAND_JOB_H_

#include <string>

#include "base/time/time.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

class TestRemoteCommandJob : public RemoteCommandJob {
 public:
  TestRemoteCommandJob(bool succeed, base::TimeDelta execution_duration);
  TestRemoteCommandJob(const TestRemoteCommandJob&) = delete;
  TestRemoteCommandJob& operator=(const TestRemoteCommandJob&) = delete;

  // RemoteCommandJob:
  enterprise_management::RemoteCommand_Type GetType() const override;

  static const char kMalformedCommandPayload[];

 private:
  class EchoPayload;

  // RemoteCommandJob:
  bool ParseCommandPayload(const std::string& command_payload) override;
  bool IsExpired(base::TimeTicks now) override;
  void RunImpl(CallbackWithResult succeed_callback,
               CallbackWithResult failed_callback) override;

  std::string command_payload_;

  const bool succeed_;
  const base::TimeDelta execution_duration_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_TEST_REMOTE_COMMAND_JOB_H_
