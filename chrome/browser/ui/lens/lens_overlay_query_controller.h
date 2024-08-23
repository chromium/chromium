// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_QUERY_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_QUERY_CONTROLLER_H_

#include "base/functional/callback.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/lens/core/mojom/lens.mojom.h"
#include "chrome/browser/lens/core/mojom/overlay_object.mojom.h"
#include "chrome/browser/lens/core/mojom/text.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/lens/lens_overlay_invocation_source.h"
#include "chrome/browser/ui/lens/lens_overlay_request_id_generator.h"
#include "chrome/browser/ui/lens/lens_overlay_url_builder.h"
#include "chrome/browser/ui/lens/ref_counted_lens_overlay_client_logs.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
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

// Callback type alias for the lens overlay full image response.
using LensOverlayFullImageResponseCallback =
    base::RepeatingCallback<void(std::vector<lens::mojom::OverlayObjectPtr>,
                                 lens::mojom::TextPtr,
                                 bool)>;
// Callback type alias for the lens overlay url response.
using LensOverlayUrlResponseCallback =
    base::RepeatingCallback<void(lens::proto::LensOverlayUrlResponse)>;
// Callback type alias for the lens overlay interaction data response.
using LensOverlayInteractionResponseCallback =
    base::RepeatingCallback<void(lens::proto::LensOverlayInteractionResponse)>;
// Callback type alias for the thumbnail image creation.
using LensOverlayThumbnailCreatedCallback =
    base::RepeatingCallback<void(const std::string&)>;
// Manages queries on behalf of a Lens overlay.
class LensOverlayQueryController {
 public:
  explicit LensOverlayQueryController(
      LensOverlayFullImageResponseCallback full_image_callback,
      LensOverlayUrlResponseCallback url_callback,
      LensOverlayInteractionResponseCallback interaction_data_callback,
      LensOverlayThumbnailCreatedCallback thumbnail_created_callback,
      variations::VariationsClient* variations_client,
      signin::IdentityManager* identity_manager,
      Profile* profile,
      lens::LensOverlayInvocationSource invocation_source,
      bool use_dark_mode);
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
      float ui_scale_factor);

  // Clears the state and resets stored values.
  void EndQuery();

  // Sends a full image request to translate the page.
  virtual void SendFullPageTranslateQuery(const std::string& source_language,
                                          const std::string& target_language);

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

 protected:
  // Creates an endpoint fetcher for fetching the request data and fetches
  // the request.
  virtual void CreateAndFetchEndpointFetcher(
      lens::LensOverlayServerRequest request_data,
      base::OnceCallback<void(std::unique_ptr<EndpointFetcher>)>
          fetcher_created_callback,
      EndpointFetcherCallback fetched_response_callback);

  // Sends a latency Gen204 ping if enabled.
  virtual void SendLatencyGen204IfEnabled(int64_t latency_ms);

  // The callback for full image requests, including upon query flow start
  // and interaction retries.
  LensOverlayFullImageResponseCallback full_image_callback_;

  // Interaction data callback for an interaction.
  LensOverlayInteractionResponseCallback interaction_data_callback_;

  // Callback for when a thumbnail image is created from a region selection.
  LensOverlayThumbnailCreatedCallback thumbnail_created_callback_;

 private:
  enum class QueryControllerState {
    // StartQueryFlow has not been called and the query controller is
    // inactive.
    kOff = 0,
    // The full image response has not been received, or is no longer valid.
    kAwaitingFullImageResponse = 1,
    // The full image response has been received and the query controller can
    // send interaction requests.
    kReceivedFullImageResponse = 2,
    // The full image response has been received and resulted in an error
    // response.
    kReceivedFullImageErrorResponse = 3,
  };

  // Processes the screenshot and fetches a full image request.
  void PrepareAndFetchFullImageRequest();

  // Continues with fetching the full image request after the screenshot has
  // been encoded.
  void OnImageDataReady(
      scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs,
      lens::ImageData image_data);

  // Creates a client context proto to be attached to a server request.
  lens::LensOverlayClientContext CreateClientContext();

  // Adds the visual search interaction log data param to the search query
  // params.
  std::map<std::string, std::string> AddVisualSearchInteractionLogData(
      std::map<std::string, std::string> additional_search_query_params,
      lens::LensOverlaySelectionType selection_type);

  // Sends the interaction data, triggering async image cropping and fetching
  // the request.
  void SendInteraction(
      lens::mojom::CenterRotatedBoxPtr region,
      std::optional<std::string> query_text,
      std::optional<std::string> object_id,
      lens::LensOverlaySelectionType selection_type,
      std::map<std::string, std::string> additional_search_query_params,
      std::optional<SkBitmap> region_bytes);

  // Continues with SendInteraction after the full image cropping is finished.
  void OnImageCropReady(
      int request_index,
      lens::mojom::CenterRotatedBoxPtr region,
      std::optional<std::string> query_text,
      std::optional<std::string> object_id,
      lens::LensOverlaySelectionType selection_type,
      std::map<std::string, std::string> additional_search_query_params,
      scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs,
      std::optional<lens::ImageCrop> image_crop);

  // Fetches the endpoint using the initial image data.
  void FetchFullImageRequest(
      std::unique_ptr<lens::LensOverlayRequestId> request_id,
      lens::ImageData image_data,
      lens::LensOverlayClientLogs client_logs);

  // Handles the endpoint fetch response for the initial request.
  void FullImageFetchResponseHandler(
      int64_t query_start_time_ms,
      std::unique_ptr<EndpointResponse> response);

  // Handles the response from a latency gen204 request.
  void OnLatencyGen204LoaderComplete(
      std::unique_ptr<std::string> response_body);

  // Handles the response from a task completion gen204 request.
  void OnTaskCompletionGen204LoaderComplete(
      std::unique_ptr<std::string> response_body);

  // Runs the full image callback with empty response data, for errors.
  void RunFullImageCallbackForError();

  // Handles the endpoint fetch response for an interaction request.
  void InteractionFetchResponseHandler(
      std::unique_ptr<EndpointResponse> response);

  // Runs the interaction callback with empty response data, for errors.
  void RunInteractionCallbackForError();

  // Helper to gate interaction fetches on whether or not the cluster
  // info has been received. If it has not been received, this function
  // sets the cluster info received callback to fetch the interaction.
  // Additionally, invokes `thumbnail_created_callback_` and passes the data
  // in `image_crop`.
  void FetchInteractionRequestAndGenerateUrlIfClusterInfoReady(
      int request_index,
      lens::mojom::CenterRotatedBoxPtr region,
      std::optional<std::string> query_text,
      std::optional<std::string> object_id,
      lens::LensOverlaySelectionType selection_type,
      std::map<std::string, std::string> additional_search_query_params,
      std::optional<lens::ImageCrop> image_crop,
      lens::LensOverlayClientLogs client_logs);

  // Fetches the endpoint for an interaction request and creates a Lens search
  // url if the request is the most recent request.
  void FetchInteractionRequestAndGenerateLensSearchUrl(
      int request_index,
      lens::mojom::CenterRotatedBoxPtr region,
      std::optional<std::string> query_text,
      std::optional<std::string> object_id,
      lens::LensOverlaySelectionType selection_type,
      std::map<std::string, std::string> additional_search_query_params,
      std::optional<lens::ImageCrop> image_crop,
      lens::LensOverlayClientLogs client_logs,
      lens::LensOverlayClusterInfo cluster_info);

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

  // Callback for when the interaction endpoint fetcher is created.
  void OnInteractionEndpointFetcherCreated(
      std::unique_ptr<EndpointFetcher> endpoint_fetcher);

  // Creates an endpoint fetcher using the received auth token data.
  void FetchEndpoint(lens::LensOverlayServerRequest request_data,
                     base::OnceCallback<void(std::unique_ptr<EndpointFetcher>)>
                         fetcher_created_callback,
                     EndpointFetcherCallback fetched_response_callback,
                     std::vector<std::string> headers);

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

  // The cluster info received callback. Will be used to send a queued
  // interaction request if an interaction is received before the initial
  // request receives the cluster info.
  base::OnceCallback<void(lens::LensOverlayClusterInfo)>
      cluster_info_received_callback_;

  // The access token fetcher used for OAuth requests.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  // The endpoint fetcher used for the full image request.
  std::unique_ptr<EndpointFetcher> full_image_endpoint_fetcher_;

  // The endpoint fetcher used for the interaction request. Only the last
  // endpoint fetcher is kept; additional fetch requests will discard
  // earlier unfinished requests.
  std::unique_ptr<EndpointFetcher> interaction_endpoint_fetcher_;

  // Loader used for latency gen204 requests.
  std::unique_ptr<network::SimpleURLLoader> latency_gen204_loader_;

  // Loader used for task completion gen204 requests.
  std::unique_ptr<network::SimpleURLLoader> task_completion_gen204_loader_;

  // Task runner used to encode/downscale the JPEG images on a separate thread.
  scoped_refptr<base::TaskRunner> encoding_task_runner_;

  // Tracks the encoding/downscaling tasks currently running for follow up
  // interactions. Does not track the encoding for the full image request
  // because it is assumed this request will finish, never need to be
  // cancelled, and all other tasks will wait on it if needed.
  std::unique_ptr<base::CancelableTaskTracker> encoding_task_tracker_;

  // Owned by Profile, and thus guaranteed to outlive this instance.
  raw_ptr<variations::VariationsClient> variations_client_;

  // Unowned IdentityManager for fetching access tokens. Could be null for
  // incognito profiles.
  raw_ptr<signin::IdentityManager> identity_manager_;

  raw_ptr<Profile> profile_;

  // The request counter, used to make sure requests are not sent out of
  // order.
  int request_counter_ = 0;

  // Whether or not the parent interaction query has been sent. This should
  // always be the first interaction in a query flow.
  bool parent_query_sent_ = false;

  // The invocation source that triggered the query flow.
  lens::LensOverlayInvocationSource invocation_source_;

  // Whether or not to use dark mode in search urls. This is only calculated
  // once per session because the search box theme is also only set once
  // per session.
  bool use_dark_mode_;

  // The current gen204 id for logging, set on each overlay invocation.
  uint64_t gen204_id_;

  base::WeakPtrFactory<LensOverlayQueryController> weak_ptr_factory_{this};
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_QUERY_CONTROLLER_H_
