// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_INTERNAL_COMPOSEBOX_QUERY_CONTROLLER_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_INTERNAL_COMPOSEBOX_QUERY_CONTROLLER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/lens/lens_overlay_request_id_generator.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
#include "third_party/lens_server_proto/lens_overlay_cluster_info.pb.h"
#include "third_party/lens_server_proto/lens_overlay_request_id.pb.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"

namespace base {
class TaskRunner;
}  // namespace base

class TemplateURLService;

namespace lens {
enum class RequestIdUpdateMode;
class ImageData;
class LensOverlayClientContext;
}  // namespace lens

namespace signin {
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

namespace variations {
class VariationsClient;
}

namespace version_info {
enum class Channel;
}  // namespace version_info

class SkBitmap;

// Callback type alias for the file upload request body proto created.
using RequestBodyProtoCreatedCallback = base::OnceCallback<void(
    lens::LensOverlayServerRequest,
    std::optional<contextual_search::FileUploadErrorType>)>;

// Callback type alias for the interaction request body proto created.
using InteractionRequestBodyProtoCreatedCallback =
    base::OnceCallback<void(lens::LensOverlayServerRequest)>;

// TODO(crbug.com/449970296): Rename this class.
class ComposeboxQueryController
    : public contextual_search::ContextualSearchContextController {
 public:
  ComposeboxQueryController(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      version_info::Channel channel,
      std::string locale,
      TemplateURLService* template_url_service,
      variations::VariationsClient* variations_client,
      std::unique_ptr<
          contextual_search::ContextualSearchContextController::ConfigParams>
          config_params);
  ~ComposeboxQueryController() override;

  // ContextualSearchContextController:
  void InitializeIfNeeded() override;
  void CreateSearchUrl(
      std::unique_ptr<CreateSearchUrlRequestInfo> search_url_request_info,
      base::OnceCallback<void(GURL)> callback) override;
  lens::ClientToAimMessage CreateClientToAimRequest(
      std::unique_ptr<CreateClientToAimRequestInfo>
          create_client_to_aim_request_info) override;
  void AddObserver(FileUploadStatusObserver* obs) override;
  void RemoveObserver(FileUploadStatusObserver* obs) override;
  void StartFileUploadFlow(
      const base::UnguessableToken& file_token,
      std::unique_ptr<lens::ContextualInputData> contextual_input_data,
      std::optional<lens::ImageEncodingOptions> image_options) override;
  bool DeleteFile(const base::UnguessableToken& file_token) override;
  void ClearFiles() override;
  std::unique_ptr<lens::proto::LensOverlaySuggestInputs> CreateSuggestInputs(
      const std::vector<base::UnguessableToken>& attached_context_tokens)
      override;
  const contextual_search::FileInfo* GetFileInfo(
      const base::UnguessableToken& file_token) override;
  std::vector<const contextual_search::FileInfo*> GetFileInfoList() override;
  base::WeakPtr<ContextualSearchContextController> AsWeakPtr() override;

  // Returns a request id to use for the viewport image upload request for the
  // given file info, setting the viewport request id on the file info if it is
  // different from the request id.
  virtual lens::LensOverlayRequestId GetRequestIdForViewportImage(
      const base::UnguessableToken& file_token);

  // Enum for testing to track the state of the query controller.
  enum class QueryControllerState {
    // The initial state, before InitializeIfNeeded() is called.
    kOff = 0,
    // The cluster info request is in flight.
    kAwaitingClusterInfoResponse = 1,
    // The cluster info response has been received and is valid.
    kClusterInfoReceived = 2,
    // The cluster info response was not received, or the cluster info has
    // expired.
    kClusterInfoInvalid = 3,
  };

 protected:
  // Struct containing information about an individual network request.
  // TODO(crbug.com/441351005): Make this struct private and rename it.
  struct UploadRequest {
   public:
    UploadRequest();
    ~UploadRequest();

    // The time the request was sent.
    base::Time start_time;
    // The time the response was received.
    base::Time response_time;
    // The response code of the request. 0 if the response has not been
    // received.
    int response_code = 0;
    // The request body to be sent to the server. Will be set asynchronously
    // after StartFileUploadFlow() is called.
    std::unique_ptr<lens::LensOverlayServerRequest> request_body;
    // The endpoint fetcher used for the request.
    std::unique_ptr<endpoint_fetcher::EndpointFetcher> endpoint_fetcher_;
  };

  // Struct containing file information for a file upload.
  // TODO(crbug.com/441351005): Make this struct private and rename it.
  struct FileInfo : public contextual_search::FileInfo {
   public:
    FileInfo();
    ~FileInfo() override;

    // Gets a pointer to the request ID for this request for testing.
    lens::LensOverlayRequestId GetRequestIdForTesting() const {
      return request_id;
    }

    // Gets a pointer to the viewport request ID for this request for testing.
    lens::LensOverlayRequestId* GetViewportRequestIdForTesting() const {
      return viewport_request_id_.get();
    }

   private:
    friend class ComposeboxQueryController;
    friend class ComposeboxQueryControllerIOS;

    // The request ID for the viewport associated with this request, if it is
    // different from the request ID. Set by StartFileUploadFlow() when
    // use_separate_request_ids_for_multi_context_viewport_images_ is true.
    std::unique_ptr<lens::LensOverlayRequestId> viewport_request_id_;

    // The headers to attach to the request. Will be set asynchronously after
    // StartFileUploadFlow() is called.
    std::unique_ptr<std::vector<std::string>> request_headers_;

    std::vector<uint8_t> file_content;

    // The access token fetcher used for getting OAuth for the file upload
    // request. Will be discarded after the OAuth headers are created.
    std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
        file_upload_access_token_fetcher_;

    // The upload requests.
    std::vector<std::unique_ptr<UploadRequest>> upload_requests_;

    // The number of outstanding network requests. This is set in
    // StartFileUploadFlow() and decremented when successful network responses
    // are received.
    size_t num_outstanding_network_requests_ = 0;
  };

  // Creates the request body proto for an image and calls the callback with the
  // request.
  static void CreateFileUploadRequestProtoWithImageDataAndContinue(
      lens::LensOverlayRequestId request_id,
      lens::LensOverlayClientContext client_context,
      scoped_refptr<lens::RefCountedLensOverlayClientLogs> client_logs,
      RequestBodyProtoCreatedCallback callback,
      lens::ImageData image_data);

  // Creates the request body proto for an image and calls the callback with the
  // request.
  virtual void CreateImageUploadRequest(
      lens::LensOverlayRequestId request_id,
      const std::vector<uint8_t>& image_data,
      std::optional<lens::ImageEncodingOptions> options,
      RequestBodyProtoCreatedCallback callback);

  // Returns the EndpointFetcher to use with the given params. Protected to
  // allow overriding in tests to mock server responses.
  using UploadProgressCallback =
      base::RepeatingCallback<void(uint64_t position, uint64_t total)>;
  virtual std::unique_ptr<endpoint_fetcher::EndpointFetcher>
  CreateEndpointFetcher(std::string request_string,
                        const GURL& fetch_url,
                        endpoint_fetcher::HttpMethod http_method,
                        base::TimeDelta timeout,
                        const std::vector<std::string>& request_headers,
                        const std::vector<std::string>& cors_exempt_headers,
                        UploadProgressCallback upload_progress_callback);

  // Creates the client context for Lens requests. Protected to allow access
  // from tests.
  lens::LensOverlayClientContext CreateClientContext() const;

  // Clears all cluster info.
  void ClearClusterInfo();

  // Resets the request cluster info state. Protected to allow tests to
  // override.
  virtual void ResetRequestClusterInfoState();

  // Sends an interaction request. Protected to allow tests
  // to override.
  virtual void SendInteractionRequest(
      std::unique_ptr<lens::LensOverlayRequestId> request_id,
      std::string query_text,
      std::optional<lens::ImageCrop> image_crop,
      std::optional<lens::LensOverlayClientLogs> client_logs,
      std::optional<lens::LensOverlaySelectionType> lens_overlay_selection_type,
      base::OnceCallback<void(lens::LensOverlayInteractionResponse)>
          interaction_response_callback);

  // The internal state of the query controller. Protected to allow tests to
  // access the state. Do not modify this state directly, use
  // SetQueryControllerState() instead.
  QueryControllerState query_controller_state_ = QueryControllerState::kOff;

  // Callback for when the query controller state changes. Protected to allow
  // tests to set the callback.
  base::RepeatingCallback<void(QueryControllerState state)>
      on_query_controller_state_changed_callback_;

  // The map of active files, keyed by the file token.
  // Protected to allow tests to access the files.
  // TODO(crbug.com/461865548): Determine if the query controller needs to send
  // the active files from previously committed turns to the server for
  // follow-up turns.
  std::map<base::UnguessableToken, std::unique_ptr<FileInfo>> active_files_;

  // Task runner used to create the file upload request proto asynchronously.
  scoped_refptr<base::TaskRunner> create_request_task_runner_;

 private:
  // Data class for constructing an interaction request to the Lens server.
  struct LensServerInteractionRequest {
   public:
    explicit LensServerInteractionRequest(
        std::unique_ptr<lens::LensOverlayRequestId> request_id);
    ~LensServerInteractionRequest();

    // Returns the sequence ID of the request this data belongs to. Used
    // for cancelling any requests that have been superseded by another.
    int sequence_id() const { return request_id_->sequence_id(); }

    // The request ID for this request.
    const std::unique_ptr<lens::LensOverlayRequestId> request_id_;

    // The access token fetcher used for getting OAuth for the interaction
    // upload request. Will be discarded after the OAuth headers are created.
    std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
        interaction_access_token_fetcher_;

    // The endpoint fetcher used for the interaction request.
    std::unique_ptr<endpoint_fetcher::EndpointFetcher>
        interaction_endpoint_fetcher_;

    // The request to be sent to the server. Must be set prior to making the
    // request.
    std::unique_ptr<lens::LensOverlayServerRequest> request_;

    // The headers to attach to the request.
    std::unique_ptr<std::vector<std::string>> request_headers_;

    // A callback to run once the request has been sent. This is optional, but
    // can be used to run some logic once the request has been sent.
    std::optional<base::OnceClosure> request_sent_callback_;

    // The callback to run when the interaction response is received.
    base::OnceCallback<void(lens::LensOverlayInteractionResponse)>
        interaction_response_callback_;

    // Whether or not the request has been sent.
    bool request_sent_ = false;

    // Whether or not the interaction request details have been attached to
    // a vsint param in a search query url / postmessage. This should only
    // occur once per interaction request.
    bool interaction_details_used_in_vsint_ = false;
  };

  // Returns a mutable pointer to allow internal modifications.
  FileInfo* GetMutableFileInfo(const base::UnguessableToken& file_token);

  // Fetches the OAuth headers and calls the callback with the headers. If the
  // OAuth cannot be retrieved (like if the user is not logged in), the callback
  // will be called with an empty vector. Returns the access token fetcher
  // making the request so it can be kept alive.
  using OAuthHeadersCreatedCallback =
      base::OnceCallback<void(std::vector<std::string>)>;
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
  CreateOAuthHeadersAndContinue(OAuthHeadersCreatedCallback callback);

  // Gets an OAuth token for the cluster info request and proceeds with sending
  // a LensOverlayServerClusterInfoRequest to get the cluster info.
  void FetchClusterInfo();

  // Asynchronous handler for when the fetch cluster info request headers are
  // ready. Creates the endpoint fetcher and sends the cluster info request.
  void SendClusterInfoNetworkRequest(std::vector<std::string> request_headers);

  // Handles the response from the cluster info request.
  void HandleClusterInfoResponse(
      std::unique_ptr<endpoint_fetcher::EndpointResponse> response);

  // Sets the query controller state and notifies the query controller state
  // changed callback if it has changed.
  void SetQueryControllerState(QueryControllerState new_state);

  // Updates the file upload status and notifies the file upload status
  // observers with an optional error type if the upload failed.
  void UpdateFileUploadStatus(
      const base::UnguessableToken& file_token,
      contextual_search::FileUploadStatus status,
      std::optional<contextual_search::FileUploadErrorType> error_type);

  // Handler for when the image from an image file upload is decoded. Creates
  // the request body proto and calls the callback with the request.
  void ProcessDecodedImageAndContinue(lens::LensOverlayRequestId request_id,
                                      const lens::ImageEncodingOptions& options,
                                      RequestBodyProtoCreatedCallback callback,
                                      const SkBitmap& bitmap);

  // Creates the request body protos for the file and viewport upload requests
  // and calls the callbacks with the request.
  void CreateUploadRequestBodiesAndContinue(
      const base::UnguessableToken& file_token,
      std::unique_ptr<lens::ContextualInputData> contextual_input_data,
      std::optional<lens::ImageEncodingOptions> options);

  // Callback that takes the image request body proto and adds the pdf page
  // index to it.
  void AddPageIndexToImageUploadRequestAndContinue(
      std::optional<size_t> pdf_page_index,
      RequestBodyProtoCreatedCallback callback,
      lens::LensOverlayServerRequest request,
      std::optional<contextual_search::FileUploadErrorType> error_type);

  // Callback that takes the request body proto and adds the
  // has_lens_usage_intent bool to it.
  void AddLensUsageIntentToUploadRequestAndContinue(
      bool has_lens_usage_intent,
      RequestBodyProtoCreatedCallback callback,
      lens::LensOverlayServerRequest request,
      std::optional<contextual_search::FileUploadErrorType> error_type);

  // Asynchronous handler for when an upload request body is ready.
  void OnUploadRequestBodyReady(
      const base::UnguessableToken& file_token,
      size_t request_index,
      lens::LensOverlayServerRequest request,
      std::optional<contextual_search::FileUploadErrorType> error_type);

  // Asynchronous handler for when the request headers for uploading file and
  // viewport data are ready.
  void OnUploadRequestHeadersReady(const base::UnguessableToken& file_token,
                                   std::vector<std::string> headers);

  // Sends the upload request if the request body, headers, and cluster
  // info are ready.
  void MaybeSendUploadNetworkRequest(const base::UnguessableToken& file_token,
                                     size_t request_index);

  // Creates the endpoint fetcher and sends the upload network request.
  void SendUploadNetworkRequest(FileInfo* file_info, size_t request_index);

  // Asynchronous handler for when the request headers for the interaction
  // request are ready.
  void OnInteractionRequestHeadersReady(std::vector<std::string> headers);

  // Sends pending the interaction request if the request body, headers, and
  // cluster info are ready.
  void TrySendInteractionRequest();

  // The callback for when the interaction request endpoint fetcher is created.
  void OnInteractionEndpointFetcherCreated(
      std::unique_ptr<endpoint_fetcher::EndpointFetcher> endpoint_fetcher);

  // Handles the response from the interaction request.
  void HandleInteractionResponse(
      std::unique_ptr<endpoint_fetcher::EndpointResponse> response);

  // Callback for when an upload endpoint fetcher is created, storing it
  // updating the file info state.
  void OnUploadEndpointFetcherCreated(
      const base::UnguessableToken& file_token,
      size_t request_index,
      std::unique_ptr<endpoint_fetcher::EndpointFetcher> endpoint_fetcher);

  // Handles the response from an upload request.
  void HandleUploadResponse(
      const base::UnguessableToken& file_token,
      size_t request_index,
      std::unique_ptr<endpoint_fetcher::EndpointResponse> response);

  // Performs the fetch request.
  void PerformFetchRequest(
      lens::LensOverlayServerRequest* request,
      std::vector<std::string>* request_headers,
      base::TimeDelta timeout,
      base::OnceCallback<
          void(std::unique_ptr<endpoint_fetcher::EndpointFetcher>)>
          fetcher_created_callback,
      endpoint_fetcher::EndpointFetcherCallback response_received_callback,
      UploadProgressCallback upload_progress_callback = base::NullCallback());

  // Creates the encoded visual search interaction log data and attaches it to
  // the url param list.
  void AddEncodedVisualSearchInteractionLogDataParam(
      const FileInfo* file_info,
      const std::optional<std::string>& query_text,
      std::optional<lens::LensOverlaySelectionType> lens_overlay_selection_type,
      std::map<std::string, std::string>& url_params_map);

  // Constructs the visual search interaction data based on the file info and
  // interaction request data.
  std::optional<lens::LensOverlayVisualSearchInteractionData>
  ConstructVisualSearchInteractionData(
      const FileInfo* file_info,
      const std::optional<std::string>& query_text,
      std::optional<lens::LensOverlaySelectionType>
          lens_overlay_selection_type);

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

  // The url loader factory to use for Lens network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The channel to use for Lens network requests.
  version_info::Channel channel_;

  // The locale used for creating the client context.
  std::string locale_;

  // The request id generator for this query flow instance.
  lens::LensOverlayRequestIdGenerator request_id_generator_;

  // The observer list, managed via AddObserver() and RemoveObserver().
  base::ObserverList<FileUploadStatusObserver> observers_;

  // Owned by the Profile, and thus guaranteed to outlive this instance.
  const raw_ptr<TemplateURLService> template_url_service_;

  // Owned by the Profile, and thus guaranteed to outlive this instance.
  const raw_ptr<variations::VariationsClient> variations_client_;

  // Whether or not to send the lns_surface parameter.
  // TODO(crbug.com/430070871): Remove this once the server supports the
  // `lns_surface` parameter.
  bool send_lns_surface_;

  // If `send_lns_surface_` is true, whether to suppress the `lns_surface`
  // parameter if there is no image upload. Does nothing if `send_lns_surface_`
  // is false.
  bool suppress_lns_surface_param_if_no_image_;

  // Whether or not to use the multiple-input id request generation flow.
  bool enable_multi_context_input_flow_;

  // Whether or not to include viewport images with page context uploads.
  // TODO(crbug.com/448647393): Remove this once the server supports viewport
  // images for multi-context input.
  bool enable_viewport_images_;

  // Whether or not to send viewport images with separate request ids from
  // their associated page context, for the multi-context input flow.
  // Does nothing if `enable_multi_context_input_flow_` is false or if
  // `enable_viewport_images_` is false.
  bool use_separate_request_ids_for_multi_context_viewport_images_;

  // Whether to offer ZPS for the first document attachment, when multiple
  // attachments are available (true), or the only attachment if exactly one
  // attachment is available (false).
  bool prioritize_suggestions_for_the_first_attached_document_;

  // Whether or not to attach the page title and url directly to the suggest
  // request params.
  bool attach_page_title_and_url_to_suggest_requests_;

  // The data for the interaction request in progress. Is null if no
  // interaction request has been made.
  std::unique_ptr<LensServerInteractionRequest>
      latest_interaction_request_data_;

  // The number of files that are sent in the AIM request.
  int num_files_in_request_ = 0;

  // The latest pending search URL request that was not sent due to waiting on
  // cluster info.
  base::OnceClosure pending_search_url_request_;

  base::WeakPtrFactory<ComposeboxQueryController> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_INTERNAL_COMPOSEBOX_QUERY_CONTROLLER_H_
