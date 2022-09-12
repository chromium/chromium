// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SYSTEM_PROXY_FAKE_SYSTEM_PROXY_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SYSTEM_PROXY_FAKE_SYSTEM_PROXY_CLIENT_H_

#include "chromeos/ash/components/dbus/system_proxy/system_proxy_client.h"
#include "chromeos/ash/components/dbus/system_proxy/system_proxy_service.pb.h"
#include "dbus/object_proxy.h"

namespace ash {

class COMPONENT_EXPORT(CHROMEOS_DBUS) FakeSystemProxyClient
    : public SystemProxyClient,
      public SystemProxyClient::TestInterface {
 public:
  FakeSystemProxyClient();
  FakeSystemProxyClient(const FakeSystemProxyClient&) = delete;
  FakeSystemProxyClient& operator=(const FakeSystemProxyClient&) = delete;
  ~FakeSystemProxyClient() override;

  // SystemProxyClient implementation.
  void SetAuthenticationDetails(
      const system_proxy::SetAuthenticationDetailsRequest& request,
      SetAuthenticationDetailsCallback callback) override;
  void SetWorkerActiveSignalCallback(WorkerActiveCallback callback) override;
  void SetAuthenticationRequiredSignalCallback(
      AuthenticationRequiredCallback callback) override;
  void ClearUserCredentials(
      const system_proxy::ClearUserCredentialsRequest& request,
      ClearUserCredentialsCallback callback) override;
  void ShutDownProcess(const system_proxy::ShutDownRequest& request,
                       ShutDownProcessCallback callback) override;

  void ConnectToWorkerSignals() override;

  SystemProxyClient::TestInterface* GetTestInterface() override;

  // SystemProxyClient::TestInterface implementation.
  int GetSetAuthenticationDetailsCallCount() const override;
  int GetShutDownCallCount() const override;
  int GetClearUserCredentialsCount() const override;
  system_proxy::SetAuthenticationDetailsRequest
  GetLastAuthenticationDetailsRequest() const override;
  void SendAuthenticationRequiredSignal(
      const system_proxy::AuthenticationRequiredDetails& details) override;
  void SendWorkerActiveSignal(
      const system_proxy::WorkerActiveSignalDetails& details) override;

 private:
  system_proxy::SetAuthenticationDetailsRequest last_set_auth_details_request_;
  int set_credentials_call_count_ = 0;
  int shut_down_call_count_ = 0;
  int clear_user_credentials_call_count_ = 0;
  bool connect_to_worker_signals_called_ = false;
  // Signal callbacks.
  SystemProxyClient::WorkerActiveCallback worker_active_callback_;
  SystemProxyClient::AuthenticationRequiredCallback auth_required_callback_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SYSTEM_PROXY_FAKE_SYSTEM_PROXY_CLIENT_H_
