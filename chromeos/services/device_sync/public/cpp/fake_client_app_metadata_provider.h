// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_FAKE_CLIENT_APP_METADATA_PROVIDER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_FAKE_CLIENT_APP_METADATA_PROVIDER_H_

#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "chromeos/services/device_sync/public/cpp/client_app_metadata_provider.h"

namespace chromeos {

namespace device_sync {

// Implementation of ClientAppMetadataProvider for use in tests.
class FakeClientAppMetadataProvider : public ClientAppMetadataProvider {
 public:
  struct GetMetadataRequest {
    GetMetadataRequest(const std::string& gcm_registration_id,
                       ClientAppMetadataProvider::GetMetadataCallback callback);
    GetMetadataRequest(GetMetadataRequest&&);
    ~GetMetadataRequest();

    std::string gcm_registration_id;
    ClientAppMetadataProvider::GetMetadataCallback callback;
  };

  FakeClientAppMetadataProvider();
  ~FakeClientAppMetadataProvider() override;

  // ClientAppMetadataProvider:
  void GetClientAppMetadata(
      const std::string& gcm_registration_id,
      ClientAppMetadataProvider::GetMetadataCallback callback) override;

  // Returns the array of GetMetadataRequests from GetClientAppMetadata() calls,
  // ordered from first call to last call. Because this array is returned by
  // reference, the client can invoke the callback of the i-th call using the
  // following:
  //
  //     std::move(metadata_requests()[i].callback).Run(...)
  std::vector<GetMetadataRequest>& metadata_requests() {
    return metadata_requests_;
  }

 private:
  std::vector<GetMetadataRequest> metadata_requests_;

  DISALLOW_COPY_AND_ASSIGN(FakeClientAppMetadataProvider);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_FAKE_CLIENT_APP_METADATA_PROVIDER_H_
