// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_NEARBY_PRESENCE_SERVER_CLIENT_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_NEARBY_PRESENCE_SERVER_CLIENT_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/nearby/common/client/nearby_api_call_flow.h"
#include "chromeos/ash/components/nearby/common/client/nearby_http_result.h"
#include "chromeos/ash/components/nearby/presence/credentials/nearby_presence_server_client.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace signin {
struct AccessTokenInfo;
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

class GoogleServiceAuthError;

namespace ash::nearby::presence {

// An implementation of NearbyPresenceServerClient that fetches access tokens
// for the primary account and makes HTTP calls using NearbyApiCallFlow.
// Callbacks are guaranteed to not be invoked after
// NearbyPresenceServerClientImpl is destroyed.
//
// TODO(b/277074086): Delete `NearbyShareClient` in favor of this class.
// This class is a copy of `NearbyShareClient`, which is Nearby Share's
// access point to make HTTP calls to the Nearby Share server. Eventually
// when Nearby Share is migrated over to Nearby Presence, `NearbyShareClient`
// will be deleted in favor of this class.
class NearbyPresenceServerClientImpl : public NearbyPresenceServerClient {
 public:
  // Interface for creating NearbyPresenceServerClient instances. Because each
  // NearbyPresenceServerClient instance can only be used for one API call, a
  // factory makes it easier to make multiple requests in sequence or in
  // parallel.
  class Factory {
   public:
    static std::unique_ptr<NearbyPresenceServerClient> Create(
        std::unique_ptr<NearbyApiCallFlow> api_call_flow,
        signin::IdentityManager* identity_manager,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<NearbyPresenceServerClient> CreateInstance(
        std::unique_ptr<NearbyApiCallFlow> api_call_flow,
        signin::IdentityManager* identity_manager,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) = 0;

   private:
    static Factory* g_test_factory_;
  };

  ~NearbyPresenceServerClientImpl() override;
  NearbyPresenceServerClientImpl(NearbyPresenceServerClientImpl&) = delete;
  NearbyPresenceServerClientImpl& operator=(NearbyPresenceServerClientImpl&) =
      delete;

  // NearbyPresenceServerClient:
  void UpdateDevice(const ash::nearby::proto::UpdateDeviceRequest& request,
                    UpdateDeviceCallback callback,
                    ErrorCallback error_callback) override;
  void ListSharedCredentials(
      const ash::nearby::proto::ListSharedCredentialsRequest& request,
      ListSharedCredentialsCallback callback,
      ErrorCallback error_callback) override;
  std::string GetAccessTokenUsed() override;

 private:
  // Creates the client using |url_loader_factory| to make the HTTP request
  // through |api_call_flow|.
  NearbyPresenceServerClientImpl(
      std::unique_ptr<ash::nearby::NearbyApiCallFlow> api_call_flow,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  enum class RequestType { kGet, kPost, kPatch };

  // Starts a call to the API given by |request_url|. The client first fetches
  // the access token and then makes the HTTP request.
  //   |request_url|: API endpoint.
  //   |request_type|: Whether the request is a GET, POST, or PATCH.
  //   |serialized_request|: Serialized request message proto that will be sent
  //                         as the body of a POST or PATCH request. Null if
  //                         request type is not POST or PATCH.
  //   |request_as_query_parameters|: The request message proto represented as
  //                                  key-value pairs that will be sent as query
  //                                  parameters in a GET request. Note: A key
  //                                  can have multiple values. Null if request
  //                                  type is not GET.
  //   |response_callback|: Callback for a successful request.
  //   |error_callback|: Callback for a failed request.
  //   |partial_traffic_annotation|: A partial tag used to mark a source of
  //                                 network traffic.
  template <class ResponseProto>
  void MakeApiCall(
      const GURL& request_url,
      RequestType request_type,
      const std::optional<std::string>& serialized_request,
      const std::optional<ash::nearby::NearbyApiCallFlow::QueryParameters>&
          request_as_query_parameters,
      base::OnceCallback<void(const ResponseProto&)> response_callback,
      ErrorCallback error_callback,
      const net::PartialNetworkTrafficAnnotationTag&
          partial_traffic_annotation);

  // Called when the access token is obtained so the API request can be made.
  template <class ResponseProto>
  void OnAccessTokenFetched(
      RequestType request_type,
      const std::optional<std::string>& serialized_request,
      const std::optional<ash::nearby::NearbyApiCallFlow::QueryParameters>&
          request_as_query_parameters,
      base::OnceCallback<void(const ResponseProto&)> response_callback,
      GoogleServiceAuthError error,
      signin::AccessTokenInfo access_token_info);

  // Called when ash::nearby::NearbyApiCallFlow completes successfully to
  // de-serialize and return the result.
  template <class ResponseProto>
  void OnFlowSuccess(
      base::OnceCallback<void(const ResponseProto&)> result_callback,
      const std::string& serialized_response);

  // Called when the current API call fails at any step.
  void OnApiCallFailed(NearbyHttpError error);

  // Constructs and executes the actual HTTP request.
  std::unique_ptr<NearbyApiCallFlow> api_call_flow_;

  const raw_ptr<signin::IdentityManager> identity_manager_;

  // Fetches the access token authorizing the API calls.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // True if an API call has been started. Remains true even after the API call
  // completes.
  bool has_call_started_ = false;

  // URL of the current request.
  GURL request_url_;

  // The access token fetched by |access_token_fetcher_|.
  std::string access_token_used_;

  // Called when the current request fails.
  ErrorCallback error_callback_;

  base::WeakPtrFactory<NearbyPresenceServerClientImpl> weak_ptr_factory_{this};
};

}  // namespace ash::nearby::presence

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_NEARBY_PRESENCE_SERVER_CLIENT_IMPL_H_
