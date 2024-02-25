// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_CLIENT_IMPL_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_CLIENT_IMPL_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/device_sync/cryptauth_api_call_flow.h"
#include "chromeos/ash/services/device_sync/cryptauth_client.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_api.pb.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace signin {
struct AccessTokenInfo;
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

class GoogleServiceAuthError;

namespace ash {

namespace device_sync {

// Implementation of CryptAuthClient.
// Note: There is no need to set the |device_classifier| field in request
// messages. CryptAuthClient will fill this field for all requests.
class CryptAuthClientImpl : public CryptAuthClient {
 public:
  // Creates the client using |url_request_context| to make the HTTP request
  // through |api_call_flow|. The |device_classifier| argument contains basic
  // device information of the caller (e.g. version and device type).
  // TODO(nohle): Remove the |device_classifier| argument when the CryptAuth v1
  // methods are no longer needed.
  CryptAuthClientImpl(
      std::unique_ptr<CryptAuthApiCallFlow> api_call_flow,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const cryptauth::DeviceClassifier& device_classifier);

  CryptAuthClientImpl(const CryptAuthClientImpl&) = delete;
  CryptAuthClientImpl& operator=(const CryptAuthClientImpl&) = delete;

  ~CryptAuthClientImpl() override;

  // CryptAuthClient:
  void GetMyDevices(const cryptauth::GetMyDevicesRequest& request,
                    GetMyDevicesCallback callback,
                    ErrorCallback error_callback,
                    const net::PartialNetworkTrafficAnnotationTag&
                        partial_traffic_annotation) override;
  void FindEligibleUnlockDevices(
      const cryptauth::FindEligibleUnlockDevicesRequest& request,
      FindEligibleUnlockDevicesCallback callback,
      ErrorCallback error_callback) override;
  void FindEligibleForPromotion(
      const cryptauth::FindEligibleForPromotionRequest& request,
      FindEligibleForPromotionCallback callback,
      ErrorCallback error_callback) override;
  void SendDeviceSyncTickle(
      const cryptauth::SendDeviceSyncTickleRequest& request,
      SendDeviceSyncTickleCallback callback,
      ErrorCallback error_callback,
      const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation)
      override;
  void ToggleEasyUnlock(const cryptauth::ToggleEasyUnlockRequest& request,
                        ToggleEasyUnlockCallback callback,
                        ErrorCallback error_callback) override;
  void SetupEnrollment(const cryptauth::SetupEnrollmentRequest& request,
                       SetupEnrollmentCallback callback,
                       ErrorCallback error_callback) override;
  void FinishEnrollment(const cryptauth::FinishEnrollmentRequest& request,
                        FinishEnrollmentCallback callback,
                        ErrorCallback error_callback) override;
  void SyncKeys(const cryptauthv2::SyncKeysRequest& request,
                SyncKeysCallback callback,
                ErrorCallback error_callback) override;
  void EnrollKeys(const cryptauthv2::EnrollKeysRequest& request,
                  EnrollKeysCallback callback,
                  ErrorCallback error_callback) override;
  void SyncMetadata(const cryptauthv2::SyncMetadataRequest& request,
                    SyncMetadataCallback callback,
                    ErrorCallback error_callback) override;
  void ShareGroupPrivateKey(
      const cryptauthv2::ShareGroupPrivateKeyRequest& request,
      ShareGroupPrivateKeyCallback callback,
      ErrorCallback error_callback) override;
  void BatchNotifyGroupDevices(
      const cryptauthv2::BatchNotifyGroupDevicesRequest& request,
      BatchNotifyGroupDevicesCallback callback,
      ErrorCallback error_callback) override;
  void BatchGetFeatureStatuses(
      const cryptauthv2::BatchGetFeatureStatusesRequest& request,
      BatchGetFeatureStatusesCallback callback,
      ErrorCallback error_callback) override;
  void BatchSetFeatureStatuses(
      const cryptauthv2::BatchSetFeatureStatusesRequest& request,
      BatchSetFeatureStatusesCallback callback,
      ErrorCallback error_callback) override;
  void GetDevicesActivityStatus(
      const cryptauthv2::GetDevicesActivityStatusRequest& request,
      GetDevicesActivityStatusCallback callback,
      ErrorCallback error_callback) override;
  std::string GetAccessTokenUsed() override;

 private:
  enum class RequestType { kGet, kPost };

  // Starts a call to the API given by |request_url|. The client first fetches
  // the access token and then makes the HTTP request.
  //   |request_url|: API endpoint.
  //   |request_type|: Whether the request is a GET or POST.
  //   |serialized_request|: Serialized request message proto that will be sent
  //                         as the body of a POST request. Null if
  //                         request type is not POST.
  //   |request_as_query_parameters|: The request message proto represented as
  //                                  key-value pairs that will be sent as query
  //                                  parameters in a GET request. Note: A key
  //                                  can have multiple values. Null if request
  //                                  type is not GET.
  //   |response_callback|: Callback for a successful request.
  //   |error_callback|: Callback for a failed request.
  //   |partial_traffic_annotation|: A partial tag used to mark a source of
  template <class ResponseProto>
  void MakeApiCall(
      const GURL& request_url,
      RequestType request_type,
      const std::optional<std::string>& serialized_request,
      const std::optional<std::vector<std::pair<std::string, std::string>>>&
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
      const std::optional<std::vector<std::pair<std::string, std::string>>>&
          request_as_query_parameters,
      base::OnceCallback<void(const ResponseProto&)> response_callback,
      GoogleServiceAuthError error,
      signin::AccessTokenInfo access_token_info);

  // Called with CryptAuthApiCallFlow completes successfully to deserialize and
  // return the result.
  template <class ResponseProto>
  void OnFlowSuccess(
      base::OnceCallback<void(const ResponseProto&)> result_callback,
      const std::string& serialized_response);

  // Called when the current API call fails at any step.
  void OnApiCallFailed(NetworkRequestError error);

  // Returns a copy of the input request with the device classifier field set.
  // Only used for CryptAuth v1 protos.
  template <class RequestProto>
  RequestProto RequestWithDeviceClassifierSet(const RequestProto& request);

  // Constructs and executes the actual HTTP request.
  std::unique_ptr<CryptAuthApiCallFlow> api_call_flow_;

  raw_ptr<signin::IdentityManager> identity_manager_;

  // Fetches the access token authorizing the API calls.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  // The context for network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Contains basic device info of the client making the request that is sent to
  // CryptAuth with each API call.
  const cryptauth::DeviceClassifier device_classifier_;

  // True if an API call has been started. Remains true even after the API call
  // completes.
  bool has_call_started_;

  // URL of the current request.
  GURL request_url_;

  // The access token fetched by |access_token_fetcher_|.
  std::string access_token_used_;

  // Called when the current request fails.
  ErrorCallback error_callback_;

  base::WeakPtrFactory<CryptAuthClientImpl> weak_ptr_factory_{this};
};

// Implementation of CryptAuthClientFactory.
class CryptAuthClientFactoryImpl : public CryptAuthClientFactory {
 public:
  // |identity_manager|: Gets the user's access token.
  //     Not owned, so |identity_manager| needs to outlive this object.
  // |url_request_context|: The request context to make the HTTP requests.
  // |device_classifier|: Contains basic device information of the client.
  CryptAuthClientFactoryImpl(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const cryptauth::DeviceClassifier& device_classifier);

  CryptAuthClientFactoryImpl(const CryptAuthClientFactoryImpl&) = delete;
  CryptAuthClientFactoryImpl& operator=(const CryptAuthClientFactoryImpl&) =
      delete;

  ~CryptAuthClientFactoryImpl() override;

  // CryptAuthClientFactory:
  std::unique_ptr<CryptAuthClient> CreateInstance() override;

 private:
  raw_ptr<signin::IdentityManager> identity_manager_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const cryptauth::DeviceClassifier device_classifier_;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_CLIENT_IMPL_H_
