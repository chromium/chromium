// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_QUERY_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_QUERY_CONTROLLER_H_

#include <optional>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/lens/core/mojom/lens.mojom.h"
#include "chrome/browser/lens/core/mojom/overlay_object.mojom.h"
#include "chrome/browser/lens/core/mojom/text.mojom.h"
#include "chrome/browser/ui/lens/lens_overlay_gen204_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_request_id_generator.h"
#include "chrome/browser/ui/lens/lens_overlay_url_builder.h"
#include "chrome/browser/ui/lens/ref_counted_lens_overlay_client_logs.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/lens_server_proto/lens_overlay_client_context.pb.h"
#include "third_party/lens_server_proto/lens_overlay_cluster_info.pb.h"
#include "third_party/lens_server_proto/lens_overlay_image_crop.pb.h"
#include "third_party/lens_server_proto/lens_overlay_image_data.pb.h"
#include "third_party/lens_server_proto/lens_overlay_interaction_request_metadata.pb.h"
#include "third_party/lens_server_proto/lens_overlay_selection_type.pb.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"
#include "third_party/lens_server_proto/lens_overlay_service_deps.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

class Profile;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace variations {
class VariationsClient;
}  // namespace variations

namespace lens {

// The possible page content types for a contextual query.
enum class PageContentMimeType {
  kNone = 0,
  kPdf = 1,
  kHtml = 2,
  kPlainText = 3,
};

// Callback type alias for the lens overlay full image response.
using LensOverlayFullImageResponseCallback =
    base::RepeatingCallback<void(std::vector<lens::mojom::OverlayObjectPtr>,
                                 lens::mojom::TextPtr,
                                 bool)>;
// Callback type alias for the lens overlay url response.
using LensOverlayUrlResponseCallback =
    base::RepeatingCallback<void(lens::proto::LensOverlayUrlResponse)>;
// Callback type alias for the lens overlay suggest inputs response.
using LensOverlaySuggestInputsCallback =
    base::RepeatingCallback<void(lens::proto::LensOverlaySuggestInputs)>;
// Callback type alias for the thumbnail image creation.
using LensOverlayThumbnailCreatedCallback =
    base::RepeatingCallback<void(const std::string&)>;
// Callback type alias for the OAuth headers created.
using OAuthHeadersCreatedCallback =
    base::OnceCallback<void(std::vector<std::string>)>;

// Manages queries on behalf of a Lens overlay.
class LensOverlayQueryController {
 public:
  LensOverlayQueryController(
      LensOverlayFullImageResponseCallback full_image_callback,
      LensOverlayUrlResponseCallback url_callback,
      LensOverlaySuggestInputsCallback suggest_inputs_callback,
      LensOverlayThumbnailCreatedCallback thumbnail_created_callback,
      variations::VariationsClient* variations_client,
      signin::IdentityManager* identity_manager,
      Profile* profile,
      lens::LensOverlayInvocationSource invocation_source,
      bool use_dark_mode,
      lens::LensOverlayGen204Controller* gen204_controller);
  virtual ~LensOverlayQueryController();

  // Starts a query flow by sending a request to Lens using the screenshot,
  // returning the response to the full image callback. Should be called
  // exactly once. Override these methods to stub out network requests for
  // testing.
  virtual void StartQueryFlow(
      const SkBitmap& screenshot,
      std::optional<GURL> page_url,
      std::optional<std::string> page_title,
      std::vector<lens::mojom::CenterRotatedBoxPtr> significant_region_boxes,
      base::span<const uint8_t> underlying_content_bytes,
      lens::PageContentMimeType underlying_content_type,
      float ui_scale_factor);

  // Clears the state and resets stored values.
  void EndQuery();

  // Sends a full image request to translate the page.
  virtual void SendFullPageTranslateQuery(const std::string& source_language,
                                          const std::string& target_language);

  // Sends a full image request with no translate options as a result of
  // ending translate mode.
  virtual void SendEndTranslateModeQuery();

  // Sends a region search interaction. Expected to be called multiple times. If
  // region_bytes are included, those will be sent to Lens instead of cropping
  // the region out of the screenshot. This should be used to provide a higher
  // definition image than image cropping would provide.
  virtual void SendRegionSearch(
      lens::mojom::CenterRotatedBoxPtr region,
      lens::LensOverlaySelectionType lens_selection_type,
      std::map<std::string, std::string> additional_search_query_params,
      std::optional<SkBitmap> region_bytes);

  // Sends a text-only interaction. Expected to be called multiple times.
  void SendTextOnlyQuery(
      const std::string& query_text,
      TextOnlyQueryType text_only_query_type,
      std::map<std::string, std::string> additional_search_query_params);

  // Sends a multimodal interaction. Expected to be called multiple times.
  virtual void SendMultimodalRequest(
      lens::mojom::CenterRotatedBoxPtr region,
      const std::string& query_text,
      lens::LensOverlaySelectionType lens_selection_type,
      std::map<std::string, std::string> additional_search_query_params,
      std::optional<SkBitmap> region_bytes);

  // Sends a task completion Gen204 ping for certain user actions.
  virtual void SendTaskCompletionGen204IfEnabled(
      lens::mojom::UserAction user_action);

  // Sends a semantic event Gen204 ping.
  virtual void SendSemanticEventGen204IfEnabled(
      lens::mojom::SemanticEvent event);

  uint64_t gen204_id() const { return gen204_id_; }

 protected:
  // Returns the EndpointFetcher to use with the given params. Protected to
  // allow overriding in tests to mock server responses.
  virtual std::unique_ptr<EndpointFetcher> CreateEndpointFetcher(
      lens::LensOverlayServerRequest* request,
      const GURL& fetch_url,
      const std::string& http_method,
      const std::vector<std::string>& request_headers,
      const std::vector<std::string>& cors_exempt_headers);

  // Sends a latency Gen204 ping if enabled.
  virtual void SendLatencyGen204IfEnabled(int64_t latency_ms,
                                          bool is_translate_query);

  // The callback for full image requests, including upon query flow start
  // and interaction retries.
  LensOverlayFullImageResponseCallback full_image_callback_;

  // Suggest inputs callback, used for sending Lens suggest data to the
  // search box.
  LensOverlaySuggestInputsCallback suggest_inputs_callback_;

  // Callback for when a thumbnail image is created from a region selection.
  LensOverlayThumbnailCreatedCallback thumbnail_created_callback_;

 private:
  enum class QueryControllerState {
    // StartQueryFlow has not been called and the query controller is
    // inactive.
    kOff = 0,
    // StartQueryFlow has been called, but the cluster info has not been
    // received so we cannot proceed to sending the full image request.
    kAwaitingClusterInfoResponse = 1,
    // The cluster info response has been received so we can proceed to sending
    // the full image request.
    kReceivedClusterInfoResponse = 2,
    // The full image response has not been received, or is no longer valid.
    kAwaitingFullImageResponse = 3,
    // The full image response has been received and the query controller can
    // send interaction requests.
    kReceivedFullImageResponse = 4,
    // The full image response has been received and resulted in an error
    // response.
    kReceivedFullImageErrorResponse = 5,
  };

  // Data class for constructing a fetch request to the Lens servers.
  // All fields that are required for the request should use std::unique_ptr to
  // validate all fields are non-null prior to making a request. If a field does
  // not need to be set, but should be included if it is set, use std::optional.
  struct LensServerFetchRequest {
   public:
    LensServerFetchRequest(int sequence_id, base::TimeTicks query_start_time);
    ~LensServerFetchRequest();

    // The sequence ID of the request this data belongs to. Used for cancelling
    // any requests that have been superseded by another.
    const int sequence_id_;

    // The start time of the query.
    const base::TimeTicks query_start_time_;

    // The request ID for this request.
    std::unique_ptr<lens::LensOverlayRequestId> request_id_;

    // The request to be sent to the server. Must be set prior to making the
    // request.
    std::unique_ptr<lens::LensOverlayServerRequest> request_;

    // The headers to attach to the request.
    std::unique_ptr<std::vector<std::string>> request_headers_;

    // A callback to run once the request has been sent. This is optional, but
    // can be used to run some logic once the request has been sent.
    std::optional<base::OnceClosure> request_sent_callback_;
  };

  // Makes a LensOverlayServerClusterInfoRequest to get the cluster info. Will
  // continue to the FullImageRequest once a response is received.
  void FetchClusterInfoRequest();

  // Creates the endpoint fetcher and sends the cluster info request.
  void PerformClusterInfoFetchRequest(std::vector<std::string> request_headers);

  // Handles the response from the cluster info request. If a successful request
  // was made, kicks off the full image request to use the retrieved server
  // session id. If the request failed, the full image request will still be
  // tried, just without the server session id.
  void ClusterInfoFetchResponseHandler(
      std::unique_ptr<EndpointResponse> response);

  // Processes the screenshot and fetches a full image request.
  void PrepareAndFetchFullImageRequest();

  // Does any preprocessing on the image data outside of encoding the
  // screenshot bytes that needs to be done before attaching the ImageData to
  // the full image request.
  void PrepareImageDataForFullImageRequest(
      scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs,
      lens::ImageData image_data);

  // Creates the FullImageRequest that is sent to the server and tries to
  // perform the request. If all async flows have not finished, the attempt to
  // perform the request will be ignored, and the last async flow to finish
  // will perform the request.
  void CreateFullImageRequestAndTryPerformFullImageRequest(
      int sequence_id,
      scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs,
      lens::ImageData image_data);

  // Creates the OAuth headers to be used in the full image request. If the
  // users OAuth is unavailable, will fallback to using the API key. If all
  // async flows have not finished, the attempt to perform the request will be
  // ignored, and the last async flow to finish will perform the request.
  void CreateOAuthHeadersAndTryPerformFullPageRequest(int sequence_id);

  // Called when an asynchronous piece of data needed to make the full image
  // request is ready. Once this has been invoked with every necessary piece of
  // data with the same sequence_id, it will call PerformFullImageRequest to
  // send the request to the server. Ignores any data received from an old
  // sequence_id.
  void FullImageRequestDataReady(int sequence_id,
                                 lens::LensOverlayServerRequest request);
  void FullImageRequestDataReady(int sequence_id,
                                 std::vector<std::string> headers);
  // Helper to the above, used to actually validate the data prior to calling
  // PerformFullImageRequest().
  void FullImageRequestDataHelper(int sequence_id);

  // Verifies the given sequence_id is still the most recent.
  bool IsCurrentFullImageSequence(int sequence_id);

  // Creates the endpoint fetcher and send the full image request to the server.
  void PerformFullImageRequest();

  // Handles the endpoint fetch response for the full image request.
  void FullImageFetchResponseHandler(
      int request_sequence_id,
      std::unique_ptr<EndpointResponse> response);

  // Runs the full image callback with empty response data, for errors.
  void RunFullImageCallbackForError();

  // Creates a full image request with the page content bytes and sends it to
  // the server.
  void PrepareAndFetchPageContentRequest();

  // Performs the page content request. This is a send and forget request, so we
  // are not expecting a response.
  void PerformPageContentRequest(lens::LensOverlayServerRequest request,
                                 std::vector<std::string> headers);

  // Sends the interaction data, triggering async image cropping and fetching
  // the request.
  void SendInteraction(
      lens::mojom::CenterRotatedBoxPtr region,
      std::optional<std::string> query_text,
      std::optional<std::string> object_id,
      lens::LensOverlaySelectionType selection_type,
      std::map<std::string, std::string> additional_search_query_params,
      std::optional<SkBitmap> region_bytes);

  // Creates the interaction request that is sent to the server and tries to
  // perform the interaction request. If not all asynchronous flows have
  // finished, the attempt to perform the request will be ignored. Only the last
  // asynchronous flow to finish will perform the request.
  void CreateInteractionRequestAndTryPerformInteractionRequest(
      int sequence_id,
      lens::mojom::CenterRotatedBoxPtr region,
      std::optional<std::string> query_text,
      std::optional<std::string> object_id,
      scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs,
      std::unique_ptr<lens::LensOverlayRequestId> request_id,
      std::optional<lens::ImageCrop> image_crop);

  // Creates the OAuth headers that get attached to the interaction request to
  // authenticate the user. After, tries to perform the interaction request. If
  // not all asynchronous flows have finished, the attempt to perform the
  // request will be ignored. Only the last asynchronous flow to finish will
  // perform the request.
  void CreateOAuthHeadersAndTryPerformInteractionRequest(int sequence_id);

  // Called when an asynchronous piece of data needed to make the interaction
  // request is ready. Once this has been invoked with every necessary piece of
  // data with the same sequence_id, it will call PerformInteractionRequest to
  // send the request to the server. Ignores any data received from an old
  // sequence_id.
  void InteractionRequestDataReady(int sequence_id,
                                   lens::LensOverlayServerRequest request);
  void InteractionRequestDataReady(int sequence_id,
                                   std::vector<std::string> headers);

  // If all data needed to PerformInteractionRequest is available, will call
  // PerformInteractionRequest to fetch the request. If any async flow has not
  // finished, it will ignore the request with the assumption
  // TryPerformInteractionRequest will be called again once the flow has
  // finished. Will also ensure the full image response has been received. If
  // the full image response has not been received, will kick off the full image
  // response flow with a callback to send this interaction request after.
  void TryPerformInteractionRequest(int sequence_id);

  // Verifies the given sequence_id is still the most recent.
  bool IsCurrentInteractionSequence(int sequence_id);

  // Creates the endpoint fetcher and send the full image request to the server.
  void PerformInteractionRequest();

  // Creates the URL to load in the side panel and sends it to the callback.
  void CreateSearchUrlAndSendToCallback(
      std::optional<std::string> query_text,
      std::map<std::string, std::string> additional_search_query_params,
      lens::LensOverlaySelectionType selection_type,
      std::unique_ptr<lens::LensOverlayRequestId> request_id);

  // Handles the endpoint fetch response for an interaction request.
  void InteractionFetchResponseHandler(
      int sequence_id,
      std::unique_ptr<EndpointResponse> response);

  // Runs the interaction callback with empty response data, for errors.
  void RunInteractionCallbackForError();

  // Creates an endpoint fetcher with the given request_headers to perform the
  // given request. Calls fetcher_created_callback when the EndpointFetcher is
  // created to keep it alive while the request is being made.
  // response_received_callback is invoked once the request returns a response.
  void PerformFetchRequest(
      lens::LensOverlayServerRequest* request,
      std::vector<std::string>* request_headers,
      base::OnceCallback<void(std::unique_ptr<EndpointFetcher>)>
          fetcher_created_callback,
      EndpointFetcherCallback response_received_callback);

  // Creates a client context proto to be attached to a server request.
  lens::LensOverlayClientContext CreateClientContext();

  // Fetches the OAuth headers and calls the callback with the headers. If the
  // OAuth cannot be retrieve (like if the user is not logged in), the callback
  // will be called with an empty vector. Returns the access token fetcher
  // making the request so it can be kept alive.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
  CreateOAuthHeadersAndContinue(OAuthHeadersCreatedCallback callback);

  // Adds the visual search interaction log data param to the search query
  // params.
  std::map<std::string, std::string> AddVisualSearchInteractionLogData(
      std::map<std::string, std::string> additional_search_query_params,
      lens::LensOverlaySelectionType selection_type);

  // Creates the metadata for an interaction request using the latest
  // interaction and image crop data.
  lens::LensOverlayServerRequest CreateInteractionRequest(
      lens::mojom::CenterRotatedBoxPtr region,
      std::optional<std::string> query_text,
      std::optional<std::string> object_id,
      std::optional<lens::ImageCrop> image_crop,
      lens::LensOverlayClientLogs client_logs,
      std::unique_ptr<lens::LensOverlayRequestId> request_id);

  // Resets the request cluster info state.
  void ResetRequestClusterInfoState();

  // Callback for when the full image endpoint fetcher is created.
  void OnFullImageEndpointFetcherCreated(
      std::unique_ptr<EndpointFetcher> endpoint_fetcher);

  // Callback for when the page content endpoint fetcher is created.
  void OnPageContentEndpointFetcherCreated(
      std::unique_ptr<EndpointFetcher> endpoint_fetcher);

  // Callback for when the interaction endpoint fetcher is created.
  void OnInteractionEndpointFetcherCreated(
      std::unique_ptr<EndpointFetcher> endpoint_fetcher);

  // The request id generator.
  std::unique_ptr<lens::LensOverlayRequestIdGenerator> request_id_generator_;

  // The original screenshot image.
  SkBitmap original_screenshot_;

  // The dimensions of the resized bitmap. Needed in case geometry needs to be
  // recaclulated. For example, in the case of translated words.
  gfx::Size resized_bitmap_size_;

  // The page url, if it is allowed to be shared.
  std::optional<GURL> page_url_;

  // The page title, if it is allowed to be shared.
  std::optional<std::string> page_title_;

  // Options needed to send a translate request with the proper parameters.
  struct TranslateOptions {
    std::string source_language;
    std::string target_language;

    TranslateOptions(const std::string& source, const std::string& target)
        : source_language(source), target_language(target) {}
  };
  std::optional<TranslateOptions> translate_options_;

  // Bounding boxes for significant regions identified in the original
  // screenshot image.
  std::vector<lens::mojom::CenterRotatedBoxPtr> significant_region_boxes_;

  // The UI Scaling Factor of the underlying page, if it has been passed in.
  // Else 0.
  float ui_scale_factor_ = 0;

  // The current state.
  QueryControllerState query_controller_state_ = QueryControllerState::kOff;

  // The callback for full image requests, including upon query flow start
  // and interaction retries.
  LensOverlayUrlResponseCallback url_callback_;

  // The last received cluster info.
  std::optional<lens::LensOverlayClusterInfo> cluster_info_ = std::nullopt;

  // The full image response received callback. Will be used to send a queued
  // interaction request if an interaction is received before the full image
  // response has been received.
  base::OnceClosure full_image_response_received_callback_;

  // TODO(b/370805019): All our flows are requesting the same headers, so
  // ideally we use one fetcher that returns the same headers wherever they are
  // needed.
  // The access token fetcher used for getting OAuth for cluster info requests.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      cluster_info_access_token_fetcher_;

  // The access token fetcher used for getting OAuth for full image requests.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      full_image_access_token_fetcher_;

  // The access token fetcher used for getting OAuth for page content requests.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      page_content_access_token_fetcher_;

  // The access token fetcher used for getting OAuth for interaction requests.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      interaction_access_token_fetcher_;

  // The data for the full image request in progress. Is null if no full image
  // request has been made.
  std::unique_ptr<LensServerFetchRequest> latest_full_image_request_data_;

  // The data for the interaction request in progress. Is null if no interaction
  // request has been made.
  std::unique_ptr<LensServerFetchRequest> latest_interaction_request_data_;

  // The endpoint fetcher used for the cluster info request.
  std::unique_ptr<EndpointFetcher> cluster_info_endpoint_fetcher_;

  // The endpoint fetcher used for the full image request.
  std::unique_ptr<EndpointFetcher> full_image_endpoint_fetcher_;

  // The endpoint fetcher used for the page content request.
  std::unique_ptr<EndpointFetcher> page_content_endpoint_fetcher_;

  // The endpoint fetcher used for the interaction request. Only the last
  // endpoint fetcher is kept; additional fetch requests will discard
  // earlier unfinished requests.
  std::unique_ptr<EndpointFetcher> interaction_endpoint_fetcher_;

  // Task runner used to encode/downscale the JPEG images on a separate thread.
  scoped_refptr<base::TaskRunner> encoding_task_runner_;

  // Tracks the encoding/downscaling tasks currently running for follow up
  // interactions. Does not track the encoding for the full image request
  // because it is assumed this request will finish, never need to be
  // cancelled, and all other tasks will wait on it if needed.
  std::unique_ptr<base::CancelableTaskTracker> encoding_task_tracker_;

  // Owned by Profile, and thus guaranteed to outlive this instance.
  const raw_ptr<variations::VariationsClient> variations_client_;

  // Unowned IdentityManager for fetching access tokens. Could be null for
  // incognito profiles.
  const raw_ptr<signin::IdentityManager> identity_manager_;

  const raw_ptr<Profile> profile_;

  // The bytes of the content the user is viewing. Owned by
  // LensOverlayController. Will be empty if no bytes to the underlying page
  // could be provided.
  base::span<const uint8_t> underlying_content_bytes_;

  // The mime type of underlying_content_bytes. Will be kNone if
  // underlying_content_bytes_ is empty.
  lens::PageContentMimeType underlying_content_type_;

  // Whether or not the parent interaction query has been sent. This should
  // always be the first interaction in a query flow.
  bool parent_query_sent_ = false;

  // The invocation source that triggered the query flow.
  lens::LensOverlayInvocationSource invocation_source_;

  // Whether or not to use dark mode in search urls. This is only calculated
  // once per session because the search box theme is also only set once
  // per session.
  bool use_dark_mode_;

  // The controller for sending gen204 pings. Owned and set by the overlay
  // controller. Guaranteed to outlive this class.
  const raw_ptr<lens::LensOverlayGen204Controller> gen204_controller_;

  // The current gen204 id for logging, set on each overlay invocation.
  uint64_t gen204_id_ = 0;

  base::WeakPtrFactory<LensOverlayQueryController> weak_ptr_factory_{this};
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_QUERY_CONTROLLER_H_
