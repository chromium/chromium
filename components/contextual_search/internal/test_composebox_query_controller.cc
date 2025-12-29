// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/internal/test_composebox_query_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/lens/lens_features.h"
#include "components/variations/variations.mojom.h"
#include "components/variations/variations_client.h"
#include "google_apis/common/api_error_codes.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"

using endpoint_fetcher::EndpointFetcher;
using endpoint_fetcher::EndpointFetcherCallback;
using endpoint_fetcher::EndpointResponse;
using endpoint_fetcher::HttpMethod;

FakeEndpointFetcher::FakeEndpointFetcher(EndpointResponse response)
    : EndpointFetcher(
          net::DefineNetworkTrafficAnnotation("compbosebox_mock_fetcher",
                                              R"()")),
      response_(response) {}

void FakeEndpointFetcher::PerformRequest(
    EndpointFetcherCallback endpoint_fetcher_callback,
    const char* key) {
  if (!disable_responding_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(endpoint_fetcher_callback),
                       std::make_unique<EndpointResponse>(response_)));
  }
}

bool FakeVariationsClient::IsOffTheRecord() const {
  return false;
}

variations::mojom::VariationsHeadersPtr
FakeVariationsClient::GetVariationsHeaders() const {
  base::flat_map<variations::mojom::GoogleWebVisibility, std::string> headers =
      {{variations::mojom::GoogleWebVisibility::FIRST_PARTY, "123xyz"}};
  return variations::mojom::VariationsHeaders::New(headers);
}

namespace contextual_search {

TestComposeboxQueryController::TestComposeboxQueryController(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    version_info::Channel channel,
    std::string locale,
    TemplateURLService* template_url_service,
    variations::VariationsClient* variations_client,
    std::unique_ptr<ContextualSearchContextController::ConfigParams>
        config_params,
    bool enable_cluster_info_ttl)
    : ComposeboxQueryController(identity_manager,
                                url_loader_factory,
                                channel,
                                locale,
                                template_url_service,
                                variations_client,
                                std::move(config_params)),
      enable_cluster_info_ttl_(enable_cluster_info_ttl) {}
TestComposeboxQueryController::~TestComposeboxQueryController() = default;

std::unique_ptr<EndpointFetcher>
TestComposeboxQueryController::CreateEndpointFetcher(
    std::string request_string,
    const GURL& fetch_url,
    HttpMethod http_method,
    base::TimeDelta timeout,
    const std::vector<std::string>& request_headers,
    const std::vector<std::string>& cors_exempt_headers,
    UploadProgressCallback upload_progress_callback) {
  std::string fake_server_response_string;
  google_apis::ApiErrorCode fake_server_response_code =
      google_apis::ApiErrorCode::HTTP_SUCCESS;
  // Whether or not to disable the response.
  bool disable_response = false;

  bool is_cluster_info_request =
      fetch_url == GURL(lens::features::GetLensOverlayClusterInfoEndpointUrl());

  if (is_cluster_info_request) {
    // Cluster info request.
    num_cluster_info_fetch_requests_sent_++;
    if (next_cluster_info_request_should_return_error_) {
      fake_server_response_code =
          google_apis::ApiErrorCode::HTTP_INTERNAL_SERVER_ERROR;
    } else {
      fake_server_response_string =
          fake_cluster_info_response_.SerializeAsString();
    }
  } else {
    num_file_upload_requests_sent_++;
    last_sent_fetch_url_ = fetch_url;
    if (next_file_upload_request_should_return_error_) {
      fake_server_response_code =
          google_apis::ApiErrorCode::HTTP_INTERNAL_SERVER_ERROR;
    } else {
      fake_server_response_string =
          fake_file_upload_response_.SerializeAsString();
    }

    lens::LensOverlayServerRequest sent_request;
    sent_request.ParseFromString(request_string);

    if (sent_request.has_interaction_request()) {
      sent_interaction_requests_.push_back(sent_request);
    } else {
    sent_upload_requests_.push_back(sent_request);
    }
  }

  last_sent_cors_exempt_headers_.clear();
  for (const auto& header : cors_exempt_headers) {
    last_sent_cors_exempt_headers_.push_back(header);
  }

  // Create the fake endpoint fetcher to return the fake response.
  EndpointResponse fake_endpoint_response;
  fake_endpoint_response.response = fake_server_response_string;
  fake_endpoint_response.http_status_code = fake_server_response_code;

  auto response = std::make_unique<FakeEndpointFetcher>(fake_endpoint_response);
  response->disable_responding_ = disable_response;

  for (auto& callback : on_endpoint_fetcher_created_callbacks_) {
    callback.Run();
  }
  return response;
}

void TestComposeboxQueryController::ResetRequestClusterInfoState() {
  if (!enable_cluster_info_ttl_) {
    return;
  }
  ComposeboxQueryController::ResetRequestClusterInfoState();
}

}  // namespace contextual_search
