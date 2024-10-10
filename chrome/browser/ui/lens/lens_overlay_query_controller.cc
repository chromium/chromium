// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_query_controller.h"

#include <optional>

#include "base/base64url.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lens/core/mojom/geometry.mojom.h"
#include "chrome/browser/lens/core/mojom/overlay_object.mojom-forward.h"
#include "chrome/browser/lens/core/mojom/text.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/lens/lens_overlay_image_helper.h"
#include "chrome/browser/ui/lens/lens_overlay_proto_converter.h"
#include "chrome/browser/ui/lens/lens_overlay_url_builder.h"
#include "chrome/browser/ui/lens/ref_counted_lens_overlay_client_logs.h"
#include "chrome/common/channel_info.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/lens/lens_features.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/variations/variations.mojom.h"
#include "components/variations/variations_client.h"
#include "components/variations/variations_ids_provider.h"
#include "components/version_info/channel.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/google_api_keys.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "third_party/lens_server_proto/lens_overlay_client_platform.pb.h"
#include "third_party/lens_server_proto/lens_overlay_filters.pb.h"
#include "third_party/lens_server_proto/lens_overlay_platform.pb.h"
#include "third_party/lens_server_proto/lens_overlay_polygon.pb.h"
#include "third_party/lens_server_proto/lens_overlay_request_id.pb.h"
#include "third_party/lens_server_proto/lens_overlay_surface.pb.h"
#include "third_party/lens_server_proto/lens_overlay_visual_search_interaction_data.pb.h"
#include "ui/gfx/geometry/rect.h"

namespace lens {

namespace {

// The name string for the header for variations information.
constexpr char kClientDataHeader[] = "X-Client-Data";
constexpr char kHttpGetMethod[] = "GET";
constexpr char kHttpPostMethod[] = "POST";
constexpr char kContentTypeKey[] = "Content-Type";
constexpr char kContentType[] = "application/x-protobuf";
constexpr char kDeveloperKey[] = "X-Developer-Key";
constexpr char kSessionIdQueryParameterKey[] = "gsessionid";
constexpr char kOAuthConsumerName[] = "LensOverlayQueryController";
constexpr char kStartTimeQueryParameter[] = "qsubts";
constexpr char kGen204IdentifierQueryParameter[] = "plla";
constexpr char kVisualSearchInteractionDataQueryParameterKey[] = "vsint";
constexpr char kPdfMimeType[] = "application/pdf";
constexpr char kPlainTextMimeType[] = "text/plain";
constexpr char kHtmlMimeType[] = "text/html";
constexpr char kVisualInputTypeQueryParameterKey[] = "vit";
constexpr char kPdfVisualInputTypeQueryParameterValue[] = "pdf";
constexpr char kWebpageVisualInputTypeQueryParameterValue[] = "wp";
constexpr char kContextualVisualInputTypeQueryParameterValue[] = "video";

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotationTag =
    net::DefineNetworkTrafficAnnotation("lens_overlay", R"(
        semantics {
          sender: "Lens"
          description: "A request to the service handling the Lens "
            "Overlay feature in Chrome."
          trigger: "The user triggered a Lens Overlay Flow by entering "
            "the experience via the right click menu option for "
            "searching images on the page."
          data: "Image and user interaction data. Only the screenshot "
            "of the current webpage viewport (image bytes) and user "
            "interaction data (coordinates of a box within the "
            "screenshot or tapped object-id) are sent."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "hujasonx@google.com"
            }
            contacts {
              email: "lens-chrome@google.com"
            }
          }
          user_data {
            type: USER_CONTENT
            type: WEB_CONTENT
          }
          last_reviewed: "2024-04-11"
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting: "This feature is only shown in menus by default and does "
            "nothing without explicit user action, so there is no setting to "
            "disable the feature."
          chrome_policy {
            LensOverlaySettings {
              LensOverlaySettings: 1
            }
          }
        }
      )");

lens::CoordinateType ConvertToServerCoordinateType(
    lens::mojom::CenterRotatedBox_CoordinateType type) {
  switch (type) {
    case lens::mojom::CenterRotatedBox_CoordinateType::kNormalized:
      return lens::CoordinateType::NORMALIZED;
    case lens::mojom::CenterRotatedBox_CoordinateType::kImage:
      return lens::CoordinateType::IMAGE;
    case lens::mojom::CenterRotatedBox_CoordinateType::kUnspecified:
      return lens::CoordinateType::COORDINATE_TYPE_UNSPECIFIED;
  }
}

lens::CenterRotatedBox ConvertToServerCenterRotatedBox(
    lens::mojom::CenterRotatedBoxPtr box) {
  lens::CenterRotatedBox out_box;
  out_box.set_center_x(box->box.x());
  out_box.set_center_y(box->box.y());
  out_box.set_width(box->box.width());
  out_box.set_height(box->box.height());
  out_box.set_coordinate_type(
      ConvertToServerCoordinateType(box->coordinate_type));
  return out_box;
}

std::vector<std::string> CreateOAuthHeader(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  std::vector<std::string> headers;
  if (error.state() == GoogleServiceAuthError::NONE) {
    headers.push_back(kDeveloperKey);
    headers.push_back(GaiaUrls::GetInstance()->oauth2_chrome_client_id());
    headers.push_back(net::HttpRequestHeaders::kAuthorization);
    headers.push_back(
        base::StringPrintf("Bearer %s", access_token_info.token.c_str()));
  }
  return headers;
}

std::vector<std::string> CreateVariationsHeaders(
    variations::mojom::VariationsHeadersPtr variations) {
  std::vector<std::string> headers;
  if (variations.is_null()) {
    return headers;
  }

  headers.push_back(kClientDataHeader);
  // The endpoint is always a Google property.
  headers.push_back(variations->headers_map.at(
      variations::mojom::GoogleWebVisibility::FIRST_PARTY));

  return headers;
}

std::map<std::string, std::string> AddStartTimeQueryParam(
    std::map<std::string, std::string> additional_search_query_params) {
  int64_t current_time_ms = base::Time::Now().InMillisecondsSinceUnixEpoch();
  additional_search_query_params.insert(
      {kStartTimeQueryParameter, base::NumberToString(current_time_ms)});
  return additional_search_query_params;
}

std::map<std::string, std::string> AddVisualInputTypeQueryParam(
    std::map<std::string, std::string> additional_search_query_params,
    PageContentMimeType content_type) {
  // Default contextual visual input type.
  std::string vitValue = kContextualVisualInputTypeQueryParameterValue;
  switch (content_type) {
    case PageContentMimeType::kPdf:
      if (lens::features::UsePdfVitParam()) {
        vitValue = kPdfVisualInputTypeQueryParameterValue;
      }
      break;
    case PageContentMimeType::kHtml:
    case PageContentMimeType::kPlainText:
      if (lens::features::UseWebpageVitParam()) {
        vitValue = kWebpageVisualInputTypeQueryParameterValue;
      }
      break;
    case PageContentMimeType::kNone:
      break;
  }
  additional_search_query_params.insert(
      {kVisualInputTypeQueryParameterKey, vitValue});
  return additional_search_query_params;
}

std::string ContentTypeToString(PageContentMimeType content_type) {
  switch (content_type) {
    case PageContentMimeType::kPdf:
      return kPdfMimeType;
    case PageContentMimeType::kHtml:
      return kHtmlMimeType;
    case PageContentMimeType::kPlainText:
      return kPlainTextMimeType;
    case PageContentMimeType::kNone:
      return "";
  }
}

lens::LensOverlayClientLogs::LensOverlayEntryPoint
LenOverlayEntryPointFromInvocationSource(
    lens::LensOverlayInvocationSource invocation_source) {
  switch (invocation_source) {
    case lens::LensOverlayInvocationSource::kAppMenu:
      return lens::LensOverlayClientLogs::APP_MENU;
    case lens::LensOverlayInvocationSource::kContentAreaContextMenuPage:
      return lens::LensOverlayClientLogs::PAGE_CONTEXT_MENU;
    case lens::LensOverlayInvocationSource::kContentAreaContextMenuImage:
      return lens::LensOverlayClientLogs::IMAGE_CONTEXT_MENU;
    case lens::LensOverlayInvocationSource::kOmnibox:
      return lens::LensOverlayClientLogs::OMNIBOX_BUTTON;
    case lens::LensOverlayInvocationSource::kToolbar:
      return lens::LensOverlayClientLogs::TOOLBAR_BUTTON;
    case lens::LensOverlayInvocationSource::kFindInPage:
      return lens::LensOverlayClientLogs::FIND_IN_PAGE;
  }
  return lens::LensOverlayClientLogs::UNKNOWN_ENTRY_POINT;
}

}  // namespace

LensOverlayQueryController::LensOverlayQueryController(
    LensOverlayFullImageResponseCallback full_image_callback,
    LensOverlayUrlResponseCallback url_callback,
    LensOverlaySuggestInputsCallback suggest_inputs_callback,
    LensOverlayThumbnailCreatedCallback thumbnail_created_callback,
    variations::VariationsClient* variations_client,
    signin::IdentityManager* identity_manager,
    Profile* profile,
    lens::LensOverlayInvocationSource invocation_source,
    bool use_dark_mode,
    lens::LensOverlayGen204Controller* gen204_controller)
    : full_image_callback_(std::move(full_image_callback)),
      suggest_inputs_callback_(std::move(suggest_inputs_callback)),
      thumbnail_created_callback_(std::move(thumbnail_created_callback)),
      request_id_generator_(
          std::make_unique<lens::LensOverlayRequestIdGenerator>()),
      url_callback_(std::move(url_callback)),
      variations_client_(variations_client),
      identity_manager_(identity_manager),
      profile_(profile),
      invocation_source_(invocation_source),
      use_dark_mode_(use_dark_mode),
      gen204_controller_(gen204_controller) {
  encoding_task_runner_ = base::ThreadPool::CreateTaskRunner(
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  encoding_task_tracker_ = std::make_unique<base::CancelableTaskTracker>();
}

LensOverlayQueryController::~LensOverlayQueryController() {
  EndQuery();
}

void LensOverlayQueryController::StartQueryFlow(
    const SkBitmap& screenshot,
    std::optional<GURL> page_url,
    std::optional<std::string> page_title,
    std::vector<lens::mojom::CenterRotatedBoxPtr> significant_region_boxes,
    base::span<const uint8_t> underlying_content_bytes,
    PageContentMimeType underlying_content_type,
    float ui_scale_factor) {
  original_screenshot_ = screenshot;
  page_url_ = page_url;
  page_title_ = page_title;
  significant_region_boxes_ = std::move(significant_region_boxes);
  underlying_content_bytes_ = underlying_content_bytes;
  underlying_content_type_ = underlying_content_type;
  ui_scale_factor_ = ui_scale_factor;
  gen204_id_ = base::RandUint64();
  gen204_controller_->OnQueryFlowStart(invocation_source_, profile_,
                                       gen204_id_);

  // Reset translation languages in case they were set in a previous request.
  translate_options_.reset();

  PrepareAndFetchFullImageRequest();
}

void LensOverlayQueryController::EndQuery() {
  gen204_controller_->OnQueryFlowEnd(
      request_id_generator_->GetBase32EncodedAnalyticsId());
  full_image_endpoint_fetcher_.reset();
  interaction_endpoint_fetcher_.reset();
  full_image_response_received_callback_.Reset();
  cluster_info_access_token_fetcher_.reset();
  full_image_access_token_fetcher_.reset();
  interaction_access_token_fetcher_.reset();
  page_url_.reset();
  page_title_.reset();
  translate_options_.reset();
  cluster_info_.reset();
  encoding_task_tracker_->TryCancelAll();
  query_controller_state_ = QueryControllerState::kOff;
}

void LensOverlayQueryController::SendFullPageTranslateQuery(
    const std::string& source_language,
    const std::string& target_language) {
  translate_options_ = TranslateOptions(source_language, target_language);

  // Send a normal full image request. The parameters to make it a translate
  // request will be set when the actual request is sent based on the instance
  // variables.
  PrepareAndFetchFullImageRequest();
}

void LensOverlayQueryController::SendEndTranslateModeQuery() {
  translate_options_.reset();
  PrepareAndFetchFullImageRequest();
}

void LensOverlayQueryController::SendRegionSearch(
    lens::mojom::CenterRotatedBoxPtr region,
    lens::LensOverlaySelectionType lens_selection_type,
    std::map<std::string, std::string> additional_search_query_params,
    std::optional<SkBitmap> region_bytes) {
  SendInteraction(/*region=*/std::move(region), /*query_text=*/std::nullopt,
                  /*object_id=*/std::nullopt, lens_selection_type,
                  additional_search_query_params, region_bytes);
}

void LensOverlayQueryController::SendTextOnlyQuery(
    const std::string& query_text,
    TextOnlyQueryType text_only_query_type,
    std::map<std::string, std::string> additional_search_query_params) {
  // Although the text only flow might not send an interaction request, we
  // should replace any in-flight interaction requests to cancel previously
  // issued fetches.
  latest_interaction_request_data_ = std::make_unique<LensServerFetchRequest>(
      request_id_generator_
          ->GetNextRequestId(RequestIdUpdateMode::kInteractionRequest)
          ->sequence_id(),
      /*query_start_time_ms=*/base::TimeTicks::Now());

  // If content bytes exist on a text only query, contextualize the query via a
  // Lens request, instead of going straight through GWS.
  if (!underlying_content_bytes_.empty()) {
    // Include the vit to get contextualized results.
    additional_search_query_params = AddVisualInputTypeQueryParam(
        additional_search_query_params, underlying_content_type_);

    // TODO(b/362816047): Send the correct selection type once it is ready.
    SendInteraction(/*region=*/nullptr, query_text,
                    /*object_id=*/std::nullopt, lens::UNKNOWN_SELECTION_TYPE,
                    additional_search_query_params, std::nullopt);
    return;
  }

  // Add the start time to the query params now, so that any additional
  // client processing time is included.
  additional_search_query_params =
      AddStartTimeQueryParam(additional_search_query_params);

  // The visual search interaction log data should be added as late as possible,
  // so that is_parent_query can be accurately set if the user issues multiple
  // interactions in quick succession.
  if (lens::features::SendVisualSearchInteractionParamForLensTextQueries() &&
      text_only_query_type == TextOnlyQueryType::kLensTextSelection) {
    additional_search_query_params = AddVisualSearchInteractionLogData(
        additional_search_query_params, lens::SELECT_TEXT_HIGHLIGHT);
  }

  lens::proto::LensOverlayUrlResponse lens_overlay_url_response;
  lens_overlay_url_response.set_url(
      lens::BuildTextOnlySearchURL(
          query_text, page_url_, page_title_, additional_search_query_params,
          invocation_source_, text_only_query_type, use_dark_mode_)
          .spec());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(url_callback_, lens_overlay_url_response));
}

void LensOverlayQueryController::SendMultimodalRequest(
    lens::mojom::CenterRotatedBoxPtr region,
    const std::string& query_text,
    lens::LensOverlaySelectionType multimodal_selection_type,
    std::map<std::string, std::string> additional_search_query_params,
    std::optional<SkBitmap> region_bytes) {
  if (base::TrimWhitespaceASCII(query_text, base::TRIM_ALL).empty()) {
    return;
  }
  SendInteraction(/*region=*/std::move(region), query_text,
                  /*object_id=*/std::nullopt, multimodal_selection_type,
                  additional_search_query_params, region_bytes);
}

void LensOverlayQueryController::SendTaskCompletionGen204IfEnabled(
    lens::mojom::UserAction user_action) {
  gen204_controller_->SendTaskCompletionGen204IfEnabled(
      request_id_generator_->GetBase32EncodedAnalyticsId(), user_action);
}

void LensOverlayQueryController::SendSemanticEventGen204IfEnabled(
    lens::mojom::SemanticEvent event) {
  gen204_controller_->SendSemanticEventGen204IfEnabled(event);
}

std::unique_ptr<EndpointFetcher>
LensOverlayQueryController::CreateEndpointFetcher(
    lens::LensOverlayServerRequest* request,
    const GURL& fetch_url,
    const std::string& http_method,
    const std::vector<std::string>& request_headers,
    const std::vector<std::string>& cors_exempt_headers) {
  // If provided, serialize the request to a string to include as the request
  // post data.
  std::string request_string;
  if (request) {
    CHECK(request->SerializeToString(&request_string));
  }

  return std::make_unique<EndpointFetcher>(
      /*url_loader_factory=*/profile_
          ? profile_->GetURLLoaderFactory().get()
          : g_browser_process->shared_url_loader_factory(),
      /*url=*/fetch_url,
      /*http_method=*/http_method,
      /*content_type=*/kContentType,
      base::Milliseconds(lens::features::GetLensOverlayServerRequestTimeout()),
      /*post_data=*/request_string,
      /*headers=*/request_headers,
      /*cors_exempt_headers=*/cors_exempt_headers,
      /*annotation_tag=*/kTrafficAnnotationTag, chrome::GetChannel(),
      /*request_params=*/
      EndpointFetcher::RequestParams::Builder()
          .SetCredentialsMode(CredentialsMode::kInclude)
          .Build());
}

void LensOverlayQueryController::SendLatencyGen204IfEnabled(
    int64_t latency_ms,
    bool is_translate_query) {
  gen204_controller_->SendLatencyGen204IfEnabled(latency_ms,
                                                 is_translate_query);
}

LensOverlayQueryController::LensServerFetchRequest::LensServerFetchRequest(
    int sequence_id,
    base::TimeTicks query_start_time)
    : sequence_id_(sequence_id), query_start_time_(query_start_time) {}
LensOverlayQueryController::LensServerFetchRequest::~LensServerFetchRequest() =
    default;

void LensOverlayQueryController::FetchClusterInfoRequest() {
  query_controller_state_ = QueryControllerState::kAwaitingClusterInfoResponse;

  // There should not be any in-flight cluster info request.
  CHECK(!cluster_info_access_token_fetcher_);
  cluster_info_access_token_fetcher_ =
      CreateOAuthHeadersAndContinue(base::BindOnce(
          &LensOverlayQueryController::PerformClusterInfoFetchRequest,
          weak_ptr_factory_.GetWeakPtr()));
}

void LensOverlayQueryController::PerformClusterInfoFetchRequest(
    std::vector<std::string> request_headers) {
  // Add protobuf content type to the request headers.
  request_headers.push_back(kContentTypeKey);
  request_headers.push_back(kContentType);

  // Get client experiment variations to include in the request.
  std::vector<std::string> cors_exempt_headers =
      CreateVariationsHeaders(variations_client_->GetVariationsHeaders());

  // Generate the URL to fetch.
  GURL fetch_url = GURL(lens::features::GetLensOverlayClusterInfoEndpointUrl());

  // Create the EndpointFetcher, responsible for making the request using our
  // given params. Store in class variable to keep endpoint fetcher alive until
  // the request is made.
  cluster_info_endpoint_fetcher_ = CreateEndpointFetcher(
      nullptr, fetch_url, kHttpGetMethod, request_headers, cors_exempt_headers);

  // Finally, perform the request.
  cluster_info_endpoint_fetcher_->PerformRequest(
      base::BindOnce(
          &LensOverlayQueryController::ClusterInfoFetchResponseHandler,
          weak_ptr_factory_.GetWeakPtr()),
      google_apis::GetAPIKey().c_str());
}

void LensOverlayQueryController::ClusterInfoFetchResponseHandler(
    std::unique_ptr<EndpointResponse> response) {
  cluster_info_endpoint_fetcher_.reset();
  query_controller_state_ = QueryControllerState::kReceivedClusterInfoResponse;

  if (response->http_status_code != google_apis::ApiErrorCode::HTTP_SUCCESS) {
    // If there was an error with the cluster info request, we should still try
    // and send the full image request as a fallback.
    PrepareAndFetchFullImageRequest();
    return;
  }

  lens::LensOverlayServerClusterInfoResponse server_response;
  const std::string response_string = response->response;
  bool parse_successful = server_response.ParseFromArray(
      response_string.data(), response_string.size());
  if (!parse_successful) {
    // If there was an error with the cluster info request, we should still try
    // and send the full image request as a fallback.
    PrepareAndFetchFullImageRequest();
    return;
  }

  // Store the cluster info.
  cluster_info_ = std::make_optional<lens::LensOverlayClusterInfo>();
  cluster_info_->set_server_session_id(server_response.server_session_id());
  cluster_info_->set_search_session_id(server_response.search_session_id());

  // Continue with the full image request which will use the session id from the
  // cluster info we just received.
  PrepareAndFetchFullImageRequest();
  PrepareAndFetchPageContentRequest();
}

void LensOverlayQueryController::PrepareAndFetchFullImageRequest() {
  if (query_controller_state_ ==
      QueryControllerState::kAwaitingClusterInfoResponse) {
    // If we are still waiting for the cluster info response, we can't send the
    // full image request yet. Once the cluster info response is received,
    // PrepareAndFetchFullImageRequest will be called again.
    return;
  }

  // If the optimized flow is enabled, request the cluster info prior to making
  // the full image request.
  if (!cluster_info_ && lens::features::UseOptimizedRequestFlow()) {
    FetchClusterInfoRequest();
    return;
  }

  // There can be multiple full image requests that are called. For example,
  // when translate mode is enabled after opening the overlay or when turning
  // translate mode back off after enabling. Reset if there is one pending.
  full_image_endpoint_fetcher_.reset();
  query_controller_state_ = QueryControllerState::kAwaitingFullImageResponse;

  // Create the client logs needed throughout the async process to attach to
  // the full image request.
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  ref_counted_logs->client_logs().set_lens_overlay_entry_point(
      LenOverlayEntryPointFromInvocationSource(invocation_source_));

  // Get request_id for this request flow.
  auto request_id = request_id_generator_->GetNextRequestId(
      RequestIdUpdateMode::kFullImageRequest);
  int current_sequence_id = request_id->sequence_id();

  // Initialize latest_full_image_request_data_ with the latest sequence_id to
  // ensure once the async processes finish, no new full image request has
  // started.
  latest_full_image_request_data_ = std::make_unique<LensServerFetchRequest>(
      current_sequence_id,
      /*query_start_time=*/base::TimeTicks::Now());
  latest_full_image_request_data_->request_id_ = std::move(request_id);

  // Preparing for the full image request requires multiple async flows to
  // complete before the request is ready to be send to the server. We start
  // these flows here, and each flow completes by calling
  // FullImageRequestDataReady() with its data. FullImageRequestDataReady() will
  // handle waiting for all necessary flows to complete before performing the
  // request.
  //
  // Async Flow 1: Creating the full image request.
  // Do the image encoding asynchronously to prevent the main thread from
  // blocking on the encoding.
  encoding_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&lens::DownscaleAndEncodeBitmap, original_screenshot_,
                     ui_scale_factor_, ref_counted_logs),
      base::BindOnce(&LensOverlayQueryController::
                         CreateFullImageRequestAndTryPerformFullImageRequest,
                     weak_ptr_factory_.GetWeakPtr(), current_sequence_id,
                     ref_counted_logs));

  // Async Flow 2: Retrieve the OAuth headers.
  CreateOAuthHeadersAndTryPerformFullPageRequest(current_sequence_id);
}

void LensOverlayQueryController::PrepareImageDataForFullImageRequest(
    scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs,
    lens::ImageData image_data) {
  ref_counted_logs->client_logs().set_paella_id(gen204_id_);

  resized_bitmap_size_ = gfx::Size(image_data.image_metadata().width(),
                                   image_data.image_metadata().height());

  AddSignificantRegions(image_data, std::move(significant_region_boxes_));
}

void LensOverlayQueryController::
    CreateFullImageRequestAndTryPerformFullImageRequest(
        int sequence_id,
        scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs,
        lens::ImageData image_data) {
  DCHECK_EQ(query_controller_state_,
            QueryControllerState::kAwaitingFullImageResponse);
  PrepareImageDataForFullImageRequest(ref_counted_logs, image_data);

  // Create the request.
  lens::LensOverlayServerRequest request;
  request.mutable_client_logs()->CopyFrom(ref_counted_logs->client_logs());
  lens::LensOverlayRequestContext request_context;

  // The request ID is guaranteed to exist since it was created in
  // PrepareAndFetchFullImageRequest().
  CHECK(latest_full_image_request_data_->request_id_);
  request_context.mutable_request_id()->CopyFrom(
      *latest_full_image_request_data_->request_id_);

  request_context.mutable_client_context()->CopyFrom(CreateClientContext());
  request.mutable_objects_request()->mutable_request_context()->CopyFrom(
      request_context);
  request.mutable_objects_request()->mutable_image_data()->CopyFrom(image_data);

  // The content bytes ideally are uploaded separately once the cluster info is
  // retrieved. However, if the cluster info request fails, we should include
  // them in this request. So if page content bytes are available, and the
  // cluster info was not retrieved, included them in this request.
  if (!underlying_content_bytes_.empty() && !cluster_info_.has_value()) {
    lens::Payload payload;
    payload.mutable_content_data()->assign(underlying_content_bytes_.begin(),
                                           underlying_content_bytes_.end());
    payload.set_content_type(ContentTypeToString(underlying_content_type_));
    request.mutable_objects_request()->mutable_payload()->CopyFrom(payload);
  }

  FullImageRequestDataReady(sequence_id, request);
}

void LensOverlayQueryController::CreateOAuthHeadersAndTryPerformFullPageRequest(
    int sequence_id) {
  DCHECK_EQ(query_controller_state_,
            QueryControllerState::kAwaitingFullImageResponse);

  // If there is already a pending access token fetcher, we purposefully
  // override it since we no longer care about the previous request.
  full_image_access_token_fetcher_ =
      CreateOAuthHeadersAndContinue(base::BindOnce(
          static_cast<void (LensOverlayQueryController::*)(
              int, std::vector<std::string>)>(
              &LensOverlayQueryController::FullImageRequestDataReady),
          weak_ptr_factory_.GetWeakPtr(), sequence_id));
}

void LensOverlayQueryController::FullImageRequestDataReady(
    int sequence_id,
    lens::LensOverlayServerRequest request) {
  if (!IsCurrentFullImageSequence(sequence_id)) {
    // Ignore superseded request.
    return;
  }

  latest_full_image_request_data_->request_ =
      std::make_unique<lens::LensOverlayServerRequest>(request);
  FullImageRequestDataHelper(sequence_id);
}

void LensOverlayQueryController::FullImageRequestDataReady(
    int sequence_id,
    std::vector<std::string> headers) {
  if (!IsCurrentFullImageSequence(sequence_id)) {
    // Ignore superseded request.
    return;
  }

  full_image_access_token_fetcher_.reset();
  latest_full_image_request_data_->request_headers_ =
      std::make_unique<std::vector<std::string>>(headers);
  FullImageRequestDataHelper(sequence_id);
}

void LensOverlayQueryController::FullImageRequestDataHelper(int sequence_id) {
  CHECK(latest_full_image_request_data_->sequence_id_ == sequence_id);
  if (latest_full_image_request_data_->request_ &&
      latest_full_image_request_data_->request_headers_) {
    PerformFullImageRequest();
  }
}

bool LensOverlayQueryController::IsCurrentFullImageSequence(int sequence_id) {
  CHECK(latest_full_image_request_data_);
  return latest_full_image_request_data_->sequence_id_ == sequence_id;
}

void LensOverlayQueryController::PerformFullImageRequest() {
  PerformFetchRequest(
      latest_full_image_request_data_->request_.get(),
      latest_full_image_request_data_->request_headers_.get(),
      base::BindOnce(
          &LensOverlayQueryController::OnFullImageEndpointFetcherCreated,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&LensOverlayQueryController::FullImageFetchResponseHandler,
                     weak_ptr_factory_.GetWeakPtr(),
                     latest_full_image_request_data_->sequence_id_));
}

void LensOverlayQueryController::FullImageFetchResponseHandler(
    int request_sequence_id,
    std::unique_ptr<EndpointResponse> response) {
  // If this request sequence ID does not match the latest sent then we should
  // ignore the response.
  if (latest_full_image_request_data_->sequence_id_ != request_sequence_id) {
    return;
  }

  DCHECK_EQ(query_controller_state_,
            QueryControllerState::kAwaitingFullImageResponse);

  CHECK(full_image_endpoint_fetcher_);
  full_image_endpoint_fetcher_.reset();
  query_controller_state_ = QueryControllerState::kReceivedFullImageResponse;

  if (response->http_status_code != google_apis::ApiErrorCode::HTTP_SUCCESS) {
    RunFullImageCallbackForError();
    return;
  }

  lens::LensOverlayServerResponse server_response;
  const std::string response_string = response->response;
  bool parse_successful = server_response.ParseFromArray(
      response_string.data(), response_string.size());
  if (!parse_successful) {
    RunFullImageCallbackForError();
    return;
  }

  if (!server_response.has_objects_response() ||
      !server_response.objects_response().has_cluster_info()) {
    RunFullImageCallbackForError();
    return;
  }

  base::TimeDelta elapsed_time =
      base::TimeTicks::Now() -
      latest_full_image_request_data_->query_start_time_;
  SendLatencyGen204IfEnabled(elapsed_time.InMilliseconds(),
                             translate_options_.has_value());

  if (!cluster_info_.has_value()) {
    cluster_info_ = std::make_optional<lens::LensOverlayClusterInfo>();
    cluster_info_->CopyFrom(server_response.objects_response().cluster_info());
  }

  // Clear the cluster info after its lifetime expires.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&LensOverlayQueryController::ResetRequestClusterInfoState,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Seconds(
          lens::features::GetLensOverlayClusterInfoLifetimeSeconds()));

  if (!full_image_response_received_callback_.is_null()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(full_image_response_received_callback_));
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(full_image_callback_,
                                lens::CreateObjectsMojomArrayFromServerResponse(
                                    server_response),
                                lens::CreateTextMojomFromServerResponse(
                                    server_response, resized_bitmap_size_),
                                /*is_error=*/false));
}

void LensOverlayQueryController::RunFullImageCallbackForError() {
  ResetRequestClusterInfoState();
  // Needs to be set to received response so this query can be retried on the
  // next interaction request.
  query_controller_state_ =
      QueryControllerState::kReceivedFullImageErrorResponse;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(full_image_callback_,
                                std::vector<lens::mojom::OverlayObjectPtr>(),
                                /*text=*/nullptr, /*is_error=*/true));
}

void LensOverlayQueryController::PrepareAndFetchPageContentRequest() {
  if (!cluster_info_ || underlying_content_bytes_.empty()) {
    // Cannot send this request without cluster info. No need to send the
    // request without underlying content bytes.
    return;
  }

  // Create the request.
  lens::LensOverlayServerRequest request;
  lens::LensOverlayRequestContext request_context;

  // Use the same request ID as the full image request. It is guaranteed to
  // exist since the full image request was started first.
  CHECK(latest_full_image_request_data_->request_id_);
  request_context.mutable_request_id()->CopyFrom(
      *latest_full_image_request_data_->request_id_);

  request_context.mutable_client_context()->CopyFrom(CreateClientContext());
  request.mutable_objects_request()->mutable_request_context()->CopyFrom(
      request_context);

  lens::Payload payload;
  payload.mutable_content_data()->assign(underlying_content_bytes_.begin(),
                                         underlying_content_bytes_.end());
  payload.set_content_type(ContentTypeToString(underlying_content_type_));
  request.mutable_objects_request()->mutable_payload()->CopyFrom(payload);

  page_content_access_token_fetcher_ = CreateOAuthHeadersAndContinue(
      base::BindOnce(&LensOverlayQueryController::PerformPageContentRequest,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request)));
}

void LensOverlayQueryController::PerformPageContentRequest(
    lens::LensOverlayServerRequest request,
    std::vector<std::string> headers) {
  page_content_access_token_fetcher_.reset();

  // Pass no response callback because this is a fire and forget request.
  PerformFetchRequest(
      &request, &headers,
      base::BindOnce(
          &LensOverlayQueryController::OnPageContentEndpointFetcherCreated,
          weak_ptr_factory_.GetWeakPtr()),
      base::DoNothing());
}

void LensOverlayQueryController::SendInteraction(
    lens::mojom::CenterRotatedBoxPtr region,
    std::optional<std::string> query_text,
    std::optional<std::string> object_id,
    lens::LensOverlaySelectionType selection_type,
    std::map<std::string, std::string> additional_search_query_params,
    std::optional<SkBitmap> region_bytes) {
  // Cancel any pending encoding from previous SendInteraction requests.
  encoding_task_tracker_->TryCancelAll();
  // Reset any pending interaction requests that will get fired via the full
  // image response callback.
  full_image_response_received_callback_.Reset();

  // Create the logs used across the async.
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  ref_counted_logs->client_logs().set_lens_overlay_entry_point(
      LenOverlayEntryPointFromInvocationSource(invocation_source_));
  ref_counted_logs->client_logs().set_paella_id(gen204_id_);

  // Add the start time to the query params now, so that any additional
  // client processing time is included.
  additional_search_query_params =
      AddStartTimeQueryParam(additional_search_query_params);

  // Get request_id for this request flow.
  auto server_request_id = request_id_generator_->GetNextRequestId(
      RequestIdUpdateMode::kInteractionRequest);
  int current_sequence_id = server_request_id->sequence_id();

  // Initialize latest_interaction_request_data_ with the latest sequence_id to
  // ensure once the async processes finish, no new interaction request has
  // started.
  latest_interaction_request_data_ = std::make_unique<LensServerFetchRequest>(
      current_sequence_id,
      /*query_start_time_ms=*/base::TimeTicks::Now());

  // Add the create URL callback to be run after the request is sent.
  latest_interaction_request_data_->request_sent_callback_ = base::BindOnce(
      &LensOverlayQueryController::CreateSearchUrlAndSendToCallback,
      weak_ptr_factory_.GetWeakPtr(), query_text,
      additional_search_query_params, selection_type,
      request_id_generator_->GetNextRequestId(RequestIdUpdateMode::kSearchUrl));

  // The interaction request requires multiple async flows to complete before
  // the request is ready to be send to the server. We start these flows here,
  // and each flow completes by calling InteractionRequestDataReady() with its
  // data. InteractionRequestDataReady() will handle waiting for all necessary
  // flows to complete before performing the request.
  //
  // Async Flow 1: Downscale the image region for the interaction request.
  // Do the image encoding asynchronously to prevent the main thread from
  // blocking on the encoding.
  encoding_task_tracker_->PostTaskAndReplyWithResult(
      encoding_task_runner_.get(), FROM_HERE,
      base::BindOnce(&lens::DownscaleAndEncodeBitmapRegionIfNeeded,
                     original_screenshot_, region.Clone(), region_bytes,
                     ref_counted_logs),
      base::BindOnce(
          &LensOverlayQueryController::
              CreateInteractionRequestAndTryPerformInteractionRequest,
          weak_ptr_factory_.GetWeakPtr(), current_sequence_id, region.Clone(),
          query_text, object_id, ref_counted_logs,
          std::move(server_request_id)));

  // Async Flow 2: Retrieve the OAuth headers.
  CreateOAuthHeadersAndTryPerformInteractionRequest(current_sequence_id);
}

void LensOverlayQueryController::
    CreateInteractionRequestAndTryPerformInteractionRequest(
        int sequence_id,
        lens::mojom::CenterRotatedBoxPtr region,
        std::optional<std::string> query_text,
        std::optional<std::string> object_id,
        scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs,
        std::unique_ptr<lens::LensOverlayRequestId> request_id,
        std::optional<lens::ImageCrop> image_crop) {
  // The request index should match our counter after encoding finishes.
  CHECK(sequence_id == latest_interaction_request_data_->sequence_id_);

  // Pass the image crop for this request to the thumbnail created callback.
  if (image_crop.has_value()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(thumbnail_created_callback_,
                                  image_crop->image().image_content()));
  }

  // Create the interaction request.
  lens::LensOverlayServerRequest server_request = CreateInteractionRequest(
      std::move(region), query_text, object_id, image_crop,
      ref_counted_logs->client_logs(), std::move(request_id));

  // Continue the async process.
  InteractionRequestDataReady(sequence_id, std::move(server_request));
}

void LensOverlayQueryController::
    CreateOAuthHeadersAndTryPerformInteractionRequest(int sequence_id) {
  // If there is already a pending access token fetcher, we purposefully
  // override it to cancel the old request.
  interaction_access_token_fetcher_ =
      CreateOAuthHeadersAndContinue(base::BindOnce(
          static_cast<void (LensOverlayQueryController::*)(
              int, std::vector<std::string>)>(
              &LensOverlayQueryController::InteractionRequestDataReady),
          weak_ptr_factory_.GetWeakPtr(), sequence_id));
}

void LensOverlayQueryController::InteractionRequestDataReady(
    int sequence_id,
    lens::LensOverlayServerRequest request) {
  if (!IsCurrentInteractionSequence(sequence_id)) {
    // Ignore superseded request.
    return;
  }

  latest_interaction_request_data_->request_ =
      std::make_unique<lens::LensOverlayServerRequest>(request);
  TryPerformInteractionRequest(sequence_id);
}

void LensOverlayQueryController::InteractionRequestDataReady(
    int sequence_id,
    std::vector<std::string> headers) {
  if (!IsCurrentInteractionSequence(sequence_id)) {
    // Ignore superseded request.
    return;
  }

  interaction_access_token_fetcher_.reset();
  latest_interaction_request_data_->request_headers_ =
      std::make_unique<std::vector<std::string>>(headers);
  TryPerformInteractionRequest(sequence_id);
}

void LensOverlayQueryController::TryPerformInteractionRequest(int sequence_id) {
  CHECK(latest_interaction_request_data_->sequence_id_ == sequence_id);
  if (!latest_interaction_request_data_->request_ ||
      !latest_interaction_request_data_->request_headers_) {
    // Exit early since not all request data is ready.
    return;
  }

  //  If a full image request is in flight, wait for the full image response
  //  before sending the request.
  if (query_controller_state_ ==
          QueryControllerState::kAwaitingClusterInfoResponse ||
      query_controller_state_ ==
          QueryControllerState::kAwaitingFullImageResponse) {
    full_image_response_received_callback_ = base::BindOnce(
        &LensOverlayQueryController::TryPerformInteractionRequest,
        weak_ptr_factory_.GetWeakPtr(), sequence_id);
    return;
  }

  // If the cluster info is missing and the full image response has already been
  // received, we must restart the query flow by resending the full image
  // request.
  if (!cluster_info_.has_value()) {
    full_image_response_received_callback_ = base::BindOnce(
        &LensOverlayQueryController::TryPerformInteractionRequest,
        weak_ptr_factory_.GetWeakPtr(), sequence_id);

    if (query_controller_state_ ==
            QueryControllerState::kReceivedFullImageResponse ||
        query_controller_state_ ==
            QueryControllerState::kReceivedFullImageErrorResponse) {
      PrepareAndFetchFullImageRequest();
    }
    return;
  }

  // All elements needed are ready so perform the request.
  PerformInteractionRequest();
}

bool LensOverlayQueryController::IsCurrentInteractionSequence(int sequence_id) {
  CHECK(latest_interaction_request_data_);
  return latest_interaction_request_data_->sequence_id_ == sequence_id;
}

void LensOverlayQueryController::PerformInteractionRequest() {
  // The interaction request is composed of two steps, sending the request to
  // the server, and creating the URL to load in the side panel.
  PerformFetchRequest(
      latest_interaction_request_data_->request_.get(),
      latest_interaction_request_data_->request_headers_.get(),
      base::BindOnce(
          &LensOverlayQueryController::OnInteractionEndpointFetcherCreated,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(
          &LensOverlayQueryController::InteractionFetchResponseHandler,
          weak_ptr_factory_.GetWeakPtr(),
          latest_interaction_request_data_->sequence_id_));

  // Run the callback to create the search URL and pass it to the side panel.
  CHECK(latest_interaction_request_data_->request_sent_callback_.has_value());
  std::move(latest_interaction_request_data_->request_sent_callback_.value())
      .Run();
}

void LensOverlayQueryController::CreateSearchUrlAndSendToCallback(
    std::optional<std::string> query_text,
    std::map<std::string, std::string> additional_search_query_params,
    lens::LensOverlaySelectionType selection_type,
    std::unique_ptr<lens::LensOverlayRequestId> request_id) {
  // Cluster info must be set already.
  CHECK(cluster_info_.has_value());

  additional_search_query_params.insert(
      {kGen204IdentifierQueryParameter,
       base::NumberToString(gen204_id_).c_str()});

  // The visual search interaction log data should be added as late as possible,
  // so that is_parent_query can be accurately set if the user issues multiple
  // interactions in quick succession.
  additional_search_query_params = AddVisualSearchInteractionLogData(
      additional_search_query_params, selection_type);

  // Generate and send the Lens search url.
  lens::proto::LensOverlayUrlResponse lens_overlay_url_response;
  lens_overlay_url_response.set_url(
      lens::BuildLensSearchURL(query_text, page_url_, page_title_,
                               std::move(request_id), cluster_info_.value(),
                               additional_search_query_params,
                               invocation_source_, use_dark_mode_)
          .spec());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(url_callback_, lens_overlay_url_response));
}

void LensOverlayQueryController::InteractionFetchResponseHandler(
    int sequence_id,
    std::unique_ptr<EndpointResponse> response) {
  // If this request sequence ID does not match the latest sent then we should
  // ignore the response.
  if (latest_interaction_request_data_->sequence_id_ != sequence_id) {
    return;
  }

  if (response->http_status_code != google_apis::ApiErrorCode::HTTP_SUCCESS) {
    RunInteractionCallbackForError();
    return;
  }

  lens::LensOverlayServerResponse server_response;
  const std::string response_string = response->response;
  bool parse_successful = server_response.ParseFromArray(
      response_string.data(), response_string.size());
  if (!parse_successful) {
    RunInteractionCallbackForError();
    return;
  }

  if (!server_response.has_interaction_response()) {
    RunInteractionCallbackForError();
    return;
  }

  lens::proto::LensOverlaySuggestInputs lens_overlay_interaction_response;
  lens_overlay_interaction_response.set_encoded_image_signals(
      server_response.interaction_response().encoded_response());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(suggest_inputs_callback_,
                                lens_overlay_interaction_response));
}

void LensOverlayQueryController::RunInteractionCallbackForError() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(suggest_inputs_callback_,
                                lens::proto::LensOverlaySuggestInputs()));
}

void LensOverlayQueryController::PerformFetchRequest(
    lens::LensOverlayServerRequest* request,
    std::vector<std::string>* request_headers,
    base::OnceCallback<void(std::unique_ptr<EndpointFetcher>)>
        fetcher_created_callback,
    EndpointFetcherCallback response_received_callback) {
  CHECK(request);
  CHECK(request_headers);

  // Get client experiment variations to include in the request.
  std::vector<std::string> cors_exempt_headers =
      CreateVariationsHeaders(variations_client_->GetVariationsHeaders());

  // Generate the URL to fetch to and include the server session id if present.
  GURL fetch_url = GURL(lens::features::GetLensOverlayEndpointURL());
  if (cluster_info_.has_value()) {
    // The endpoint fetches should use the server session id from the cluster
    // info.
    fetch_url = net::AppendOrReplaceQueryParameter(
        fetch_url, kSessionIdQueryParameterKey,
        cluster_info_->server_session_id());
  }

  // Create the EndpointFetcher, responsible for making the request using our
  // given params.
  std::unique_ptr<EndpointFetcher> endpoint_fetcher =
      CreateEndpointFetcher(request, fetch_url, kHttpPostMethod,
                            *request_headers, cors_exempt_headers);
  EndpointFetcher* fetcher = endpoint_fetcher.get();

  // Run callback that the fetcher was created. This is used to keep the
  // endpoint_fetcher alive while the request is made.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(fetcher_created_callback),
                                std::move(endpoint_fetcher)));

  // Finally, perform the request.
  fetcher->PerformRequest(std::move(response_received_callback),
                          google_apis::GetAPIKey().c_str());
}

lens::LensOverlayClientContext
LensOverlayQueryController::CreateClientContext() {
  lens::LensOverlayClientContext context;
  context.set_surface(lens::SURFACE_CHROMIUM);
  context.set_platform(lens::WEB);
  context.mutable_rendering_context()->set_rendering_environment(
      lens::RENDERING_ENV_LENS_OVERLAY);
  context.mutable_client_filters()->add_filter()->set_filter_type(
      lens::AUTO_FILTER);
  context.mutable_locale_context()->set_language(
      g_browser_process->GetApplicationLocale());
  context.mutable_locale_context()->set_region(
      icu::Locale(g_browser_process->GetApplicationLocale().c_str())
          .getCountry());

  // Add the appropriate context filters. If source and target languages have
  // been set, this should add translate.
  if (translate_options_.has_value()) {
    context.mutable_client_filters()->clear_filter();
    lens::AppliedFilter* translate_filter =
        context.mutable_client_filters()->add_filter();
    translate_filter->set_filter_type(lens::TRANSLATE);
    translate_filter->mutable_translate()->set_source_language(
        translate_options_->source_language);
    translate_filter->mutable_translate()->set_target_language(
        translate_options_->target_language);
  }

  std::unique_ptr<icu::TimeZone> zone(icu::TimeZone::createDefault());
  icu::UnicodeString time_zone_id, time_zone_canonical_id;
  zone->getID(time_zone_id);
  UErrorCode status = U_ZERO_ERROR;
  icu::TimeZone::getCanonicalID(time_zone_id, time_zone_canonical_id, status);
  if (status == U_ZERO_ERROR) {
    std::string zone_id_str;
    time_zone_canonical_id.toUTF8String(zone_id_str);
    context.mutable_locale_context()->set_time_zone(zone_id_str);
  }

  return context;
}

std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
LensOverlayQueryController::CreateOAuthHeadersAndContinue(
    OAuthHeadersCreatedCallback callback) {
  // Use OAuth if the flag is enabled and the user is logged in.
  if (lens::features::UseOauthForLensOverlayRequests() && identity_manager_ &&
      identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    signin::AccessTokenFetcher::TokenCallback token_callback =
        base::BindOnce(&lens::CreateOAuthHeader).Then(std::move(callback));
    signin::ScopeSet oauth_scopes;
    oauth_scopes.insert(GaiaConstants::kLensOAuth2Scope);

    // If an access token fetcher is already in flight, it is intentionally
    // replaced by this newer one.
    return std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
        kOAuthConsumerName, identity_manager_, oauth_scopes,
        std::move(token_callback),
        signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
        signin::ConsentLevel::kSignin);
  }

  // Fall back to fetching the endpoint directly using API key.
  std::move(callback).Run(std::vector<std::string>());
  return nullptr;
}

std::map<std::string, std::string>
LensOverlayQueryController::AddVisualSearchInteractionLogData(
    std::map<std::string, std::string> additional_search_query_params,
    lens::LensOverlaySelectionType selection_type) {
  lens::LensOverlayVisualSearchInteractionData interaction_data;
  interaction_data.mutable_log_data()->mutable_filter_data()->set_filter_type(
      lens::AUTO_FILTER);
  interaction_data.mutable_log_data()
      ->mutable_user_selection_data()
      ->set_selection_type(selection_type);
  interaction_data.mutable_log_data()->set_is_parent_query(!parent_query_sent_);
  interaction_data.mutable_log_data()->set_client_platform(
      lens::CLIENT_PLATFORM_LENS_OVERLAY);
  parent_query_sent_ = true;

  std::string serialized_proto;
  CHECK(interaction_data.SerializeToString(&serialized_proto));
  std::string encoded_proto;
  base::Base64UrlEncode(serialized_proto,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_proto);
  additional_search_query_params.insert(
      {kVisualSearchInteractionDataQueryParameterKey, encoded_proto});
  return additional_search_query_params;
}

lens::LensOverlayServerRequest
LensOverlayQueryController::CreateInteractionRequest(
    lens::mojom::CenterRotatedBoxPtr region,
    std::optional<std::string> query_text,
    std::optional<std::string> object_id,
    std::optional<lens::ImageCrop> image_crop,
    lens::LensOverlayClientLogs client_logs,
    std::unique_ptr<lens::LensOverlayRequestId> request_id) {
  lens::LensOverlayServerRequest server_request;
  server_request.mutable_client_logs()->CopyFrom(client_logs);
  lens::LensOverlayRequestContext request_context;
  request_context.mutable_request_id()->CopyFrom(*request_id.get());
  request_context.mutable_client_context()->CopyFrom(CreateClientContext());
  server_request.mutable_interaction_request()
      ->mutable_request_context()
      ->CopyFrom(request_context);

  lens::LensOverlayInteractionRequestMetadata interaction_request_metadata;
  if (!region.is_null() && image_crop.has_value()) {
    // Add the region for region search and multimodal requests.
    server_request.mutable_interaction_request()
        ->mutable_image_crop()
        ->CopyFrom(*image_crop);
    interaction_request_metadata.set_type(
        lens::LensOverlayInteractionRequestMetadata::REGION_SEARCH);
    interaction_request_metadata.mutable_selection_metadata()
        ->mutable_region()
        ->mutable_region()
        ->CopyFrom(ConvertToServerCenterRotatedBox(region.Clone()));

    // Add the text, for multimodal requests.
    if (query_text.has_value()) {
      interaction_request_metadata.mutable_query_metadata()
          ->mutable_text_query()
          ->set_query(*query_text);
    }
  } else if (object_id.has_value()) {
    // Add object request details.
    interaction_request_metadata.set_type(
        lens::LensOverlayInteractionRequestMetadata::TAP);
    interaction_request_metadata.mutable_selection_metadata()
        ->mutable_object()
        ->set_object_id(*object_id);
  } else if (query_text.has_value()) {
    // If there is only `query_text`, this is a contextual flow.
    // TOOD(b/362816047): Send correct LensOverlayInteractionRequestMetadata,
    // once the server is ready for it.
    interaction_request_metadata.set_type(
        lens::LensOverlayInteractionRequestMetadata::CONTEXTUAL_SEARCH_QUERY);
    interaction_request_metadata.mutable_query_metadata()
        ->mutable_text_query()
        ->set_query(*query_text);
  } else {
    // There should be a region or an object id in the request.
    NOTREACHED_IN_MIGRATION();
  }

  server_request.mutable_interaction_request()
      ->mutable_interaction_request_metadata()
      ->CopyFrom(interaction_request_metadata);
  return server_request;
}

void LensOverlayQueryController::ResetRequestClusterInfoState() {
  full_image_response_received_callback_.Reset();
  interaction_endpoint_fetcher_.reset();
  cluster_info_ = std::nullopt;
  request_id_generator_->ResetRequestId();
  parent_query_sent_ = false;
}

void LensOverlayQueryController::OnFullImageEndpointFetcherCreated(
    std::unique_ptr<EndpointFetcher> endpoint_fetcher) {
  full_image_endpoint_fetcher_ = std::move(endpoint_fetcher);
}

void LensOverlayQueryController::OnPageContentEndpointFetcherCreated(
    std::unique_ptr<EndpointFetcher> endpoint_fetcher) {
  page_content_endpoint_fetcher_ = std::move(endpoint_fetcher);
}

void LensOverlayQueryController::OnInteractionEndpointFetcherCreated(
    std::unique_ptr<EndpointFetcher> endpoint_fetcher) {
  interaction_endpoint_fetcher_ = std::move(endpoint_fetcher);
}
}  // namespace lens
