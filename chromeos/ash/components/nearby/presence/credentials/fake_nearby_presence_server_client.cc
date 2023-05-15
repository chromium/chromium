// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/credentials/fake_nearby_presence_server_client.h"

namespace ash::nearby::presence {

FakeNearbyPresenceServerClient::Factory::Factory() = default;

FakeNearbyPresenceServerClient::Factory::~Factory() = default;

std::unique_ptr<NearbyPresenceServerClient>
FakeNearbyPresenceServerClient::Factory::CreateInstance(
    std::unique_ptr<NearbyApiCallFlow> api_call_flow,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  auto instance = std::make_unique<FakeNearbyPresenceServerClient>();
  last_created_fake_server_client_ = instance.get();
  return instance;
}

FakeNearbyPresenceServerClient::FakeNearbyPresenceServerClient() = default;

FakeNearbyPresenceServerClient::~FakeNearbyPresenceServerClient() = default;

void FakeNearbyPresenceServerClient::SetAccessTokenUsed(
    const std::string& token) {
  access_token_used_ = token;
}

void FakeNearbyPresenceServerClient::UpdateDevice(
    const ash::nearby::proto::UpdateDeviceRequest& request,
    UpdateDeviceCallback callback,
    ErrorCallback error_callback) {
  update_device_callback_ = std::move(callback);
  update_device_error_callback_ = std::move(error_callback);
}

void FakeNearbyPresenceServerClient::ListPublicCertificates(
    const ash::nearby::proto::ListPublicCertificatesRequest& request,
    ListPublicCertificatesCallback callback,
    ErrorCallback error_callback) {
  list_public_certificates_callback_ = std::move(callback);
  list_public_certificates_error_callback_ = std::move(error_callback);
}

void FakeNearbyPresenceServerClient::InvokeUpdateDeviceSuccessCallback(
    const ash::nearby::proto::UpdateDeviceResponse& response) {
  CHECK(update_device_callback_);
  std::move(update_device_callback_).Run(response);
}

void FakeNearbyPresenceServerClient::InvokeUpdateDeviceErrorCallback(
    ash::nearby::NearbyHttpError error) {
  CHECK(update_device_error_callback_);
  std::move(update_device_error_callback_).Run(error);
}

void FakeNearbyPresenceServerClient::
    InvokeListPublicCertificatesSuccessCallback(
        const ash::nearby::proto::ListPublicCertificatesResponse& response) {
  CHECK(list_public_certificates_callback_);
  std::move(list_public_certificates_callback_).Run(response);
}

void FakeNearbyPresenceServerClient::InvokeListPublicCertificatesErrorCallback(
    ash::nearby::NearbyHttpError error) {
  CHECK(list_public_certificates_error_callback_);
  std::move(list_public_certificates_error_callback_).Run(error);
}

std::string FakeNearbyPresenceServerClient::GetAccessTokenUsed() {
  return access_token_used_;
}

}  // namespace ash::nearby::presence
