// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/system_proxy/fake_system_proxy_client.h"

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/dbus/system_proxy/system_proxy_service.pb.h"

namespace ash {

FakeSystemProxyClient::FakeSystemProxyClient() = default;

FakeSystemProxyClient::~FakeSystemProxyClient() = default;

void FakeSystemProxyClient::SetAuthenticationDetails(
    const system_proxy::SetAuthenticationDetailsRequest& request,
    SetAuthenticationDetailsCallback callback) {
  ++set_credentials_call_count_;
  last_set_auth_details_request_ = request;
  system_proxy::SetAuthenticationDetailsResponse response;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), response));
}

void FakeSystemProxyClient::ClearUserCredentials(
    const system_proxy::ClearUserCredentialsRequest& request,
    ClearUserCredentialsCallback callback) {
  ++clear_user_credentials_call_count_;
  system_proxy::ClearUserCredentialsResponse response;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), response));
}

void FakeSystemProxyClient::ShutDownProcess(
    const system_proxy::ShutDownRequest& request,
    ShutDownProcessCallback callback) {
  ++shut_down_call_count_;
  system_proxy::ShutDownResponse response;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), response));
}

void FakeSystemProxyClient::SetWorkerActiveSignalCallback(
    WorkerActiveCallback callback) {
  worker_active_callback_ = callback;
}
void FakeSystemProxyClient::SetAuthenticationRequiredSignalCallback(
    AuthenticationRequiredCallback callback) {
  auth_required_callback_ = callback;
}

void FakeSystemProxyClient::ConnectToWorkerSignals() {
  connect_to_worker_signals_called_ = true;
}

SystemProxyClient::TestInterface* FakeSystemProxyClient::GetTestInterface() {
  return this;
}

int FakeSystemProxyClient::GetSetAuthenticationDetailsCallCount() const {
  return set_credentials_call_count_;
}

int FakeSystemProxyClient::GetShutDownCallCount() const {
  return shut_down_call_count_;
}

int FakeSystemProxyClient::GetClearUserCredentialsCount() const {
  return clear_user_credentials_call_count_;
}

system_proxy::SetAuthenticationDetailsRequest
FakeSystemProxyClient::GetLastAuthenticationDetailsRequest() const {
  return last_set_auth_details_request_;
}

void FakeSystemProxyClient::SendAuthenticationRequiredSignal(
    const system_proxy::AuthenticationRequiredDetails& details) {
  if (!connect_to_worker_signals_called_) {
    return;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(auth_required_callback_, details));
}

void FakeSystemProxyClient::SendWorkerActiveSignal(
    const system_proxy::WorkerActiveSignalDetails& details) {
  DCHECK(worker_active_callback_);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(worker_active_callback_, details));
}

}  // namespace ash
