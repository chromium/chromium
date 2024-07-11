// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_query_controller.h"

#include "base/base64url.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lens/core/mojom/geometry.mojom.h"
#include "chrome/browser/lens/core/mojom/overlay_object.mojom-forward.h"
#include "chrome/browser/lens/core/mojom/text.mojom.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/lens/lens_overlay_image_helper.h"
#include "chrome/browser/ui/lens/lens_overlay_proto_converter.h"
#include "chrome/browser/ui/lens/lens_overlay_url_builder.h"
#include "chrome/common/channel_info.h"
#include "components/lens/lens_features.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/search_engines/template_url_service.h"
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
#include "services/network/public/cpp/resource_request.h"
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

const int64_t kMaxDownloadBytes = 1024 * 1024;

// The name string for the header for variations information.
constexpr char kClientDataHeader[] = "X-Client-Data";
constexpr char kHttpMethod[] = "POST";
constexpr char kContentType[] = "application/x-protobuf";
constexpr char kDeveloperKey[] = "X-Developer-Key";
constexpr char kSessionIdQueryParameterKey[] = "gsessionid";
constexpr char kOAuthConsumerName[] = "LensOverlayQueryController";
constexpr char kStartTimeQueryParameter[] = "qsubts";
constexpr char kGen204IdentifierQueryParameter[] = "plla";
constexpr char kVisualSearchInteractionDataQueryParameterKey[] = "vsint";

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

std::map<std::string, std::string> AddStartTimeQueryParam(
    std::map<std::string, std::string> additional_search_query_params) {
  int64_t current_time_ms = base::Time::Now().InMillisecondsSinceUnixEpoch();
  additional_search_query_params.insert(
      {kStartTimeQueryParameter, base::NumberToString(current_time_ms)});
  return additional_search_query_params;
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
    LensOverlayInteractionResponseCallback interaction_data_callback,
    LensOverlayThumbnailCreatedCallback thumbnail_created_callback,
    variations::VariationsClient* variations_client,
    signin::IdentityManager* identity_manager,
    Profile* profile,
    lens::LensOverlayInvocationSource invocation_source,
    bool use_dark_mode)
    : full_image_callback_(std::move(full_image_callback)),
      interaction_data_callback_(std::move(interaction_data_callback)),
      thumbnail_created_callback_(std::move(thumbnail_created_callback)),
      request_id_generator_(
          std::make_unique<lens::LensOverlayRequestIdGenerator>()),
      url_callback_(std::move(url_callback)),
      variations_client_(variations_client),
      identity_manager_(identity_manager),
      profile_(profile),
      invocation_source_(invocation_source),
      use_dark_mode_(use_dark_mode) {}

LensOverlayQueryController::~LensOverlayQueryController() = default;

void LensOverlayQueryController::StartQueryFlow(
    const SkBitmap& screenshot,
    std::optional<GURL> page_url,
    std::optional<std::string> page_title,
    std::vector<lens::mojom::CenterRotatedBoxPtr> significant_region_boxes,
    float ui_scale_factor) {
  original_screenshot_ = screenshot;
  page_url_ = page_url;
  page_title_ = page_title;
  significant_region_boxes_ = std::move(significant_region_boxes);
  ui_scale_factor_ = ui_scale_factor;
  gen204_id_ = base::RandUint64();

  PrepareAndFetchFullImageRequest();
}

void LensOverlayQueryController::PrepareAndFetchFullImageRequest() {
  DCHECK(query_controller_state_ !=
         QueryControllerState::kAwaitingFullImageResponse);
  query_controller_state_ = QueryControllerState::kAwaitingFullImageResponse;

  lens::LensOverlayClientLogs client_logs;
  client_logs.set_lens_overlay_entry_point(
      LenOverlayEntryPointFromInvocationSource(invocation_source_));
  lens::ImageData image_data = DownscaleAndEncodeBitmap(
      original_screenshot_, ui_scale_factor_, client_logs);
  if (lens::features::GetLensOverlaySendLatencyGen204()) {
    client_logs.set_paella_id(gen204_id_);
  }

  AddSignificantRegions(image_data, std::move(significant_region_boxes_));
  FetchFullImageRequest(request_id_generator_->GetNextRequestId(), image_data,
                        client_logs);
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

void LensOverlayQueryController::FetchFullImageRequest(
    std::unique_ptr<lens::LensOverlayRequestId> request_id,
    lens::ImageData image_data,
    lens::LensOverlayClientLogs client_logs) {
  DCHECK_EQ(query_controller_state_,
            QueryControllerState::kAwaitingFullImageResponse);
  // Create the request.
  lens::LensOverlayServerRequest request;
  request.mutable_client_logs()->CopyFrom(client_logs);
  lens::LensOverlayRequestContext request_context;
  request_context.mutable_request_id()->CopyFrom(*request_id.get());
  request_context.mutable_client_context()->CopyFrom(CreateClientContext());
  request.mutable_objects_request()->mutable_request_context()->CopyFrom(
      request_context);
  request.mutable_objects_request()->mutable_image_data()->CopyFrom(image_data);

  int64_t query_start_time_ms =
      base::Time::Now().InMillisecondsSinceUnixEpoch();

  // Fetch the request.
  CreateAndFetchEndpointFetcher(
      request,
      base::BindOnce(
          &LensOverlayQueryController::OnFullImageEndpointFetcherCreated,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&LensOverlayQueryController::FullImageFetchResponseHandler,
                     weak_ptr_factory_.GetWeakPtr(), query_start_time_ms));
}

void LensOverlayQueryController::FullImageFetchResponseHandler(
    int64_t query_start_time_ms,
    std::unique_ptr<EndpointResponse> response) {
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

  int64_t elapsed_time =
      base::Time::Now().InMillisecondsSinceUnixEpoch() - query_start_time_ms;
  SendLatencyGen204IfEnabled(elapsed_time);

  cluster_info_ = std::make_optional<lens::LensOverlayClusterInfo>();
  cluster_info_->CopyFrom(server_response.objects_response().cluster_info());

  // Clear the cluster info after its lifetime expires.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&LensOverlayQueryController::ResetRequestClusterInfoState,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Seconds(
          lens::features::GetLensOverlayClusterInfoLifetimeSeconds()));

  if (!cluster_info_received_callback_.is_null()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(cluster_info_received_callback_),
                                  *cluster_info_));
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          full_image_callback_,
          lens::CreateObjectsMojomArrayFromServerResponse(server_response),
          lens::CreateTextMojomFromServerResponse(server_response),
          /*is_error=*/false));
}

void LensOverlayQueryController::SendLatencyGen204IfEnabled(
    int64_t latency_ms) {
  if (lens::features::GetLensOverlaySendLatencyGen204() &&
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven()) {
    std::string query =
        base::StringPrintf("gen_204?atyp=csi&%s=%s&rt=fpof.%s&s=web",
                           kGen204IdentifierQueryParameter,
                           base::NumberToString(gen204_id_).c_str(),
                           base::NumberToString(latency_ms).c_str());
    auto fetch_url = GURL(TemplateURLServiceFactory::GetForProfile(profile_)
                              ->search_terms_data()
                              .GoogleBaseURLValue())
                         .Resolve(query);
    auto request = std::make_unique<network::ResourceRequest>();
    request->url = fetch_url;
    gen204_loader_ = network::SimpleURLLoader::Create(std::move(request),
                                                      kTrafficAnnotationTag);
    gen204_loader_->DownloadToString(
        profile_->GetURLLoaderFactory().get(),
        base::BindOnce(&LensOverlayQueryController::OnGen204LoaderComplete,
                       weak_ptr_factory_.GetWeakPtr()),
        kMaxDownloadBytes);
  }
}

void LensOverlayQueryController::OnGen204LoaderComplete(
    std::unique_ptr<std::string> response_body) {
  gen204_loader_.reset();
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
                                nullptr, /*is_error=*/true));
}

void LensOverlayQueryController::EndQuery() {
  full_image_endpoint_fetcher_.reset();
  interaction_endpoint_fetcher_.reset();
  cluster_info_received_callback_.Reset();
  access_token_fetcher_.reset();
  page_url_.reset();
  page_title_.reset();
  cluster_info_.reset();
  query_controller_state_ = QueryControllerState::kOff;
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

void LensOverlayQueryController::SendMultimodalRequest(
    lens::mojom::CenterRotatedBoxPtr region,
    const std::string& query_text,
    lens::LensOverlaySelectionType multimodal_selection_type,
    std::map<std::string, std::string> additional_search_query_params) {
  if (base::TrimWhitespaceASCII(query_text, base::TRIM_ALL).empty()) {
    return;
  }
  SendInteraction(/*region=*/std::move(region),
                  /*query_text=*/std::make_optional<std::string>(query_text),
                  /*object_id=*/std::nullopt, multimodal_selection_type,
                  additional_search_query_params,
                  /*region_bytes*/ std::nullopt);
}

void LensOverlayQueryController::SendTextOnlyQuery(
    const std::string& query_text,
    TextOnlyQueryType text_only_query_type,
    std::map<std::string, std::string> additional_search_query_params) {
  // Increment the request counter to cancel previously issued fetches.
  request_counter_++;

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

void LensOverlayQueryController::SendInteraction(
    lens::mojom::CenterRotatedBoxPtr region,
    std::optional<std::string> query_text,
    std::optional<std::string> object_id,
    lens::LensOverlaySelectionType selection_type,
    std::map<std::string, std::string> additional_search_query_params,
    std::optional<SkBitmap> region_bytes) {
  request_counter_++;
  int request_index = request_counter_;
  lens::LensOverlayClientLogs client_logs;
  client_logs.set_lens_overlay_entry_point(
      LenOverlayEntryPointFromInvocationSource(invocation_source_));
  if (lens::features::GetLensOverlaySendLatencyGen204()) {
    client_logs.set_paella_id(gen204_id_);
  }

  // Add the start time to the query params now, so that image downscaling
  // and other client processing time is included.
  additional_search_query_params =
      AddStartTimeQueryParam(additional_search_query_params);

  std::optional<lens::ImageCrop> image_crop =
      DownscaleAndEncodeBitmapRegionIfNeeded(
          original_screenshot_, region.Clone(), region_bytes, client_logs);
  FetchInteractionRequestAndGenerateUrlIfClusterInfoReady(
      request_index, region.Clone(), query_text, object_id, selection_type,
      additional_search_query_params, image_crop, client_logs);
}

void LensOverlayQueryController::
    FetchInteractionRequestAndGenerateUrlIfClusterInfoReady(
        int request_index,
        lens::mojom::CenterRotatedBoxPtr region,
        std::optional<std::string> query_text,
        std::optional<std::string> object_id,
        lens::LensOverlaySelectionType selection_type,
        std::map<std::string, std::string> additional_search_query_params,
        std::optional<lens::ImageCrop> image_crop,
        lens::LensOverlayClientLogs client_logs) {
  if (cluster_info_.has_value()) {
    FetchInteractionRequestAndGenerateLensSearchUrl(
        request_index, std::move(region), query_text, object_id, selection_type,
        additional_search_query_params, image_crop, client_logs,
        *cluster_info_);
  } else {
    cluster_info_received_callback_ =
        base::BindOnce(&LensOverlayQueryController::
                           FetchInteractionRequestAndGenerateLensSearchUrl,
                       weak_ptr_factory_.GetWeakPtr(), request_index,
                       std::move(region), query_text, object_id, selection_type,
                       additional_search_query_params, image_crop, client_logs);

    // If the cluster info is missing but we have already received a full image
    // response, the query must be restarted.
    if (query_controller_state_ ==
            QueryControllerState::kReceivedFullImageResponse ||
        query_controller_state_ ==
            QueryControllerState::kReceivedFullImageErrorResponse) {
      PrepareAndFetchFullImageRequest();
    }
  }

  if (image_crop.has_value()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(thumbnail_created_callback_,
                                  image_crop->image().image_content()));
  }
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
  } else {
    // There should be a region or an object id in the request.
    NOTREACHED_IN_MIGRATION();
  }

  server_request.mutable_interaction_request()
      ->mutable_interaction_request_metadata()
      ->CopyFrom(interaction_request_metadata);
  return server_request;
}

void LensOverlayQueryController::
    FetchInteractionRequestAndGenerateLensSearchUrl(
        int request_index,
        lens::mojom::CenterRotatedBoxPtr region,
        std::optional<std::string> query_text,
        std::optional<std::string> object_id,
        lens::LensOverlaySelectionType selection_type,
        std::map<std::string, std::string> additional_search_query_params,
        std::optional<lens::ImageCrop> image_crop,
        lens::LensOverlayClientLogs client_logs,
        lens::LensOverlayClusterInfo cluster_info) {
  if (request_index != request_counter_) {
    // Early exit if this is an old request.
    return;
  }

  if (lens::features::GetLensOverlaySendLatencyGen204()) {
    additional_search_query_params.insert(
        {kGen204IdentifierQueryParameter,
         base::NumberToString(gen204_id_).c_str()});
  }

  // The visual search interaction log data should be added as late as possible,
  // so that is_parent_query can be accurately set if the user issues multiple
  // interactions in quick succession.
  additional_search_query_params = AddVisualSearchInteractionLogData(
      additional_search_query_params, selection_type);

  // Update the analytics id of the request id for the new interaction.
  request_id_generator_->CreateNewAnalyticsId();

  // Fetch the interaction request.
  lens::LensOverlayServerRequest server_request = CreateInteractionRequest(
      std::move(region), query_text, object_id, image_crop, client_logs,
      request_id_generator_->GetNextRequestId());
  CreateAndFetchEndpointFetcher(
      server_request,
      base::BindOnce(
          &LensOverlayQueryController::OnInteractionEndpointFetcherCreated,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(
          &LensOverlayQueryController::InteractionFetchResponseHandler,
          weak_ptr_factory_.GetWeakPtr()));

  // Generate and send the Lens search url.
  lens::proto::LensOverlayUrlResponse lens_overlay_url_response;
  lens_overlay_url_response.set_url(
      lens::BuildLensSearchURL(
          query_text, request_id_generator_->GetNextRequestId(), cluster_info,
          additional_search_query_params, invocation_source_, use_dark_mode_)
          .spec());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(url_callback_, lens_overlay_url_response));
}

void LensOverlayQueryController::InteractionFetchResponseHandler(
    std::unique_ptr<EndpointResponse> response) {
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

  lens::proto::LensOverlayInteractionResponse lens_overlay_interaction_response;
  lens_overlay_interaction_response.set_suggest_signals(
      server_response.interaction_response().encoded_response());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(interaction_data_callback_,
                                lens_overlay_interaction_response));
}

void LensOverlayQueryController::RunInteractionCallbackForError() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(interaction_data_callback_,
                                lens::proto::LensOverlayInteractionResponse()));
}

void LensOverlayQueryController::ResetRequestClusterInfoState() {
  cluster_info_received_callback_.Reset();
  interaction_endpoint_fetcher_.reset();
  cluster_info_ = std::nullopt;
  request_id_generator_->ResetRequestId();
  parent_query_sent_ = false;
}

void LensOverlayQueryController::CreateAndFetchEndpointFetcher(
    lens::LensOverlayServerRequest request_data,
    base::OnceCallback<void(std::unique_ptr<EndpointFetcher>)>
        fetcher_created_callback,
    EndpointFetcherCallback fetched_response_callback) {
  // Use OAuth if the flag is enabled and the user is logged in.
  if (lens::features::UseOauthForLensOverlayRequests() && identity_manager_ &&
      identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    signin::AccessTokenFetcher::TokenCallback token_callback =
        base::BindOnce(&lens::CreateOAuthHeader)
            .Then(base::BindOnce(&LensOverlayQueryController::FetchEndpoint,
                                 weak_ptr_factory_.GetWeakPtr(), request_data,
                                 std::move(fetcher_created_callback),
                                 std::move(fetched_response_callback)));
    signin::ScopeSet oauth_scopes;
    oauth_scopes.insert(GaiaConstants::kLensOAuth2Scope);

    // If an access token fetcher is already in flight, it is intentionally
    // replaced by this newer one.
    access_token_fetcher_ =
        std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
            kOAuthConsumerName,
            identity_manager_, oauth_scopes, std::move(token_callback),
            signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
            signin::ConsentLevel::kSignin);
    return;
  }

  // Fall back to fetching the endpoint directly using API key.
  FetchEndpoint(request_data, std::move(fetcher_created_callback),
                std::move(fetched_response_callback),
                /*headers=*/std::vector<std::string>());
}

void LensOverlayQueryController::OnFullImageEndpointFetcherCreated(
    std::unique_ptr<EndpointFetcher> endpoint_fetcher) {
  full_image_endpoint_fetcher_ = std::move(endpoint_fetcher);
}

void LensOverlayQueryController::OnInteractionEndpointFetcherCreated(
    std::unique_ptr<EndpointFetcher> endpoint_fetcher) {
  interaction_endpoint_fetcher_ = std::move(endpoint_fetcher);
}

void LensOverlayQueryController::FetchEndpoint(
    lens::LensOverlayServerRequest request_data,
    base::OnceCallback<void(std::unique_ptr<EndpointFetcher>)>
        fetcher_created_callback,
    EndpointFetcherCallback fetched_response_callback,
    std::vector<std::string> headers) {
  access_token_fetcher_.reset();
  std::string request_data_string;
  CHECK(request_data.SerializeToString(&request_data_string));
  std::vector<std::string> cors_exempt_headers;

  variations::mojom::VariationsHeadersPtr variations =
      variations_client_->GetVariationsHeaders();
  if (!variations.is_null()) {
    cors_exempt_headers.push_back(kClientDataHeader);
    // The endpoint is always a Google property.
    cors_exempt_headers.push_back(variations->headers_map.at(
        variations::mojom::GoogleWebVisibility::FIRST_PARTY));
  }

  GURL fetch_url = GURL(lens::features::GetLensOverlayEndpointURL());
  if (cluster_info_.has_value()) {
    // The endpoint fetches should use the server session id from the cluster
    // info.
    fetch_url = net::AppendOrReplaceQueryParameter(
        fetch_url, kSessionIdQueryParameterKey,
        cluster_info_->server_session_id());
  }

  std::unique_ptr<EndpointFetcher> endpoint_fetcher =
      std::make_unique<EndpointFetcher>(
          /*url_loader_factory=*/g_browser_process->shared_url_loader_factory(),
          /*url=*/fetch_url,
          /*http_method=*/kHttpMethod,
          /*content_type=*/kContentType,
          base::Milliseconds(
              lens::features::GetLensOverlayServerRequestTimeout()),
          /*post_data=*/request_data_string,
          /*headers=*/headers,
          /*cors_exempt_headers=*/cors_exempt_headers,
          /*annotation_tag=*/kTrafficAnnotationTag,
          /*is_stable_channel=*/chrome::GetChannel() ==
              version_info::Channel::STABLE);
  EndpointFetcher* fetcher = endpoint_fetcher.get();
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(fetcher_created_callback),
                                std::move(endpoint_fetcher)));
  fetcher->PerformRequest(std::move(fetched_response_callback),
                          google_apis::GetAPIKey().c_str());
}

}  // namespace lens
