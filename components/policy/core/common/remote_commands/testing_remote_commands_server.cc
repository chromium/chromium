// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/remote_commands/testing_remote_commands_server.h"

#include <iterator>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/hash/sha1.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "crypto/signature_creator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

namespace {

std::string SignDataWithTestKey(const std::string& data) {
  std::unique_ptr<crypto::RSAPrivateKey> private_key =
      PolicyBuilder::CreateTestSigningKey();
  std::string sha1 = base::SHA1HashString(data);
  std::vector<uint8_t> digest(sha1.begin(), sha1.end());
  std::vector<uint8_t> result;
  CHECK(crypto::SignatureCreator::Sign(private_key.get(),
                                       crypto::SignatureCreator::SHA1,
                                       digest.data(), digest.size(), &result));
  return std::string(result.begin(), result.end());
}

}  // namespace

struct TestingRemoteCommandsServer::RemoteCommandWithCallback {
  RemoteCommandWithCallback(em::RemoteCommand command_proto,
                            base::Optional<em::SignedData> signed_command_proto,
                            base::TimeTicks issued_time,
                            ResultReportedCallback reported_callback)
      : command_proto(command_proto),
        signed_command_proto(signed_command_proto),
        issued_time(issued_time),
        reported_callback(std::move(reported_callback)) {}
  RemoteCommandWithCallback(RemoteCommandWithCallback&& other) = default;
  RemoteCommandWithCallback& operator=(RemoteCommandWithCallback&& other) =
      default;

  ~RemoteCommandWithCallback() {}

  em::RemoteCommand command_proto;
  base::Optional<em::SignedData> signed_command_proto;
  base::TimeTicks issued_time;
  ResultReportedCallback reported_callback;
};

TestingRemoteCommandsServer::TestingRemoteCommandsServer()
    : clock_(base::DefaultTickClock::GetInstance()),
      task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  weak_ptr_to_this_ = weak_factory_.GetWeakPtr();
}

TestingRemoteCommandsServer::~TestingRemoteCommandsServer() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Commands are removed from the queue when a result is reported. Only
  // commands for which no result was expected should remain in the queue.
  for (const auto& command_with_callback : commands_)
    EXPECT_TRUE(command_with_callback.reported_callback.is_null());
}

void TestingRemoteCommandsServer::IssueCommand(
    em::RemoteCommand_Type type,
    const std::string& payload,
    ResultReportedCallback reported_callback,
    bool skip_next_fetch) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);

  em::RemoteCommand command;
  command.set_type(type);
  command.set_command_id(++last_generated_unique_id_);
  if (!payload.empty())
    command.set_payload(payload);

  RemoteCommandWithCallback command_with_callback(
      command, base::nullopt, clock_->NowTicks(), std::move(reported_callback));
  if (skip_next_fetch)
    commands_issued_after_next_fetch_.push_back(
        std::move(command_with_callback));
  else
    commands_.push_back(std::move(command_with_callback));
}

void TestingRemoteCommandsServer::IssueSignedCommand(
    ResultReportedCallback reported_callback,
    em::RemoteCommand* command_in,
    em::PolicyData* policy_data_in,
    em::SignedData* signed_data_in) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);

  em::RemoteCommand command;
  em::PolicyData policy_data;
  em::SignedData signed_data;

  if (command_in) {
    command = *command_in;
  } else {
    command.set_target_device_id("acme-device");
    command.set_type(em::RemoteCommand_Type_COMMAND_ECHO_TEST);
    command.set_command_id(++last_generated_unique_id_);
  }

  if (policy_data_in) {
    policy_data = *policy_data_in;
  } else {
    policy_data.set_policy_type("google/chromeos/remotecommand");
    EXPECT_TRUE(command.SerializeToString(policy_data.mutable_policy_value()));
  }

  if (signed_data_in) {
    signed_data = *signed_data_in;
  } else {
    EXPECT_TRUE(policy_data.SerializeToString(signed_data.mutable_data()));
    signed_data.set_signature(SignDataWithTestKey(signed_data.data()));
  }

  RemoteCommandWithCallback command_with_callback(
      command, signed_data, clock_->NowTicks(), std::move(reported_callback));
  commands_.push_back(std::move(command_with_callback));
}

void TestingRemoteCommandsServer::FetchCommands(
    std::unique_ptr<RemoteCommandJob::UniqueIDType> last_command_id,
    const RemoteCommandResults& previous_job_results,
    std::vector<em::RemoteCommand>* fetched_commands,
    std::vector<em::SignedData>* signed_commands) {
  base::AutoLock auto_lock(lock_);

  for (const auto& job_result : previous_job_results) {
    EXPECT_TRUE(job_result.has_command_id());
    EXPECT_TRUE(job_result.has_result());

    bool found_command = false;
    ResultReportedCallback reported_callback;

    if (job_result.command_id() == -1) {
      // The result can have command_id equal to -1 in case a signed command was
      // rejected at the validation stage before it could be unpacked.
      CHECK(commands_.size() == 1);
      found_command = true;
      reported_callback = std::move(commands_[0].reported_callback);
      commands_.clear();
    }

    if (last_command_id) {
      // This relies on us generating commands with increasing IDs.
      EXPECT_GE(*last_command_id, job_result.command_id());
    }

    for (auto it = commands_.begin(); it != commands_.end(); ++it) {
      if (it->command_proto.command_id() == job_result.command_id()) {
        reported_callback = std::move(it->reported_callback);
        commands_.erase(it);
        found_command = true;
        break;
      }
    }

    // Verify that the command result is for an existing command actually
    // expecting a result.
    EXPECT_TRUE(found_command);

    if (!reported_callback.is_null()) {
      // Post task to the original thread which will report the result.
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&TestingRemoteCommandsServer::ReportJobResult,
                         weak_ptr_to_this_, std::move(reported_callback),
                         job_result));
    }
  }

  for (const auto& command_with_callback : commands_) {
    if (command_with_callback.signed_command_proto) {
      // Signed commands.
      signed_commands->push_back(
          command_with_callback.signed_command_proto.value());
    } else if (!last_command_id ||
               command_with_callback.command_proto.command_id() >
                   *last_command_id) {
      // Old style, unsigned commands.
      fetched_commands->push_back(command_with_callback.command_proto);
      // Simulate the age of commands calculation on the server side.
      fetched_commands->back().set_age_of_command(
          (clock_->NowTicks() - command_with_callback.issued_time)
              .InMilliseconds());
    }
  }

  // Push delayed commands into the main queue.
  commands_.insert(
      commands_.end(),
      std::make_move_iterator(commands_issued_after_next_fetch_.begin()),
      std::make_move_iterator(commands_issued_after_next_fetch_.end()));
  commands_issued_after_next_fetch_.clear();
}

void TestingRemoteCommandsServer::SetClock(const base::TickClock* clock) {
  DCHECK(thread_checker_.CalledOnValidThread());
  clock_ = clock;
}

size_t TestingRemoteCommandsServer::NumberOfCommandsPendingResult() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return commands_.size();
}

void TestingRemoteCommandsServer::ReportJobResult(
    ResultReportedCallback reported_callback,
    const em::RemoteCommandResult& job_result) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::move(reported_callback).Run(job_result);
}

}  // namespace policy
