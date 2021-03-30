// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/u2f/u2f_client.h"

#include <utility>

#include <google/protobuf/message_lite.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/u2f/fake_u2f_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/u2f/dbus-constants.h"

namespace chromeos {
namespace {

U2FClient* g_instance = nullptr;

// UMA histogram names.
constexpr char kMakeCredentialStatusHistogram[] =
    "WebAuthentication.ChromeOS.MakeCredentialStatus";
constexpr char kGetAssertionStatusHistogram[] =
    "WebAuthentication.ChromeOS.GetAssertionStatus";

// Some methods trigger an OS modal dialog that needs to be completed or
// dismissed before the method returns. These methods are cancelled explicitly
// once original WebAuthn request times out. Hence, they are invoked with
// infinite DBus timeouts.
const int kU2FInfiniteTimeout = dbus::ObjectProxy::TIMEOUT_INFINITE;

// Timeout for all other methods.
constexpr int kU2FShortTimeout = 3000;

class U2FClientImpl : public U2FClient {
 public:
  U2FClientImpl() = default;
  ~U2FClientImpl() override = default;
  U2FClientImpl(const U2FClientImpl&) = delete;
  U2FClientImpl& operator=(const U2FClientImpl&) = delete;

  void Init(dbus::Bus* bus) {
    proxy_ = bus->GetObjectProxy(u2f::kU2FServiceName,
                                 dbus::ObjectPath(u2f::kU2FServicePath));
  }

  template <typename ResponseProto>
  void HandleResponse(DBusMethodCallback<ResponseProto> callback,
                      dbus::Response* response);

  // U2FClient:
  void WaitForServiceToBeAvailable(
      WaitForServiceToBeAvailableCallback callback) override;
  void IsUvpaa(const u2f::IsUvpaaRequest& request,
               DBusMethodCallback<u2f::IsUvpaaResponse> callback) override;
  void IsU2FEnabled(const u2f::IsUvpaaRequest& request,
                    DBusMethodCallback<u2f::IsUvpaaResponse> callback) override;
  void MakeCredential(
      const u2f::MakeCredentialRequest& request,
      DBusMethodCallback<u2f::MakeCredentialResponse> callback) override;
  void GetAssertion(
      const u2f::GetAssertionRequest& request,
      DBusMethodCallback<u2f::GetAssertionResponse> callback) override;
  void HasCredentials(
      const u2f::HasCredentialsRequest& request,
      DBusMethodCallback<u2f::HasCredentialsResponse> callback) override;
  void HasLegacyU2FCredentials(
      const u2f::HasCredentialsRequest& request,
      DBusMethodCallback<u2f::HasCredentialsResponse> callback) override;
  void CancelWebAuthnFlow(
      const u2f::CancelWebAuthnFlowRequest& request,
      DBusMethodCallback<u2f::CancelWebAuthnFlowResponse> callback) override;

 private:
  dbus::ObjectProxy* proxy_ = nullptr;

  base::WeakPtrFactory<U2FClientImpl> weak_factory_{this};
};

template <typename ResponseProto>
void U2FClientImpl::HandleResponse(DBusMethodCallback<ResponseProto> callback,
                                   dbus::Response* response) {
  if (!response) {
    std::move(callback).Run(base::nullopt);
    return;
  }
  dbus::MessageReader reader(response);
  ResponseProto response_proto;
  if (!reader.PopArrayOfBytesAsProto(&response_proto)) {
    std::move(callback).Run(base::nullopt);
    return;
  }
  std::move(callback).Run(std::move(response_proto));
}

void U2FClientImpl::WaitForServiceToBeAvailable(
    WaitForServiceToBeAvailableCallback callback) {
  proxy_->WaitForServiceToBeAvailable(std::move(callback));
}

void U2FClientImpl::IsUvpaa(const u2f::IsUvpaaRequest& request,
                            DBusMethodCallback<u2f::IsUvpaaResponse> callback) {
  dbus::MethodCall method_call(u2f::kU2FInterface, u2f::kU2FIsUvpaa);
  dbus::MessageWriter writer(&method_call);
  writer.AppendProtoAsArrayOfBytes(request);
  proxy_->CallMethod(
      &method_call, kU2FShortTimeout,
      base::BindOnce(&U2FClientImpl::HandleResponse<u2f::IsUvpaaResponse>,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void U2FClientImpl::IsU2FEnabled(
    const u2f::IsUvpaaRequest& request,
    DBusMethodCallback<u2f::IsUvpaaResponse> callback) {
  dbus::MethodCall method_call(u2f::kU2FInterface, u2f::kU2FIsU2fEnabled);
  dbus::MessageWriter writer(&method_call);
  writer.AppendProtoAsArrayOfBytes(request);
  proxy_->CallMethod(
      &method_call, kU2FShortTimeout,
      base::BindOnce(&U2FClientImpl::HandleResponse<u2f::IsUvpaaResponse>,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void U2FClientImpl::MakeCredential(
    const u2f::MakeCredentialRequest& request,
    DBusMethodCallback<u2f::MakeCredentialResponse> callback) {
  dbus::MethodCall method_call(u2f::kU2FInterface, u2f::kU2FMakeCredential);
  dbus::MessageWriter writer(&method_call);
  writer.AppendProtoAsArrayOfBytes(request);
  auto uma_callback_wrapper = base::BindOnce(
      [](DBusMethodCallback<u2f::MakeCredentialResponse> callback,
         base::Optional<u2f::MakeCredentialResponse> response) {
        if (response) {
          base::UmaHistogramEnumeration(
              kMakeCredentialStatusHistogram, response->status(),
              static_cast<u2f::MakeCredentialResponse::MakeCredentialStatus>(
                  u2f::MakeCredentialResponse::MakeCredentialStatus_ARRAYSIZE));
        }
        std::move(callback).Run(response);
      },
      std::move(callback));
  proxy_->CallMethod(
      &method_call, kU2FInfiniteTimeout,
      base::BindOnce(
          &U2FClientImpl::HandleResponse<u2f::MakeCredentialResponse>,
          weak_factory_.GetWeakPtr(), std::move(uma_callback_wrapper)));
}

void U2FClientImpl::GetAssertion(
    const u2f::GetAssertionRequest& request,
    DBusMethodCallback<u2f::GetAssertionResponse> callback) {
  dbus::MethodCall method_call(u2f::kU2FInterface, u2f::kU2FGetAssertion);
  dbus::MessageWriter writer(&method_call);
  writer.AppendProtoAsArrayOfBytes(request);
  auto uma_callback_wrapper = base::BindOnce(
      [](DBusMethodCallback<u2f::GetAssertionResponse> callback,
         base::Optional<u2f::GetAssertionResponse> response) {
        if (response) {
          base::UmaHistogramEnumeration(
              kGetAssertionStatusHistogram, response->status(),
              static_cast<u2f::GetAssertionResponse::GetAssertionStatus>(
                  u2f::GetAssertionResponse::GetAssertionStatus_ARRAYSIZE));
        }
        std::move(callback).Run(response);
      },
      std::move(callback));
  proxy_->CallMethod(
      &method_call, kU2FInfiniteTimeout,
      base::BindOnce(&U2FClientImpl::HandleResponse<u2f::GetAssertionResponse>,
                     weak_factory_.GetWeakPtr(),
                     std::move(uma_callback_wrapper)));
}

void U2FClientImpl::HasCredentials(
    const u2f::HasCredentialsRequest& request,
    DBusMethodCallback<u2f::HasCredentialsResponse> callback) {
  dbus::MethodCall method_call(u2f::kU2FInterface, u2f::kU2FHasCredentials);
  dbus::MessageWriter writer(&method_call);
  writer.AppendProtoAsArrayOfBytes(request);
  proxy_->CallMethod(
      &method_call, kU2FShortTimeout,
      base::BindOnce(
          &U2FClientImpl::HandleResponse<u2f::HasCredentialsResponse>,
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

void U2FClientImpl::HasLegacyU2FCredentials(
    const u2f::HasCredentialsRequest& request,
    DBusMethodCallback<u2f::HasCredentialsResponse> callback) {
  dbus::MethodCall method_call(u2f::kU2FInterface,
                               u2f::kU2FHasLegacyCredentials);
  dbus::MessageWriter writer(&method_call);
  writer.AppendProtoAsArrayOfBytes(request);
  proxy_->CallMethod(
      &method_call, kU2FShortTimeout,
      base::BindOnce(
          &U2FClientImpl::HandleResponse<u2f::HasCredentialsResponse>,
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

void U2FClientImpl::CancelWebAuthnFlow(
    const u2f::CancelWebAuthnFlowRequest& request,
    DBusMethodCallback<u2f::CancelWebAuthnFlowResponse> callback) {
  dbus::MethodCall method_call(u2f::kU2FInterface, u2f::kU2FCancelWebAuthnFlow);
  dbus::MessageWriter writer(&method_call);
  writer.AppendProtoAsArrayOfBytes(request);
  proxy_->CallMethod(
      &method_call, kU2FShortTimeout,
      base::BindOnce(
          &U2FClientImpl::HandleResponse<u2f::CancelWebAuthnFlowResponse>,
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

}  // namespace

U2FClient::U2FClient() {
  CHECK(!g_instance);
  g_instance = this;
}

U2FClient::~U2FClient() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void U2FClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new U2FClientImpl())->Init(bus);
}

// static
void U2FClient::InitializeFake() {
  new FakeU2FClient();
}

// static
void U2FClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
  // The destructor resets |g_instance|.
  DCHECK(!g_instance);
}

// static
U2FClient* U2FClient::Get() {
  CHECK(g_instance);
  return g_instance;
}

}  // namespace chromeos
