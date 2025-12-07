// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_query_controller.h"

#include <optional>

#include "base/base64url.h"
#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lens/core/mojom/geometry.mojom.h"
#include "chrome/browser/lens/core/mojom/overlay_object.mojom-forward.h"
#include "chrome/browser/lens/core/mojom/text.mojom-forward.h"
#include "chrome/browser/lens/core/mojom/text.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/lens/lens_overlay_gen204_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_image_helper.h"
#include "chrome/browser/ui/lens/lens_overlay_proto_converter.h"
#include "chrome/browser/ui/lens/lens_overlay_url_builder.h"
#include "chrome/browser/ui/lens/lens_search_feature_flag_utils.h"
#include "chrome/common/channel_info.h"
#include "components/base32/base32.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/lens/lens_overlay_permission_utils.h"
#include "components/lens/lens_payload_construction.h"
#include "components/lens/lens_request_construction.h"
#include "components/lens/lens_url_utils.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "components/lens/ref_counted_lens_overlay_client_logs.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
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
#include "third_party/lens_server_proto/lens_overlay_document.pb.h"
#include "third_party/lens_server_proto/lens_overlay_filters.pb.h"
#include "third_party/lens_server_proto/lens_overlay_image_crop.pb.h"
#include "third_party/lens_server_proto/lens_overlay_image_data.pb.h"
#include "third_party/lens_server_proto/lens_overlay_interaction_request_metadata.pb.h"
#include "third_party/lens_server_proto/lens_overlay_platform.pb.h"
#include "third_party/lens_server_proto/lens_overlay_polygon.pb.h"
#include "third_party/lens_server_proto/lens_overlay_request_id.pb.h"
#include "third_party/lens_server_proto/lens_overlay_request_type.pb.h"
#include "third_party/lens_server_proto/lens_overlay_service_deps.pb.h"
#include "third_party/lens_server_proto/lens_overlay_surface.pb.h"
#include "third_party/lens_server_proto/lens_overlay_visual_search_interaction_data.pb.h"
#include "ui/gfx/geometry/rect.h"

using endpoint_fetcher::CredentialsMode;
using endpoint_fetcher::EndpointFetcher;
using endpoint_fetcher::EndpointFetcherCallback;
using endpoint_fetcher::EndpointResponse;
using endpoint_fetcher::HttpMethod;

namespace lens {

using LatencyType = LensOverlayGen204Controller::LatencyType;

namespace {

// The name string for the header for variations information.
constexpr char kContentTypeKey[] = "Content-Type";
constexpr char kContentType[] = "application/x-protobuf";
constexpr char kSessionIdQueryParameterKey[] = "gsessionid";
constexpr char kGen204IdentifierQueryParameter[] = "plla";
constexpr char kVisualSearchInteractionDataQueryParameterKey[] = "vsint";
constexpr char kVisualInputTypeQueryParameterKey[] = "vit";

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotationTag =
    net::DefineNetworkTrafficAnnotation("lens_overlay", R"(
        semantics {
          sender: "Lens"
          description: "A request to the service handling the Lens "
            "Overlay feature in Chrome."
          trigger: "The user triggered a Lens Overlay Flow by entering "
            "the experience via the right click menu option for "
            "searching images on the page."
          data: "If the contextual searchbox is enabled, the viewport "
            "screenshot, page content, page URL and user interaction data are "
            "sent to the server. Page content refers to the page the user is "
            "on, extracted, and sent to the server as bytes. If the contextual "
            "searchbox is disabled, only the screenshot of the current webpage "
            "viewport (image bytes) and user interaction data (coordinates of "
            "a box within the screenshot or tapped object-id) are sent."
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
          last_reviewed: "2024-11-06"
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

// Creates a request id with an invalid (negative) sequence id. This is used to
// indicate that the request id is invalid, e.g. to indicate that there is no
// valid interaction data associated with a text-only query in
// IsCurrentInteractionSequence.
std::unique_ptr<lens::LensOverlayRequestId> CreateInvalidRequestId() {
  auto request_id = std::make_unique<lens::LensOverlayRequestId>();
  request_id->set_sequence_id(-1);
  return request_id;
}

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

std::map<std::string, std::string> AddVisualInputTypeQueryParam(
    std::map<std::string, std::string> additional_search_query_params,
    lens::MimeType content_type) {
  std::string vitValue = lens::VitQueryParamValueForMimeType(content_type);
  additional_search_query_params.insert(
      {kVisualInputTypeQueryParameterKey, vitValue});
  return additional_search_query_params;
}

lens::LensOverlayInteractionRequestMetadata::Type ContentTypeToInteractionType(
    lens::MimeType content_type) {
  switch (content_type) {
    case lens::MimeType::kPdf:
      return lens::LensOverlayInteractionRequestMetadata::PDF_QUERY;
    case lens::MimeType::kHtml:
    case lens::MimeType::kPlainText:
    case lens::MimeType::kAnnotatedPageContent:
      return lens::LensOverlayInteractionRequestMetadata::WEBPAGE_QUERY;
    case lens::MimeType::kUnknown:
      break;
    case lens::MimeType::kImage:
    case lens::MimeType::kVideo:
    case lens::MimeType::kAudio:
    case lens::MimeType::kJson:
      // These content types are not supported for the page content upload flow.
      NOTREACHED() << "Unsupported option in page content upload";
  }
  return lens::LensOverlayInteractionRequestMetadata::CONTEXTUAL_SEARCH_QUERY;
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
    case lens::LensOverlayInvocationSource::kContentAreaContextMenuText:
      return lens::LensOverlayClientLogs::TEXT_CONTEXT_MENU;
    case lens::LensOverlayInvocationSource::kContentAreaContextMenuVideo:
      return lens::LensOverlayClientLogs::VIDEO_CONTEXT_MENU;
    case lens::LensOverlayInvocationSource::kOmnibox:
      return lens::LensOverlayClientLogs::OMNIBOX_BUTTON;
    case lens::LensOverlayInvocationSource::kOmniboxContextualSuggestion:
      return lens::LensOverlayClientLogs::OMNIBOX_CONTEXTUAL_SUGGESTION;
    case lens::LensOverlayInvocationSource::kOmniboxPageAction:
      return lens::LensOverlayClientLogs::OMNIBOX_PAGE_ACTION;
    case lens::LensOverlayInvocationSource::kHomeworkActionChip:
      return lens::LensOverlayClientLogs::HOMEWORK_ACTION_CHIP;
    case lens::LensOverlayInvocationSource::kToolbar:
      return lens::LensOverlayClientLogs::TOOLBAR_BUTTON;
    case lens::LensOverlayInvocationSource::kFindInPage:
      return lens::LensOverlayClientLogs::FIND_IN_PAGE;
    case lens::LensOverlayInvocationSource::kLVFShutterButton:
    case lens::LensOverlayInvocationSource::kLVFGallery:
    case lens::LensOverlayInvocationSource::kContextMenu:
    case lens::LensOverlayInvocationSource::kAIHub:
    case lens::LensOverlayInvocationSource::kFREPromo:
      NOTREACHED() << "Invocation source not supported.";
  }
  return lens::LensOverlayClientLogs::UNKNOWN_ENTRY_POINT;
}

// Divides the content_bytes into small chunks, which are then compressed.
std::vector<std::string> MakeChunks(base::span<const uint8_t> content_bytes) {
  base::SpanReader reader(content_bytes);
  size_t max_chunk_size = lens::features::GetLensOverlayChunkSizeBytes();
  std::vector<std::string> chunks;

  while (reader.remaining() > 0) {
    size_t chunk_size = std::min(reader.remaining(), max_chunk_size);
    auto current_chunk = reader.Read(chunk_size);
    CHECK(current_chunk.has_value());

    std::string chunk;
    const bool success = lens::ZstdCompressBytes(current_chunk.value(), &chunk);
    if (!success) {
      // If any of the chunks fail to compress, then the request should fail.
      return std::vector<std::string>();
    }

    chunks.push_back(chunk);
  }
  return chunks;
}

// Creates the lens::LensOverlayUploadChunkRequest for the given chunk.
lens::LensOverlayUploadChunkRequest CreateUploadChunkRequest(
    int64_t chunk_id,
    int64_t total_chunks,
    std::string chunk,
    lens::LensOverlayRequestContext request_context) {
  lens::LensOverlayUploadChunkRequest request;
  request.mutable_request_context()->CopyFrom(request_context);
  request.mutable_debug_options()->set_total_chunks(total_chunks);
  request.set_chunk_id(chunk_id);
  request.mutable_chunk_bytes()->assign(chunk.begin(), chunk.end());
  return request;
}

// Returns the lens::Payload to be sent after uploading chunked data using the
// repeated Content field instead of the deprecated payload fields.
lens::Payload CreatePageContentPayloadForChunks(
    base::span<const lens::PageContent> page_content,
    lens::MimeType primary_content_type,
    GURL page_url,
    std::optional<std::string> page_title,
    int64_t total_stored_chunks,
    bool is_read_retry) {
  lens::Payload payload;
  auto* content = payload.mutable_content();

  if (!page_url.is_empty()) {
    content->set_webpage_url(page_url.spec());
  }
  if (page_title.has_value() && !page_title.value().empty()) {
    content->set_webpage_title(page_title.value());
  }

  auto* content_data = content->add_content_data();
  content_data->set_content_type(
      lens::MimeTypeToContentType(primary_content_type));
  content_data->mutable_stored_chunk_options()->set_read_stored_chunks(true);
  content_data->mutable_stored_chunk_options()->set_total_stored_chunks(
      total_stored_chunks);
  content_data->mutable_stored_chunk_options()->set_is_read_retry(
      is_read_retry);
  content_data->set_compression_type(lens::CompressionType::ZSTD);
  return payload;
}

// Returns the lens::Payload using the repeated Content field instead of the
// deprecated payload fields.
lens::Payload CreatePageContentPayload(
    base::span<const lens::PageContent> page_contents,
    GURL page_url,
    std::optional<std::string> page_title) {
  lens::Payload payload;
  auto* content = payload.mutable_content();

  if (!page_url.is_empty()) {
    content->set_webpage_url(page_url.spec());
  }
  if (page_title.has_value() && !page_title.value().empty()) {
    content->set_webpage_title(page_title.value());
  }

  for (const lens::PageContent& page_content : page_contents) {
    auto* content_data = content->add_content_data();
    content_data->set_content_type(
        MimeTypeToContentType(page_content.content_type_));

    // Compress PDF bytes.
    if (page_content.content_type_ == lens::MimeType::kPdf) {
      // If compression is successful, set the compression type and return.
      // Otherwise, fall back to the original bytes.
      if (lens::ZstdCompressBytes(page_content.bytes_,
                                  content_data->mutable_data())) {
        content_data->set_compression_type(lens::CompressionType::ZSTD);
        continue;
      }
    }

    // Add non compressed bytes. This happens if compression fails or its not
    // a PDF.
    content_data->mutable_data()->assign(page_content.bytes_.begin(),
                                         page_content.bytes_.end());
  }

  return payload;
}

}  // namespace

PageContent::PageContent() : content_type_(lens::MimeType::kUnknown) {}
PageContent::PageContent(std::vector<uint8_t> bytes,
                         lens::MimeType content_type)
    : bytes_(bytes), content_type_(content_type) {}
PageContent::PageContent(const PageContent& other) = default;
PageContent::~PageContent() = default;

LensOverlayQueryController::LensOverlayQueryController(
    LensOverlayFullImageResponseCallback full_image_callback,
    LensOverlayUrlResponseCallback url_callback,
    LensOverlayInteractionResponseCallback interaction_response_callback,
    LensOverlaySuggestInputsCallback suggest_inputs_callback,
    LensOverlayThumbnailCreatedCallback thumbnail_created_callback,
    UploadProgressCallback page_content_upload_progress_callback,
    variations::VariationsClient* variations_client,
    signin::IdentityManager* identity_manager,
    Profile* profile,
    lens::LensOverlayInvocationSource invocation_source,
    bool use_dark_mode,
    lens::LensOverlayGen204Controller* gen204_controller)
    : full_image_callback_(std::move(full_image_callback)),
      interaction_response_callback_(std::move(interaction_response_callback)),
      suggest_inputs_callback_(std::move(suggest_inputs_callback)),
      thumbnail_created_callback_(std::move(thumbnail_created_callback)),
      page_content_upload_progress_callback_(
          std::move(page_content_upload_progress_callback)),
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
  compression_task_runner_ = base::ThreadPool::CreateTaskRunner(
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  encoding_task_tracker_ = std::make_unique<base::CancelableTaskTracker>();
  compression_task_tracker_ = std::make_unique<base::CancelableTaskTracker>();
}

LensOverlayQueryController::~LensOverlayQueryController() {
  EndQuery();
}

void LensOverlayQueryController::StartQueryFlow(
    const SkBitmap& screenshot,
    GURL page_url,
    std::optional<std::string> page_title,
    std::vector<lens::mojom::CenterRotatedBoxPtr> significant_region_boxes,
    base::span<const lens::PageContent> underlying_page_contents,
    lens::MimeType primary_content_type,
    std::optional<uint32_t> pdf_current_page,
    float ui_scale_factor,
    base::TimeTicks invocation_time) {
  original_screenshot_ = screenshot;
  page_url_ = page_url;
  page_title_ = page_title;
  significant_region_boxes_ = std::move(significant_region_boxes);
  underlying_page_contents_ = underlying_page_contents;
  primary_content_type_ = primary_content_type;
  pdf_current_page_ = pdf_current_page;
  ui_scale_factor_ = ui_scale_factor;
  invocation_time_ = invocation_time;
  gen204_id_ = base::RandUint64();
  gen204_controller_->OnQueryFlowStart(invocation_source_, profile_,
                                       gen204_id_);

  if (primary_content_type_ != lens::MimeType::kUnknown) {
    suggest_inputs_.set_contextual_visual_input_type(
        lens::VitQueryParamValueForMimeType(primary_content_type_));
    RunSuggestInputsCallback();
  }

  // Reset translation languages in case they were set in a previous request.
  translate_options_.reset();

  PrepareAndFetchFullImageRequest();
}

void LensOverlayQueryController::EndQuery() {
  ResetPageContentData();
  gen204_controller_->OnQueryFlowEnd();
  full_image_endpoint_fetcher_.reset();
  interaction_endpoint_fetcher_.reset();
  pending_interaction_callback_.Reset();
  cluster_info_access_token_fetcher_.reset();
  full_image_access_token_fetcher_.reset();
  interaction_access_token_fetcher_.reset();
  page_url_ = GURL();
  page_title_.reset();
  translate_options_.reset();
  cluster_info_.reset();
  encoding_task_tracker_->TryCancelAll();
  compression_task_tracker_->TryCancelAll();
  query_controller_state_ = QueryControllerState::kOff;
}

void LensOverlayQueryController::MaybeRestartQueryFlow() {
  if (query_controller_state_ == QueryControllerState::kClusterInfoExpired ||
      query_controller_state_ == QueryControllerState::kWaitingForPermissions) {
    PrepareAndFetchFullImageRequest();
  }
}

void LensOverlayQueryController::SendFullPageTranslateQuery(
    const std::string& source_language,
    const std::string& target_language) {
  CHECK(query_controller_state_ != QueryControllerState::kOff)
      << "SendFullPageTranslateQuery called when query controller is off";
  translate_options_ = TranslateOptions(source_language, target_language);

  // Send a normal full image request. The parameters to make it a translate
  // request will be set when the actual request is sent based on the instance
  // variables.
  PrepareAndFetchFullImageRequest();
}

void LensOverlayQueryController::SendEndTranslateModeQuery() {
  CHECK(query_controller_state_ != QueryControllerState::kOff)
      << "SendEndTranslateModeQuery called when query controller is off";

  translate_options_.reset();
  PrepareAndFetchFullImageRequest();
}

void LensOverlayQueryController::ResetPageContentData() {
  underlying_page_contents_ = base::span<const lens::PageContent>();
  primary_content_type_ = lens::MimeType::kUnknown;
  pdf_current_page_ = std::nullopt;
  page_url_ = GURL();
  page_title_ = std::nullopt;
  partial_content_ = base::span<const std::u16string>();
}

void LensOverlayQueryController::SendUpdatedPageContent(
    std::optional<base::span<const lens::PageContent>> underlying_page_content,
    std::optional<lens::MimeType> primary_content_type,
    std::optional<GURL> new_page_url,
    std::optional<std::string> new_page_title,
    std::optional<uint32_t> pdf_current_page,
    const SkBitmap& screenshot) {
  CHECK(query_controller_state_ != QueryControllerState::kOff)
      << "SendUpdatedPageContent called when query controller is off";

  if (underlying_page_content.has_value()) {
    underlying_page_contents_ = underlying_page_content.value();
    primary_content_type_ = primary_content_type.value();
    page_url_ = new_page_url.value();
    page_title_ = new_page_title;
  }
  pdf_current_page_ = pdf_current_page;
  if (!screenshot.drawsNothing()) {
    original_screenshot_ = screenshot;
  }

  suggest_inputs_.set_contextual_visual_input_type(
      VitQueryParamValueForMimeType(primary_content_type_));
  RunSuggestInputsCallback();

  if (query_controller_state_ ==
      QueryControllerState::kAwaitingClusterInfoResponse) {
    // If we are waiting for the cluster info response, we should not send the
    // page content update request immediately. Instead, the cluster info
    // response handler will call PrepareAndFetchFullImageRequest and
    // PrepareAndFetchPageContentRequest.
    return;
  }
  if (!screenshot.drawsNothing()) {
    PrepareAndFetchFullImageRequest();
  }
  if (underlying_page_content.has_value()) {
    PrepareAndFetchPageContentRequest();
  }
}

void LensOverlayQueryController::SendPartialPageContentRequest(
    base::span<const std::u16string> partial_content) {
  CHECK(query_controller_state_ != QueryControllerState::kOff)
      << "SendPartialPageContentRequest called when query controller is off";
  partial_content_ = partial_content;

  PrepareAndFetchPartialPageContentRequest();
}

void LensOverlayQueryController::SendRegionSearch(
    base::Time query_start_time,
    lens::mojom::CenterRotatedBoxPtr region,
    lens::LensOverlaySelectionType lens_selection_type,
    std::map<std::string, std::string> additional_search_query_params,
    std::optional<SkBitmap> region_bytes) {
  CHECK(query_controller_state_ != QueryControllerState::kOff)
      << "SendRegionSearch called when query controller is off";

  SendInteraction(query_start_time, /*region=*/std::move(region),
                  /*query_text=*/std::nullopt,
                  /*object_id=*/std::nullopt, lens_selection_type,
                  additional_search_query_params, region_bytes,
                  lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
}

void LensOverlayQueryController::SendContextualTextQuery(
    base::Time query_start_time,
    const std::string& query_text,
    lens::LensOverlaySelectionType lens_selection_type,
    std::map<std::string, std::string> additional_search_query_params) {
  CHECK(query_controller_state_ != QueryControllerState::kOff)
      << "SendContextualTextQuery called when query controller is off";
  if (underlying_page_contents_.empty()) {
    SendTextOnlyQuery(query_start_time, query_text, lens_selection_type,
                      additional_search_query_params);
    return;
  }

  // If the contextual search query shouldn't be sent now, hold it until the
  // full page content upload is finished and/or the full image query for an
  // updated screenshot is finished.
  if (!ShouldSendContextualSearchQuery()) {
    pending_contextual_query_callback_ = base::BindOnce(
        &LensOverlayQueryController::SendContextualTextQuery,
        weak_ptr_factory_.GetWeakPtr(), query_start_time, query_text,
        lens_selection_type, additional_search_query_params);
    if (lens::features::IsLensOverlayNonBlockingPrivacyNoticeEnabled() &&
        !cluster_info_.has_value()) {
      // If the cluster info is expired, restart a new query flow so the pending
      // interaction request will be sent once the cluster info is available.
      MaybeRestartQueryFlow();
    }
    return;
  }

  // Include the vit to get contextualized results.
  additional_search_query_params = AddVisualInputTypeQueryParam(
      additional_search_query_params, primary_content_type_);

  SendInteraction(query_start_time, /*region=*/nullptr, query_text,
                  /*object_id=*/std::nullopt, lens_selection_type,
                  additional_search_query_params, std::nullopt,
                  MimeTypeToMediaType(primary_content_type_,
                                      /*has_viewport_screenshot=*/true));
}

void LensOverlayQueryController::SendTextOnlyQuery(
    base::Time query_start_time,
    const std::string& query_text,
    lens::LensOverlaySelectionType lens_selection_type,
    std::map<std::string, std::string> additional_search_query_params) {
  CHECK(query_controller_state_ != QueryControllerState::kOff)
      << "SendTextOnlyQuery called when query controller is off";

  // Although the text only flow might not send an interaction request, we
  // should replace any in-flight interaction requests to cancel previously
  // issued fetches.
  latest_interaction_request_data_ = std::make_unique<LensServerFetchRequest>(
      CreateInvalidRequestId(),
      /*query_start_time_ms=*/base::TimeTicks::Now());

  // The visual search interaction log data should be added as late as possible,
  // so that is_parent_query can be accurately set if the user issues multiple
  // interactions in quick succession.
  if (lens::features::SendVisualSearchInteractionParamForLensTextQueries() &&
      IsLensTextSelectionType(lens_selection_type)) {
    visual_search_interaction_data_ =
        BuildVisualSearchInteractionLogData(query_text, lens_selection_type);
    std::string encoded_vsint = EncodeVisualSearchInteractionLogData(
        visual_search_interaction_data_.value());
    suggest_inputs_.set_encoded_visual_search_interaction_log_data(
        encoded_vsint);
    additional_search_query_params.insert(
        {kVisualSearchInteractionDataQueryParameterKey, encoded_vsint});
  } else {
    visual_search_interaction_data_.reset();
    suggest_inputs_.clear_encoded_visual_search_interaction_log_data();
  }
  suggest_inputs_.clear_encoded_image_signals();
  RunSuggestInputsCallback();

  lens::proto::LensOverlayUrlResponse lens_overlay_url_response;
  lens_overlay_url_response.set_url(
      lens::BuildTextOnlySearchURL(query_start_time, query_text, page_url_,
                                   page_title_, additional_search_query_params,
                                   invocation_source_, lens_selection_type,
                                   use_dark_mode_)
          .spec());
  lens_overlay_url_response.set_page_url(page_url_.spec());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(url_callback_, lens_overlay_url_response));
}

void LensOverlayQueryController::SendMultimodalRequest(
    base::Time query_start_time,
    lens::mojom::CenterRotatedBoxPtr region,
    const std::string& query_text,
    lens::LensOverlaySelectionType multimodal_selection_type,
    std::map<std::string, std::string> additional_search_query_params,
    std::optional<SkBitmap> region_bytes) {
  CHECK(query_controller_state_ != QueryControllerState::kOff)
      << "SendMultimodalRequest called when query controller is off";

  if (base::TrimWhitespaceASCII(query_text, base::TRIM_ALL).empty()) {
    return;
  }
  SendInteraction(query_start_time, /*region=*/std::move(region), query_text,
                  /*object_id=*/std::nullopt, multimodal_selection_type,
                  additional_search_query_params, region_bytes,
                  lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
}

void LensOverlayQueryController::SendTaskCompletionGen204IfEnabled(
    lens::mojom::UserAction user_action) {
  SendTaskCompletionGen204IfEnabled(latest_encoded_analytics_id_, user_action,
                                    latest_request_id_);
}

void LensOverlayQueryController::SendSemanticEventGen204IfEnabled(
    lens::mojom::SemanticEvent event) {
  std::optional<lens::LensOverlayRequestId> request_id = std::nullopt;
  if (event == lens::mojom::SemanticEvent::kTextGleamsViewStart) {
    request_id = std::make_optional(latest_request_id_);
  }
  SendSemanticEventGen204IfEnabled(event, request_id);
}

std::unique_ptr<lens::LensOverlayRequestId>
LensOverlayQueryController::GetNextRequestId(
    RequestIdUpdateMode update_mode,
    lens::LensOverlayRequestId::MediaType media_type) {
  std::unique_ptr<lens::LensOverlayRequestId> request_id =
      request_id_generator_->GetNextRequestId(update_mode, media_type);
  latest_request_id_ = *request_id.get();
  latest_encoded_analytics_id_ =
      request_id_generator_->GetBase32EncodedAnalyticsId();
  std::string encoded_request_id = Base64EncodeRequestId(*request_id);
  suggest_inputs_.set_encoded_request_id(encoded_request_id);
  RunSuggestInputsCallback();
  return request_id;
}

void LensOverlayQueryController::RunSuggestInputsCallback() {
  suggest_inputs_.set_send_gsession_vsrid_for_contextual_suggest(true);
  suggest_inputs_.set_send_gsession_vsrid_vit_for_lens_suggest(
      lens::features::GetLensOverlaySendLensInputsForLensSuggest() ||
      lens::features::GetAimSuggestionsEnabled());
  suggest_inputs_.set_send_vsint_for_lens_suggest(
      lens::features::
          GetLensOverlaySendLensVisualInteractionDataForLensSuggest());
  if (cluster_info_.has_value()) {
    suggest_inputs_.set_search_session_id(cluster_info_->search_session_id());
  } else {
    suggest_inputs_.clear_search_session_id();
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(suggest_inputs_callback_, suggest_inputs_));
}

void LensOverlayQueryController::ResetRequestClusterInfoStateForTesting() {
  ResetRequestClusterInfoState();
}

std::unique_ptr<EndpointFetcher>
LensOverlayQueryController::CreateEndpointFetcher(
    std::string request_string,
    const GURL& fetch_url,
    HttpMethod http_method,
    base::TimeDelta timeout,
    const std::vector<std::string>& request_headers,
    const std::vector<std::string>& cors_exempt_headers,
    UploadProgressCallback upload_progress_callback) {
  return std::make_unique<EndpointFetcher>(
      /*url_loader_factory=*/profile_
          ? profile_->GetURLLoaderFactory().get()
          : g_browser_process->shared_url_loader_factory(),
      /*identity_manager=*/nullptr,
      EndpointFetcher::RequestParams::Builder(http_method,
                                              kTrafficAnnotationTag)
          .SetAuthType(endpoint_fetcher::CHROME_API_KEY)
          .SetChannel(chrome::GetChannel())
          .SetContentType(kContentType)
          .SetCorsExemptHeaders(cors_exempt_headers)
          .SetCredentialsMode(CredentialsMode::kInclude)
          .SetHeaders(request_headers)
          .SetPostData(std::move(request_string))
          .SetSetSiteForCookies(true)
          .SetTimeout(timeout)
          .SetUrl(fetch_url)
          .SetUploadProgressCallback(std::move(upload_progress_callback))
          .Build());
}

void LensOverlayQueryController::SendLatencyGen204IfEnabled(
    lens::LensOverlayGen204Controller::LatencyType latency_type,
    base::TimeTicks start_time_ticks,
    std::string vit_query_param_value,
    std::optional<base::TimeDelta> cluster_info_latency,
    std::optional<std::string> encoded_analytics_id,
    std::optional<lens::LensOverlayRequestId> request_id) {
  base::TimeDelta latency_duration = base::TimeTicks::Now() - start_time_ticks;
  gen204_controller_->SendLatencyGen204IfEnabled(
      latency_type, latency_duration, vit_query_param_value,
      cluster_info_latency, encoded_analytics_id, request_id);
}

void LensOverlayQueryController::SendTaskCompletionGen204IfEnabled(
    std::string encoded_analytics_id,
    lens::mojom::UserAction user_action,
    lens::LensOverlayRequestId request_id) {
  gen204_controller_->SendTaskCompletionGen204IfEnabled(
      encoded_analytics_id, user_action, request_id);
}

void LensOverlayQueryController::SendSemanticEventGen204IfEnabled(
    lens::mojom::SemanticEvent event,
    std::optional<lens::LensOverlayRequestId> request_id) {
  gen204_controller_->SendSemanticEventGen204IfEnabled(event, request_id);
}

LensOverlayQueryController::LensServerFetchRequest::LensServerFetchRequest(
    std::unique_ptr<lens::LensOverlayRequestId> request_id,
    base::TimeTicks query_start_time)
    : request_id_(std::move(request_id)), query_start_time_(query_start_time) {}
LensOverlayQueryController::LensServerFetchRequest::~LensServerFetchRequest() =
    default;

std::string LensOverlayQueryController::GetVsridForNewTab() {
  std::unique_ptr<lens::LensOverlayRequestId> request_id =
      request_id_generator_->GetNextRequestId(
          RequestIdUpdateMode::kOpenInNewTab,
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  return Base64EncodeRequestId(*request_id);
}

void LensOverlayQueryController::FetchClusterInfoRequest() {
  query_controller_state_ = QueryControllerState::kAwaitingClusterInfoResponse;

  // There should not be any in-flight cluster info request.
  CHECK(!cluster_info_access_token_fetcher_);
  cluster_info_access_token_fetcher_ =
      CreateOAuthHeadersAndContinue(base::BindOnce(
          &LensOverlayQueryController::PerformClusterInfoFetchRequest,
          weak_ptr_factory_.GetWeakPtr(),
          /*query_start_time=*/base::TimeTicks::Now()));
}

void LensOverlayQueryController::PerformClusterInfoFetchRequest(
    base::TimeTicks query_start_time,
    std::vector<std::string> request_headers) {
  cluster_info_access_token_fetcher_.reset();

  // Add protobuf content type to the request headers.
  request_headers.push_back(kContentTypeKey);
  request_headers.push_back(kContentType);

  // Get client experiment variations to include in the request.
  std::vector<std::string> cors_exempt_headers =
      CreateVariationsHeaders(variations_client_);

  // Generate the URL to fetch.
  GURL fetch_url = GURL(lens::features::GetLensOverlayClusterInfoEndpointUrl());

  HttpMethod request_method;
  std::string request_string;
  request_method = HttpMethod::kPost;

  // Create the client context to include in the request.
  lens::LensOverlayClientContext client_context = CreateClientContext();
  lens::LensOverlayServerClusterInfoRequest request;
  request.set_enable_search_session_id(true);
  request.set_surface(client_context.surface());
  request.set_platform(client_context.platform());
  request.mutable_rendering_context()->CopyFrom(
      client_context.rendering_context());
  CHECK(request.SerializeToString(&request_string));

  // Create the EndpointFetcher, responsible for making the request using our
  // given params. Store in class variable to keep endpoint fetcher alive until
  // the request is made.
  cluster_info_endpoint_fetcher_ = CreateEndpointFetcher(
      std::move(request_string), fetch_url, request_method,
      base::Milliseconds(lens::features::GetLensOverlayServerRequestTimeout()),
      request_headers, cors_exempt_headers, base::DoNothing());

  // Finally, perform the request.
  cluster_info_endpoint_fetcher_->PerformRequest(
      base::BindOnce(
          &LensOverlayQueryController::ClusterInfoFetchResponseHandler,
          weak_ptr_factory_.GetWeakPtr(), query_start_time),
      google_apis::GetAPIKey().c_str());

  SendInitialLatencyGen204IfNotAlreadySent(
      LatencyType::kInvocationToInitialClusterInfoRequestSent,
      VitQueryParamValueForMimeType(primary_content_type_),
      /*request_id=*/std::nullopt);
}

void LensOverlayQueryController::ClusterInfoFetchResponseHandler(
    base::TimeTicks query_start_time,
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
  if (!server_response.ParseFromString(response->response)) {
    // If there was an error with the cluster info request, we should still try
    // and send the full image request as a fallback.
    PrepareAndFetchFullImageRequest();
    return;
  }

  // Store the cluster info.
  cluster_info_ = std::make_optional<lens::LensOverlayClusterInfo>();
  cluster_info_->set_server_session_id(server_response.server_session_id());
  cluster_info_->set_search_session_id(server_response.search_session_id());

  // If routing info is enabled, store the routing info to be included in
  // followup requests.
  if (lens::features::IsLensOverlayRoutingInfoEnabled() &&
      server_response.has_routing_info() &&
      !request_id_generator_->HasRoutingInfo()) {
    std::unique_ptr<lens::LensOverlayRequestId> request_id =
        request_id_generator_->SetRoutingInfo(server_response.routing_info());
    suggest_inputs_.set_encoded_request_id(Base64EncodeRequestId(*request_id));
  }

  // Update the suggest inputs with the cluster info's search session id.
  RunSuggestInputsCallback();

  // Clear the cluster info after its lifetime expires.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&LensOverlayQueryController::ResetRequestClusterInfoState,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Seconds(
          lens::features::GetLensOverlayClusterInfoLifetimeSeconds()));

  // Store the fetch response time.
  cluster_info_fetch_response_time_ = base::TimeTicks::Now() - query_start_time;

  // Continue with the full image request which will use the session id from the
  // cluster info we just received.
  PrepareAndFetchFullImageRequest();
  PrepareAndFetchPageContentRequest();
  PrepareAndFetchPartialPageContentRequest();
}

void LensOverlayQueryController::PrepareAndFetchFullImageRequest() {
  // If permissions have not yet been granted, exit early. Once permissions are
  // granted and the cluster info response is received,
  // PrepareAndFetchFullImageRequest will be called again.
  if (!DidUserGrantLensOverlayNeededPermissions(profile_->GetPrefs())) {
    query_controller_state_ = QueryControllerState::kWaitingForPermissions;
    return;
  }

  if (query_controller_state_ ==
      QueryControllerState::kAwaitingClusterInfoResponse) {
    // If we are still waiting for the cluster info response, we can't send the
    // full image request yet. Once the cluster info response is received,
    // PrepareAndFetchFullImageRequest will be called again.
    return;
  }

  // If the cluster info optimization is enabled, request the cluster info prior
  // to making the full image request. Also do this for the contextual search
  // flow since the request flow for contextual searchbox will fail without the
  // cluster info handshake.
  if (!cluster_info_ &&
      (lens::features::IsLensOverlayClusterInfoOptimizationEnabled() ||
       lens::IsLensOverlayContextualSearchboxEnabled())) {
    FetchClusterInfoRequest();
    return;
  }

  // If the screenshot draws nothing, return.
  if (original_screenshot_.drawsNothing()) {
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
  ref_counted_logs->client_logs().set_metrics_collection_disabled(
      !g_browser_process->GetMetricsServicesManager() ||
      !g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());
  ref_counted_logs->client_logs().set_lens_overlay_entry_point(
      LenOverlayEntryPointFromInvocationSource(invocation_source_));

  // Initialize latest_full_image_request_data_ with a next request id to
  // ensure once the async processes finish, no new full image request has
  // started.
  latest_full_image_request_data_ = std::make_unique<LensServerFetchRequest>(
      GetNextRequestId(initial_request_id_
                           ? RequestIdUpdateMode::kFullImageRequest
                           : RequestIdUpdateMode::kInitialRequest,
                       lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE),
      /*query_start_time=*/base::TimeTicks::Now());
  int current_sequence_id = latest_full_image_request_data_->sequence_id();

  // If this is the first full image request, store the request id for all the
  // other first batch of requests to use.
  if (initial_request_id_ == nullptr) {
    initial_request_id_ = std::make_unique<lens::LensOverlayRequestId>();
    initial_request_id_->CopyFrom(
        *latest_full_image_request_data_->request_id_);
  }

  // If there is a pending interaction, we can create and issue it now that the
  // cluster info and full-image request id are available.
  if (cluster_info_.has_value() && pending_interaction_callback_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(pending_interaction_callback_));
  }

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

  // The request ID is guaranteed to exist since it is set in the constructor
  // of latest_full_image_request_data_.
  DCHECK(latest_full_image_request_data_->request_id_);
  request_context.mutable_request_id()->CopyFrom(
      *latest_full_image_request_data_->request_id_);

  request_context.mutable_client_context()->CopyFrom(CreateClientContext());
  request.mutable_objects_request()->mutable_request_context()->CopyFrom(
      request_context);
  request.mutable_objects_request()->mutable_image_data()->CopyFrom(image_data);

  if (pdf_current_page_.has_value()) {
    request.mutable_objects_request()
        ->mutable_viewport_request_context()
        ->set_pdf_page_number(pdf_current_page_.value());
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
  CHECK(latest_full_image_request_data_->sequence_id() == sequence_id);
  if (latest_full_image_request_data_->request_ &&
      latest_full_image_request_data_->request_headers_) {
    PerformFullImageRequest();
  }
}

bool LensOverlayQueryController::IsCurrentFullImageSequence(int sequence_id) {
  CHECK(latest_full_image_request_data_);
  return latest_full_image_request_data_->sequence_id() == sequence_id;
}

void LensOverlayQueryController::PerformFullImageRequest() {
  PerformFetchRequest(
      latest_full_image_request_data_->request_.get(),
      latest_full_image_request_data_->request_headers_.get(),
      base::Milliseconds(lens::features::GetLensOverlayServerRequestTimeout()),
      base::BindOnce(
          &LensOverlayQueryController::OnFullImageEndpointFetcherCreated,
          weak_ptr_factory_.GetWeakPtr(),
          *latest_full_image_request_data_->request_id_.get()),
      base::BindOnce(&LensOverlayQueryController::FullImageFetchResponseHandler,
                     weak_ptr_factory_.GetWeakPtr(),
                     latest_full_image_request_data_->sequence_id()));
}

void LensOverlayQueryController::FullImageFetchResponseHandler(
    int request_sequence_id,
    std::unique_ptr<EndpointResponse> response) {
  // If this request sequence ID does not match the latest sent then we should
  // ignore the response.
  if (latest_full_image_request_data_->sequence_id() != request_sequence_id) {
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
  if (!server_response.ParseFromString(response->response)) {
    RunFullImageCallbackForError();
    return;
  }

  if (!server_response.has_objects_response()) {
    RunFullImageCallbackForError();
    return;
  }

  if (!cluster_info_.has_value()) {
    if (!server_response.objects_response().has_cluster_info()) {
      RunFullImageCallbackForError();
      return;
    }

    cluster_info_ = std::make_optional<lens::LensOverlayClusterInfo>();
    cluster_info_->CopyFrom(server_response.objects_response().cluster_info());

    // Clear the cluster info after its lifetime expires.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &LensOverlayQueryController::ResetRequestClusterInfoState,
            weak_ptr_factory_.GetWeakPtr()),
        base::Seconds(
            lens::features::GetLensOverlayClusterInfoLifetimeSeconds()));

    // If routing info is enabled, store the routing info to be included in
    // followup requests.
    if (lens::features::IsLensOverlayRoutingInfoEnabled() &&
        cluster_info_->has_routing_info() &&
        !request_id_generator_->HasRoutingInfo()) {
      std::unique_ptr<lens::LensOverlayRequestId> new_request_id =
          request_id_generator_->SetRoutingInfo(cluster_info_->routing_info());
      suggest_inputs_.set_encoded_request_id(
          Base64EncodeRequestId(*new_request_id));
    }
  }

  SendFullImageLatencyGen204IfEnabled(
      latest_full_image_request_data_->query_start_time_,
      translate_options_.has_value(), kImageVisualInputTypeQueryParameterValue);

  // Image signals and vsint are only valid after an interaction request.
  visual_search_interaction_data_.reset();
  suggest_inputs_.clear_encoded_image_signals();
  suggest_inputs_.clear_encoded_visual_search_interaction_log_data();
  RunSuggestInputsCallback();

  if (pending_interaction_callback_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(pending_interaction_callback_));
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
  // If permissions have not yet been granted, exit early. The full image
  // request will recall this method once permissions are granted and the
  // cluster info is fetched.
  if (!DidUserGrantLensOverlayNeededPermissions(profile_->GetPrefs())) {
    return;
  }

  if (query_controller_state_ == QueryControllerState::kClusterInfoExpired ||
      query_controller_state_ == QueryControllerState::kWaitingForPermissions) {
    // If the cluster info has expired, we need to refetch the cluster info. The
    // full image request will recall this method once the cluster info is
    // fetched.
    MaybeRestartQueryFlow();
    return;
  }

  if (underlying_page_contents_.empty() ||
      underlying_page_contents_.front().bytes_.empty()) {
    //  No need to send the request without underlying content bytes.
    return;
  }

  compression_task_tracker_->TryCancelAll();
  page_contents_request_start_time_ = base::TimeTicks::Now();
  page_content_request_in_progress_ = true;
  chunk_upload_in_progress_ = false;
  retrying_page_content_upload_ = false;
  remaining_upload_chunk_responses_ = 0;
  remaining_chunk_retries = lens::features::GetLensOverlayUploadChunkRetries();

  // The initial request id should be set by the time we get here. If not, call
  // below will crash.
  CHECK(initial_request_id_);
  auto media_type = MimeTypeToMediaType(primary_content_type_,
                                        /*has_viewport_screenshot=*/true);
  auto request_id =
      is_first_page_contents_request_
          ? *initial_request_id_
          : *GetNextRequestId(lens::RequestIdUpdateMode::kPageContentRequest,
                              media_type);
  if (is_first_page_contents_request_) {
    // The initial request id will have the media type set to IMAGE. Change it
    // to the correct media type for the page content request.
    request_id.set_media_type(media_type);
  }

  // Send a chunk request if the upload is a PDF larger than the chunk size.
  // If not, send a normal page content request.
  if (lens::features::IsLensOverlayUploadChunkingEnabled() &&
      primary_content_type_ == lens::MimeType::kPdf &&
      underlying_page_contents_.front().bytes_.size() >
          lens::features::GetLensOverlayChunkSizeBytes()) {
    chunk_upload_in_progress_ = true;
    // Post MakeChunks to a task off the main thread so compression does not
    // throttle the main thread.
    compression_task_tracker_->PostTaskAndReplyWithResult(
        compression_task_runner_.get(), FROM_HERE,
        base::BindOnce(&MakeChunks, underlying_page_contents_.front().bytes_),
        base::BindOnce(
            &LensOverlayQueryController::PrepareAndFetchUploadChunkRequests,
            weak_ptr_factory_.GetWeakPtr(), request_id));
  } else {
    // Post CreatePageContentPayload to a task off the main thread so
    // compression does not throttle the main thread.
    compression_task_tracker_->PostTaskAndReplyWithResult(
        compression_task_runner_.get(), FROM_HERE,
        base::BindOnce(&CreatePageContentPayload, underlying_page_contents_,
                       page_url_, page_title_),
        base::BindOnce(
            &LensOverlayQueryController::PrepareAndFetchPageContentRequestPart2,
            weak_ptr_factory_.GetWeakPtr(), request_id));
  }

  // If this is the second or later page content request, the partial page
  // content should no longer be considered first.
  if (!is_first_page_contents_request_) {
    is_first_partial_page_contents_request_ = false;
  }
  // Any subsequent page content requests will be considered non-first.
  is_first_page_contents_request_ = false;
}

void LensOverlayQueryController::PrepareAndFetchUploadChunkRequests(
    lens::LensOverlayRequestId request_id,
    std::vector<std::string> chunks) {
  if (!chunks.size()) {
    return;
  }
  chunk_progress = std::vector<size_t>(chunks.size());
  total_chunk_progress_ = 0;
  total_chunk_upload_size_ = 0;

  lens::LensOverlayRequestContext request_context;
  request_context.mutable_request_id()->CopyFrom(request_id);
  request_context.mutable_client_context()->CopyFrom(CreateClientContext());

  std::vector<lens::LensOverlayUploadChunkRequest> requests;
  for (size_t i = 0; i < chunks.size(); i++) {
    total_chunk_upload_size_ += chunks[i].size();
    requests.push_back(
        CreateUploadChunkRequest(i, chunks.size(), chunks[i], request_context));
  }
  pending_upload_chunk_requests_ = requests;
  upload_chunk_sequence_id = request_id.sequence_id();

  chunk_upload_access_token_fetcher_ =
      CreateOAuthHeadersAndContinue(base::BindOnce(
          &LensOverlayQueryController::PrepareAndFetchUploadChunkRequestsPart2,
          weak_ptr_factory_.GetWeakPtr()));
}

void LensOverlayQueryController::PrepareAndFetchUploadChunkRequestsPart2(
    std::vector<std::string> headers) {
  chunk_upload_access_token_fetcher_.reset();
  pending_upload_chunk_headers_ = headers;
  remaining_upload_chunk_responses_ = pending_upload_chunk_requests_.size();
  for (size_t i = 0; i < pending_upload_chunk_requests_.size(); i++) {
    FetchUploadChunkRequest(i);
  }
}

void LensOverlayQueryController::FetchUploadChunkRequest(
    size_t chunk_request_index) {
  auto& request = pending_upload_chunk_requests_[chunk_request_index];
  std::string request_string;
  CHECK(request.SerializeToString(&request_string));

  PerformFetchRequest(
      std::move(request_string), &pending_upload_chunk_headers_,
      base::Milliseconds(
          lens::features::GetLensOverlayUploadChunkRequestTimeoutMs()),
      base::BindOnce(
          &LensOverlayQueryController::OnChunkUploadEndpointFetcherCreated,
          weak_ptr_factory_.GetWeakPtr(),
          request.request_context().request_id()),
      base::BindOnce(&LensOverlayQueryController::UploadChunkResponseHandler,
                     weak_ptr_factory_.GetWeakPtr(),
                     request.request_context().request_id(),
                     pending_upload_chunk_requests_.size()),
      base::BindRepeating(
          &LensOverlayQueryController::UploadChunkProgressHandler,
          weak_ptr_factory_.GetWeakPtr(), chunk_request_index),
      GURL(lens::features::GetLensOverlayUploadChunkEndpointURL()));
}

void LensOverlayQueryController::UploadChunkResponseHandler(
    lens::LensOverlayRequestId request_id,
    size_t total_chunks,
    std::unique_ptr<EndpointResponse> response) {
  // If there is a newer sequence id, a new request has been initiated before
  // this one has completed. Do nothing and return.
  if (request_id.sequence_id() != upload_chunk_sequence_id) {
    return;
  }

  remaining_upload_chunk_responses_--;
  // If this is the last chunk to receive a response, perform the page content
  // request.
  if (remaining_upload_chunk_responses_ == 0) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&CreatePageContentPayloadForChunks,
                       underlying_page_contents_, primary_content_type_,
                       page_url_, page_title_, total_chunks,
                       retrying_page_content_upload_),
        base::BindOnce(
            &LensOverlayQueryController::PrepareAndFetchPageContentRequestPart2,
            weak_ptr_factory_.GetWeakPtr(), request_id));
  }
}

void LensOverlayQueryController::PrepareAndFetchPageContentRequestPart2(
    lens::LensOverlayRequestId request_id,
    lens::Payload payload) {
  // Create the request.
  lens::LensOverlayServerRequest request;
  lens::LensOverlayRequestContext request_context;

  request_context.mutable_request_id()->CopyFrom(request_id);
  request_context.mutable_client_context()->CopyFrom(CreateClientContext());
  request.mutable_objects_request()->mutable_request_context()->CopyFrom(
      request_context);

  request.mutable_objects_request()->mutable_payload()->CopyFrom(payload);

  // Save the request in case it needs to be resent. Modify the payload to set
  // the is_read_retry param. Note that this not used if
  // RetryUploadChunkRequests is called as a new payload is used instead.
  pending_page_content_request_.CopyFrom(request);
  pending_page_content_request_.mutable_objects_request()
      ->mutable_payload()
      ->mutable_content()
      ->mutable_content_data(0)
      ->mutable_stored_chunk_options()
      ->set_is_read_retry(true);

  page_content_access_token_fetcher_ = CreateOAuthHeadersAndContinue(
      base::BindOnce(&LensOverlayQueryController::PerformPageContentRequest,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request)));
}

void LensOverlayQueryController::PerformPageContentRequest(
    lens::LensOverlayServerRequest request,
    std::vector<std::string> headers) {
  page_content_access_token_fetcher_.reset();

  PerformFetchRequest(
      &request, &headers,
      base::Milliseconds(
          lens::features::GetLensOverlayPageContentRequestTimeoutMs()),
      base::BindOnce(
          &LensOverlayQueryController::OnPageContentEndpointFetcherCreated,
          weak_ptr_factory_.GetWeakPtr(),
          request.objects_request().request_context().request_id()),
      base::BindOnce(&LensOverlayQueryController::PageContentResponseHandler,
                     weak_ptr_factory_.GetWeakPtr(),
                     request.objects_request().request_context().request_id()),
      // If this is a chunked upload, upload progress will have already been
      // reported by the chunk uploads, so skip passing in the upload progress
      // handler here.
      chunk_upload_in_progress_
          ? base::NullCallback()
          : base::BindRepeating(
                &LensOverlayQueryController::PageContentUploadProgressHandler,
                weak_ptr_factory_.GetWeakPtr()));
}

void LensOverlayQueryController::PageContentResponseHandler(
    lens::LensOverlayRequestId request_id,
    std::unique_ptr<EndpointResponse> response) {
  page_content_endpoint_fetcher_.reset();

  // Ensure the page content upload doesn't need to be retried.
  // If it does, exit early.
  if (MaybeRetryPageContentUpload(std::move(response))) {
    return;
  }

  // The upload progress handler is not guaranteed to execute, so if a response
  // is received, mark the request as no longer in progress to allow the
  // interaction request to be sent.
  PageContentUploadFinished();

  // If the chunk uploads have already completed, or if upload chunking was not
  // done, this will send the gen204 ping and clear the endpoint fetchers.
  MaybeSendPageContentUploadLatencyGen204(request_id);
}

bool LensOverlayQueryController::MaybeRetryPageContentUpload(
    std::unique_ptr<EndpointResponse> response) {
  // Check if the server response contains missing chunk errors to handle.
  // Proceed without handling if out of retries.
  if (remaining_chunk_retries > 0) {
    remaining_chunk_retries--;
    lens::LensOverlayServerResponse server_response;
    bool parse_successful = server_response.ParseFromString(response->response);
    if (parse_successful &&
        server_response.error().error_type() ==
            LensOverlayServerError_ErrorType::
                LensOverlayServerError_ErrorType_MISSING_CHUNKS) {
      auto missing_chunks_metadata =
          server_response.error().missing_chunks_metadata();
      if (!missing_chunks_metadata.has_chunk_metadata()) {
        // Interaction request likely misrouted. Resend it.
        retrying_page_content_upload_ = true;
        page_content_access_token_fetcher_ =
            CreateOAuthHeadersAndContinue(base::BindOnce(
                &LensOverlayQueryController::PerformPageContentRequest,
                weak_ptr_factory_.GetWeakPtr(), pending_page_content_request_));
        return true;
      }
      if (missing_chunks_metadata.missing_chunk_ids_size() > 0) {
        // Missing chunks. Resend the missing chunks.
        retrying_page_content_upload_ = true;
        chunk_upload_access_token_fetcher_ =
            CreateOAuthHeadersAndContinue(base::BindOnce(
                &LensOverlayQueryController::RetryUploadChunkRequests,
                weak_ptr_factory_.GetWeakPtr(),
                server_response.error()
                    .missing_chunks_metadata()
                    .missing_chunk_ids()));
        return true;
      }
    }
  }
  return false;
}

void LensOverlayQueryController::RetryUploadChunkRequests(
    const google::protobuf::RepeatedField<int64_t>& chunk_ids,
    std::vector<std::string> headers) {
  chunk_upload_access_token_fetcher_.reset();
  pending_upload_chunk_headers_ = headers;
  remaining_upload_chunk_responses_ = chunk_ids.size();
  for (int64_t chunk_id : chunk_ids) {
    FetchUploadChunkRequest(chunk_id);
  }
}

void LensOverlayQueryController::MaybeSendPageContentUploadLatencyGen204(
    lens::LensOverlayRequestId request_id) {
  if (!page_content_request_in_progress_ &&
      remaining_upload_chunk_responses_ == 0) {
    chunk_upload_endpoint_fetchers_.clear();
    SendLatencyGen204IfEnabled(
        LatencyType::kPageContentUploadLatency,
        page_contents_request_start_time_,
        VitQueryParamValueForMimeType(primary_content_type_),
        /*cluster_info_latency=*/std::nullopt,
        /*encoded_analytics_id=*/std::nullopt,
        std::make_optional<lens::LensOverlayRequestId>(request_id));
  }
}

void LensOverlayQueryController::PageContentUploadProgressHandler(
    uint64_t position,
    uint64_t total) {
  if (page_content_upload_progress_callback_) {
    page_content_upload_progress_callback_.Run(position, total);
  }
}

void LensOverlayQueryController::UploadChunkProgressHandler(
    size_t chunk_request_index,
    uint64_t position,
    uint64_t total) {
  // Caller of this callback should be sequenced.

  // Save the reported position of each chunk to the chunk_progress vector.
  // Instead of repeatedly summing over the entire vector, increment the total
  // chunk progress by the difference between the currently reported position
  // and the last reported position of the chunk.
  total_chunk_progress_ += position - chunk_progress[chunk_request_index];
  chunk_progress[chunk_request_index] = position;

  // Overhead causes the total progress to be very slightly above the total
  // upload size (by about 0.01%). Cap to avoid reporting progress > 100%.
  if (total_chunk_progress_ > total_chunk_upload_size_) {
    total_chunk_progress_ = total_chunk_upload_size_;
  }

  if (page_content_upload_progress_callback_) {
    page_content_upload_progress_callback_.Run(total_chunk_progress_,
                                               total_chunk_upload_size_);
  }
}

void LensOverlayQueryController::PageContentUploadFinished() {
  pending_page_content_request_.Clear();
  page_content_request_in_progress_ = false;
  chunk_upload_in_progress_ = false;
  retrying_page_content_upload_ = false;
  chunk_progress.clear();
  if (pending_contextual_query_callback_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(pending_contextual_query_callback_));
  }
}

void LensOverlayQueryController::PrepareAndFetchPartialPageContentRequest() {
  // If permissions have not yet been granted, exit early. The full image
  // request will recall this method once permissions are granted and the
  // cluster info is fetched.
  if (!DidUserGrantLensOverlayNeededPermissions(profile_->GetPrefs())) {
    return;
  }

  if (!cluster_info_ || !IsPartialPageContentSubstantial()) {
    // Cannot send this request without cluster info. Do not send the request
    // if the partial page content is not substantial enough to yield deatialed
    // results.
    return;
  }

  partial_page_contents_request_start_time_ = base::TimeTicks::Now();

  // Create the request.
  lens::LensOverlayServerRequest request;
  lens::LensOverlayRequestContext request_context;

  // If this is the first partial page content request, use the initial request
  // id. Otherwise, use the request id generator.
  if (is_first_partial_page_contents_request_) {
    CHECK(initial_request_id_);
    request_context.mutable_request_id()->CopyFrom(*initial_request_id_);
  } else {
    request_context.mutable_request_id()->CopyFrom(*GetNextRequestId(
        lens::RequestIdUpdateMode::kPartialPageContentRequest,
        MimeTypeToMediaType(primary_content_type_,
                            /*has_viewport_screenshot=*/true)));
  }
  request_context.mutable_client_context()->CopyFrom(CreateClientContext());
  request.mutable_objects_request()->mutable_request_context()->CopyFrom(
      request_context);

  // Create the partial page content payload.
  lens::Payload payload;
  payload.set_request_type(lens::RequestType::REQUEST_TYPE_EARLY_PARTIAL_PDF);

  // Add the partial page content to the payload.
  lens::LensOverlayDocument partial_pdf_document;
  for (size_t i = 0; i < partial_content_.size(); ++i) {
    const auto& page_text = partial_content_[i];
    auto* page = partial_pdf_document.add_pages();
    page->set_page_number(i + 1);
    page->add_text_segments(base::UTF16ToUTF8(page_text));
  }

  auto* content = payload.mutable_content();
  auto* content_data = content->add_content_data();
  content_data->set_content_type(
      lens::ContentData::CONTENT_TYPE_EARLY_PARTIAL_PDF);
  partial_pdf_document.SerializeToString(content_data->mutable_data());

  // Add the page url to the payload if it is available.
  if (!page_url_.is_empty()) {
    content->set_webpage_url(page_url_.spec());
  }
  if (page_title_.has_value() && !page_title_.value().empty()) {
    content->set_webpage_title(page_title_.value());
  }

  request.mutable_objects_request()->mutable_payload()->CopyFrom(payload);

  partial_page_content_access_token_fetcher_ =
      CreateOAuthHeadersAndContinue(base::BindOnce(
          &LensOverlayQueryController::PerformPartialPageContentRequest,
          weak_ptr_factory_.GetWeakPtr(), std::move(request)));

  is_first_partial_page_contents_request_ = false;
}

void LensOverlayQueryController::PerformPartialPageContentRequest(
    lens::LensOverlayServerRequest request,
    std::vector<std::string> headers) {
  partial_page_content_access_token_fetcher_.reset();

  PerformFetchRequest(
      &request, &headers,
      base::Milliseconds(
          lens::features::GetLensOverlayPageContentRequestTimeoutMs()),
      base::BindOnce(&LensOverlayQueryController::
                         OnPartialPageContentEndpointFetcherCreated,
                     weak_ptr_factory_.GetWeakPtr(),
                     request.objects_request().request_context().request_id()),
      base::BindOnce(
          &LensOverlayQueryController::PartialPageContentResponseHandler,
          weak_ptr_factory_.GetWeakPtr(),
          request.objects_request().request_context().request_id()));
}

void LensOverlayQueryController::PartialPageContentResponseHandler(
    lens::LensOverlayRequestId request_id,
    std::unique_ptr<EndpointResponse> response) {
  partial_page_content_endpoint_fetcher_.reset();

  SendLatencyGen204IfEnabled(
      LatencyType::kPartialPageContentUploadLatency,
      partial_page_contents_request_start_time_,
      VitQueryParamValueForMimeType(primary_content_type_),
      /*cluster_info_latency=*/std::nullopt,
      /*encoded_analytics_id=*/std::nullopt,
      std::make_optional<lens::LensOverlayRequestId>(request_id));
}

void LensOverlayQueryController::SendInteraction(
    base::Time query_start_time,
    lens::mojom::CenterRotatedBoxPtr region,
    std::optional<std::string> query_text,
    std::optional<std::string> object_id,
    lens::LensOverlaySelectionType selection_type,
    std::map<std::string, std::string> additional_search_query_params,
    std::optional<SkBitmap> region_bytes,
    lens::LensOverlayRequestId::MediaType media_type) {
  // Cancel any pending encoding from previous SendInteraction requests.
  encoding_task_tracker_->TryCancelAll();
  // Reset any pending interaction requests that will get fired via the full
  // image request / response handlers.
  pending_interaction_callback_.Reset();

  // If the cluster info is missing add the interaction to the pending callback
  // to be sent once the cluster info is available.
  if (!cluster_info_.has_value()) {
    pending_interaction_callback_ = base::BindOnce(
        &LensOverlayQueryController::SendInteraction,
        weak_ptr_factory_.GetWeakPtr(), query_start_time, std::move(region),
        query_text, object_id, selection_type, additional_search_query_params,
        region_bytes, media_type);

    // If the cluster info is expired, restart a new query flow so the pending
    // interaction request will be sent once the cluster info is available.
    MaybeRestartQueryFlow();
    return;
  }

  if (!latest_full_image_request_data_) {
    // The request id sequence for the interaction request must follow a full
    // image request. If we have not yet created a full image request id, the
    // request id generator will not be ready to create the interaction request
    // id. In that case, save the interaction data to create the request after
    // the full image request id sequence has been incremented.
    pending_interaction_callback_ = base::BindOnce(
        &LensOverlayQueryController::SendInteraction,
        weak_ptr_factory_.GetWeakPtr(), query_start_time, std::move(region),
        query_text, object_id, selection_type, additional_search_query_params,
        region_bytes, media_type);
    return;
  }

  // Create the logs used across the async.
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  ref_counted_logs->client_logs().set_lens_overlay_entry_point(
      LenOverlayEntryPointFromInvocationSource(invocation_source_));
  ref_counted_logs->client_logs().set_paella_id(gen204_id_);
  ref_counted_logs->client_logs().set_metrics_collection_disabled(
      !g_browser_process->GetMetricsServicesManager() ||
      !g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());

  // Initialize latest_interaction_request_data_ with a new request ID to
  // ensure once the async processes finish, no new interaction request has
  // started.
  latest_interaction_request_data_ = std::make_unique<LensServerFetchRequest>(
      GetNextRequestId(RequestIdUpdateMode::kInteractionRequest, media_type),
      /*query_start_time_ms=*/base::TimeTicks::Now());
  int current_sequence_id = latest_interaction_request_data_->sequence_id();

  // Add the create URL callback to be run after the request is sent.
  latest_interaction_request_data_->request_sent_callback_ = base::BindOnce(
      &LensOverlayQueryController::CreateSearchUrlAndSendToCallback,
      weak_ptr_factory_.GetWeakPtr(), query_start_time, query_text,
      additional_search_query_params, selection_type,
      GetNextRequestId(RequestIdUpdateMode::kSearchUrl, media_type));

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
          query_text, object_id, ref_counted_logs));

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
        std::optional<lens::ImageCropAndBitmap> image_crop_and_bitmap) {
  // The request index should match our counter after encoding finishes.
  CHECK(sequence_id == latest_interaction_request_data_->sequence_id());

  // Pass the image crop and region bitmap for this request to the thumbnail
  // created callback.
  if (image_crop_and_bitmap.has_value()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            thumbnail_created_callback_,
            image_crop_and_bitmap->image_crop.image().image_content(),
            image_crop_and_bitmap->region_bitmap));
  }

  // Create the interaction request.
  lens::LensOverlayServerRequest server_request = CreateInteractionRequest(
      std::move(region), query_text, object_id,
      image_crop_and_bitmap
          ? std::make_optional(image_crop_and_bitmap->image_crop)
          : std::nullopt,
      ref_counted_logs->client_logs());

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
  if (!IsCurrentInteractionSequence(sequence_id)) {
    // Ignore superseded request.
    return;
  }

  if (!latest_interaction_request_data_->request_ ||
      !latest_interaction_request_data_->request_headers_) {
    // Exit early since not all request data is ready.
    return;
  }

  // Allow the query controller to perform the interaction request before the
  // full image response is received if the early interaction optimization is
  // enabled.
  if (lens::features::IsLensOverlayEarlyInteractionOptimizationEnabled() &&
      query_controller_state_ ==
          QueryControllerState::kAwaitingFullImageResponse &&
      cluster_info_.has_value()) {
    PerformInteractionRequest();
    return;
  }

  //  If a full image request is in flight, wait for the full image response
  //  before sending the request.
  if (query_controller_state_ ==
          QueryControllerState::kAwaitingClusterInfoResponse ||
      query_controller_state_ ==
          QueryControllerState::kAwaitingFullImageResponse) {
    pending_interaction_callback_ = base::BindOnce(
        &LensOverlayQueryController::TryPerformInteractionRequest,
        weak_ptr_factory_.GetWeakPtr(), sequence_id);
    return;
  }

  // All elements needed are ready so perform the request.
  PerformInteractionRequest();
}

bool LensOverlayQueryController::IsCurrentInteractionSequence(int sequence_id) {
  CHECK(latest_interaction_request_data_);
  return latest_interaction_request_data_->sequence_id() == sequence_id;
}

void LensOverlayQueryController::PerformInteractionRequest() {
  // The interaction request is composed of two steps, sending the request to
  // the server, and creating the URL to load in the side panel.
  PerformFetchRequest(
      latest_interaction_request_data_->request_.get(),
      latest_interaction_request_data_->request_headers_.get(),
      base::Milliseconds(lens::features::GetLensOverlayServerRequestTimeout()),
      base::BindOnce(
          &LensOverlayQueryController::OnInteractionEndpointFetcherCreated,
          weak_ptr_factory_.GetWeakPtr(),
          *latest_interaction_request_data_->request_id_.get()),
      base::BindOnce(
          &LensOverlayQueryController::InteractionFetchResponseHandler,
          weak_ptr_factory_.GetWeakPtr(),
          latest_interaction_request_data_->sequence_id()));

  // Run the callback to create the search URL and pass it to the side panel.
  CHECK(latest_interaction_request_data_->request_sent_callback_.has_value());
  std::move(latest_interaction_request_data_->request_sent_callback_.value())
      .Run();
}

void LensOverlayQueryController::CreateSearchUrlAndSendToCallback(
    base::Time query_start_time,
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
  visual_search_interaction_data_ =
      BuildVisualSearchInteractionLogData(query_text, selection_type);
  std::string encoded_vsint = EncodeVisualSearchInteractionLogData(
      visual_search_interaction_data_.value());
  additional_search_query_params.insert(
      {kVisualSearchInteractionDataQueryParameterKey, encoded_vsint});
  suggest_inputs_.set_encoded_visual_search_interaction_log_data(encoded_vsint);
  RunSuggestInputsCallback();

  // Generate and send the Lens search url.
  lens::proto::LensOverlayUrlResponse lens_overlay_url_response;
  lens_overlay_url_response.set_url(
      lens::BuildLensSearchURL(
          query_start_time, query_text, page_url_, page_title_,
          std::move(request_id), cluster_info_.value(),
          additional_search_query_params, invocation_source_, use_dark_mode_)
          .spec());
  lens_overlay_url_response.set_page_url(page_url_.spec());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(url_callback_, lens_overlay_url_response));
}

void LensOverlayQueryController::InteractionFetchResponseHandler(
    int sequence_id,
    std::unique_ptr<EndpointResponse> response) {
  // If this request sequence ID does not match the latest sent then we should
  // ignore the response.
  if (latest_interaction_request_data_->sequence_id() != sequence_id) {
    return;
  }

  if (response->http_status_code != google_apis::ApiErrorCode::HTTP_SUCCESS) {
    RunInteractionCallbackForError();
    return;
  }

  lens::LensOverlayServerResponse server_response;
  if (!server_response.ParseFromString(response->response)) {
    RunInteractionCallbackForError();
    return;
  }

  if (!server_response.has_interaction_response()) {
    RunInteractionCallbackForError();
    return;
  }

  // Attach the analytics id associated with the interaction request to the
  // latency gen204 ping. This may differ from latest_encoded_analytics_id_ if
  // the user makes an objects request while the interaction request is in
  // flight.
  std::string encoded_analytics_id = base32::Base32Encode(
      base::as_byte_span(
          latest_interaction_request_data_->request_id_.get()->analytics_id()),
      base32::Base32EncodePolicy::OMIT_PADDING);
  SendLatencyGen204IfEnabled(
      LatencyType::kInteractionRequestFetchLatency,
      latest_interaction_request_data_->query_start_time_,
      VitQueryParamValueForMimeType(primary_content_type_),
      /*cluster_info_latency=*/std::nullopt,
      std::make_optional(encoded_analytics_id),
      *latest_interaction_request_data_->request_id_.get());

  if (!(lens::IsLensOverlayContextualSearchboxEnabled() &&
        !lens::features::GetLensOverlaySendImageSignalsForLensSuggest())) {
    // Always include the image signals unless the contextual searchbox is
    // enabled and the image signals feature flag is disabled.
    suggest_inputs_.set_encoded_image_signals(
        server_response.interaction_response().encoded_response());
    RunSuggestInputsCallback();
  }

  if (server_response.interaction_response().has_text()) {
    interaction_response_callback_.Run(CreateTextMojomFromInteractionResponse(
        server_response.interaction_response(),
        latest_interaction_request_data_.get()
            ->request_->interaction_request()
            .image_crop()
            .zoomed_crop(),
        resized_bitmap_size_));
  }

  RunSuggestInputsCallback();
}

void LensOverlayQueryController::RunInteractionCallbackForError() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(suggest_inputs_callback_,
                                lens::proto::LensOverlaySuggestInputs()));
}

void LensOverlayQueryController::SendFullImageLatencyGen204IfEnabled(
    base::TimeTicks start_time_ticks,
    bool is_translate_query,
    std::string vit_query_param_value) {
  SendInitialLatencyGen204IfNotAlreadySent(
      LatencyType::kInvocationToInitialFullPageObjectsResponseReceived,
      vit_query_param_value,
      *latest_full_image_request_data_->request_id_.get());

  SendLatencyGen204IfEnabled(
      is_translate_query ? lens::LensOverlayGen204Controller::LatencyType::
                               kFullPageTranslateRequestFetchLatency
                         : lens::LensOverlayGen204Controller::LatencyType::
                               kFullPageObjectsRequestFetchLatency,
      start_time_ticks, vit_query_param_value,
      cluster_info_fetch_response_time_,
      /*encoded_analytics_id=*/std::nullopt,
      *latest_full_image_request_data_->request_id_.get());
  cluster_info_fetch_response_time_.reset();
}

void LensOverlayQueryController::SendInitialLatencyGen204IfNotAlreadySent(
    lens::LensOverlayGen204Controller::LatencyType latency_type,
    std::string vit_query_param_value,
    std::optional<lens::LensOverlayRequestId> request_id) {
  if (sent_initial_latency_request_events_.contains(latency_type)) {
    return;
  }

  SendLatencyGen204IfEnabled(latency_type, invocation_time_,
                             vit_query_param_value,
                             /*cluster_info_latency=*/std::nullopt,
                             /*encoded_analytics_id=*/std::nullopt, request_id);
  sent_initial_latency_request_events_.insert(latency_type);
}

void LensOverlayQueryController::PerformFetchRequest(
    lens::LensOverlayServerRequest* request,
    std::vector<std::string>* request_headers,
    base::TimeDelta timeout,
    base::OnceCallback<void(std::unique_ptr<EndpointFetcher>)>
        fetcher_created_callback,
    EndpointFetcherCallback response_received_callback,
    UploadProgressCallback upload_progress_callback) {
  CHECK(request);
  std::string request_string;
  CHECK(request->SerializeToString(&request_string));
  GURL fetch_url = GURL(lens::features::GetLensOverlayEndpointURL());
  PerformFetchRequest(std::move(request_string), request_headers, timeout,
                      std::move(fetcher_created_callback),
                      std::move(response_received_callback),
                      std::move(upload_progress_callback), fetch_url);
}

void LensOverlayQueryController::PerformFetchRequest(
    std::string request_string,
    std::vector<std::string>* request_headers,
    base::TimeDelta timeout,
    base::OnceCallback<void(std::unique_ptr<EndpointFetcher>)>
        fetcher_created_callback,
    EndpointFetcherCallback response_received_callback,
    UploadProgressCallback upload_progress_callback,
    GURL fetch_url) {
  CHECK(request_headers);

  // Get client experiment variations to include in the request.
  std::vector<std::string> cors_exempt_headers =
      CreateVariationsHeaders(variations_client_);

  // Generate the URL to fetch to and include the server session id if present.
  if (cluster_info_.has_value()) {
    // The endpoint fetches should use the server session id from the cluster
    // info.
    fetch_url = net::AppendOrReplaceQueryParameter(
        fetch_url, kSessionIdQueryParameterKey,
        cluster_info_->server_session_id());
  }

  // Create the EndpointFetcher, responsible for making the request using our
  // given params.
  std::unique_ptr<EndpointFetcher> endpoint_fetcher = CreateEndpointFetcher(
      std::move(request_string), fetch_url, HttpMethod::kPost, timeout,
      *request_headers, cors_exempt_headers,
      std::move(upload_progress_callback));
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
  if (lens::features::IsUpdatedClientContextEnabled()) {
    context.set_surface(lens::SURFACE_LENS_OVERLAY);
    context.set_platform(lens::PLATFORM_LENS_OVERLAY);
  } else {
    context.set_surface(lens::SURFACE_CHROMIUM);
    context.set_platform(lens::PLATFORM_WEB);
    context.mutable_rendering_context()->set_rendering_environment(
        lens::RENDERING_ENV_LENS_OVERLAY);
  }
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

    // If an access token fetcher is already in flight, it is intentionally
    // replaced by this newer one.
    return std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
        signin::OAuthConsumerId::kLensOverlayQueryController, identity_manager_,
        std::move(token_callback),
        signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
        signin::ConsentLevel::kSignin);
  }

  // Fall back to fetching the endpoint directly using API key.
  std::move(callback).Run(std::vector<std::string>());
  return nullptr;
}

lens::LensOverlayVisualSearchInteractionData
LensOverlayQueryController::BuildVisualSearchInteractionLogData(
    const std::optional<std::string>& selected_text,
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

  // If there was an interaction request made, then the selection type, region
  // and object id should be set if they exist. The interaction request may not
  // exist if the user made a text-selection query.
  if (latest_interaction_request_data_->request_ &&
      latest_interaction_request_data_->request_->has_interaction_request()) {
    auto sent_interaction_request =
        latest_interaction_request_data_->request_->interaction_request();
    interaction_data.set_interaction_type(
        sent_interaction_request.interaction_request_metadata().type());
    if (sent_interaction_request.has_interaction_request_metadata() &&
        sent_interaction_request.interaction_request_metadata()
            .has_selection_metadata() &&
        sent_interaction_request.interaction_request_metadata()
            .selection_metadata()
            .has_object()) {
      interaction_data.set_object_id(
          sent_interaction_request.interaction_request_metadata()
              .selection_metadata()
              .object()
              .object_id());
    } else if (sent_interaction_request.has_image_crop()) {
      // The zoomed crop field should only be set if the object id is not set.
      interaction_data.mutable_zoomed_crop()->CopyFrom(
          sent_interaction_request.image_crop().zoomed_crop());
    }
  } else {
    // If there was no interaction request, then the selection type should be
    // set to text selection.
    interaction_data.set_interaction_type(
        lens::LensOverlayInteractionRequestMetadata::TEXT_SELECTION);
  }
  if (selected_text.has_value()) {
    interaction_data.mutable_text_select()->set_selected_texts(
        selected_text.value());
  }
  // If the interaction type of the request is either a PDF_QUERY or
  // WEPAGE_QUERY, a zoomed crop consisting of the full image should be sent.
  if (interaction_data.interaction_type() ==
          lens::LensOverlayInteractionRequestMetadata::PDF_QUERY ||
      interaction_data.interaction_type() ==
          lens::LensOverlayInteractionRequestMetadata::WEBPAGE_QUERY) {
    interaction_data.mutable_zoomed_crop()->mutable_crop()->set_center_x(0.5f);
    interaction_data.mutable_zoomed_crop()->mutable_crop()->set_center_y(0.5f);
    interaction_data.mutable_zoomed_crop()->mutable_crop()->set_width(1);
    interaction_data.mutable_zoomed_crop()->mutable_crop()->set_height(1);
    interaction_data.mutable_zoomed_crop()->mutable_crop()->set_coordinate_type(
        ::lens::CoordinateType::NORMALIZED);
    interaction_data.mutable_zoomed_crop()->set_zoom(1);
  }
  return interaction_data;
}

std::string LensOverlayQueryController::EncodeVisualSearchInteractionLogData(
    const lens::LensOverlayVisualSearchInteractionData& interaction_data) {
  // Set this to true to indicate that the initial parent query has been sent.
  // This ensures that subsequent interactions will correctly report
  // is_parent_query as false.
  parent_query_sent_ = true;
  std::string serialized_proto;
  CHECK(interaction_data.SerializeToString(&serialized_proto));
  std::string encoded_proto;
  base::Base64UrlEncode(serialized_proto,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_proto);
  return encoded_proto;
}

lens::LensOverlayServerRequest
LensOverlayQueryController::CreateInteractionRequest(
    lens::mojom::CenterRotatedBoxPtr region,
    std::optional<std::string> query_text,
    std::optional<std::string> object_id,
    std::optional<lens::ImageCrop> image_crop,
    lens::LensOverlayClientLogs client_logs) {
  lens::LensOverlayServerRequest server_request;
  server_request.mutable_client_logs()->CopyFrom(client_logs);
  // The request ID is guaranteed to exist since it is set in the constructor
  // of latest_interaction_request_data_.
  DCHECK(latest_interaction_request_data_->request_id_);

  lens::LensOverlayRequestContext request_context;
  request_context.mutable_request_id()->CopyFrom(
      *latest_interaction_request_data_->request_id_);
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
    interaction_request_metadata.set_type(
        ContentTypeToInteractionType(primary_content_type_));
    interaction_request_metadata.mutable_query_metadata()
        ->mutable_text_query()
        ->set_query(*query_text);
  } else {
    // There should be a region or an object id in the request.
    NOTREACHED();
  }

  server_request.mutable_interaction_request()
      ->mutable_interaction_request_metadata()
      ->CopyFrom(interaction_request_metadata);
  return server_request;
}

void LensOverlayQueryController::ResetRequestClusterInfoState() {
  pending_interaction_callback_.Reset();
  interaction_endpoint_fetcher_.reset();
  cluster_info_.reset();
  query_controller_state_ = QueryControllerState::kClusterInfoExpired;
  request_id_generator_->ResetRequestId();
  suggest_inputs_.Clear();
  visual_search_interaction_data_.reset();
  RunSuggestInputsCallback();
  parent_query_sent_ = false;
}

void LensOverlayQueryController::OnFullImageEndpointFetcherCreated(
    lens::LensOverlayRequestId request_id,
    std::unique_ptr<EndpointFetcher> endpoint_fetcher) {
  SendInitialLatencyGen204IfNotAlreadySent(
      LatencyType::kInvocationToInitialFullPageObjectsRequestSent,
      VitQueryParamValueForMimeType(primary_content_type_), request_id);
  full_image_endpoint_fetcher_ = std::move(endpoint_fetcher);
}

void LensOverlayQueryController::OnPageContentEndpointFetcherCreated(
    lens::LensOverlayRequestId request_id,
    std::unique_ptr<EndpointFetcher> endpoint_fetcher) {
  SendInitialLatencyGen204IfNotAlreadySent(
      LatencyType::kInvocationToInitialPageContentRequestSent,
      VitQueryParamValueForMimeType(primary_content_type_), request_id);
  page_content_endpoint_fetcher_ = std::move(endpoint_fetcher);
}

void LensOverlayQueryController::OnPartialPageContentEndpointFetcherCreated(
    lens::LensOverlayRequestId request_id,
    std::unique_ptr<EndpointFetcher> endpoint_fetcher) {
  SendInitialLatencyGen204IfNotAlreadySent(
      LatencyType::kInvocationToInitialPartialPageContentRequestSent,
      VitQueryParamValueForMimeType(primary_content_type_), request_id);
  partial_page_content_endpoint_fetcher_ = std::move(endpoint_fetcher);
}

void LensOverlayQueryController::OnInteractionEndpointFetcherCreated(
    lens::LensOverlayRequestId request_id,
    std::unique_ptr<EndpointFetcher> endpoint_fetcher) {
  SendInitialLatencyGen204IfNotAlreadySent(
      LatencyType::kInvocationToInitialInteractionRequestSent,
      VitQueryParamValueForMimeType(primary_content_type_), request_id);
  interaction_endpoint_fetcher_ = std::move(endpoint_fetcher);
}

void LensOverlayQueryController::OnChunkUploadEndpointFetcherCreated(
    lens::LensOverlayRequestId request_id,
    std::unique_ptr<EndpointFetcher> endpoint_fetcher) {
  SendInitialLatencyGen204IfNotAlreadySent(
      LatencyType::kInvocationToInitialPageContentRequestSent,
      VitQueryParamValueForMimeType(primary_content_type_), request_id);
  chunk_upload_endpoint_fetchers_.push_back(std::move(endpoint_fetcher));
}

bool LensOverlayQueryController::ShouldSendContextualSearchQuery() {
  // Can send the query if the page content request has finished.
  return !page_content_request_in_progress_ && cluster_info_.has_value();
}

bool LensOverlayQueryController::IsPartialPageContentSubstantial() {
  // If the partial page content is empty, exit early.
  if (partial_content_.empty()) {
    return false;
  }

  // Get the average number of characters per page.
  int total_characters = 0;
  for (const std::u16string& page_text : partial_content_) {
    total_characters += page_text.size();
  }
  const int characters_per_page = total_characters / partial_content_.size();

  // If the average is over the scanned pdf character per page heuristic, the
  // query is considered substantial.
  return characters_per_page >
         lens::features::GetScannedPdfCharacterPerPageHeuristic();
}
}  // namespace lens
