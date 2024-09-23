// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/u2f/u2f_client.h"

#include <utility>

#include <google/protobuf/message_lite.h>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/dbus/tpm_manager/tpm_manager.pb.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
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

// Timeout for methods which don't take time proportional to the number of total
// credentials.
constexpr int kU2FShortTimeout = 3000;
// Timeout for methods which take time proportional to the number of total
// credentials.
constexpr int kU2FMediumTimeout = 10000;

template <typename ResponseProto>
std::optional<ResponseProto> ConvertResponse(dbus::Response* dbus_response) {
  if (!dbus_response) {
    return std::nullopt;
  }
  dbus::MessageReader reader(dbus_response);
  ResponseProto response_proto;
  if (!reader.PopArrayOfBytesAsProto(&response_proto)) {
    return std::nullopt;
  }
  return response_proto;
}

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
  void IsUvpaa(const u2f::IsUvpaaRequest& request,
               DBusMethodCallback<u2f::IsUvpaaResponse> callback) override;
  void IsU2FEnabled(
      const u2f::IsU2fEnabledRequest& request,
      DBusMethodCallback<u2f::IsU2fEnabledResponse> callback) override;
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
  void CountCredentials(
      const u2f::CountCredentialsInTimeRangeRequest& request,
      DBusMethodCallback<u2f::CountCredentialsInTimeRangeResponse> callback)
      override;
  void DeleteCredentials(
      const u2f::DeleteCredentialsInTimeRangeRequest& request,
      DBusMethodCallback<u2f::DeleteCredentialsInTimeRangeResponse> callback)
      override;
  void CancelWebAuthnFlow(
      const u2f::CancelWebAuthnFlowRequest& request,
      DBusMethodCallback<u2f::CancelWebAuthnFlowResponse> callback) override;
  void GetAlgorithms(
      const u2f::GetAlgorithmsRequest& request,
      DBusMethodCallback<u2f::GetAlgorithmsResponse> callback) override;
  void GetSupportedFeatures(
      const u2f::GetSupportedFeaturesRequest& request,
      DBusMethodCallback<u2f::GetSupportedFeaturesResponse> callback) override;

 private:
  raw_ptr<dbus::ObjectProxy, LeakedDanglingUntriaged> proxy_ = nullptr;

  base::WeakPtrFactory<U2FClientImpl> weak_factory_{this};
};

template <typename ResponseProto>
void U2FClientImpl::HandleResponse(DBusMethodCallback<ResponseProto> callback,
                                   dbus::Response* response) {
  std::move(callback).Run(ConvertResponse<ResponseProto>(response));
}

void U2FClientImpl::IsUvpaa(const u2f::IsUvpaaRequest& request,
                            DBusMethodCallback<u2f::IsUvpaaResponse> callback) {
  dbus::MethodCall method_call(u2f::kU2FInterface, u2f::kU2FIsUvpaa);
  dbus::MessageWriter writer(&method_call);
  writer.AppendProtoAsArrayOfBytes(request);
  proxy_->CallMethod(
      &method_call, kU2FShortTimeout,
      base::BindOnce(
          [](DBusMethodCallback<u2f::IsUvpaaResponse> callback,
             dbus::Response* dbus_response) {
            std::optional<u2f::IsUvpaaResponse> response =
                ConvertResponse<u2f::IsUvpaaResponse>(dbus_response);
            std::move(callback).Run(std::move(response));
          },
          std::move(callback)));
}

void U2FClientImpl::IsU2FEnabled(
    const u2f::IsU2fEnabledRequest& request,
    DBusMethodCallback<u2f::IsU2fEnabledResponse> callback) {
  dbus::MethodCall method_call(u2f::kU2FInterface, u2f::kU2FIsU2fEnabled);
  dbus::MessageWriter writer(&method_call);
  writer.AppendProtoAsArrayOfBytes(request);
  proxy_->CallMethod(
      &method_call, kU2FShortTimeout,
      base::BindOnce(
          [](DBusMethodCallback<u2f::IsU2fEnabledResponse> callback,
             dbus::Response* dbus_response) {
            std::optional<u2f::IsU2fEnabledResponse> response =
                ConvertResponse<u2f::IsU2fEnabledResponse>(dbus_response);
            std::move(callback).Run(std::move(response));
          },
          std::move(callback)));
}

void U2FClientImpl::MakeCredential(
    const u2f::MakeCredentialRequest& request,
    DBusMethodCallback<u2f::MakeCredentialResponse> callback) {
  dbus::MethodCall method_call(u2f::kU2FInterface, u2f::kU2FMakeCredential);
  dbus::MessageWriter writer(&method_call);
  writer.AppendProtoAsArrayOfBytes(request);
  proxy_->CallMethod(
      &method_call, kU2FInfiniteTimeout,
      base::BindOnce(
          [](DBusMethodCallback<u2f::MakeCredentialResponse> callback,
             dbus::Response* dbus_response) {
            std::optional<u2f::MakeCredentialResponse> response =
                ConvertResponse<u2f::MakeCredentialResponse>(dbus_response);
            if (response) {
              base::UmaHistogramEnumeration(
                  kMakeCredentialStatusHistogram, response->status(),
                  static_cast<
                      u2f::MakeCredentialResponse::MakeCredentialStatus>(
                      u2f::MakeCredentialResponse::
                          MakeCredentialStatus_ARRAYSIZE));
            }
            std::move(callback).Run(std::move(response));
          },
          std::move(callback)));
}

void U2FClientImpl::GetAssertion(
    const u2f::GetAssertionRequest& request,
    DBusMethodCallback<u2f::GetAssertionResponse> callback) {
  dbus::MethodCall method_call(u2f::kU2FInterface, u2f::kU2FGetAssertion);
  dbus::MessageWriter writer(&method_call);
  writer.AppendProtoAsArrayOfBytes(request);
  auto uma_callback_wrapper = base::BindOnce(
      [](DBusMethodCallback<u2f::GetAssertionResponse> callback,
         std::optional<u2f::GetAssertionResponse> response) {
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
      &method_call, kU2FMediumTimeout,
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
      &method_call, kU2FMediumTimeout,
      base::BindOnce(
          &U2FClientImpl::HandleResponse<u2f::HasCredentialsResponse>,
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

void U2FClientImpl::CountCredentials(
    const u2f::CountCredentialsInTimeRangeRequest& request,
    DBusMethodCallback<u2f::CountCredentialsInTimeRangeResponse> callback) {
  dbus::MethodCall method_call(u2f::kU2FInterface,
                               u2f::kU2FCountCredentialsInTimeRange);
  dbus::MessageWriter writer(&method_call);
  writer.AppendProtoAsArrayOfBytes(request);
  proxy_->CallMethod(
      &method_call, kU2FMediumTimeout,
      base::BindOnce(&U2FClientImpl::HandleResponse<
                         u2f::CountCredentialsInTimeRangeResponse>,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void U2FClientImpl::DeleteCredentials(
    const u2f::DeleteCredentialsInTimeRangeRequest& request,
    DBusMethodCallback<u2f::DeleteCredentialsInTimeRangeResponse> callback) {
  dbus::MethodCall method_call(u2f::kU2FInterface,
                               u2f::kU2FDeleteCredentialsInTimeRange);
  dbus::MessageWriter writer(&method_call);
  writer.AppendProtoAsArrayOfBytes(request);
  proxy_->CallMethod(
      &method_call, kU2FMediumTimeout,
      base::BindOnce(&U2FClientImpl::HandleResponse<
                         u2f::DeleteCredentialsInTimeRangeResponse>,
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

void U2FClientImpl::GetAlgorithms(
    const u2f::GetAlgorithmsRequest& request,
    DBusMethodCallback<u2f::GetAlgorithmsResponse> callback) {
  dbus::MethodCall method_call(u2f::kU2FInterface, u2f::kU2FGetAlgorithms);
  dbus::MessageWriter writer(&method_call);
  writer.AppendProtoAsArrayOfBytes(request);
  proxy_->CallMethod(
      &method_call, kU2FShortTimeout,
      base::BindOnce(&U2FClientImpl::HandleResponse<u2f::GetAlgorithmsResponse>,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void U2FClientImpl::GetSupportedFeatures(
    const u2f::GetSupportedFeaturesRequest& request,
    DBusMethodCallback<u2f::GetSupportedFeaturesResponse> callback) {
  dbus::MethodCall method_call(u2f::kU2FInterface,
                               u2f::kU2FGetSupportedFeatures);
  dbus::MessageWriter writer(&method_call);
  writer.AppendProtoAsArrayOfBytes(request);
  proxy_->CallMethod(
      &method_call, kU2FShortTimeout,
      base::BindOnce(
          &U2FClientImpl::HandleResponse<u2f::GetSupportedFeaturesResponse>,
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

// static
void U2FClient::IsU2FServiceAvailable(
    base::OnceCallback<void(bool is_supported)> callback) {
  chromeos::TpmManagerClient::Get()->GetSupportedFeatures(
      tpm_manager::GetSupportedFeaturesRequest(),
      base::BindOnce(
          [](base::OnceCallback<void(bool is_available)> callback,
             const ::tpm_manager::GetSupportedFeaturesReply& reply) {
            std::move(callback).Run(reply.support_u2f());
          },
          std::move(callback)));
}

}  // namespace chromeos
