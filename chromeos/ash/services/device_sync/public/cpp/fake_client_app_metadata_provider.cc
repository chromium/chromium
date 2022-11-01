// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/public/cpp/fake_client_app_metadata_provider.h"

namespace ash {

namespace device_sync {

FakeClientAppMetadataProvider::GetMetadataRequest::GetMetadataRequest(
    const std::string& gcm_registration_id,
    ClientAppMetadataProvider::GetMetadataCallback callback)
    : gcm_registration_id(gcm_registration_id), callback(std::move(callback)) {}

FakeClientAppMetadataProvider::GetMetadataRequest::GetMetadataRequest(
    GetMetadataRequest&&) = default;

FakeClientAppMetadataProvider::GetMetadataRequest::~GetMetadataRequest() =
    default;

FakeClientAppMetadataProvider::FakeClientAppMetadataProvider() = default;

FakeClientAppMetadataProvider::~FakeClientAppMetadataProvider() = default;

void FakeClientAppMetadataProvider::GetClientAppMetadata(
    const std::string& gcm_registration_id,
    ClientAppMetadataProvider::GetMetadataCallback callback) {
  metadata_requests_.emplace_back(gcm_registration_id, std::move(callback));
}

}  // namespace device_sync

}  // namespace ash
