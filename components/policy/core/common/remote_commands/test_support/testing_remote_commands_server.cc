// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/remote_commands/test_support/testing_remote_commands_server.h"

#include <iterator>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/hash/sha1.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/signature_creator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

namespace {

int GetCommandIdOrDefault(const em::SignedData& signed_command) {
  em::PolicyData policy_data;
  if (!signed_command.has_data() ||
      !policy_data.ParseFromString(signed_command.data())) {
    return -1;
  }

  em::RemoteCommand command;
  if (!policy_data.has_policy_value() ||
      !command.ParseFromString(policy_data.policy_value())) {
    return -1;
  }

  return command.command_id();
}

}  // namespace

std::string SignDataWithTestKey(const std::string& data,
                                SignatureType algorithm) {
  crypto::SignatureCreator::HashAlgorithm crypto_hash_alg;

  switch (algorithm) {
    case em::PolicyFetchRequest::SHA256_RSA:
      crypto_hash_alg = crypto::SignatureCreator::SHA256;
      break;
    case em::PolicyFetchRequest::NONE:
    case em::PolicyFetchRequest::SHA1_RSA:
      crypto_hash_alg = crypto::SignatureCreator::SHA1;
      break;
  }

  std::unique_ptr<crypto::RSAPrivateKey> private_key =
      PolicyBuilder::CreateTestSigningKey();
  std::unique_ptr<crypto::SignatureCreator> signer =
      crypto::SignatureCreator::Create(private_key.get(), crypto_hash_alg);

  std::vector<uint8_t> input(data.begin(), data.end());
  std::vector<uint8_t> result;

  CHECK(signer->Update(input.data(), input.size()));
  CHECK(signer->Final(&result));

  return std::string(result.begin(), result.end());
}

struct TestingRemoteCommandsServer::RemoteCommandWithCallback {
  RemoteCommandWithCallback(const em::SignedData& command_proto,
                            base::TimeTicks issued_time,
                            ResultReportedCallback reported_callback)
      : command_id(GetCommandIdOrDefault(command_proto)),
        command_proto(command_proto),
        issued_time(issued_time),
        reported_callback(std::move(reported_callback)) {}

  RemoteCommandWithCallback(RemoteCommandWithCallback&& other) = default;
  RemoteCommandWithCallback& operator=(RemoteCommandWithCallback&& other) =
      default;

  ~RemoteCommandWithCallback() = default;

  int command_id;
  em::SignedData command_proto;
  base::TimeTicks issued_time;
  ResultReportedCallback reported_callback;
};

TestingRemoteCommandsServer::TestingRemoteCommandsServer()
    : clock_(base::DefaultTickClock::GetInstance()),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
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
    const em::SignedData& signed_data,
    ResultReportedCallback reported_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);

  commands_.emplace_back(signed_data, clock_->NowTicks(),
                         std::move(reported_callback));
}

void TestingRemoteCommandsServer::OnNextFetchCommandsCallReturnNothing() {
  return_nothing_for_next_fetch_ = true;
}

std::vector<em::SignedData> TestingRemoteCommandsServer::FetchCommands(
    std::unique_ptr<RemoteCommandJob::UniqueIDType> last_command_id,
    const RemoteCommandResults& previous_job_results) {
  base::AutoLock auto_lock(lock_);

  HandleRemoteCommandResults(previous_job_results);

  if (return_nothing_for_next_fetch_) {
    return_nothing_for_next_fetch_ = false;
    return {};
  }

  std::vector<em::SignedData> result;
  for (const auto& command_with_callback : commands_)
    result.push_back(command_with_callback.command_proto);

  return result;
}

void TestingRemoteCommandsServer::HandleRemoteCommandResults(
    const RemoteCommandResults& results) {
  for (const auto& job_result : results) {
    EXPECT_TRUE(job_result.has_command_id());
    EXPECT_TRUE(job_result.has_result());

    bool found_command = false;
    ResultReportedCallback reported_callback;

    if (job_result.command_id() == -1) {
      // The result can have command_id equal to -1 in case a signed command was
      // rejected at the validation stage before it could be unpacked.
      CHECK_EQ(commands_.size(), 1lu);
      found_command = true;
      reported_callback = std::move(commands_[0].reported_callback);
      commands_.clear();
    }

    for (auto it = commands_.begin(); it != commands_.end(); ++it) {
      if (it->command_id == job_result.command_id()) {
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
