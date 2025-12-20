// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_QUERY_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_QUERY_CONTROLLER_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "chrome/browser/lens/core/mojom/lens.mojom.h"
#include "chrome/browser/lens/core/mojom/overlay_object.mojom.h"
#include "chrome/browser/lens/core/mojom/text.mojom.h"
#include "chrome/browser/ui/lens/lens_overlay_gen204_controller.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/lens/lens_overlay_request_id_generator.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "components/lens/ref_counted_lens_overlay_client_logs.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/lens_server_proto/lens_overlay_client_context.pb.h"
#include "third_party/lens_server_proto/lens_overlay_cluster_info.pb.h"
#include "third_party/lens_server_proto/lens_overlay_selection_type.pb.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"
#include "third_party/lens_server_proto/lens_overlay_service_deps.pb.h"
#include "third_party/lens_server_proto/lens_overlay_visual_search_interaction_data.pb.h"
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

class ImageCrop;
class ImageData;
class LensComposeboxController;
struct ImageCropAndBitmap;

// Data struct representing content data to be sent to the Lens server.
struct PageContent {
  PageContent();
  PageContent(std::vector<uint8_t> bytes, lens::MimeType content_type);
  PageContent(const PageContent& other);
  ~PageContent();

 public:
  std::vector<uint8_t> bytes_;
  lens::MimeType content_type_;
};

// Callback type alias for the lens overlay full image response.
using LensOverlayFullImageResponseCallback =
    base::RepeatingCallback<void(std::vector<lens::mojom::OverlayObjectPtr>,
                                 lens::mojom::TextPtr,
                                 bool)>;
// Callback type alias for the lens overlay url response.
using LensOverlayUrlResponseCallback =
    base::RepeatingCallback<void(lens::proto::LensOverlayUrlResponse)>;
// Callback type alias for the lens overlay interaction response.
using LensOverlayInteractionResponseCallback =
    base::RepeatingCallback<void(lens::mojom::TextPtr)>;
// Callback type alias for the thumbnail image creation.
using LensOverlayThumbnailCreatedCallback =
    base::RepeatingCallback<void(const std::string&, const SkBitmap&)>;
// Callback type alias for the OAuth headers created.
using OAuthHeadersCreatedCallback =
    base::OnceCallback<void(std::vector<std::string>)>;
using UploadProgressCallback =
    base::RepeatingCallback<void(uint64_t position, uint64_t total)>;

// Manages queries on behalf of a Lens overlay.
class LensOverlayQueryController {
 public:
  LensOverlayQueryController(
      LensOverlayFullImageResponseCallback full_image_callback,
      LensOverlayUrlResponseCallback url_callback,
      LensOverlayInteractionResponseCallback interaction_callback,
      LensOverlayThumbnailCreatedCallback thumbnail_created_callback,
      UploadProgressCallback page_content_upload_progress_callback,
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
      GURL page_url,
      std::optional<std::string> page_title,
      std::vector<lens::mojom::CenterRotatedBoxPtr> significant_region_boxes,
      base::span<const PageContent> underlying_page_contents,
      lens::MimeType primary_content_type,
      std::optional<uint32_t> pdf_current_page,
      float ui_scale_factor,
      base::TimeTicks invocation_time);

  // Clears the state and resets stored values.
  void EndQuery();

  // Restarts the query flow if its in a state where no cluster info is
  // available.
  void MaybeRestartQueryFlow();

  // Sends a full image request to translate the page.
  virtual void SendFullPageTranslateQuery(const std::string& source_language,
                                          const std::string& target_language);

  // Sends a full image request with no translate options as a result of
  // ending translate mode.
  virtual void SendEndTranslateModeQuery();

  // Resets the page content data to avoid using stale data in the request flow.
  // Caller should call this before changing the page content data this class
  // points to, to avoid dangling pointers.
  virtual void ResetPageContentData();

  // Sends requests to the server to update the page content and/or make a new
  // full image request.
  // The first content data is the most important and will be used for
  // generating request types. `primary_content_type` is used for generating the
  // request type and vit param.
  virtual void SendUpdatedPageContent(
      std::optional<base::span<const lens::PageContent>>
          underlying_page_contents,
      std::optional<lens::MimeType> primary_content_type,
      std::optional<GURL> new_page_url,
      std::optional<std::string> new_page_title,
      std::optional<uint32_t> pdf_current_page,
      const SkBitmap& screenshot);

  // Sends a request to the server with a portion of the page content.
  // `partial_content` should be a subset of the full page content. This request
  // is used to give the server an early signal of the page content.
  virtual void SendPartialPageContentRequest(
      base::span<const std::u16string> partial_content);

  // Sends a region search interaction. Expected to be called multiple times. If
  // region_bytes are included, those will be sent to Lens instead of cropping
  // the region out of the screenshot. This should be used to provide a higher
  // definition image than image cropping would provide.
  virtual void SendRegionSearch(
      base::Time query_start_time,
      lens::mojom::CenterRotatedBoxPtr region,
      lens::LensOverlaySelectionType lens_selection_type,
      std::map<std::string, std::string> additional_search_query_params,
      std::optional<SkBitmap> region_bytes);

  // Sends a text-only interaction. Expected to be called multiple times.
  virtual void SendTextOnlyQuery(
      base::Time query_start_time,
      const std::string& query_text,
      lens::LensOverlaySelectionType lens_selection_type,
      std::map<std::string, std::string> additional_search_query_params);

  // Sends a text query interaction contextualized to the current page. Expected
  // to be called multiple times.
  virtual void SendContextualTextQuery(
      base::Time query_start_time,
      const std::string& query_text,
      lens::LensOverlaySelectionType lens_selection_type,
      std::map<std::string, std::string> additional_search_query_params);

  // Sends a multimodal interaction. Expected to be called multiple times.
  virtual void SendMultimodalRequest(
      base::Time query_start_time,
      lens::mojom::CenterRotatedBoxPtr region,
      const std::string& query_text,
      lens::LensOverlaySelectionType lens_selection_type,
      std::map<std::string, std::string> additional_search_query_params,
      std::optional<SkBitmap> region_bytes);

  // Sends a task completion Gen204 ping for certain user actions.
  void SendTaskCompletionGen204IfEnabled(lens::mojom::UserAction user_action);

  // Sends a semantic event Gen204 ping.
  virtual void SendSemanticEventGen204IfEnabled(
      lens::mojom::SemanticEvent event);

  bool IsPageContentUploadInProgress() const {
    return page_content_endpoint_fetcher_.get() != nullptr;
  }

  // Returns whether the query controller is off.
  bool IsOff() { return query_controller_state_ == QueryControllerState::kOff; }

  uint64_t gen204_id() const { return gen204_id_; }

  // Returns the search session id for the current query flow.
  std::string search_session_id() const {
    return cluster_info_->search_session_id();
  }

  // Testing method to reset the cluster info state.
  void ResetRequestClusterInfoStateForTesting();

  // Returns a vsrid string for opening in a new tab, to replace the vsrid in
  // an existing query URL. Will not affect the state of this controller's
  // request id generator or analytics id.
  std::string GetVsridForNewTab();

  // Returns the latest visual search interaction data sent to the server.
  // This does not take into account whether the selection was cleared by the
  // user.
  std::optional<lens::LensOverlayVisualSearchInteractionData>
  GetVisualSearchInteractionData() {
    return visual_search_interaction_data_;
  }

  // After this is called, for the remainder of the current session, requests do
  // not need to check prefs for whether the user has granted permissions
  // permanently.
  void GrantPermissionForSession();

  // Returns true if either GrantPermissionForSession() has been called or the
  // user has granted permissions permanently.
  bool HasPermissionForSession();

  base::TimeTicks partial_page_contents_request_start_time_for_testing() const {
    return partial_page_contents_request_start_time_;
  }

  lens::LensOverlayRequestIdGenerator* request_id_generator_for_testing() {
    return request_id_generator_.get();
  }

  const lens::proto::LensOverlaySuggestInputs& GetLensSuggestInputs() const {
    return suggest_inputs_;
  }

  void SetSuggestInputsReadyCallback(base::RepeatingClosure callback) {
    suggest_inputs_ready_callback_ = std::move(callback);
  }

  size_t total_chunk_progress_for_testing() { return total_chunk_progress_; }

  size_t total_chunk_upload_size_for_testing() {
    return total_chunk_upload_size_;
  }

  friend class lens::LensComposeboxController;

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

  // Sends a latency Gen204 ping if enabled, calculating the latency duration
  // from the start time ticks and base::TimeTicks::Now(). The encoded request
  // id is optional because not all latency events have an associated request
  // id, such as the cluster info fetch.
  virtual void SendLatencyGen204IfEnabled(
      lens::LensOverlayGen204Controller::LatencyType latency_type,
      base::TimeTicks start_time_ticks,
      std::string vit_query_param_value,
      std::optional<base::TimeDelta> cluster_info_latency,
      std::optional<std::string> encoded_analytics_id,
      std::optional<lens::LensOverlayRequestId> request_id);

  // Sends a task completion Gen204 ping for certain user actions with
  // the given analytics id. Protected to allow overriding in tests to
  // check the encoded analytics id and request id.
  virtual void SendTaskCompletionGen204IfEnabled(
      std::string encoded_analytics_id,
      lens::mojom::UserAction user_action,
      lens::LensOverlayRequestId request_id);

  // Sends a semantic event Gen204 ping. Protected to allow overriding in tests
  // to check the request id. The request id can be unset for events that
  // do not have an associated request id (e.g. text gleam view end).
  virtual void SendSemanticEventGen204IfEnabled(
      lens::mojom::SemanticEvent event,
      std::optional<lens::LensOverlayRequestId> request_id);

  // Updates the request id based on the given update mode and returns the
  // request id proto. Also updates the suggest signals with the new request id
  // and runs the suggest inputs callback.
  std::unique_ptr<lens::LensOverlayRequestId> GetNextRequestId(
      lens::RequestIdUpdateMode update_mode,
      lens::LensOverlayRequestId::MediaType media_type);

  // Updates the suggest inputs with the feature params and latest cluster info
  // response, then runs the callback. The request id in the suggest inputs will
  // if the parameter is not null.
  virtual void RunSuggestInputsCallback();

  // The callback for full image requests, including upon query flow start
  // and interaction retries.
  LensOverlayFullImageResponseCallback full_image_callback_;

  // The callback for interaction requests, including text received.
  LensOverlayInteractionResponseCallback interaction_response_callback_;

  // Callback for when the suggest inputs are ready.
  base::RepeatingClosure suggest_inputs_ready_callback_;

  // Callback for when a thumbnail image is created from a region selection.
  LensOverlayThumbnailCreatedCallback thumbnail_created_callback_;

  // Callback for when the page content upload progress is updated.
  UploadProgressCallback page_content_upload_progress_callback_;

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
    // The cluster info has expired and a new query flow needs to be started.
    kClusterInfoExpired = 6,
    // Permissions have not yet been granted, and a new query flow needs to be
    // started after they are granted.
    kWaitingForPermissions = 6,
  };

  // Data class for constructing a fetch request to the Lens servers.
  // All fields that are required for the request should use std::unique_ptr to
  // validate all fields are non-null prior to making a request. If a field does
  // not need to be set, but should be included if it is set, use std::optional.
  struct LensServerFetchRequest {
   public:
    LensServerFetchRequest(
        std::unique_ptr<lens::LensOverlayRequestId> request_id,
        base::TimeTicks query_start_time);
    ~LensServerFetchRequest();

    // Returns the sequence ID of the request this data belongs to. Used
    // for cancelling any requests that have been superseded by another.
    int sequence_id() const { return request_id_->sequence_id(); }

    // The request ID for this request.
    const std::unique_ptr<lens::LensOverlayRequestId> request_id_;

    // The start time of the query.
    const base::TimeTicks query_start_time_;

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
  void PerformClusterInfoFetchRequest(base::TimeTicks query_start_time,
                                      std::vector<std::string> request_headers);

  // Handles the response from the cluster info request. If a successful request
  // was made, kicks off the full image request to use the retrieved server
  // session id. If the request failed, the full image request will still be
  // tried, just without the server session id.
  void ClusterInfoFetchResponseHandler(
      base::TimeTicks query_start_time,
      std::unique_ptr<endpoint_fetcher::EndpointResponse> response);

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
      std::unique_ptr<endpoint_fetcher::EndpointResponse> response);

  // Runs the full image callback with empty response data, for errors.
  void RunFullImageCallbackForError();

  // Prepares the data needed for the page content request (aka compression) and
  // then sends the request.
  void PrepareAndFetchPageContentRequest();

  // Creates chunk upload requests for the given chunks.
  void PrepareAndFetchUploadChunkRequests(lens::LensOverlayRequestId request_id,
                                          std::vector<std::string> chunks);

  // Performs the chunk upload requests.
  void PrepareAndFetchUploadChunkRequestsPart2(
      std::vector<std::string> headers);

  // Retries the upload chunks with the given ids.
  void RetryUploadChunkRequests(
      const google::protobuf::RepeatedField<int64_t>& chunk_ids,
      std::vector<std::string> headers);

  // Performs the chunk upload request with the given index.
  void FetchUploadChunkRequest(size_t chunk_request_index);

  // Handles the endpoint fetch response for chunk upload requests. When a
  // response is received for the last chunk, initiates the page content
  // request.
  void UploadChunkResponseHandler(
      lens::LensOverlayRequestId request_id,
      size_t total_chunks,
      std::unique_ptr<endpoint_fetcher::EndpointResponse> response);

  // Creates the PageContentRequest that is sent to the server and performs the
  // request. Prefer to use PrepareAndFetchPageContentRequest() directly since
  // it calls this method after doing the necessary preprocessing. By this
  // point, the preprocessing should be complete and it is expected that the
  // request will be sent. If a check needs to be done before sending the
  // request, it myst be done in PrepareAndFetchPageContentRequest() instead of
  // this method.
  void PrepareAndFetchPageContentRequestPart2(
      lens::LensOverlayRequestId request_id,
      lens::Payload payload);

  // Performs the page content request. This is a send and forget request, so we
  // are not expecting a response.
  void PerformPageContentRequest(lens::LensOverlayServerRequest request,
                                 std::vector<std::string> headers);

  // Handles the endpoint fetch response for the page content request.
  void PageContentResponseHandler(
      lens::LensOverlayRequestId request_id,
      std::unique_ptr<endpoint_fetcher::EndpointResponse> response);

  // Retry the page content upload if necessary. Returns whether or not the
  // page content upload was retried.
  bool MaybeRetryPageContentUpload(
      std::unique_ptr<endpoint_fetcher::EndpointResponse> response);

  // Sends a page content upload latency Gen204 ping if enabled.
  void MaybeSendPageContentUploadLatencyGen204(
      lens::LensOverlayRequestId request_id);

  // Handles the prgress of the page content upload request.
  void PageContentUploadProgressHandler(uint64_t position, uint64_t total);

  // Handles the progress of chunk upload requests by calculating the total
  // progress across all chunks.
  void UploadChunkProgressHandler(size_t chunk_request_index,
                                  uint64_t position,
                                  uint64_t total);

  // Marks that the page content upload is no longer in progress and sends the
  // pending contextual query.
  void PageContentUploadFinished();

  // Creates a full image request with the partial page content bytes and sends
  // it to the server.
  void PrepareAndFetchPartialPageContentRequest();

  // Performs the partial page content request. This is a send and forget
  // request, so we are not expecting to use the response.
  void PerformPartialPageContentRequest(lens::LensOverlayServerRequest request,
                                        std::vector<std::string> headers);

  // Handles the endpoint fetch response for the partial page content request.
  void PartialPageContentResponseHandler(
      lens::LensOverlayRequestId request_id,
      std::unique_ptr<endpoint_fetcher::EndpointResponse> response);

  // Sends the interaction data, triggering async image cropping and fetching
  // the request.
  void SendInteraction(
      base::Time query_start_time,
      lens::mojom::CenterRotatedBoxPtr region,
      std::optional<std::string> query_text,
      std::optional<std::string> object_id,
      lens::LensOverlaySelectionType selection_type,
      std::map<std::string, std::string> additional_search_query_params,
      std::optional<SkBitmap> region_bytes,
      lens::LensOverlayRequestId::MediaType media_type);

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
      std::optional<lens::ImageCropAndBitmap> image_crop_and_bitmap);

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
      base::Time query_start_time,
      std::optional<std::string> query_text,
      std::map<std::string, std::string> additional_search_query_params,
      lens::LensOverlaySelectionType selection_type,
      std::unique_ptr<lens::LensOverlayRequestId> request_id);

  // Handles the endpoint fetch response for an interaction request.
  void InteractionFetchResponseHandler(
      int sequence_id,
      std::unique_ptr<endpoint_fetcher::EndpointResponse> response);

  // Runs the interaction callback with empty response data, for errors.
  void RunInteractionCallbackForError();

  // Sends a full image request latency Gen204 ping if enabled. Also logs the
  // cluster info latency if it is available.
  void SendFullImageLatencyGen204IfEnabled(base::TimeTicks start_time_ticks,
                                           bool is_translate_query,
                                           std::string vit_query_param_value);

  // Logs a latency gen204 for an initial latency gen204, only once per type
  // per query flow, if gen204 logging is enabled.
  void SendInitialLatencyGen204IfNotAlreadySent(
      lens::LensOverlayGen204Controller::LatencyType latency_type,
      std::string vit_query_param_value,
      std::optional<lens::LensOverlayRequestId> request_id);

  // Performs the given server request.
  void PerformFetchRequest(
      lens::LensOverlayServerRequest* request,
      std::vector<std::string>* request_headers,
      base::TimeDelta timeout,
      base::OnceCallback<
          void(std::unique_ptr<endpoint_fetcher::EndpointFetcher>)>
          fetcher_created_callback,
      endpoint_fetcher::EndpointFetcherCallback response_received_callback,
      UploadProgressCallback upload_progress_callback = base::NullCallback());

  // Creates an endpoint fetcher with the given request_headers to perform the
  // given request. Calls fetcher_created_callback when the EndpointFetcher is
  // created to keep it alive while the request is being made.
  // response_received_callback is invoked once the request returns a response.
  void PerformFetchRequest(
      std::string request_string,
      std::vector<std::string>* request_headers,
      base::TimeDelta timeout,
      base::OnceCallback<
          void(std::unique_ptr<endpoint_fetcher::EndpointFetcher>)>
          fetcher_created_callback,
      endpoint_fetcher::EndpointFetcherCallback response_received_callback,
      UploadProgressCallback upload_progress_callback,
      GURL fetch_url);

  // Creates a client context proto to be attached to a server request.
  lens::LensOverlayClientContext CreateClientContext();

  // Fetches the OAuth headers and calls the callback with the headers. If the
  // OAuth cannot be retrieve (like if the user is not logged in), the callback
  // will be called with an empty vector. Returns the access token fetcher
  // making the request so it can be kept alive.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
  CreateOAuthHeadersAndContinue(OAuthHeadersCreatedCallback callback);

  // Builds the LensOverlayVisualSearchInteractionData proto from the given
  // parameters.
  lens::LensOverlayVisualSearchInteractionData
  BuildVisualSearchInteractionLogData(
      const std::optional<std::string>& selected_text,
      lens::LensOverlaySelectionType selection_type);

  // Encodes the LensOverlayVisualSearchInteractionData proto to a base64url
  // encoded string.
  std::string EncodeVisualSearchInteractionLogData(
      const lens::LensOverlayVisualSearchInteractionData& interaction_data);

  // Creates the metadata for an interaction request using the latest
  // interaction and image crop data.
  lens::LensOverlayServerRequest CreateInteractionRequest(
      lens::mojom::CenterRotatedBoxPtr region,
      std::optional<std::string> query_text,
      std::optional<std::string> object_id,
      std::optional<lens::ImageCrop> image_crop,
      lens::LensOverlayClientLogs client_logs);

  // Resets the request cluster info state.
  void ResetRequestClusterInfoState();

  // Callback for when the interaction response returned text that should be
  // passed to the overlay.
  void RunInteractionResponseTextReceivedCallback(lens::mojom::TextPtr text);

  // Callback for when the full image endpoint fetcher is created.
  void OnFullImageEndpointFetcherCreated(
      lens::LensOverlayRequestId request_id,
      std::unique_ptr<endpoint_fetcher::EndpointFetcher> endpoint_fetcher);

  // Callback for when the page content endpoint fetcher is created.
  void OnPageContentEndpointFetcherCreated(
      lens::LensOverlayRequestId request_id,
      std::unique_ptr<endpoint_fetcher::EndpointFetcher> endpoint_fetcher);

  // Callback for when the partial page content endpoint fetcher is created.
  void OnPartialPageContentEndpointFetcherCreated(
      lens::LensOverlayRequestId request_id,
      std::unique_ptr<endpoint_fetcher::EndpointFetcher> endpoint_fetcher);

  // Callback for when the interaction endpoint fetcher is created.
  void OnInteractionEndpointFetcherCreated(
      lens::LensOverlayRequestId request_id,
      std::unique_ptr<endpoint_fetcher::EndpointFetcher> endpoint_fetcher);

  // Callback for when a chunk upload endpoint fetcher is created.
  void OnChunkUploadEndpointFetcherCreated(
      lens::LensOverlayRequestId request_id,
      std::unique_ptr<endpoint_fetcher::EndpointFetcher> endpoint_fetcher);

  // Returns whether or not the contextual search query should be sent now or
  // held until the full page content upload is finished. This is only true if
  // the page content upload is finished and the cluster info is available. If
  // the cluster info is not available, the query should be held until the
  // cluster info becomes available.
  bool ShouldSendContextualSearchQuery();

  // Returns whether the partial page content contains enough text to yield
  // detailed enough results.
  bool IsPartialPageContentSubstantial();

  // The request id generator.
  std::unique_ptr<lens::LensOverlayRequestIdGenerator> request_id_generator_;

  // The original screenshot image.
  SkBitmap original_screenshot_;

  // The dimensions of the resized bitmap. Needed in case geometry needs to be
  // recaclulated. For example, in the case of translated words.
  gfx::Size resized_bitmap_size_;

  // The page url. Empty if it is not allowed to be shared.
  GURL page_url_;

  // The page title, if it is allowed to be shared.
  std::optional<std::string> page_title_;

  // The current page of the PDF document if page_content_type_ is kPdf.
  std::optional<uint32_t> pdf_current_page_;

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

  // The time the query flow was invoked.
  base::TimeTicks invocation_time_;

  // The time the page contents request was started.
  base::TimeTicks page_contents_request_start_time_;

  // The time the partial page contents request was started.
  base::TimeTicks partial_page_contents_request_start_time_;

  // The current state.
  QueryControllerState query_controller_state_ = QueryControllerState::kOff;

  // The callback for full image requests, including upon query flow start
  // and interaction retries.
  LensOverlayUrlResponseCallback url_callback_;

  // The last received cluster info.
  std::optional<lens::LensOverlayClusterInfo> cluster_info_;

  // The callback for issuing a pending interaction request. Will be used to
  // send the interaction request after the cluster info is available and the
  // full image request id sequence is ready, if the interaction occurred
  // before the full image response was received.
  base::OnceClosure pending_interaction_callback_;

  // TODO(b/370805019): All our flows are requesting the same headers, so
  // ideally we use one fetcher that returns the same headers wherever they are
  // needed.
  // The access token fetcher used for getting OAuth for cluster info requests.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      cluster_info_access_token_fetcher_;

  // The access token fetcher used for getting OAuth for full image requests.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      full_image_access_token_fetcher_;

  // The access token fetcher used for getting OAuth for chunk upload requests.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      chunk_upload_access_token_fetcher_;

  // The access token fetcher used for getting OAuth for page content requests.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      page_content_access_token_fetcher_;

  // The access token fetcher used for getting OAuth for partial page content
  // requests.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      partial_page_content_access_token_fetcher_;

  // The access token fetcher used for getting OAuth for interaction requests.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      interaction_access_token_fetcher_;

  // The request id to use for the first batch of requests. The requests sent on
  // the start of the query flow will use this request id. Subsequent requests
  // will use the request id generator.
  std::unique_ptr<lens::LensOverlayRequestId> initial_request_id_;

  // The data for the full image request in progress. Is null if no full image
  // request has been made.
  std::unique_ptr<LensServerFetchRequest> latest_full_image_request_data_;

  // The data for the interaction request in progress. Is null if no interaction
  // request has been made.
  std::unique_ptr<LensServerFetchRequest> latest_interaction_request_data_;

  // The endpoint fetcher used for the cluster info request.
  std::unique_ptr<endpoint_fetcher::EndpointFetcher>
      cluster_info_endpoint_fetcher_;

  // The endpoint fetcher used for the full image request.
  std::unique_ptr<endpoint_fetcher::EndpointFetcher>
      full_image_endpoint_fetcher_;

  // The endpoint fetcher used for the page content request.
  std::unique_ptr<endpoint_fetcher::EndpointFetcher>
      page_content_endpoint_fetcher_;

  // The endpoint fetcher used for the partial page content request.
  std::unique_ptr<endpoint_fetcher::EndpointFetcher>
      partial_page_content_endpoint_fetcher_;

  // The endpoint fetcher used for the interaction request. Only the last
  // endpoint fetcher is kept; additional fetch requests will discard
  // earlier unfinished requests.
  std::unique_ptr<endpoint_fetcher::EndpointFetcher>
      interaction_endpoint_fetcher_;

  // The endpoint fetchers used for the chunk upload requests.
  std::vector<std::unique_ptr<endpoint_fetcher::EndpointFetcher>>
      chunk_upload_endpoint_fetchers_;

  // Task runner used to compress page content bytes on a separate thread.
  scoped_refptr<base::TaskRunner> compression_task_runner_;

  // Tracks the compress tasks currently running for page content requests.
  std::unique_ptr<base::CancelableTaskTracker> compression_task_tracker_;

  // Task runner used to encode/downscale the JPEG images on a separate thread.
  scoped_refptr<base::TaskRunner> encoding_task_runner_;

  // Tracks the encoding/downscaling tasks currently running for follow up
  // interactions. Does not track the encoding for the full image request
  // because it is assumed this request will finish, never need to be
  // cancelled, and all other tasks will wait on it if needed.
  std::unique_ptr<base::CancelableTaskTracker> encoding_task_tracker_;

  // Upload chunk requests being sent.
  std::vector<lens::LensOverlayUploadChunkRequest>
      pending_upload_chunk_requests_;

  // Headers to be sent with each upload chunk request.
  std::vector<std::string> pending_upload_chunk_headers_;

  // Number of upload chunk responses expected. Set to
  // pending_upload_chunk_requests_.size() when starting to send requests, and
  // decremented each time a request receives a response.
  size_t remaining_upload_chunk_responses_;

  // The sequence ID for the upload chunk requests that were last started. Used
  // to verify that the responses received correspond to the latest upload.
  int upload_chunk_sequence_id;

  // A copy of the page content request being sent, in case it needs to be
  // resent.
  lens::LensOverlayServerRequest pending_page_content_request_;

  // Number of times to retry after receiving a missing chunks error. If this
  // happens when the value is zero, proceed without attempting to resend.
  size_t remaining_chunk_retries;

  // The current suggest inputs. The fields in this proto are updated
  // whenever new data is available (i.e. after an objects or interaction
  // response is received) and the overlay controller notified via the
  // suggest inputs callback.
  lens::proto::LensOverlaySuggestInputs suggest_inputs_;

  // The current visual search interaction data. This is used to send the vsint
  // param with the interaction request.
  std::optional<lens::LensOverlayVisualSearchInteractionData>
      visual_search_interaction_data_;

  // Owned by Profile, and thus guaranteed to outlive this instance.
  const raw_ptr<variations::VariationsClient> variations_client_;

  // Unowned IdentityManager for fetching access tokens. Could be null for
  // incognito profiles.
  const raw_ptr<signin::IdentityManager> identity_manager_;

  const raw_ptr<Profile> profile_;

  // The data of the content the user is viewing. Owned by
  // LensOverlayController. Will be empty if no data to the underlying page
  // could be provided.
  base::raw_span<const PageContent> underlying_page_contents_;

  // The primary content type the underlying_page_contents_ is associated with.
  // This MimeType is used for generating the request type and vit param.
  lens::MimeType primary_content_type_;

  // A span of text that represents a part of the content held in underlying
  // content bytes.
  base::raw_span<const std::u16string> partial_content_;

  // Whether or not the parent interaction query has been sent. This should
  // always be the first interaction in a query flow.
  bool parent_query_sent_ = false;

  // Whether or not a page content upload request is in progress.
  bool page_content_request_in_progress_ = false;

  // Whether or not the upload is being chunked.
  bool chunk_upload_in_progress_ = false;

  // True if a page content upload is being retried due to missing chunk errors.
  bool retrying_page_content_upload_ = false;

  // Stores the last reported upload progress position for each chunk, indexed
  // by chunk id.
  std::vector<size_t> chunk_progress;

  // The sum of the last reported upload progress position of each chunk.
  size_t total_chunk_progress_ = 0;

  // Total size of data being uploaded during a chunk upload.
  size_t total_chunk_upload_size_ = 0;

  // Callback for a pending contextual query that is waiting for the page
  // content request to finish uploading.
  base::OnceClosure pending_contextual_query_callback_;

  // Whether or not this is the first page contents request being sent to the
  // server. Used to determine whether or not to generate a new request id for
  // the next request.
  bool is_first_page_contents_request_ = true;

  // Whether or not this is the first partial page contents request being sent
  // to the server. Used to determine whether or not to generate a new request
  // id for the next request.
  bool is_first_partial_page_contents_request_ = true;

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

  // The latest request id. Updated on each full-image and interaction
  // request. This may differ from the request id in the
  // latest_interaction_request_data_ if the user makes a text-only query,
  // because no interaction request is made in that case even though the request
  // id is incremented.
  lens::LensOverlayRequestId latest_request_id_;

  // The latest encoded analytics id. Updated on each full-image and
  // interaction request. This may differ from the analytics id in the
  // latest_interaction_request_data_ if the user makes a text-only query,
  // because no interaction request is made in that case even though the
  // request id is incremented. This is always equal to the analytics id in the
  // latest_request_id_ field.
  std::string latest_encoded_analytics_id_;

  // The time it took from sending the cluster info request to receiving
  // the response.
  std::optional<base::TimeDelta> cluster_info_fetch_response_time_;

  // Latency event gen204 request tracker. Used to determine whether or not to
  // log initial latency metrics for the request. This is only used to track
  // latency events that should only be logged once per query flow.
  base::flat_set<lens::LensOverlayGen204Controller::LatencyType>
      sent_initial_latency_request_events_;

  // True if permission has been granted for the current session, so requests do
  // not need to check prefs for whether the user has granted permissions
  // permanently.
  bool has_permission_for_session_ = false;

  base::WeakPtrFactory<LensOverlayQueryController> weak_ptr_factory_{this};
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_QUERY_CONTROLLER_H_
