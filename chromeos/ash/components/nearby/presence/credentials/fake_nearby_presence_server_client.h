// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_FAKE_NEARBY_PRESENCE_SERVER_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_FAKE_NEARBY_PRESENCE_SERVER_CLIENT_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/nearby/presence/credentials/nearby_presence_server_client.h"
#include "chromeos/ash/components/nearby/presence/credentials/nearby_presence_server_client_impl.h"
#include "chromeos/ash/components/nearby/presence/proto/list_shared_credentials_rpc.pb.h"
#include "chromeos/ash/components/nearby/presence/proto/update_device_rpc.pb.h"

namespace ash::nearby::presence {

// A fake implementation of the Nearby Presence HTTP client that stores all
// request data. Only use in unit tests.
class FakeNearbyPresenceServerClient : public NearbyPresenceServerClient {
 public:
  // Factory that creates FakeNearbyPresenceServerClient instances. Use in
  // NearbyPresenceServerClientImpl::Factory::SetFactoryForTesting() in unit
  // tests.
  class Factory : public NearbyPresenceServerClientImpl::Factory {
   public:
    Factory();
    ~Factory() override;

    FakeNearbyPresenceServerClient* fake_server_client() {
      return last_created_fake_server_client_;
    }

   private:
    // NearbyPresenceServerClientImpl::Factory:
    std::unique_ptr<NearbyPresenceServerClient> CreateInstance(
        std::unique_ptr<NearbyApiCallFlow> api_call_flow,
        signin::IdentityManager* identity_manager,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
        override;

    raw_ptr<FakeNearbyPresenceServerClient, AcrossTasksDanglingUntriaged>
        last_created_fake_server_client_ = nullptr;
  };

  FakeNearbyPresenceServerClient();
  ~FakeNearbyPresenceServerClient() override;

  void SetAccessTokenUsed(const std::string& token);

  void InvokeUpdateDeviceSuccessCallback(
      const ash::nearby::proto::UpdateDeviceResponse& response);
  void InvokeUpdateDeviceErrorCallback(ash::nearby::NearbyHttpError error);
  void InvokeListSharedCredentialsSuccessCallback(
      const ash::nearby::proto::ListSharedCredentialsResponse& response);
  void InvokeListSharedCredentialsErrorCallback(
      ash::nearby::NearbyHttpError error);

 private:
  // NearbyPresenceServerClient:
  void UpdateDevice(const ash::nearby::proto::UpdateDeviceRequest& request,
                    UpdateDeviceCallback callback,
                    ErrorCallback error_callback) override;
  void ListSharedCredentials(
      const ash::nearby::proto::ListSharedCredentialsRequest& request,
      ListSharedCredentialsCallback callback,
      ErrorCallback error_callback) override;
  std::string GetAccessTokenUsed() override;

  std::string access_token_used_;
  UpdateDeviceCallback update_device_callback_;
  ListSharedCredentialsCallback list_shared_credentials_callback_;
  ErrorCallback update_device_error_callback_;
  ErrorCallback list_shared_credentials_error_callback_;
};

}  // namespace ash::nearby::presence

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_FAKE_NEARBY_PRESENCE_SERVER_CLIENT_H_
