// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/upstart/fake_upstart_client.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/authpolicy/fake_authpolicy_client.h"
#include "chromeos/dbus/kerberos/fake_kerberos_client.h"
#include "chromeos/dbus/kerberos/kerberos_client.h"
#include "chromeos/dbus/media_analytics/fake_media_analytics_client.h"

namespace chromeos {

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
                                 VoidDBusMethodCallback callback) {
  const bool result =
      start_job_cb_ ? start_job_cb_.Run(job, upstart_env) : true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void FakeUpstartClient::StopJob(const std::string& job,
                                const std::vector<std::string>& upstart_env,
                                VoidDBusMethodCallback callback) {
  const bool result = stop_job_cb_ ? stop_job_cb_.Run(job, upstart_env) : true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
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
    VoidDBusMethodCallback callback) {
  DLOG_IF(WARNING, FakeMediaAnalyticsClient::Get()->process_running())
      << "Trying to start media analytics which is already started.";
  FakeMediaAnalyticsClient::Get()->set_process_running(true);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeUpstartClient::RestartMediaAnalytics(VoidDBusMethodCallback callback) {
  FakeMediaAnalyticsClient::Get()->set_process_running(false);
  FakeMediaAnalyticsClient::Get()->set_process_running(true);
  FakeMediaAnalyticsClient::Get()->SetStateSuspended();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeUpstartClient::StopMediaAnalytics() {
  DLOG_IF(WARNING, !FakeMediaAnalyticsClient::Get()->process_running())
      << "Trying to stop media analytics which is not started.";
  FakeMediaAnalyticsClient::Get()->set_process_running(false);
}

void FakeUpstartClient::StopMediaAnalytics(VoidDBusMethodCallback callback) {
  FakeMediaAnalyticsClient::Get()->set_process_running(false);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeUpstartClient::StartWilcoDtcService(VoidDBusMethodCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeUpstartClient::StopWilcoDtcService(VoidDBusMethodCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeUpstartClient::StartArcDataSnapshotd(VoidDBusMethodCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeUpstartClient::StopArcDataSnapshotd(VoidDBusMethodCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

}  // namespace chromeos
