// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_CLIENT_NEARBY_API_CALL_FLOW_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_CLIENT_NEARBY_API_CALL_FLOW_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "chromeos/ash/components/nearby/common/client/nearby_api_call_flow.h"
#include "google_apis/gaia/oauth2_api_call_flow.h"

namespace ash::nearby {

// NearbyApiCallFlowImpl is a wrapper around OAuth2ApiCallFlow
// that provides convenience methods StartGetRequest, StartPostRequest,
// and StartPatchRequest.
// We assume the following:
//   * A POST or PATCH request's body is the serialized request proto,
//   * A GET request encodes the request proto as query parameters and has no
//     body,
//   * The response body is the serialized response proto.
class NearbyApiCallFlowImpl : public NearbyApiCallFlow,
                              public OAuth2ApiCallFlow {
 public:
  NearbyApiCallFlowImpl();
  ~NearbyApiCallFlowImpl() override;

  NearbyApiCallFlowImpl(const NearbyApiCallFlowImpl&) = delete;
  NearbyApiCallFlowImpl& operator=(const NearbyApiCallFlowImpl&) = delete;

  // NearbyApiCallFlow
  void StartPostRequest(
      const GURL& request_url,
      const std::string& serialized_request,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& access_token,
      ResultCallback&& result_callback,
      ErrorCallback&& error_callback) override;
  void StartPatchRequest(
      const GURL& request_url,
      const std::string& serialized_request,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& access_token,
      ResultCallback&& result_callback,
      ErrorCallback&& error_callback) override;
  void StartGetRequest(
      const GURL& request_url,
      const QueryParameters& request_as_query_parameters,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& access_token,
      ResultCallback&& result_callback,
      ErrorCallback&& error_callback) override;
  void SetPartialNetworkTrafficAnnotation(
      const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation)
      override;

 protected:
  // Reduce the visibility of OAuth2ApiCallFlow::Start() to avoid exposing
  // overloaded methods.
  using OAuth2ApiCallFlow::Start;

  // google_apis::OAuth2ApiCallFlow:
  GURL CreateApiCallUrl() override;
  net::HttpRequestHeaders CreateApiCallHeaders() override;
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
  // The URL of the endpoint serving the request.
  GURL request_url_;

  // The HTTP method to use.
  std::string request_http_method_;

  // Serialized request message proto that will be sent in the request body.
  // Null if request is GET.
  std::optional<std::string> serialized_request_;

  // The request message proto represented as key-value pairs that will be sent
  // as query parameters in the API GET request. Note: A key can have multiple
  // values. Null if HTTP method is not GET.
  std::optional<QueryParameters> request_as_query_parameters_;

  // Callback invoked with the serialized response message proto when the flow
  // completes successfully.
  ResultCallback result_callback_;

  // Callback invoked with an error message when the flow fails.
  ErrorCallback error_callback_;

  std::unique_ptr<net::PartialNetworkTrafficAnnotationTag>
      partial_network_annotation_;
};

}  // namespace ash::nearby

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_CLIENT_NEARBY_API_CALL_FLOW_IMPL_H_
