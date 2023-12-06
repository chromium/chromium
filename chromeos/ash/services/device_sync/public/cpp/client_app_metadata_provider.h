// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_PUBLIC_CPP_CLIENT_APP_METADATA_PROVIDER_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_PUBLIC_CPP_CLIENT_APP_METADATA_PROVIDER_H_

#include <optional>

#include "base/functional/callback_forward.h"

namespace cryptauthv2 {
class ClientAppMetadata;
}  // namespace cryptauthv2

namespace ash {

namespace device_sync {

// Provides the cryptauthv2::ClientAppMetadata object associated with the
// current device. cryptauthv2::ClientAppMetadata describes properties of this
// Chromebook and is not expected to change except when the OS version is
// updated.
class ClientAppMetadataProvider {
 public:
  ClientAppMetadataProvider() = default;

  ClientAppMetadataProvider(const ClientAppMetadataProvider&) = delete;
  ClientAppMetadataProvider& operator=(const ClientAppMetadataProvider&) =
      delete;

  virtual ~ClientAppMetadataProvider() = default;

  using GetMetadataCallback = base::OnceCallback<void(
      const std::optional<cryptauthv2::ClientAppMetadata>&)>;

  // Fetches the ClientAppMetadata for the current device; if the operation
  // fails, null is passed to the callback.
  virtual void GetClientAppMetadata(const std::string& gcm_registration_id,
                                    GetMetadataCallback callback) = 0;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_PUBLIC_CPP_CLIENT_APP_METADATA_PROVIDER_H_
