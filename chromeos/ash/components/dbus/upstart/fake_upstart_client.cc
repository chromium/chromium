// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/upstart/fake_upstart_client.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/dbus/kerberos/fake_kerberos_client.h"
#include "chromeos/ash/components/dbus/kerberos/kerberos_client.h"
#include "chromeos/ash/components/dbus/media_analytics/fake_media_analytics_client.h"

namespace ash {

namespace {
// Used to track the fake instance, mirrors the instance in the base class.
FakeUpstartClient* g_instance = nullptr;
}  // namespace

FakeUpstartClient::FakeUpstartClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

FakeUpstartClient::~FakeUpstartClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
FakeUpstartClient* FakeUpstartClient::Get() {
  return g_instance;
}

FakeUpstartClient::StartJobResult::StartJobResult(
    bool success,
    std::optional<std::string> error_name,
    std::optional<std::string> error_message)
    : success(success),
      error_name(std::move(error_name)),
      error_message(std::move(error_message)) {}

FakeUpstartClient::StartJobResult::~StartJobResult() = default;

void FakeUpstartClient::StartJob(const std::string& job,
                                 const std::vector<std::string>& upstart_env,
                                 chromeos::VoidDBusMethodCallback callback) {
  if (is_recording_) {
    upstart_operations_.emplace_back(job, upstart_env,
                                     UpstartOperationType::START);
  }
  const bool result =
      start_job_cb_ ? start_job_cb_.Run(job, upstart_env).success : true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void FakeUpstartClient::StartJobWithTimeout(
    const std::string& job,
    const std::vector<std::string>& upstart_env,
    chromeos::VoidDBusMethodCallback callback,
    int timeout_ms) {
  StartJob(job, upstart_env, std::move(callback));
}

void FakeUpstartClient::StartJobWithErrorDetails(
    const std::string& job,
    const std::vector<std::string>& upstart_env,
    StartJobWithErrorDetailsCallback callback) {
  if (is_recording_) {
    upstart_operations_.emplace_back(job, upstart_env,
                                     UpstartOperationType::START);
  }
  const StartJobResult result = start_job_cb_
                                    ? start_job_cb_.Run(job, upstart_env)
                                    : StartJobResult(true /* success */);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result.success,
                                result.error_name, result.error_message));
}

void FakeUpstartClient::StopJob(const std::string& job,
                                const std::vector<std::string>& upstart_env,
                                chromeos::VoidDBusMethodCallback callback) {
  if (is_recording_) {
    upstart_operations_.emplace_back(job, upstart_env,
                                     UpstartOperationType::STOP);
  }
  const bool result = stop_job_cb_ ? stop_job_cb_.Run(job, upstart_env) : true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void FakeUpstartClient::StartMediaAnalytics(
    const std::vector<std::string>& /* upstart_env */,
    chromeos::VoidDBusMethodCallback callback) {
  DLOG_IF(WARNING, FakeMediaAnalyticsClient::Get()->process_running())
      << "Trying to start media analytics which is already started.";
  FakeMediaAnalyticsClient::Get()->set_process_running(true);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeUpstartClient::RestartMediaAnalytics(
    chromeos::VoidDBusMethodCallback callback) {
  FakeMediaAnalyticsClient::Get()->set_process_running(false);
  FakeMediaAnalyticsClient::Get()->set_process_running(true);
  FakeMediaAnalyticsClient::Get()->SetStateSuspended();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeUpstartClient::StopMediaAnalytics() {
  DLOG_IF(WARNING, !FakeMediaAnalyticsClient::Get()->process_running())
      << "Trying to stop media analytics which is not started.";
  FakeMediaAnalyticsClient::Get()->set_process_running(false);
}

void FakeUpstartClient::StopMediaAnalytics(
    chromeos::VoidDBusMethodCallback callback) {
  FakeMediaAnalyticsClient::Get()->set_process_running(false);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeUpstartClient::StartRecordingUpstartOperations() {
  if (is_recording_) {
    LOG(WARNING) << "Already recording Upstart operations";
  }
  is_recording_ = true;
}

std::vector<FakeUpstartClient::UpstartOperation>
FakeUpstartClient::GetRecordedUpstartOperationsForJob(const std::string& name) {
  std::vector<FakeUpstartClient::UpstartOperation> filtered_ops;
  base::ranges::copy_if(
      upstart_operations_, std::back_inserter(filtered_ops),
      [&name](const UpstartOperation& op) { return op.name == name; });
  return filtered_ops;
}

FakeUpstartClient::UpstartOperation::UpstartOperation(
    const std::string& name,
    const std::vector<std::string>& env,
    UpstartOperationType type)
    : name(name), env(env), type(type) {}

FakeUpstartClient::UpstartOperation::UpstartOperation(
    const FakeUpstartClient::UpstartOperation& other) = default;

FakeUpstartClient::UpstartOperation&
FakeUpstartClient::UpstartOperation::operator=(
    const FakeUpstartClient::UpstartOperation&) = default;

FakeUpstartClient::UpstartOperation::~UpstartOperation() = default;

}  // namespace ash
