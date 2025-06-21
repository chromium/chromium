// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test_composebox_query_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "components/lens/lens_features.h"
#include "google_apis/common/api_error_codes.h"
#include "testing/gtest/include/gtest/gtest.h"

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

TestComposeboxQueryController::TestComposeboxQueryController(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    version_info::Channel channel)
    : ComposeboxQueryController(identity_manager, url_loader_factory, channel) {
}
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
    NOTREACHED();
  }

  // Create the fake endpoint fetcher to return the fake response.
  EndpointResponse fake_endpoint_response;
  fake_endpoint_response.response = fake_server_response_string;
  fake_endpoint_response.http_status_code = fake_server_response_code;

  auto response = std::make_unique<FakeEndpointFetcher>(fake_endpoint_response);
  response->disable_responding_ = disable_response;
  return response;
}
