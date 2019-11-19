// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_API_CALL_FLOW_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_API_CALL_FLOW_H_

#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/services/device_sync/network_request_error.h"
#include "google_apis/gaia/oauth2_api_call_flow.h"
#include "services/network/public/cpp/resource_response.h"

namespace chromeos {

namespace device_sync {

// Google API call flow implementation underlying all CryptAuth API calls.
// CryptAuth is a Google service that manages authorization and cryptographic
// credentials for users' devices (eg. public keys). We assume the following:
//   * A POST request's body is the serialized request proto,
//   * A GET request encodes the request proto as query parameters and has no
//     body,
//   * The response body is the serialized response proto.

class CryptAuthApiCallFlow : public OAuth2ApiCallFlow {
 public:
  typedef base::Callback<void(const std::string& serialized_response)>
      ResultCallback;
  typedef base::Callback<void(NetworkRequestError error)> ErrorCallback;

  CryptAuthApiCallFlow();
  ~CryptAuthApiCallFlow() override;

  // Starts the API POST request call.
  //   |request_url|: The URL endpoint of the API request.
  //   |serialized_request|: A serialized proto containing the request data.
  //   |access_token|: The access token for whom to make the request.
  //   |result_callback|: Called when the flow completes successfully
  //                      with a serialized response proto.
  //   |error_callback|: Called when the flow completes with an error.
  virtual void StartPostRequest(
      const GURL& request_url,
      const std::string& serialized_request,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& access_token,
      const ResultCallback& result_callback,
      const ErrorCallback& error_callback);

  // Starts the API GET request call.
  //   |request_url|: The URL endpoint of the API request.
  //   |request_as_query_parameters|: The request proto represented as key-value
  //                                  pairs to be sent as query parameters.
  //                                  Note: A key can have multiple values.
  //   |access_token|: The access token for whom to make the request.
  //   |result_callback|: Called when the flow completes successfully
  //                      with a serialized response proto.
  //   |error_callback|: Called when the flow completes with an error.
  virtual void StartGetRequest(
      const GURL& request_url,
      const std::vector<std::pair<std::string, std::string>>&
          request_as_query_parameters,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& access_token,
      const ResultCallback& result_callback,
      const ErrorCallback& error_callback);

  void SetPartialNetworkTrafficAnnotation(
      const net::PartialNetworkTrafficAnnotationTag&
          partial_traffic_annotation) {
    partial_network_annotation_.reset(
        new net::PartialNetworkTrafficAnnotationTag(
            partial_traffic_annotation));
  }

 protected:
  // Reduce the visibility of OAuth2ApiCallFlow::Start() to avoid exposing
  // overloaded methods.
  using OAuth2ApiCallFlow::Start;

  // google_apis::OAuth2ApiCallFlow:
  GURL CreateApiCallUrl() override;
  std::string CreateApiCallBody() override;
  std::string CreateApiCallBodyContentType() override;
  std::string GetRequestTypeForBody(const std::string& body) override;
  void ProcessApiCallSuccess(const network::mojom::URLResponseHead* head,
                             std::unique_ptr<std::string> body) override;
  void ProcessApiCallFailure(int net_error,
                             const network::mojom::URLResponseHead* head,
                             std::unique_ptr<std::string> body) override;
  net::PartialNetworkTrafficAnnotationTag GetNetworkTrafficAnnotationTag()
      override;

 private:
  // The URL of the CryptAuth endpoint serving the request.
  GURL request_url_;

  // Serialized request message proto that will be sent in the API POST request.
  // Null if request type is not POST.
  base::Optional<std::string> serialized_request_;

  // The request message proto represented as key-value pairs that will be sent
  // as query parameters in the API GET request. Note: A key can have multiple
  // values. Null if request type is not GET.
  base::Optional<std::vector<std::pair<std::string, std::string>>>
      request_as_query_parameters_;

  // Callback invoked with the serialized response message proto when the flow
  // completes successfully.
  ResultCallback result_callback_;

  // Callback invoked with an error message when the flow fails.
  ErrorCallback error_callback_;

  std::unique_ptr<net::PartialNetworkTrafficAnnotationTag>
      partial_network_annotation_;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthApiCallFlow);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_API_CALL_FLOW_H_
