// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/remote_commands/test_support/echo_remote_command_job.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"

namespace policy {

const int kCommandExpirationTimeInHours = 3;

namespace em = enterprise_management;

const char EchoRemoteCommandJob::kMalformedCommandPayload[] =
    "_MALFORMED_COMMAND_PAYLOAD_";

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

void EchoRemoteCommandJob::RunImpl(CallbackWithResult result_callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(result_callback),
                     succeed_ ? ResultType::kSuccess : ResultType::kFailure,
                     command_payload_),
      execution_duration_);
}

}  // namespace policy
