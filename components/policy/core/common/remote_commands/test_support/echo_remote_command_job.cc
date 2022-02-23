// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/remote_commands/test_support/echo_remote_command_job.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"

namespace policy {

const int kCommandExpirationTimeInHours = 3;

namespace em = enterprise_management;

const char EchoRemoteCommandJob::kMalformedCommandPayload[] =
    "_MALFORMED_COMMAND_PAYLOAD_";

class EchoRemoteCommandJob::EchoPayload
    : public RemoteCommandJob::ResultPayload {
 public:
  explicit EchoPayload(const std::string& payload) : payload_(payload) {}
  EchoPayload(const EchoPayload&) = delete;
  EchoPayload& operator=(const EchoPayload&) = delete;

  // RemoteCommandJob::ResultPayload:
  std::unique_ptr<std::string> Serialize() override;

 private:
  const std::string payload_;
};

std::unique_ptr<std::string> EchoRemoteCommandJob::EchoPayload::Serialize() {
  return std::make_unique<std::string>(payload_);
}

EchoRemoteCommandJob::EchoRemoteCommandJob(bool succeed,
                                           base::TimeDelta execution_duration)
    : succeed_(succeed), execution_duration_(execution_duration) {
  DCHECK_LT(base::Seconds(0), execution_duration_);
}

em::RemoteCommand_Type EchoRemoteCommandJob::GetType() const {
  return em::RemoteCommand_Type_COMMAND_ECHO_TEST;
}

bool EchoRemoteCommandJob::ParseCommandPayload(
    const std::string& command_payload) {
  if (command_payload == kMalformedCommandPayload)
    return false;
  command_payload_ = command_payload;
  return true;
}

bool EchoRemoteCommandJob::IsExpired(base::TimeTicks now) {
  return !issued_time().is_null() &&
         now > issued_time() + base::Hours(kCommandExpirationTimeInHours);
}

void EchoRemoteCommandJob::RunImpl(CallbackWithResult succeed_callback,
                                   CallbackWithResult failed_callback) {
  std::unique_ptr<ResultPayload> echo_payload(
      new EchoPayload(command_payload_));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          succeed_ ? std::move(succeed_callback) : std::move(failed_callback),
          std::move(echo_payload)),
      execution_duration_);
}

}  // namespace policy
