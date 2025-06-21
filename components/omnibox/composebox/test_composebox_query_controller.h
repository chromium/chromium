// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_COMPOSEBOX_TEST_COMPOSEBOX_QUERY_CONTROLLER_H_
#define COMPONENTS_OMNIBOX_COMPOSEBOX_TEST_COMPOSEBOX_QUERY_CONTROLLER_H_

#include <memory>

#include "base/functional/callback.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "composebox_query_controller.h"

class FakeEndpointFetcher : public endpoint_fetcher::EndpointFetcher {
 public:
  explicit FakeEndpointFetcher(endpoint_fetcher::EndpointResponse response);
  void PerformRequest(
      endpoint_fetcher::EndpointFetcherCallback endpoint_fetcher_callback,
      const char* key) override;

  bool disable_responding_ = false;

 private:
  endpoint_fetcher::EndpointResponse response_;
};

// Helper for testing features that use the ComposeboxQueryController.
// The only logic in this class should be for setting up fake network responses
// and tracking sent request data to maximize testing coverage.
class TestComposeboxQueryController : public ComposeboxQueryController {
 public:
  explicit TestComposeboxQueryController(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      version_info::Channel channel);
  ~TestComposeboxQueryController() override;

  // Mutators.
  void set_fake_cluster_info_response(
      lens::LensOverlayServerClusterInfoResponse response) {
    fake_cluster_info_response_ = response;
  }

  void set_next_cluster_info_request_should_return_error(
      bool set_next_cluster_info_request_should_return_error) {
    next_cluster_info_request_should_return_error_ =
        set_next_cluster_info_request_should_return_error;
  }

  void set_on_query_controller_state_changed_callback(
      QueryControllerStateChangedCallback callback) {
    on_query_controller_state_changed_callback_ = std::move(callback);
  }

  // Accessors.
  const int& num_cluster_info_fetch_requests_sent() const {
    return num_cluster_info_fetch_requests_sent_;
  }

  QueryControllerState query_controller_state() const {
    return query_controller_state_;
  }

 protected:
  std::unique_ptr<endpoint_fetcher::EndpointFetcher> CreateEndpointFetcher(
      std::string request_string,
      const GURL& fetch_url,
      endpoint_fetcher::HttpMethod http_method,
      base::TimeDelta timeout,
      const std::vector<std::string>& request_headers,
      const std::vector<std::string>& cors_exempt_headers,
      UploadProgressCallback upload_progress_callback) override;

  // The fake response to return for cluster info requests.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response_;

  // The number of cluster info fetch requests sent by the query controller.
  int num_cluster_info_fetch_requests_sent_ = 0;

  // If true, the next cluster info request will return an error.
  bool next_cluster_info_request_should_return_error_ = false;
};

#endif  // COMPONENTS_OMNIBOX_COMPOSEBOX_TEST_COMPOSEBOX_QUERY_CONTROLLER_H_
