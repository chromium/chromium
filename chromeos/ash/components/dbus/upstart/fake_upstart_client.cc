// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/upstart/fake_upstart_client.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/dbus/authpolicy/fake_authpolicy_client.h"
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

void FakeUpstartClient::StartJob(const std::string& job,
                                 const std::vector<std::string>& upstart_env,
                                 chromeos::VoidDBusMethodCallback callback) {
  const bool result =
      start_job_cb_ ? start_job_cb_.Run(job, upstart_env) : true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void FakeUpstartClient::StartJobWithErrorDetails(
    const std::string& job,
    const std::vector<std::string>& upstart_env,
    StartJobWithErrorDetailsCallback callback) {
  const bool result =
      start_job_cb_ ? start_job_cb_.Run(job, upstart_env) : true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result, absl::nullopt,
                                absl::nullopt));
}

void FakeUpstartClient::StopJob(const std::string& job,
                                const std::vector<std::string>& upstart_env,
                                chromeos::VoidDBusMethodCallback callback) {
  const bool result = stop_job_cb_ ? stop_job_cb_.Run(job, upstart_env) : true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void FakeUpstartClient::StartAuthPolicyService() {
  FakeAuthPolicyClient::Get()->SetStarted(true);
}

void FakeUpstartClient::RestartAuthPolicyService() {
  DLOG_IF(WARNING, !FakeAuthPolicyClient::Get()->started())
      << "Trying to restart authpolicyd which is not started";
  FakeAuthPolicyClient::Get()->SetStarted(true);
}

void FakeUpstartClient::StartLacrosChrome(
    const std::vector<std::string>& upstart_env) {}

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

void FakeUpstartClient::StartWilcoDtcService(
    chromeos::VoidDBusMethodCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeUpstartClient::StopWilcoDtcService(
    chromeos::VoidDBusMethodCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeUpstartClient::StartArcDataSnapshotd(
    const std::vector<std::string>& upstart_env,
    chromeos::VoidDBusMethodCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeUpstartClient::StopArcDataSnapshotd(
    chromeos::VoidDBusMethodCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

}  // namespace ash
