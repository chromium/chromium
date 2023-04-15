// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_NEARBY_PRESENCE_SERVER_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_NEARBY_PRESENCE_SERVER_CLIENT_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "chromeos/ash/components/nearby/common/client/nearby_http_result.h"

namespace ash::nearby::proto {
class ListPublicCertificatesRequest;
class ListPublicCertificatesResponse;
class UpdateDeviceRequest;
class UpdateDeviceResponse;
}  // namespace ash::nearby::proto

namespace ash::nearby::presence {

// Interface for making API requests to the NearbyPresence service, which
// manages certificates.
// Implementations shall only processes a single request, so create a new
// instance for each request you make. DO NOT REUSE.
class NearbyPresenceServerClient {
 public:
  // Interface for creating NearbyPresenceServerClient instances. Because each
  // NearbyPresenceServerClient instance can only be used for one API call, a
  // factory makes it easier to make multiple requests in sequence or in
  // parallel.
  class Factory {
   public:
    Factory() = default;
    virtual ~Factory() = default;

    virtual std::unique_ptr<NearbyPresenceServerClient> CreateInstance() = 0;
  };

  using ErrorCallback = base::OnceCallback<void(ash::nearby::NearbyHttpError)>;
  using ListPublicCertificatesCallback = base::OnceCallback<void(
      const ash::nearby::proto::ListPublicCertificatesResponse&)>;
  using UpdateDeviceCallback =
      base::OnceCallback<void(const ash::nearby::proto::UpdateDeviceResponse&)>;

  NearbyPresenceServerClient() = default;
  virtual ~NearbyPresenceServerClient() = default;

  // Communicates with the NearbyPresenceService server v1: UpdateDevice RPC.
  virtual void UpdateDevice(
      const ash::nearby::proto::UpdateDeviceRequest& request,
      UpdateDeviceCallback callback,
      ErrorCallback error_callback) = 0;

  // Communicates with the NearbyPresenceService server v1:
  // ListPublicCertificates RPC.
  virtual void ListPublicCertificates(
      const ash::nearby::proto::ListPublicCertificatesRequest& request,
      ListPublicCertificatesCallback callback,
      ErrorCallback error_callback) = 0;

  // Returns the access token used to make the request. If no request has been
  // made yet, this function will return an empty string.
  virtual std::string GetAccessTokenUsed() = 0;
};

}  // namespace ash::nearby::presence

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_NEARBY_PRESENCE_SERVER_CLIENT_H_
