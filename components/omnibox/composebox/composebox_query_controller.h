// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_COMPOSEBOX_COMPOSEBOX_QUERY_CONTROLLER_H_
#define COMPONENTS_OMNIBOX_COMPOSEBOX_COMPOSEBOX_QUERY_CONTROLLER_H_

#include "base/time/time.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/lens_server_proto/lens_overlay_client_context.pb.h"
#include "third_party/lens_server_proto/lens_overlay_cluster_info.pb.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"
#include "third_party/lens_server_proto/lens_overlay_surface.pb.h"

enum class SessionState {
  kNone = 0,
  kSessionStarted = 1,
  kSessionAbandoned = 2,
  kSubmittedQuery = 3,
};

enum class QueryControllerState {
  // The initial state, before NotifySessionStarted() is called.
  kOff = 0,
  // The cluster info request is in flight.
  kAwaitingClusterInfoResponse = 1,
  // The cluster info response has been received and is valid.
  kClusterInfoReceived = 2,
  // The cluster info response was not received, or the cluster info has
  // expired.
  kClusterInfoInvalid = 3,
};

namespace version_info {
enum class Channel;
}  // namespace version_info

namespace signin {
class IdentityManager;
}  // namespace signin

// Callback type alias for the OAuth headers created.
using OAuthHeadersCreatedCallback =
    base::OnceCallback<void(std::vector<std::string>)>;
// Callback type alias for the upload progress.
using UploadProgressCallback =
    base::RepeatingCallback<void(uint64_t position, uint64_t total)>;
// Callback for when the query controller state changes.
using QueryControllerStateChangedCallback =
    base::RepeatingCallback<void(QueryControllerState state)>;

class ComposeboxQueryController {
 public:
  ComposeboxQueryController(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      version_info::Channel channel);
  virtual ~ComposeboxQueryController();

  // Session management. Virtual for testing.
  virtual void NotifySessionStarted();
  virtual void NotifySessionAbandoned();

  SessionState session_state() { return session_state_; }

 protected:
  // Returns the EndpointFetcher to use with the given params. Protected to
  // allow overriding in tests to mock server responses.
  virtual std::unique_ptr<endpoint_fetcher::EndpointFetcher>
  CreateEndpointFetcher(std::string request_string,
                        const GURL& fetch_url,
                        endpoint_fetcher::HttpMethod http_method,
                        base::TimeDelta timeout,
                        const std::vector<std::string>& request_headers,
                        const std::vector<std::string>& cors_exempt_headers,
                        UploadProgressCallback upload_progress_callback);

  // The internal state of the query controller. Protected to allow tests to
  // access the state. Do not modify this state directly, use
  // SetQueryControllerState() instead.
  QueryControllerState query_controller_state_ = QueryControllerState::kOff;

  // Callback for when the query controller state changes. Protected to allow
  // tests to set the callback.
  QueryControllerStateChangedCallback
      on_query_controller_state_changed_callback_;

 private:
  // Creates the client context for Lens requests.
  lens::LensOverlayClientContext CreateClientContext();

  // Fetches the OAuth headers and calls the callback with the headers. If the
  // OAuth cannot be retrieved (like if the user is not logged in), the callback
  // will be called with an empty vector. Returns the access token fetcher
  // making the request so it can be kept alive.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
  CreateOAuthHeadersAndContinue(OAuthHeadersCreatedCallback callback);

  // Makes a LensOverlayServerClusterInfoRequest to get the cluster info.
  void FetchClusterInfoRequest();

  // Creates the endpoint fetcher and sends the cluster info request.
  void PerformClusterInfoFetchRequest(std::vector<std::string> request_headers);

  // Handles the response from the cluster info request.
  void ClusterInfoFetchResponseHandler(
      std::unique_ptr<endpoint_fetcher::EndpointResponse> response);

  // Sets the query controller state and notifies the callback if it has
  // changed.
  void SetQueryControllerState(QueryControllerState new_state);

  // The last received cluster info.
  std::optional<lens::LensOverlayClusterInfo> cluster_info_ = std::nullopt;

  // The endpoint fetcher used for the cluster info request.
  std::unique_ptr<endpoint_fetcher::EndpointFetcher>
      cluster_info_endpoint_fetcher_;

  // The access token fetcher used for getting OAuth for the cluster info
  // request. Will be discarded after the OAuth headers are created.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      cluster_info_access_token_fetcher_;

  // Unowned IdentityManager for fetching access tokens. Could be null for
  // incognito profiles.
  const raw_ptr<signin::IdentityManager> identity_manager_;

  // TODO(420701010) Create SessionMetrics struct.
  base::Time session_start_time_;

  // The url loader factory to use for Lens network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The channel to use for Lens network requests.
  version_info::Channel channel_;

  // The session state.
  SessionState session_state_ = SessionState::kNone;

  base::WeakPtrFactory<ComposeboxQueryController> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_OMNIBOX_COMPOSEBOX_COMPOSEBOX_QUERY_CONTROLLER_H_
