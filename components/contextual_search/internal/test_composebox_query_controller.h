// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_INTERNAL_TEST_COMPOSEBOX_QUERY_CONTROLLER_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_INTERNAL_TEST_COMPOSEBOX_QUERY_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "components/contextual_search/internal/composebox_query_controller.h"
#include "components/variations/variations_client.h"

namespace base {
class TimeDelta;
}  // namespace base

class GURL;

namespace endpoint_fetcher {
class EndpointFetcher;
enum class HttpMethod;
struct EndpointResponse;
}  // namespace endpoint_fetcher

namespace lens {
class LensOverlayClientContext;
class LensOverlayServerClusterInfoResponse;
}  // namespace lens

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

// Fake VariationsClient for testing.
class FakeVariationsClient : public variations::VariationsClient {
 public:
  ~FakeVariationsClient() override = default;

  bool IsOffTheRecord() const override;

  variations::mojom::VariationsHeadersPtr GetVariationsHeaders() const override;
};

// Callback for when an endpoint fetcher is created.
using EndpointFetcherCreatedCallback =
    base::RepeatingCallback<void()>;

namespace contextual_search {

// Helper for testing features that use the ComposeboxQueryController.
// The only logic in this class should be for setting up fake network responses
// and tracking sent request data to maximize testing coverage.
class TestComposeboxQueryController : public ComposeboxQueryController {
 public:
  explicit TestComposeboxQueryController(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      version_info::Channel channel,
      std::string locale,
      TemplateURLService* template_url_service,
      variations::VariationsClient* variations_client,
      std::unique_ptr<ContextualSearchContextController::ConfigParams>
          config_params,
      bool enable_cluster_info_ttl);
  ~TestComposeboxQueryController() override;

  // Mutators.
  void set_fake_cluster_info_response(
      lens::LensOverlayServerClusterInfoResponse response) {
    fake_cluster_info_response_ = response;
  }

  void set_fake_file_upload_response(lens::LensOverlayServerResponse response) {
    fake_file_upload_response_ = response;
  }

  void set_next_cluster_info_request_should_return_error(
      bool set_next_cluster_info_request_should_return_error) {
    next_cluster_info_request_should_return_error_ =
        set_next_cluster_info_request_should_return_error;
  }

  void set_next_file_upload_request_should_return_error(
      bool set_next_file_upload_request_should_return_error) {
    next_file_upload_request_should_return_error_ =
        set_next_file_upload_request_should_return_error;
  }

  void set_enable_cluster_info_ttl(bool enable_cluster_info_ttl) {
    enable_cluster_info_ttl_ = enable_cluster_info_ttl;
  }

  void set_on_query_controller_state_changed_callback(
      base::RepeatingCallback<void(QueryControllerState state)> callback) {
    on_query_controller_state_changed_callback_ = std::move(callback);
  }

  // Accessors.
  const int& num_cluster_info_fetch_requests_sent() const {
    return num_cluster_info_fetch_requests_sent_;
  }

  const int& num_file_upload_requests_sent() const {
    return num_file_upload_requests_sent_;
  }

  QueryControllerState query_controller_state() const {
    return query_controller_state_;
  }

  const GURL& last_sent_fetch_url() const { return last_sent_fetch_url_; }

  // Gets the sent upload request with the index from the end, e.g.
  // recent_sent_upload_request(0) will return the last sent upload request.
  std::optional<lens::LensOverlayServerRequest> recent_sent_upload_request(
      size_t index_from_end) const {
    size_t index = sent_upload_requests_.size() - index_from_end - 1;
    if (index < 0 || index >= sent_upload_requests_.size()) {
      return std::nullopt;
    }
    return std::make_optional(sent_upload_requests_[index]);
  }

  // Gets the sent interaction request with the index from the end, e.g.
  // recent_sent_interaction_request(0) will return the last sent interaction
  // request.
  std::optional<lens::LensOverlayServerRequest> recent_sent_interaction_request(
      size_t index_from_end) const {
    size_t index = sent_interaction_requests_.size() - index_from_end - 1;
    if (index < 0 || index >= sent_interaction_requests_.size()) {
      return std::nullopt;
    }
    return std::make_optional(sent_interaction_requests_[index]);
  }

  // Gets the last sent upload request.
  std::optional<lens::LensOverlayServerRequest> last_sent_file_upload_request()
      const {
    return recent_sent_upload_request(0);
  }

  // Gets the last sent interaction request.
  std::optional<lens::LensOverlayServerRequest> last_sent_interaction_request()
      const {
    return recent_sent_interaction_request(0);
  }

  // Gets the last sent cors exempt headers.
  std::vector<std::string> last_sent_cors_exempt_headers() const {
    return last_sent_cors_exempt_headers_;
  }

  // Gets the client context used for the requests.
  lens::LensOverlayClientContext client_context() const {
    return ComposeboxQueryController::CreateClientContext();
  }

  // Gets the FileInfo with type cast for testing.
  const ComposeboxQueryController::FileInfo* GetFileInfoForTesting(
      const base::UnguessableToken& file_token) {
    return static_cast<const ComposeboxQueryController::FileInfo*>(
        ComposeboxQueryController::GetFileInfo(file_token));
  }

  // Adds a callback to be called when an endpoint fetcher is created.
  void AddEndpointFetcherCreatedCallback(
      EndpointFetcherCreatedCallback callback) {
    on_endpoint_fetcher_created_callbacks_.push_back(std::move(callback));
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

  void ResetRequestClusterInfoState() override;

  // The fake response to return for cluster info requests.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response_;

  // The fake response to return for file upload requests.
  lens::LensOverlayServerResponse fake_file_upload_response_;

  // The number of cluster info fetch requests sent by the query controller.
  int num_cluster_info_fetch_requests_sent_ = 0;

  // The number of file upload requests sent by the query controller.
  int num_file_upload_requests_sent_ = 0;

  // If true, the next cluster info request will return an error.
  bool next_cluster_info_request_should_return_error_ = false;

  // If true, the next file upload request will return an error.
  bool next_file_upload_request_should_return_error_ = false;

  // If true, the cluster info will expire when the TTL expires as normal.
  // Set to false by default to prevent flakiness in tests that expect the
  // cluster info to be available.
  bool enable_cluster_info_ttl_;

  // The last url for which a fetch request was sent by the query controller.
  GURL last_sent_fetch_url_;

  // The sent upload requests.
  std::vector<lens::LensOverlayServerRequest> sent_upload_requests_;

  // The sent interaction requests.
  std::vector<lens::LensOverlayServerRequest> sent_interaction_requests_;

  // The endpoint fetcher created callbacks.
  std::vector<EndpointFetcherCreatedCallback>
      on_endpoint_fetcher_created_callbacks_;

  // The last sent cors exempt headers.
  std::vector<std::string> last_sent_cors_exempt_headers_;
};

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_INTERNAL_TEST_COMPOSEBOX_QUERY_CONTROLLER_H_
