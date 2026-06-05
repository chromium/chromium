// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/internal/composebox_query_controller.h"

#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base64url.h"
#include "base/debug/dump_without_crashing.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/utils.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/lens/lens_payload_construction.h"
#include "components/lens/lens_request_construction.h"
#include "components/lens/lens_url_utils.h"
#include "components/lens/ref_counted_lens_overlay_client_logs.h"
#include "components/search_engines/util.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/variations/variations_client.h"
#include "components/version_info/channel.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/google_api_keys.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "third_party/lens_server_proto/lens_overlay_contextual_inputs.pb.h"
#include "third_party/lens_server_proto/lens_overlay_interaction_request_metadata.pb.h"
#include "third_party/lens_server_proto/lens_overlay_lens_file.pb.h"
#include "third_party/lens_server_proto/lens_overlay_payload.pb.h"
#include "third_party/lens_server_proto/lens_overlay_platform.pb.h"
#include "third_party/lens_server_proto/lens_overlay_request_id.pb.h"
#include "third_party/lens_server_proto/lens_overlay_selection_type.pb.h"
#include "third_party/lens_server_proto/lens_overlay_service_deps.pb.h"
#include "third_party/lens_server_proto/lens_overlay_surface.pb.h"
#include "third_party/lens_server_proto/lens_overlay_visual_search_interaction_data.pb.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"

#if !BUILDFLAG(IS_IOS)
#include "services/data_decoder/public/cpp/decode_image.h"
#include "third_party/skia/include/core/SkBitmap.h"
#endif  // !BUILDFLAG(IS_IOS)

using endpoint_fetcher::CredentialsMode;
using endpoint_fetcher::EndpointFetcher;
using endpoint_fetcher::EndpointFetcherCallback;
using endpoint_fetcher::EndpointResponse;
using endpoint_fetcher::HttpMethod;

constexpr char kContentTypeKey[] = "Content-Type";
constexpr char kContentType[] = "application/x-protobuf";
constexpr char kSessionIdQueryParameterKey[] = "gsessionid";
constexpr char kVisualSearchInteractionQueryParameterKey[] = "vsint";
constexpr char kAddedInputsQueryParameterKey[] = "aai";
constexpr char kVisualInputTypeQueryParameter[] = "vit";
constexpr char kAimMultiContextQueryParameter[] = "amc";
constexpr char kLnsModeQueryParameterKey[] = "lns_mode";
constexpr char kLnsModeQueryParameterValue[] = "cvst";

// TODO(crbug.com/432348301): Move away from hardcoded entrypoint and lns
// surface values.
constexpr char kLnsSurfaceParameterValue[] = "42";

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotationTag =
    net::DefineNetworkTrafficAnnotation("ntp_composebox_query_controller", R"(
        semantics {
          sender: "Lens"
          description: "A request to the service handling the file uploads for "
            "the Composebox in the NTP in Chrome."
          trigger: "The user triggered a compose flow in the Chrome NTP "
            "by clicking on the button in the realbox."
          data: "Only file data that is explicitly uploaded by the user will "
            "be sent."
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
          last_reviewed: "2025-06-20"
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting: "This feature is only shown in the NTP by default and does "
            "nothing without explicit user action, so there is no setting to "
            "disable the feature."
          policy_exception_justification: "Not yet implemented."
        }
      )");

ComposeboxQueryController::UploadRequest::UploadRequest() = default;
ComposeboxQueryController::UploadRequest::~UploadRequest() = default;

ComposeboxQueryController::FileInfo::FileInfo() = default;
ComposeboxQueryController::FileInfo::~FileInfo() = default;

ComposeboxQueryController::LensServerInteractionRequest::
    LensServerInteractionRequest(
        std::unique_ptr<lens::LensOverlayRequestId> request_id)
    : request_id_(std::move(request_id)) {}
ComposeboxQueryController::LensServerInteractionRequest::
    ~LensServerInteractionRequest() = default;

ComposeboxQueryController::CreateSearchUrlRequestInfo::
    CreateSearchUrlRequestInfo() = default;
ComposeboxQueryController::CreateSearchUrlRequestInfo::
    ~CreateSearchUrlRequestInfo() = default;

ComposeboxQueryController::CreateClientToAimRequestInfo::
    CreateClientToAimRequestInfo() = default;
ComposeboxQueryController::CreateClientToAimRequestInfo::
    ~CreateClientToAimRequestInfo() = default;

namespace {

// Returns true if the file_info represents an unresolved URL upload.
bool IsUnresolvedUrlUpload(const contextual_search::FileInfo& file_info) {
  return file_info.input_data &&
         file_info.input_data->primary_content_type ==
             lens::MimeType::kUnknown &&
         !file_info.input_data->viewport_screenshot.has_value() &&
         !file_info.input_data->viewport_screenshot_bytes.has_value() &&
         !file_info.input_data->context_input.has_value() &&
         file_info.input_data->parsed_url.has_value();
}

// The maximum number of times to retry fetching cluster info.
constexpr int kMaxClusterInfoRetries = 3;

// The backoff policy for Contextual Tasks network requests.
constexpr net::BackoffEntry::Policy kClusterInfoBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    1,

    // Initial delay for exponential back-off in ms.
    500,

    // Factor by which the waiting time will be multiplied.
    2,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.2,  // 20%

    // Maximum amount of time we are willing to delay our request in ms.
    10000,  // 10 seconds.

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // Don't use initial delay unless the last request was an error.
    false,
};

void PopulateContentMetadata(lens::Payload* payload,
                             const std::optional<GURL>& page_url,
                             const std::optional<std::string>& page_title,
                             const std::optional<std::string>& file_name,
                             const std::optional<std::string>& drive_id,
                             const std::optional<std::string>& resource_key,
                             const std::optional<std::string>& parsed_url) {
  if (!page_title.has_value() && !file_name.has_value() &&
      !page_url.has_value() && !drive_id.has_value() &&
      !resource_key.has_value() && !parsed_url.has_value()) {
    return;
  }
  auto* content_metadata = payload->mutable_content_metadata();
  if (page_title.has_value()) {
    content_metadata->set_content_title(page_title.value());
  }
  if (file_name.has_value()) {
    content_metadata->set_file_name(file_name.value());
  }
  if (parsed_url.has_value() && !parsed_url->empty()) {
    content_metadata->set_url(parsed_url.value());
  } else if (page_url.has_value()) {
    content_metadata->set_url(page_url->spec());
  }
  if (drive_id.has_value()) {
    content_metadata->mutable_drive_metadata()->set_drive_id(drive_id.value());
  }
  if (resource_key.has_value()) {
    content_metadata->mutable_drive_metadata()->set_resource_key(
        resource_key.value());
  }
}

// Creates a payload for a contextual data upload request, for webpage contents
// or for uploaded pdf files.
lens::Payload CreateContentextualDataUploadPayload(
    std::vector<lens::ContextualInput> context_inputs,
    std::optional<GURL> page_url,
    std::optional<std::string> page_title,
    std::optional<std::string> drive_id,
    std::optional<std::string> resource_key,
    std::optional<std::string> file_name,
    std::optional<std::string> parsed_url) {
  lens::Payload payload;
  auto* content = payload.mutable_content();

  if (parsed_url.has_value() && !parsed_url->empty()) {
    content->set_webpage_url(parsed_url.value());
  } else if (page_url.has_value() && !page_url->is_empty()) {
    content->set_webpage_url(page_url->spec());
  }
  if (page_title.has_value() && !page_title.value().empty()) {
    content->set_webpage_title(page_title.value());
  }

  PopulateContentMetadata(&payload, page_url, page_title, file_name, drive_id,
                          resource_key, parsed_url);

  for (const lens::ContextualInput& context_input : context_inputs) {
    auto* content_data = content->add_content_data();
    content_data->set_content_type(
        MimeTypeToContentType(context_input.content_type_));

    // Compress PDF bytes.
    if (context_input.content_type_ == lens::MimeType::kPdf) {
      // If compression is successful, set the compression type and return.
      // Otherwise, fall back to the original bytes.
      if (lens::ZstdCompressBytes(context_input.bytes_,
                                  content_data->mutable_data())) {
        content_data->set_compression_type(lens::CompressionType::ZSTD);
        continue;
      }
    }

    // Add non compressed bytes. This happens if compression fails or its not
    // a PDF.
    content_data->mutable_data()->assign(context_input.bytes_.begin(),
                                         context_input.bytes_.end());
  }

  return payload;
}

// Creates the server request proto for the pdf / page content upload request.
// Called on the main thread after the payload is ready.
void CreateFileUploadRequestProtoWithPayloadAndContinue(
    lens::LensOverlayRequestId request_id,
    lens::LensOverlayClientContext client_context,
    RequestBodyProtoCreatedCallback callback,
    lens::Payload payload) {
  lens::LensOverlayServerRequest request;
  auto* objects_request = request.mutable_objects_request();
  objects_request->mutable_request_context()->mutable_request_id()->CopyFrom(
      request_id);
  objects_request->mutable_request_context()
      ->mutable_client_context()
      ->CopyFrom(client_context);
  objects_request->mutable_payload()->CopyFrom(payload);
  std::move(callback).Run(request, /*error_type=*/std::nullopt);
}

// Returns true if the context upload status is valid to include in the
// multimodal request.
bool IsValidContextUploadStatusForMultimodalRequest(
    contextual_search::ContextUploadStatus upload_status) {
  return upload_status == contextual_search::ContextUploadStatus::kProcessing ||
         upload_status == contextual_search::ContextUploadStatus::
                              kProcessingSuggestSignalsReady ||
         upload_status ==
             contextual_search::ContextUploadStatus::kUploadStarted ||
         upload_status ==
             contextual_search::ContextUploadStatus::kUploadSuccessful;
}

// Returns true if the request id corresponds to an image upload.
bool RequestIdHasImage(const lens::LensOverlayRequestId& request_id) {
  lens::LensOverlayRequestId::MediaType media_type = request_id.media_type();
  if (media_type == lens::LensOverlayRequestId::MEDIA_TYPE_RAW_FILE) {
    return request_id.mime_type().starts_with("image/") &&
           request_id.mime_type() != "image/svg+xml";
  }
  return media_type == lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE ||
         media_type ==
             lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE ||
         media_type == lens::LensOverlayRequestId::MEDIA_TYPE_PDF_AND_IMAGE;
}

// Returns the interaction type for the given media type.
lens::LensOverlayInteractionRequestMetadata::Type RequestIdToInteractionType(
    const lens::LensOverlayRequestId& request_id) {
  switch (request_id.media_type()) {
    case lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE:
    case lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE:
      return lens::LensOverlayInteractionRequestMetadata::WEBPAGE_QUERY;
    case lens::LensOverlayRequestId::MEDIA_TYPE_PDF:
    case lens::LensOverlayRequestId::MEDIA_TYPE_PDF_AND_IMAGE:
      return lens::LensOverlayInteractionRequestMetadata::PDF_QUERY;
    case lens::LensOverlayRequestId::MEDIA_TYPE_RAW_FILE:
      // Image uploads do not have a specific interaction type, and webpages
      // are not supported as raw file uploads, so only check for pdf.
      if (request_id.mime_type().starts_with("application/pdf")) {
        return lens::LensOverlayInteractionRequestMetadata::PDF_QUERY;
      }
      return lens::LensOverlayInteractionRequestMetadata::
          CONTEXTUAL_SEARCH_QUERY;
    default:
      return lens::LensOverlayInteractionRequestMetadata::
          CONTEXTUAL_SEARCH_QUERY;
  }
}

// Returns the LensOverlayVisualInputType for the given media type.
lens::LensOverlayVisualInputType RequestIdToVisualInputType(
    const lens::LensOverlayRequestId& request_id) {
  switch (request_id.media_type()) {
    case lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE:
      return lens::LensOverlayVisualInputType::VISUAL_INPUT_TYPE_UNKNOWN;
    case lens::LensOverlayRequestId::MEDIA_TYPE_PDF:
    case lens::LensOverlayRequestId::MEDIA_TYPE_PDF_AND_IMAGE:
      return lens::LensOverlayVisualInputType::VISUAL_INPUT_TYPE_PDF;
    case lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE:
    case lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE:
      return lens::LensOverlayVisualInputType::VISUAL_INPUT_TYPE_WEBPAGE;
    case lens::LensOverlayRequestId::MEDIA_TYPE_RAW_FILE:
      // Image uploads do not send a visual input type, and webpages
      // are not supported as raw file uploads, so only check for pdf.
      if (request_id.mime_type().starts_with("application/pdf")) {
        return lens::LensOverlayVisualInputType::VISUAL_INPUT_TYPE_PDF;
      }
      return lens::LensOverlayVisualInputType::VISUAL_INPUT_TYPE_UNKNOWN;
    default:
      return lens::LensOverlayVisualInputType::VISUAL_INPUT_TYPE_UNKNOWN;
  }
}

int64_t RandInt64() {
  int64_t number;
  base::RandBytes(base::byte_span_from_ref(number));
  return number;
}

bool ModalityChipHasVsrid(const lens::ModalityChipProps& modality_chip_props) {
  return modality_chip_props.has_added_input() &&
         modality_chip_props.added_input().has_lens_file() &&
         modality_chip_props.added_input().lens_file().has_vsrid();
}

}  // namespace

ComposeboxQueryController::ComposeboxQueryController(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    version_info::Channel channel,
    std::string locale,
    TemplateURLService* template_url_service,
    variations::VariationsClient* variations_client,
    std::unique_ptr<
        contextual_search::ContextualSearchContextController::ConfigParams>
        feature_params)
    : identity_manager_(identity_manager),
      url_loader_factory_(url_loader_factory),
      channel_(channel),
      locale_(locale),
      template_url_service_(template_url_service),
      variations_client_(variations_client),
      cluster_info_backoff_(&kClusterInfoBackoffPolicy) {
  send_lns_surface_ = feature_params->send_lns_surface;
  suppress_lns_surface_param_if_no_image_ =
      feature_params->suppress_lns_surface_param_if_no_image;
  enable_viewport_images_ = feature_params->enable_viewport_images;
  use_separate_request_ids_for_viewport_images_ = base::FeatureList::IsEnabled(
      lens::features::kLensUseSeparateRequestIdForViewportImages);
  prioritize_suggestions_for_the_first_attached_document_ =
      feature_params->prioritize_suggestions_for_the_first_attached_document;

  attach_page_title_and_url_to_suggest_requests_ =
      feature_params->attach_page_title_and_url_to_suggest_requests;

  create_request_task_runner_ = base::ThreadPool::CreateTaskRunner(
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

ComposeboxQueryController::~ComposeboxQueryController() {
  for (auto& observer : observers_) {
    observer.OnControllerDestroyed();
  }
}

// static
std::optional<std::string>
ComposeboxQueryController::MimeTypeStringFromFileInfo(
    const contextual_search::FileInfo& file_info) {
  if (IsUnresolvedUrlUpload(file_info)) {
    return std::nullopt;
  }
  switch (file_info.mime_type) {
    case lens::MimeType::kPdf:
      return "application/pdf";
    case lens::MimeType::kHtml:
      return "text/html";
    case lens::MimeType::kPlainText:
      return "text/plain";
    case lens::MimeType::kImage:
      // Images always use jpeg encoding.
      // TODO(crbug.com/481835802): Update this logic if webp encoding is
      // turned on.
      return "image/jpeg";
    case lens::MimeType::kAnnotatedPageContent:
      return "application/x-protobuf";
    case lens::MimeType::kUnknown:
      if (lens::features::IsLensSendRawFileMediaTypesEnabled() &&
          file_info.mime_type_string.has_value()) {
        // For raw-file (arbitrary) uploads, the Lens mime type
        // is set to kUnknown to go through generic processing, but the
        // actual mime type string is in mime_type_string.
        return file_info.mime_type_string.value();
      }
      // The mime type may be unknown for image-only LensOverlay flows, as the
      // LensOverlay does not set the primary content type unless it is a pdf or
      // webpage contextual query. In this case, return the mime type for an
      // image.
      return "image/jpeg";
    default:
      // Fail gracefully, as the mime type value is optional to set in the
      // proto.
      return std::nullopt;
  }
}

void ComposeboxQueryController::SetIsBackgrounded(bool backgrounded) {
  if (is_backgrounded_ == backgrounded) {
    return;
  }
  is_backgrounded_ = backgrounded;
  if (is_backgrounded_) {
    // Reset fetchers to stop in-flight requests and prevent new ones. We do
    // not call ClearClusterInfo or ResetRequestClusterInfoState because we want
    // to preserve the current backoff/retry state and any existing valid
    // cluster info data while backgrounded. We only invalidate the state to
    // ensure it gets re-fetched upon foregrounding.
    cluster_info_endpoint_fetcher_.reset();
    cluster_info_access_token_fetcher_.reset();
    if (query_controller_state_ != QueryControllerState::kOff) {
      SetQueryControllerState(QueryControllerState::kClusterInfoInvalid);
    }
  } else {
    if (query_controller_state_ == QueryControllerState::kClusterInfoInvalid) {
      FetchClusterInfo();
    }
  }
}

void ComposeboxQueryController::InitializeIfNeeded() {
  if (!contextual_tasks::GetIsContextualTasksLazyFetchClusterInfoEnabled()) {
    if (query_controller_state_ == QueryControllerState::kOff) {
      // The query controller state starts at kOff. If it is set to any other
      // state by the call to FetchClusterInfo(), this indicates that the
      // handshake has already been initialized.
      FetchClusterInfo();
    }
    return;
  }
}

void ComposeboxQueryController::TriggerFetchClusterInfo() {
  if (query_controller_state_ == QueryControllerState::kOff ||
      query_controller_state_ == QueryControllerState::kClusterInfoInvalid) {
    FetchClusterInfo();
  }
}

lens::LensOverlayRequestId
ComposeboxQueryController::GetRequestIdForViewportImage(
    const base::UnguessableToken& file_token) {
  auto* file_info = GetMutableFileInfo(file_token);
  if (!file_info || !file_info->request_id.has_value()) {
    return lens::LensOverlayRequestId();
  }
  if (use_separate_request_ids_for_viewport_images_) {
    // Viewport images always come from tab data.
    request_id_generator_.SetHasChromeTabData(true);
    request_id_generator_.SetIsImplicitUpload(file_info->is_implicit_upload);
    // Create a new request id for the viewport image upload request.
    file_info->viewport_request_id_ = request_id_generator_.GetNextRequestId(
        lens::RequestIdUpdateMode::kMultiContextUploadRequest,
        lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
    return *file_info->viewport_request_id_;
  }
  return file_info->request_id.value();
}

lens::AddedInputs ComposeboxQueryController::CreateAddedInputs(
    const std::vector<base::UnguessableToken>& file_tokens,
    bool include_files_without_lens_usage_intent) {
  lens::AddedInputs added_inputs;
  if (!cluster_info_.has_value()) {
    return added_inputs;
  }
  for (const auto& file_token : file_tokens) {
    auto* file_info = GetFileInfo(file_token);
    if (!file_info || !IsValidContextUploadStatusForMultimodalRequest(
                          file_info->upload_status)) {
      continue;
    }

    if (file_info->input_data &&
        !file_info->input_data->has_lens_usage_intent &&
        !include_files_without_lens_usage_intent) {
      continue;
    }

    if (file_info->input_data &&
        file_info->input_data->modality_chip_props.has_value()) {
      // Process modality chips.
      added_inputs.add_added_inputs()->CopyFrom(
          file_info->input_data->modality_chip_props->added_input());
    } else if (IsUnresolvedUrlUpload(*file_info)) {
      lens::AimThumbnail* thumbnail = added_inputs.add_turn_title_thumbnail();
      thumbnail->set_title(file_info->input_data->parsed_url.value());
      thumbnail->mutable_icon()->set_type(lens::AimIconType::ICON_TYPE_LINK);
    } else if (file_info->request_id.has_value() &&
               file_info->mime_type != lens::MimeType::kImage) {
      // Process Lens file non-image uploads. Do not create added inputs for
      // image uploads.
      lens::LensOverlayLensFile* lens_file =
          added_inputs.add_added_inputs()->mutable_lens_file();
      lens_file->set_vsrid(
          lens::Base64EncodeRequestId(file_info->request_id.value()));
      lens_file->set_sticky_cluster_token(cluster_info_->search_session_id());
      auto mime_type = MimeTypeStringFromFileInfo(*file_info);
      if (mime_type.has_value()) {
        lens_file->set_mime_type(mime_type.value());
      }
      lens_file->set_file_name(file_info->file_name);
      if (file_info->tab_title.has_value()) {
        lens_file->set_page_title(file_info->tab_title.value());
      }
      if (file_info->tab_url.has_value()) {
        lens_file->set_page_url(file_info->tab_url.value().spec());
      }
    }
  }
  return added_inputs;
}

void ComposeboxQueryController::CreateSearchUrl(
    std::unique_ptr<CreateSearchUrlRequestInfo> search_url_request_info,
    base::OnceCallback<void(GURL)> callback) {
  latest_interaction_request_data_.reset();
  num_files_in_request_ = 0;

  bool should_create_multimodal_url =
      !active_files_.empty() && !search_url_request_info->file_tokens.empty();
  // If a multimodal URL is requested, but the cluster info has not been
  // received yet, store the request info and callback for later use.
  if ((should_create_multimodal_url &&
       query_controller_state_ ==
           QueryControllerState::kAwaitingClusterInfoResponse) ||
      is_any_context_uploading()) {
    // Since 1) `startFileUploadFlow` should clear past pending/files,
    // and 2) resuming the "pause" by running `pending_search_url_request`
    // clears the stashed request (callback) and calls this function:
    // the stashed request should always be empty here.
    assert(!has_stashed_search_url_request());
    // Pause the url creation until all pending uploads are complete.
    pending_search_url_request_ =
        base::BindOnce(&ComposeboxQueryController::BeforeCreateSearchUrl,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(search_url_request_info), std::move(callback));
    return;
  }

  // If we are here, it is not multimodal URL, and all context is uploaded.
  bool is_aim_search =
      search_url_request_info->search_url_type == SearchUrlType::kAim;
  bool send_upload_type =
      base::FeatureList::IsEnabled(
          contextual_tasks::kContextualTasksSendContextualInputUploadType) &&
      contextual_tasks::kSendContextualInputUploadTypeInSearchUrl.Get() &&
      is_aim_search;
  if (is_aim_search) {
    // For AIM queries, add the added inputs param to the request url params,
    // regardless of if any of the context was a Lens upload.
    bool include_files_without_lens_usage_intent =
        !search_url_request_info->image_crop.has_value();
    lens::AddedInputs added_inputs =
        CreateAddedInputs(search_url_request_info->file_tokens,
                          include_files_without_lens_usage_intent);
    if (added_inputs.added_inputs_size() > 0 ||
        added_inputs.turn_title_thumbnail_size() > 0) {
      std::string serialized_proto;
      CHECK(added_inputs.SerializeToString(&serialized_proto));
      std::string encoded_proto;
      base::Base64UrlEncode(serialized_proto,
                            base::Base64UrlEncodePolicy::OMIT_PADDING,
                            &encoded_proto);
      search_url_request_info->additional_params.insert(
          {kAddedInputsQueryParameterKey, encoded_proto});
    }

    // Add the amc param to indicate that the query was generated from a client
    // capable of sending multi-context queries. This param is needed even if
    // there are no context uploads.
    search_url_request_info->additional_params.insert(
        {kAimMultiContextQueryParameter, "1"});
  }

  if (should_create_multimodal_url && cluster_info_.has_value()) {
    std::unique_ptr<lens::LensOverlayContextualInputs> contextual_inputs =
        std::make_unique<lens::LensOverlayContextualInputs>();
    const FileInfo* last_active_lens_file = nullptr;
    bool has_image_upload = false;
    size_t num_valid_lens_files = 0;
    for (const auto& file_token : search_url_request_info->file_tokens) {
      auto* file_info = GetMutableFileInfo(file_token);
      if (!file_info) {
        continue;
      }
      if (IsValidContextUploadStatusForMultimodalRequest(
              file_info->upload_status) &&
          file_info->request_id.has_value()) {
        num_valid_lens_files++;
        auto* contextual_input = contextual_inputs->add_inputs();
        contextual_input->mutable_request_id()->CopyFrom(
            file_info->request_id.value());
        if (send_upload_type && file_info->input_data &&
            file_info->input_data->upload_type.has_value()) {
          contextual_input->set_upload_type(
              *file_info->input_data->upload_type);
        }

        has_image_upload |= RequestIdHasImage(*file_info->request_id);

        // Add the viewport request id to the contextual inputs if it exists.
        if (file_info->viewport_request_id_) {
          auto* viewport_contextual_input = contextual_inputs->add_inputs();
          viewport_contextual_input->mutable_request_id()->CopyFrom(
              *file_info->viewport_request_id_);
          if (send_upload_type && file_info->input_data &&
              file_info->input_data->upload_type.has_value()) {
            viewport_contextual_input->set_upload_type(
                *file_info->input_data->upload_type);
          }
          has_image_upload = true;
        }
        // Find the last file, preferring non-unresolved url uploads so that
        // the correct request id is used for the interaction request.
        if (!last_active_lens_file || !IsUnresolvedUrlUpload(*file_info) ||
            IsUnresolvedUrlUpload(*last_active_lens_file)) {
          last_active_lens_file = file_info;
        }
      }
    }

    if (num_valid_lens_files > 0) {
      DCHECK(last_active_lens_file != nullptr);
      DCHECK(last_active_lens_file->request_id.has_value());
      request_id_generator_.SetHasChromeTabData(
          last_active_lens_file->tab_session_id.has_value());
      // Region search interactions, and the corresponding search url, are
      // considered implicit uploads.
      bool has_selection_type =
          search_url_request_info->lens_overlay_selection_type.has_value();
      bool has_image_crop = search_url_request_info->image_crop.has_value();
      request_id_generator_.SetIsImplicitUpload(
          last_active_lens_file->is_implicit_upload ||
          (has_selection_type && has_image_crop));
      // Trigger the interaction request on the last file if needed.
      // TODO(crbug.com/462509148): Determine how to support interaction
      // requests for multi-context input flow.
      if (has_selection_type) {
        request_id_generator_.SetContextId(
            last_active_lens_file->request_id->context_id());
        auto interaction_request_id = request_id_generator_.GetNextRequestId(
            lens::RequestIdUpdateMode::kInteractionRequest,
            has_image_crop
                ? lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE
                : last_active_lens_file->request_id->media_type());
        SendInteractionRequest(
            std::move(interaction_request_id),
            search_url_request_info->query_text,
            search_url_request_info->image_crop,
            search_url_request_info->client_logs,
            search_url_request_info->lens_overlay_selection_type,
            std::move(search_url_request_info->interaction_response_callback));
      }

      AddEncodedVisualSearchInteractionLogDataParam(
          last_active_lens_file, search_url_request_info->query_text,
          search_url_request_info->lens_overlay_selection_type,
          search_url_request_info->additional_params);

      // Add the "cvst" lns mode param to the url.
      if (is_aim_search) {
        search_url_request_info->additional_params[kLnsModeQueryParameterKey] =
            kLnsModeQueryParameterValue;
      }

      // Get the encoded visual search interaction log data.
      bool should_send_lns_surface =
          send_lns_surface_ &&
          (!suppress_lns_surface_param_if_no_image_ || has_image_upload);
      std::string lns_surface =
          should_send_lns_surface ? kLnsSurfaceParameterValue : std::string();
      if (contextual_inputs->inputs_size() == 1 && !send_upload_type) {
        auto context_media_type =
            search_url_request_info->image_crop.has_value()
                ? lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE
                : last_active_lens_file->request_id->media_type();
        bool is_raw_file = last_active_lens_file->request_id->media_type() ==
                           lens::LensOverlayRequestId::MEDIA_TYPE_RAW_FILE;
        // If there is only contextual input, create a search url using the
        // vsrid (single-context) parameter.
        bool is_translate =
            search_url_request_info->lens_overlay_selection_type ==
            lens::TRANSLATE_CHIP;
        if (!is_translate &&
            (search_url_request_info->search_url_type ==
                 SearchUrlType::kStandard ||
             base::FeatureList::IsEnabled(
                 lens::features::kLensSendVitForSingleContextNextQueries))) {
          // Single-context queries should send the vit parameter if it is a
          // standard (non-AIM) query, or if the flag to send the vit parameter
          // for single context next queries is enabled, and if the media type
          // is not "img".
          std::string vit_value =
              lens::VitQueryParamValueForMediaType(context_media_type);
          if (context_media_type !=
                  lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE &&
              !vit_value.empty()) {
            search_url_request_info->additional_params.insert(
                {kVisualInputTypeQueryParameter, vit_value});
          }
        }
        std::unique_ptr<lens::LensOverlayRequestId> request_id = nullptr;
        if (!is_translate) {
          if (is_aim_search) {
            request_id = std::make_unique<lens::LensOverlayRequestId>(
                *last_active_lens_file->request_id);
          } else {
            request_id =
                is_raw_file
                    ? request_id_generator_.GetNextRequestId(
                          lens::RequestIdUpdateMode::kSearchUrl,
                          last_active_lens_file->request_id->mime_type(),
                          lens::LensOverlayRequestId::MEDIA_TYPE_RAW_FILE)
                    : request_id_generator_.GetNextRequestId(
                          lens::RequestIdUpdateMode::kSearchUrl,
                          context_media_type);
          }
        }
        std::move(callback).Run(GetUrlForMultimodalSearch(
            template_url_service_, is_aim_search,
            search_url_request_info->aim_entry_point,
            search_url_request_info->query_start_time,
            cluster_info_->search_session_id(), std::move(request_id),
            search_url_request_info->invocation_source, lns_surface,
            base::UTF8ToUTF16(search_url_request_info->query_text),
            std::move(search_url_request_info->additional_params)));
      } else {
        // If there are multiple valid files, create a search url using the
        // contextual inputs.
        std::move(callback).Run(GetUrlForMultimodalSearch(
            template_url_service_, is_aim_search,
            search_url_request_info->aim_entry_point,
            search_url_request_info->query_start_time,
            cluster_info_->search_session_id(), std::move(contextual_inputs),
            search_url_request_info->invocation_source, lns_surface,
            base::UTF8ToUTF16(search_url_request_info->query_text),
            std::move(search_url_request_info->additional_params)));
      }
      return;
    }
  }

  // For queries in which the cluster info has expired, or without valid
  // Lens contextual inputs, use the non-Lens AIM url creation flow.
  // TODO(crbug.com/432125987): Handle file reupload after cluster info
  // expiration.
  std::move(callback).Run(GetUrlForAim(
      template_url_service_, search_url_request_info->aim_entry_point,
      search_url_request_info->query_start_time,
      base::UTF8ToUTF16(search_url_request_info->query_text),
      search_url_request_info->invocation_source,
      std::move(search_url_request_info->additional_params)));
}

lens::ClientToAimMessage ComposeboxQueryController::CreateClientToAimRequest(
    std::unique_ptr<CreateClientToAimRequestInfo>
        create_client_to_aim_request_info) {
  lens::ClientToAimMessage client_to_aim_message;
  lens::SubmitQuery* submit_query =
      client_to_aim_message.mutable_submit_query();
  submit_query->mutable_payload()->set_query_text(
      create_client_to_aim_request_info->query_text);
  submit_query->mutable_payload()->set_query_text_source(
      create_client_to_aim_request_info->query_text_source);

  submit_query->mutable_payload()->set_model_mode(static_cast<lens::ModelMode>(
      create_client_to_aim_request_info->active_model));
  submit_query->mutable_payload()->set_tool_mode(static_cast<lens::ToolMode>(
      create_client_to_aim_request_info->active_tool));

  // Add additional CGI params.
  for (const auto& param :
       create_client_to_aim_request_info->additional_cgi_params) {
    (*submit_query->mutable_payload()->mutable_cgi_params())[param.first] =
        param.second;
  }

  // Add context turn metadata.
  for (const auto& context_turn_metadata :
       create_client_to_aim_request_info->context_turn_metadata) {
    (*submit_query->mutable_payload()->add_context_turn_metadata()) =
        context_turn_metadata;
  }

  // Add the request id data for each file token.
  if (!active_files_.empty() && cluster_info_.has_value()) {
    bool is_region_interaction =
        create_client_to_aim_request_info
            ->force_include_latest_interaction_request_data &&
        latest_interaction_request_data_ &&
        latest_interaction_request_data_->has_image_crop();

    for (const auto& file_token :
         create_client_to_aim_request_info->file_tokens) {
      auto* file_info = GetFileInfo(file_token);
      if (!file_info ||
          !IsValidContextUploadStatusForMultimodalRequest(
              file_info->upload_status) ||
          !file_info->request_id.has_value()) {
        // Only valid Lens file uploads should have LensImageQueryData created
        // for them. Other contexts should rely on being added to the
        // AddedInputs field in the payload.
        continue;
      }
      lens::LensImageQueryData* lens_image_query_data =
          submit_query->mutable_payload()->add_lens_image_query_data();
      lens_image_query_data->set_search_session_id(
          cluster_info_->search_session_id());
      lens_image_query_data->mutable_request_id()->CopyFrom(
          file_info->request_id.value());
      bool is_overlay_token =
          create_client_to_aim_request_info->overlay_token.has_value() &&
          file_token == *create_client_to_aim_request_info->overlay_token;
      auto media_type =
          (is_region_interaction && is_overlay_token)
              ? lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE
              : file_info->request_id->media_type();
      lens_image_query_data->mutable_request_id()->set_media_type(media_type);
      auto visual_input_type =
          RequestIdToVisualInputType(lens_image_query_data->request_id());
      if (visual_input_type !=
          lens::LensOverlayVisualInputType::VISUAL_INPUT_TYPE_UNKNOWN) {
        lens_image_query_data->set_visual_input_type(visual_input_type);
      }

      if (base::FeatureList::IsEnabled(
              contextual_tasks::
                  kContextualTasksSendContextualInputUploadType) &&
          contextual_tasks::kSendContextualInputUploadTypeInAimRequest.Get()) {
        if (file_info->input_data &&
            file_info->input_data->upload_type.has_value()) {
          lens_image_query_data->set_contextual_input_upload_type(
              *file_info->input_data->upload_type);
        }
      }

      // Only force interaction data for region searches when the overlay is
      // open.
      std::optional<lens::LensOverlayVisualSearchInteractionData>
          visual_search_interaction_data = ConstructVisualSearchInteractionData(
              static_cast<const FileInfo*>(file_info),
              create_client_to_aim_request_info->query_text, std::nullopt,
              is_overlay_token
                  ? create_client_to_aim_request_info
                        ->force_include_latest_interaction_request_data
                  : false);
      if (visual_search_interaction_data.has_value()) {
        lens_image_query_data->mutable_visual_search_interaction_data()
            ->CopyFrom(visual_search_interaction_data.value());
      }
    }

    // Add added inputs.
    lens::AddedInputs added_inputs =
        CreateAddedInputs(create_client_to_aim_request_info->file_tokens,
                          /*include_files_without_lens_usage_intent=*/false);
    if (added_inputs.added_inputs_size() > 0 ||
        added_inputs.turn_title_thumbnail_size() > 0) {
      submit_query->mutable_payload()->mutable_added_inputs()->CopyFrom(
          added_inputs);
    }
  }

  return client_to_aim_message;
}

void ComposeboxQueryController::AddObserver(ContextUploadStatusObserver* obs) {
  observers_.AddObserver(obs);
}

void ComposeboxQueryController::RemoveObserver(
    ContextUploadStatusObserver* obs) {
  observers_.RemoveObserver(obs);
}

void ComposeboxQueryController::StartFileUploadFlow(
    const base::UnguessableToken& file_token,
    std::unique_ptr<lens::ContextualInputData> contextual_input_data,
    std::optional<lens::ImageEncodingOptions> image_options) {
  // When the lazy fetch feature is enabled, the cluster info is fetched when a
  // file upload is needed rather than on initialization.
  if (contextual_tasks::GetIsContextualTasksLazyFetchClusterInfoEnabled() &&
      query_controller_state_ == QueryControllerState::kOff) {
    FetchClusterInfo();
  }
  if (pending_search_url_request_) {
    // If there is a pending search url creation request, fail it immediately,
    // as the new file upload should take priority and the pending url creation
    // request is now stale.
    std::move(pending_search_url_request_).Run(/*failure=*/true);
    ClearFiles();
  }
  // Create a file info struct to hold the file upload data.
  auto file_info = std::make_unique<FileInfo>();
#if BUILDFLAG(IS_IOS)
  // Ensure the app doesn't suspend while we are uploading a file. By creating a
  // ScopedCriticalAction, we tell the system that a critical task is running,
  // granting a grace period if the app is backgrounded.
  file_info->background_action =
      std::make_unique<base::ios::ScopedCriticalAction>("ComposeboxFileUpload");
#endif
  file_info->file_token = file_token;
  if (contextual_input_data->primary_content_type.has_value()) {
    file_info->mime_type = contextual_input_data->primary_content_type.value();
  } else {
    file_info->mime_type = lens::MimeType::kUnknown;
  }

  // At this lower level, files start off as `kNotUploaded`. At
  // this level, this counts as uploading.
  // But when delayed tabs come into consideration,
  // `kNotUploaded` should not be considered uploading.
  file_info->upload_status =
      contextual_search::ContextUploadStatus::kNotUploaded;
  file_info->tab_url = contextual_input_data->page_url;
  file_info->tab_title = contextual_input_data->page_title;
  file_info->tab_session_id = contextual_input_data->tab_session_id;
  file_info->mime_type_string = contextual_input_data->mime_type_string;
  if (contextual_input_data->file_name.has_value()) {
    file_info->file_name = contextual_input_data->file_name.value();
  }
  file_info->is_implicit_upload = contextual_input_data->is_implicit_upload;
  file_info->input_data =
      std::make_unique<lens::ContextualInputData>(*contextual_input_data);

  pending_context_uploads_.insert(file_token);
  auto [it, inserted] = active_files_.emplace(file_token, std::move(file_info));
  DCHECK(inserted);
  FileInfo& current_file_info = *it->second;

  if (contextual_input_data->modality_chip_props.has_value()) {
    // If the input data is for a modality chip, then mark the file as uploaded
    // because the chip is was received from the server.
    current_file_info.input_data->modality_chip_props =
        std::move(contextual_input_data->modality_chip_props.value());
    // If the modality chip contains a pre-computed vsrid, parse it and set it
    // as the request_id. This ensures that the query and any interactions on
    // this chip use the correct request ID and visual input type.
    if (ModalityChipHasVsrid(
            *current_file_info.input_data->modality_chip_props)) {
      auto parsed_request_id =
          lens::LensOverlayRequestIdGenerator::ParseRequestId(
              current_file_info.input_data->modality_chip_props->added_input()
                  .lens_file()
                  .vsrid());
      if (parsed_request_id) {
        current_file_info.request_id = *std::move(parsed_request_id);
      }
    }
    // Modality chips are pre-loaded by the server and do not need client-side
    // uploads. However, they still require `cluster_info_` (specifically the
    // search session ID) to be present to construct a valid multimodal query.
    // If `cluster_info_` is not yet available, the status is set to
    // `kProcessing` to ensure the query submission is stashed until
    // `cluster_info_` is loaded, avoiding omitting the modality chip on the
    // first query of a session.
    if (cluster_info_.has_value()) {
      UpdateContextUploadStatus(
          file_token, contextual_search::ContextUploadStatus::kUploadSuccessful,
          std::nullopt);
    } else {
      UpdateContextUploadStatus(
          file_token, contextual_search::ContextUploadStatus::kProcessing,
          std::nullopt);
    }
    return;
  }

  if (contextual_input_data->context_input.has_value() &&
      !contextual_input_data->context_input->empty()) {
    current_file_info.file_content =
        (*contextual_input_data->context_input)[0].bytes_;
  }

  bool has_viewport_bytes =
      enable_viewport_images_ &&
      contextual_input_data->viewport_screenshot_bytes.has_value();
  bool has_viewport_bitmap =
      enable_viewport_images_ &&
      contextual_input_data->viewport_screenshot.has_value();

  bool has_viewport_screenshot = has_viewport_bitmap || has_viewport_bytes;
  bool has_context_input = contextual_input_data->context_input.has_value() &&
                           !contextual_input_data->context_input->empty();

  // Determine the update mode based on file type and viewport.
  lens::RequestIdUpdateMode base_update_mode =
      lens::RequestIdUpdateMode::kPageContentRequest;
  if (current_file_info.mime_type == lens::MimeType::kImage) {
    base_update_mode = lens::RequestIdUpdateMode::kFullImageRequest;
  } else if (has_viewport_screenshot) {
    // The input data may contain just a viewport without the actual
    // context input data, in the case that the QueryContextualizer determines
    // that the context is a reupload with an updated viewport but unchanged
    // page / pdf contents.
    if (has_context_input) {
      base_update_mode =
          lens::RequestIdUpdateMode::kPageContentWithViewportRequest;
    } else {
      base_update_mode = lens::RequestIdUpdateMode::kFullImageRequest;
    }
  }

  // For the multi-context input flow, whether or not to use the _AND_IMAGE
  // media type depends on whether or not to use separate request ids for the
  // viewport image upload request.
  bool use_has_viewport_media_type =
      has_viewport_screenshot && !use_separate_request_ids_for_viewport_images_;

  std::optional<lens::LensOverlayRequestId> previous_request_id = std::nullopt;
  if (contextual_input_data->context_id.has_value()) {
    for (const auto& [token, info] : active_files_) {
      if (!info) {
        continue;
      }
      if (info->request_id.has_value() &&
          info->request_id->context_id() ==
              contextual_input_data->context_id.value() &&
          !info->is_superceded) {
        // Mark the previous request as superceded since the new request has the
        // same context id and is a newer request.
        info->is_superceded = true;
        previous_request_id = info->request_id;
        break;
      }
    }
  }

  if (previous_request_id.has_value()) {
    // If the previous request ID is available, increment the request ID
    // based on the content type. The media type is assumed to remain
    // unchanged since the previous request id was retrieved from the same
    // source.
    auto previous_request_id_proto =
        std::make_unique<lens::LensOverlayRequestId>(
            previous_request_id.value());
    current_file_info.request_id =
        *request_id_generator_.CreateNextRequestIdForUpdate(
            std::move(previous_request_id_proto), base_update_mode);
    // Recontextualization requests may be implicit even if the original
    // request was not.
    current_file_info.request_id->set_is_implicit_upload(
        current_file_info.is_implicit_upload);
  } else if (current_file_info.input_data->drive_id.has_value()) {
    current_file_info.request_id = *request_id_generator_.GetNextRequestId(
        base_update_mode, current_file_info.mime_type_string.value(),
        lens::LensOverlayRequestId::MEDIA_TYPE_UNRESOLVED);
  } else if (IsUnresolvedUrlUpload(current_file_info)) {
    request_id_generator_.SetContextId(RandInt64());
    request_id_generator_.SetHasChromeTabData(false);
    request_id_generator_.SetIsImplicitUpload(true);
    current_file_info.request_id = *request_id_generator_.GetNextRequestId(
        lens::RequestIdUpdateMode::kMultiContextUploadRequest,
        lens::LensOverlayRequestId::MEDIA_TYPE_UNRESOLVED_URL);
  } else {
    // Unlike image uploads, PDF / page content uploads need to increment the
    // long context id instead of the image sequence id.
    int64_t context_id = current_file_info.input_data->context_id.has_value()
                             ? current_file_info.input_data->context_id.value()
                             : RandInt64();
    request_id_generator_.SetContextId(context_id);
    request_id_generator_.SetHasChromeTabData(
        current_file_info.tab_session_id.has_value());
    request_id_generator_.SetIsImplicitUpload(
        current_file_info.is_implicit_upload);
    if (current_file_info.mime_type != lens::MimeType::kImage &&
        !has_viewport_screenshot &&
        current_file_info.mime_type_string.has_value() &&
        lens::features::IsLensSendRawFileMediaTypesEnabled()) {
      current_file_info.request_id = *request_id_generator_.GetNextRequestId(
          lens::RequestIdUpdateMode::kMultiContextUploadRequest,
          current_file_info.mime_type_string.value(),
          lens::LensOverlayRequestId::MEDIA_TYPE_RAW_FILE);
    } else {
      lens::LensOverlayRequestId::MediaType media_type =
          has_context_input
              ? lens::MimeTypeToMediaType(current_file_info.mime_type,
                                          use_has_viewport_media_type)
              : lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE;
      current_file_info.request_id = *request_id_generator_.GetNextRequestId(
          lens::RequestIdUpdateMode::kMultiContextUploadRequest, media_type);
    }
  }

  UpdateContextUploadStatus(file_token,
                            contextual_search::ContextUploadStatus::kProcessing,
                            std::nullopt);

  if (query_controller_state_ == QueryControllerState::kClusterInfoInvalid) {
    // If we've exhausted retries or are still in the backoff period, fail the
    // context upload immediately.
    if (cluster_info_retries_ >= kMaxClusterInfoRetries) {
      UpdateContextUploadStatus(
          file_token, contextual_search::ContextUploadStatus::kUploadFailed,
          contextual_search::ContextUploadErrorType::kServerError);
      return;
    }

    base::TimeDelta delay = cluster_info_backoff_.GetTimeUntilRelease();
    if (delay.is_positive()) {
      UpdateContextUploadStatus(
          file_token, contextual_search::ContextUploadStatus::kUploadFailed,
          contextual_search::ContextUploadErrorType::kServerError);
      return;
    }

    FetchClusterInfo();
  }

  // If the cluster info is available, update the file upload status to ready
  // for suggest.
  // If the file upload later fails due to
  // validation failures, the suggest response will be empty so it is safe to
  // kick off the suggestions fetch at this point.
  if (cluster_info_.has_value()) {
    // TODO(crbug.com/452401443): Listen for this new status from the webui.
    UpdateContextUploadStatus(
        file_token,
        contextual_search::ContextUploadStatus::kProcessingSuggestSignalsReady,
        std::nullopt);
  }

  // If the is_page_context_eligible is set to false, then fail early.
  if (contextual_input_data->is_page_context_eligible.has_value() &&
      !contextual_input_data->is_page_context_eligible.value()) {
    // TODO(crbug.com/444276947): Consider adding a new error type for this.
    UpdateContextUploadStatus(
        file_token, contextual_search::ContextUploadStatus::kValidationFailed,
        contextual_search::ContextUploadErrorType::kBrowserProcessingError);
    return;
  }

  // Preparing for the upload requests require multiple async flows to
  // complete before the request is ready to be send to the server. Start the
  // required flows here, and each flow completes by calling the ready method,
  // i.e., OnUploadRequestBodyReady(). The ready method will handle waiting
  // for all the necessary flows to complete before performing the request.
  // Async Flow 1: Fetching the cluster info, which is shared across all
  // requests. This flow only occurs once per session and occurs in
  // InitializeIfNeeded().
  // Async Flow 2: Retrieve the OAuth headers.
  current_file_info.context_upload_access_token_fetcher_ =
      CreateOAuthHeadersAndContinue(base::BindOnce(
          &ComposeboxQueryController::OnUploadRequestHeadersReady,
          weak_ptr_factory_.GetWeakPtr(), file_token));

  // Async Flow 3: Creating the file and viewport upload request.
  CreateUploadRequestBodiesAndContinue(
      file_token, std::move(contextual_input_data), image_options);
}

// static
void ComposeboxQueryController::
    CreateFileUploadRequestProtoWithImageDataAndContinue(
        lens::LensOverlayRequestId request_id,
        lens::LensOverlayClientContext client_context,
        scoped_refptr<lens::RefCountedLensOverlayClientLogs> client_logs,
        RequestBodyProtoCreatedCallback callback,
        std::optional<GURL> page_url,
        std::optional<std::string> page_title,
        std::optional<std::string> file_name,
        lens::ImageData image_data) {
  // Validate that client-side image compression/encoding succeeded.
  if (image_data.payload().image_bytes().empty()) {
    std::move(callback).Run(
        lens::LensOverlayServerRequest(),
        contextual_search::ContextUploadErrorType::kImageProcessingError);
    return;
  }
  lens::LensOverlayServerRequest request;
  auto* objects_request = request.mutable_objects_request();
  objects_request->mutable_request_context()->mutable_request_id()->CopyFrom(
      request_id);
  objects_request->mutable_request_context()
      ->mutable_client_context()
      ->CopyFrom(client_context);

  if (file_name.has_value()) {
    image_data.mutable_image_metadata()->set_file_name(file_name.value());
  }

  PopulateContentMetadata(objects_request->mutable_payload(), page_url,
                          page_title, file_name, /*drive_id=*/std::nullopt,
                          /*resource_key=*/std::nullopt,
                          /*parsed_url=*/std::nullopt);

  objects_request->mutable_image_data()->CopyFrom(image_data);
  request.mutable_client_logs()->CopyFrom(client_logs->client_logs());
  std::move(callback).Run(std::move(request), /*error_type=*/std::nullopt);
}

std::unique_ptr<EndpointFetcher>
ComposeboxQueryController::CreateEndpointFetcher(
    std::string request_string,
    const GURL& fetch_url,
    HttpMethod http_method,
    base::TimeDelta timeout,
    const std::vector<std::string>& request_headers,
    const std::vector<std::string>& cors_exempt_headers,
    UploadProgressCallback upload_progress_callback) {
  return std::make_unique<EndpointFetcher>(
      url_loader_factory_, /*identity_manager=*/nullptr,
      EndpointFetcher::RequestParams::Builder(http_method,
                                              kTrafficAnnotationTag)
          .SetAuthType(endpoint_fetcher::CHROME_API_KEY)
          .SetChannel(channel_)
          .SetContentType(kContentType)
          .SetCorsExemptHeaders(cors_exempt_headers)
          .SetCredentialsMode(CredentialsMode::kInclude)
          .SetHeaders(request_headers)
          .SetPostData(std::move(request_string))
          .SetSetSiteForCookies(true)
          .SetTimeout(timeout)
          .SetUploadProgressCallback(std::move(upload_progress_callback))
          .SetUrl(fetch_url)
          .Build());
}

lens::LensOverlayClientContext ComposeboxQueryController::CreateClientContext()
    const {
  lens::LensOverlayClientContext context;
  context.set_surface(lens::SURFACE_LENS_OVERLAY);
  context.set_platform(lens::PLATFORM_LENS_OVERLAY);
  context.mutable_client_filters()->add_filter()->set_filter_type(
      lens::AUTO_FILTER);
  context.mutable_locale_context()->set_language(locale_);
  context.mutable_locale_context()->set_region(
      icu::Locale(locale_.c_str()).getCountry());

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

bool ComposeboxQueryController::DeleteFile(
    const base::UnguessableToken& file_token) {
  MarkContextUploadAsInTerminalState(file_token);
  return !!active_files_.erase(file_token);
}

void ComposeboxQueryController::ClearFiles() {
  pending_context_uploads_.clear();
  active_files_.clear();

  // Uploading files no longer block the search URL creation.
  // Try to resume any pending search URL creation, if it exists.
  if (pending_search_url_request_) {
    std::move(pending_search_url_request_).Run(/*failure=*/false);
  }
}

std::unique_ptr<lens::proto::LensOverlaySuggestInputs>
ComposeboxQueryController::CreateSuggestInputs(
    const std::vector<base::UnguessableToken>& attached_context_tokens) {
  std::unique_ptr<lens::proto::LensOverlaySuggestInputs> suggest_inputs =
      std::make_unique<lens::proto::LensOverlaySuggestInputs>();

  FileInfo* file_info = nullptr;

  if (prioritize_suggestions_for_the_first_attached_document_) {
    // Serve suggestions for the first attached PDF document.
    for (const auto& token : attached_context_tokens) {
      FileInfo* attachment_info = GetMutableFileInfo(token);
      // Skip past failed uploads.
      if (!attachment_info) {
        continue;
      }

      // Fall back to the first element ever added in case BE supports that.
      if (!file_info) {
        file_info = attachment_info;
      }

      // Look for the first PDF/Tab attachment and pick that in the absence over
      // any other attachment.
      if (attachment_info->mime_type == lens::MimeType::kPdf ||
          attachment_info->mime_type == lens::MimeType::kHtml ||
          attachment_info->mime_type == lens::MimeType::kPlainText ||
          attachment_info->mime_type == lens::MimeType::kAnnotatedPageContent) {
        file_info = attachment_info;
        break;
      }
    }
  } else {
    // Only a single file is supported for suggest inputs.
    if (attached_context_tokens.size() != 1) {
      return suggest_inputs;
    }
    file_info = GetMutableFileInfo(attached_context_tokens.at(0));
  }

  if (!file_info || !file_info->request_id.has_value()) {
    return suggest_inputs;
  }

  suggest_inputs->set_encoded_request_id(
      lens::Base64EncodeRequestId(file_info->request_id.value()));
  // TODO(crbug.com/445777189): Support multi-context input id flow for
  // suggest.
  suggest_inputs->set_contextual_visual_input_type(
      lens::VitQueryParamValueForMediaType(
          file_info->request_id->media_type()));

  if (attach_page_title_and_url_to_suggest_requests_) {
    suggest_inputs->set_send_page_title_and_url(true);
    suggest_inputs->set_page_title(file_info->tab_title.value_or(""));
    if (file_info->input_data &&
        file_info->input_data->parsed_url.has_value()) {
      suggest_inputs->set_page_url(file_info->input_data->parsed_url.value());
    } else if (file_info->tab_url.has_value()) {
      suggest_inputs->set_page_url(file_info->tab_url.value().spec());
    }
  }

  // If the cluster info is already available, update the suggest inputs.
  suggest_inputs->set_send_gsession_vsrid_for_contextual_suggest(true);
  if (cluster_info_.has_value()) {
    suggest_inputs->set_search_session_id(
        cluster_info_.value().search_session_id());
  }

  return suggest_inputs;
}

// TODO(crbug.com/424869589): Clean up code duplication with
// LensOverlayQueryController.
std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
ComposeboxQueryController::CreateOAuthHeadersAndContinue(
    OAuthHeadersCreatedCallback callback) {
  // Use OAuth if the user is logged in.
  if (identity_manager_ &&
      identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    signin::AccessTokenFetcher::TokenCallback token_callback =
        base::BindOnce(&lens::CreateOAuthHeader).Then(std::move(callback));
    return std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
        signin::OAuthConsumerId::kComposeboxQueryController, identity_manager_,
        std::move(token_callback),
        signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
        signin::ConsentLevel::kSignin);
  }

  // Fall back to fetching the endpoint directly using API key.
  std::move(callback).Run(std::vector<std::string>());
  return nullptr;
}

void ComposeboxQueryController::ClearClusterInfo() {
  cluster_info_access_token_fetcher_.reset();
  cluster_info_endpoint_fetcher_.reset();
  cluster_info_.reset();
  cluster_info_retries_ = 0;
  cluster_info_backoff_.Reset();
  request_id_generator_.ResetRequestId();
  SetQueryControllerState(QueryControllerState::kOff);
}

void ComposeboxQueryController::ResetRequestClusterInfoState() {
  ClearClusterInfo();
  // Iterate through any existing files and mark them as expired.
  // TODO(crbug.com/432125987): Handle file reupload after cluster info
  // expiration.
  std::vector<base::UnguessableToken> file_tokens_to_expire;
  for (const auto& [file_token, file_info] : active_files_) {
    file_tokens_to_expire.push_back(file_token);
  }

  for (const auto& file_token : file_tokens_to_expire) {
    auto* file_info = GetMutableFileInfo(file_token);
    if (!file_info) {
      continue;
    }
    // Stop the upload requests if they are in progress.
    for (const auto& upload_request : file_info->upload_requests_) {
      if (upload_request->endpoint_fetcher_) {
        upload_request->endpoint_fetcher_.reset();
      }
    }
    if (file_info->upload_status !=
        contextual_search::ContextUploadStatus::kValidationFailed) {
      UpdateContextUploadStatus(
          file_token, contextual_search::ContextUploadStatus::kUploadExpired,
          std::nullopt);
    }
  }
  SetQueryControllerState(QueryControllerState::kClusterInfoInvalid);

  // Fetch new cluster info.
  FetchClusterInfo();
}

void ComposeboxQueryController::SendInteractionRequest(
    std::unique_ptr<lens::LensOverlayRequestId> request_id,
    std::string query_text,
    std::optional<lens::ImageCrop> image_crop,
    std::optional<lens::LensOverlayClientLogs> client_logs,
    std::optional<lens::LensOverlaySelectionType> lens_overlay_selection_type,
    base::OnceCallback<void(lens::LensOverlayInteractionResponse)>
        interaction_response_callback) {
  latest_interaction_request_data_ =
      std::make_unique<LensServerInteractionRequest>(std::move(request_id));
  latest_interaction_request_data_->interaction_response_callback_ =
      std::move(interaction_response_callback);

  // Start getting the OAuth headers for the interaction request.
  latest_interaction_request_data_->interaction_access_token_fetcher_ =
      CreateOAuthHeadersAndContinue(base::BindOnce(
          &ComposeboxQueryController::OnInteractionRequestHeadersReady,
          weak_ptr_factory_.GetWeakPtr()));

  lens::LensOverlayServerRequest server_request;
  if (client_logs.has_value()) {
    server_request.mutable_client_logs()->CopyFrom(*client_logs);
  }
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

  CHECK(image_crop.has_value() || !query_text.empty());
  lens::LensOverlayInteractionRequestMetadata interaction_request_metadata;
  if (image_crop.has_value()) {
    // Add the region for region search and multimodal requests.
    server_request.mutable_interaction_request()
        ->mutable_image_crop()
        ->CopyFrom(*image_crop);
    interaction_request_metadata.set_type(
        lens::LensOverlayInteractionRequestMetadata::REGION_SEARCH);
    interaction_request_metadata.mutable_selection_metadata()
        ->mutable_region()
        ->mutable_region()
        ->CopyFrom(*image_crop->mutable_zoomed_crop()->mutable_crop());

    // Add the text, for multimodal requests.
    if (!query_text.empty()) {
      interaction_request_metadata.mutable_query_metadata()
          ->mutable_text_query()
          ->set_query(query_text);
    }
  } else if (!query_text.empty()) {
    lens::LensOverlayInteractionRequestMetadata::Type interaction_type =
        RequestIdToInteractionType(
            *latest_interaction_request_data_->request_id_);
    // If there is only `query_text`, this is a contextual flow.
    interaction_request_metadata.set_type(interaction_type);
    interaction_request_metadata.mutable_query_metadata()
        ->mutable_text_query()
        ->set_query(query_text);
  }

  server_request.mutable_interaction_request()
      ->mutable_interaction_request_metadata()
      ->CopyFrom(interaction_request_metadata);
  latest_interaction_request_data_->request_ =
      std::make_unique<lens::LensOverlayServerRequest>(
          std::move(server_request));
  TrySendInteractionRequest();
}

void ComposeboxQueryController::FetchClusterInfo() {
  if (is_backgrounded_) {
    return;
  }
  SetQueryControllerState(QueryControllerState::kAwaitingClusterInfoResponse);

  // There should not be any in-flight cluster info access token request.
  // TODO(crbug.com/452221931): Replace with a CHECK once the cause is found.
  if (cluster_info_access_token_fetcher_) {
    base::debug::DumpWithoutCrashing();
#if DCHECK_IS_ON()
    NOTREACHED() << "Cluster info access token fetcher already exists.";
#endif  // DCHECK_IS_ON()
  }
  cluster_info_access_token_fetcher_ = CreateOAuthHeadersAndContinue(
      base::BindOnce(&ComposeboxQueryController::SendClusterInfoNetworkRequest,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ComposeboxQueryController::SendClusterInfoNetworkRequest(
    std::vector<std::string> request_headers) {
  cluster_info_access_token_fetcher_.reset();

  // Add protobuf content type to the request headers.
  request_headers.push_back(kContentTypeKey);
  request_headers.push_back(kContentType);

  // Get client experiment variations to include in the request.
  std::vector<std::string> cors_exempt_headers;
  // The variations client may be null in tests.
  if (variations_client_) {
    cors_exempt_headers = lens::CreateVariationsHeaders(variations_client_);
  }

  // Generate the URL to fetch.
  GURL fetch_url = GURL(lens::features::GetLensOverlayClusterInfoEndpointUrl());

  std::string request_string;
  // Create the client context to include in the request.
  lens::LensOverlayClientContext client_context = CreateClientContext();
  lens::LensOverlayServerClusterInfoRequest request;
  request.set_surface(client_context.surface());
  request.set_platform(client_context.platform());
  CHECK(request.SerializeToString(&request_string));

  // Create the EndpointFetcher, responsible for making the request using our
  // given params. Store in class variable to keep endpoint fetcher alive until
  // the request is made.
  cluster_info_endpoint_fetcher_ = CreateEndpointFetcher(
      std::move(request_string), fetch_url, HttpMethod::kPost,
      base::Milliseconds(lens::features::GetLensOverlayServerRequestTimeout()),
      request_headers, cors_exempt_headers, base::DoNothing());

  // Finally, perform the request.
  cluster_info_endpoint_fetcher_->PerformRequest(
      base::BindOnce(&ComposeboxQueryController::HandleClusterInfoResponse,
                     weak_ptr_factory_.GetWeakPtr()),
      google_apis::GetAPIKey().c_str());
}

void ComposeboxQueryController::HandleClusterInfoResponse(
    std::unique_ptr<endpoint_fetcher::EndpointResponse> response) {
  if (is_backgrounded_) {
    return;
  }
  cluster_info_endpoint_fetcher_.reset();

  if (response->http_status_code != google_apis::ApiErrorCode::HTTP_SUCCESS) {
    ++cluster_info_retries_;

    if (cluster_info_retries_ <= kMaxClusterInfoRetries) {
      cluster_info_backoff_.InformOfRequest(false);
    }

    SetQueryControllerState(QueryControllerState::kClusterInfoInvalid);

    // Fail uploads that are waiting on cluster info.
    std::vector<base::UnguessableToken> file_tokens_to_fail;
    for (const auto& [file_token, file_info] : active_files_) {
      if (file_info->upload_status ==
          contextual_search::ContextUploadStatus::kProcessing) {
        file_tokens_to_fail.push_back(file_token);
      }
    }
    for (const auto& file_token : file_tokens_to_fail) {
      UpdateContextUploadStatus(
          file_token, contextual_search::ContextUploadStatus::kUploadFailed,
          contextual_search::ContextUploadErrorType::kServerError);
    }

    if (pending_search_url_request_) {
      std::move(pending_search_url_request_).Run(/*failure=*/false);
    }
    return;
  }

  cluster_info_backoff_.Reset();

  lens::LensOverlayServerClusterInfoResponse server_response;
  if (!server_response.ParseFromString(response->response)) {
    SetQueryControllerState(QueryControllerState::kClusterInfoInvalid);
    if (pending_search_url_request_) {
      std::move(pending_search_url_request_).Run(/*failure=*/false);
    }
    return;
  }

  // Store the cluster info.
  cluster_info_ = std::make_optional<lens::LensOverlayClusterInfo>();
  cluster_info_->set_server_session_id(server_response.server_session_id());
  cluster_info_->set_search_session_id(server_response.search_session_id());
  if (server_response.has_routing_info()) {
    cluster_info_->mutable_routing_info()->CopyFrom(
        server_response.routing_info());
    std::unique_ptr<lens::LensOverlayRequestId> request_id =
        request_id_generator_.SetRoutingInfo(server_response.routing_info());

    // Update the request id in all of the active_files_ to use the new routing
    // info.
    for (auto& [file_token, file_info] : active_files_) {
      if (file_info->request_id.has_value()) {
        file_info->request_id->mutable_routing_info()->CopyFrom(
            server_response.routing_info());
      }
      if (file_info->viewport_request_id_) {
        file_info->viewport_request_id_->mutable_routing_info()->CopyFrom(
            server_response.routing_info());
      }
    }
  }
  SetQueryControllerState(QueryControllerState::kClusterInfoReceived);

  // Collect the tokens and requests that need updating first to avoid iterator
  // invalidation if observers synchronously mutate active_files_ during status
  // updates.
  std::vector<base::UnguessableToken> modality_chips_to_succeed;
  std::vector<base::UnguessableToken> files_to_suggest_signals_ready;
  std::vector<std::pair<base::UnguessableToken, size_t>>
      upload_requests_to_send;

  for (const auto& [file_token, file_info] : active_files_) {
    if (file_info->input_data &&
        file_info->input_data->modality_chip_props.has_value()) {
      if (file_info->upload_status ==
          contextual_search::ContextUploadStatus::kProcessing) {
        modality_chips_to_succeed.push_back(file_token);
      }
    } else {
      if (file_info->upload_status ==
          contextual_search::ContextUploadStatus::kProcessing) {
        files_to_suggest_signals_ready.push_back(file_token);
      }
      for (size_t i = 0; i < file_info->upload_requests_.size(); ++i) {
        upload_requests_to_send.emplace_back(file_token, i);
      }
    }
  }

  for (const auto& file_token : modality_chips_to_succeed) {
    // Modality chips that were held in `kProcessing` status waiting for
    // `cluster_info_` are now marked as `kUploadSuccessful` because the
    // required session tokens are available. This transitions them to a
    // terminal status and triggers the query handler to resume any pending
    // stashed queries.
    UpdateContextUploadStatus(
        file_token, contextual_search::ContextUploadStatus::kUploadSuccessful,
        std::nullopt);
  }

  for (const auto& file_token : files_to_suggest_signals_ready) {
    // If the file is processing, set its state to suggest signals ready.
    UpdateContextUploadStatus(
        file_token,
        contextual_search::ContextUploadStatus::kProcessingSuggestSignalsReady,
        std::nullopt);
  }

  for (const auto& [file_token, request_index] : upload_requests_to_send) {
    // Trigger pending upload requests.
    MaybeSendUploadNetworkRequest(file_token, request_index);
  }

  if (pending_search_url_request_) {
    std::move(pending_search_url_request_).Run(/*failure=*/false);
  }

  // Clear the cluster info after its lifetime expires.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ComposeboxQueryController::ResetRequestClusterInfoState,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Seconds(
          lens::features::GetLensOverlayClusterInfoLifetimeSeconds()));
}

void ComposeboxQueryController::SetQueryControllerState(
    QueryControllerState new_state) {
  if (query_controller_state_ != new_state) {
    query_controller_state_ = new_state;
    if (on_query_controller_state_changed_callback_) {
      on_query_controller_state_changed_callback_.Run(new_state);
    }
  }
}

// Marks the file upload as in terminal state and creates search URL
// if request was stashed. File token is passed by value to avoid use-after-free
// error caused by erasing the file info from the `active_files_` map before
// this method is called.
void ComposeboxQueryController::MarkContextUploadAsInTerminalState(
    base::UnguessableToken file_token) {
  pending_context_uploads_.erase(file_token);
  if (pending_context_uploads_.empty() && pending_search_url_request_) {
    std::move(pending_search_url_request_).Run(/*failure=*/false);
  }
}

void ComposeboxQueryController::UpdateContextUploadStatus(
    base::UnguessableToken file_token,
    contextual_search::ContextUploadStatus status,
    std::optional<contextual_search::ContextUploadErrorType> error_type) {
  auto* file_info = GetMutableFileInfo(file_token);
  if (!file_info) {
    return;
  }

  for (auto& observer : observers_) {
    observer.OnContextUploadStatusChanged(file_token, file_info->mime_type,
                                          status, error_type);
  }
  if (!IsValidContextUploadStatusForMultimodalRequest(status) &&
      status != contextual_search::ContextUploadStatus::kUploadExpired) {
    active_files_.erase(file_token);
    // Once we start uploading a file in `StartFileUploadFlow`, if
    // we get `kNotUploaded` status outside of `StartFileUploadFlow`,
    // we consider it a failure, as it is the second `kNotUploaded`.
    if (status == contextual_search::ContextUploadStatus::kNotUploaded) {
      MarkContextUploadAsInTerminalState(file_token);
    }
  } else {
    file_info->upload_status = status;
  }
  if (contextual_search::IsTerminalContextStatus(status)) {
    MarkContextUploadAsInTerminalState(file_token);
  }
}

void ComposeboxQueryController::ProcessDecodedImageAndContinue(
    lens::LensOverlayRequestId request_id,
    const lens::ImageEncodingOptions& image_options,
    RequestBodyProtoCreatedCallback callback,
    std::optional<GURL> page_url,
    std::optional<std::string> page_title,
    std::optional<std::string> file_name,
    const SkBitmap& bitmap) {
#if !BUILDFLAG(IS_IOS)
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  if (bitmap.isNull() || bitmap.empty()) {
    std::move(callback).Run(
        lens::LensOverlayServerRequest(),
        contextual_search::ContextUploadErrorType::kImageProcessingError);
    return;
  }

  // If the bitmap is a viewport bitmap, it will be destroyed after the
  // owning ContextualInputData is destroyed (i.e. at the end of
  // CreateUploadRequestBodiesAndContinue). To ensure the bitmap is not
  // destroyed before it is used, make a copy of the bitmap.
  SkBitmap bitmap_copy = bitmap;

  // Downscaling and encoding is done on a background thread to avoid blocking
  // the main thread.
  create_request_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&lens::DownscaleAndEncodeBitmap, std::move(bitmap_copy),
                     ref_counted_logs, image_options),
      base::BindOnce(&ComposeboxQueryController::
                         CreateFileUploadRequestProtoWithImageDataAndContinue,
                     request_id, CreateClientContext(), ref_counted_logs,
                     std::move(callback), page_url, page_title, file_name));
#endif  // !BUILDFLAG(IS_IOS)
}

void ComposeboxQueryController::CreateImageUploadRequest(
    lens::LensOverlayRequestId request_id,
    std::vector<uint8_t> image_data,
    std::optional<lens::ImageEncodingOptions> image_options,
    std::optional<GURL> page_url,
    std::optional<std::string> page_title,
    std::optional<std::string> file_name,
    RequestBodyProtoCreatedCallback callback) {
#if !BUILDFLAG(IS_IOS)
  CHECK(image_options.has_value());
  data_decoder::DecodeImageIsolated(
      image_data, data_decoder::mojom::ImageCodec::kDefault,
      /*shrink_to_fit=*/false,
      /*max_size_in_bytes=*/std::numeric_limits<int64_t>::max(),
      /*desired_image_frame_size=*/gfx::Size(),
      base::BindOnce(&ComposeboxQueryController::ProcessDecodedImageAndContinue,
                     weak_ptr_factory_.GetWeakPtr(), request_id,
                     image_options.value(), std::move(callback), page_url,
                     page_title, file_name));
#endif  // !BUILDFLAG(IS_IOS)
}

void ComposeboxQueryController::CreateUploadRequestBodiesAndContinue(
    const base::UnguessableToken& file_token,
    std::unique_ptr<lens::ContextualInputData> contextual_input_data,
    std::optional<lens::ImageEncodingOptions> image_options) {
  auto* file_info = GetMutableFileInfo(file_token);
  if (!file_info) {
    return;
  }

  bool has_lens_usage_intent = contextual_input_data->has_lens_usage_intent;

  // If there is a viewport screenshot, create the viewport upload request body.
  // TODO(crbug.com/442685171): Pass the pdf page number to the viewport
  // upload request if available.
  if (enable_viewport_images_ &&
      contextual_input_data->viewport_screenshot_bytes.has_value()) {
    CHECK(image_options.has_value());
    size_t request_index = file_info->num_outstanding_network_requests_++;
    CreateImageUploadRequest(
        GetRequestIdForViewportImage(file_token),
        // Pass ownership of the viewport screenshot bytes to the callback.
        std::move(contextual_input_data->viewport_screenshot_bytes.value()),
        std::move(image_options), contextual_input_data->page_url,
        contextual_input_data->page_title, /*file_name=*/std::nullopt,
        base::BindOnce(
            &ComposeboxQueryController::AddPageIndexToUploadRequestAndContinue,
            weak_ptr_factory_.GetWeakPtr(),
            contextual_input_data->pdf_current_page,
            base::BindOnce(
                &ComposeboxQueryController::
                    AddLensUsageIntentToUploadRequestAndContinue,
                weak_ptr_factory_.GetWeakPtr(), has_lens_usage_intent,
                base::BindOnce(
                    &ComposeboxQueryController::OnUploadRequestBodyReady,
                    weak_ptr_factory_.GetWeakPtr(), file_token,
                    request_index))));
  } else if (enable_viewport_images_ &&
             contextual_input_data->viewport_screenshot.has_value()) {
    CHECK(image_options.has_value());
    size_t request_index = file_info->num_outstanding_network_requests_++;
    ProcessDecodedImageAndContinue(
        GetRequestIdForViewportImage(file_token), image_options.value(),
        base::BindOnce(
            &ComposeboxQueryController::AddPageIndexToUploadRequestAndContinue,
            weak_ptr_factory_.GetWeakPtr(),
            contextual_input_data->pdf_current_page,
            base::BindOnce(
                &ComposeboxQueryController::
                    AddLensUsageIntentToUploadRequestAndContinue,
                weak_ptr_factory_.GetWeakPtr(), has_lens_usage_intent,
                base::BindOnce(
                    &ComposeboxQueryController::OnUploadRequestBodyReady,
                    weak_ptr_factory_.GetWeakPtr(), file_token,
                    request_index))),
        contextual_input_data->page_url, contextual_input_data->page_title,
        /*file_name=*/std::nullopt,
        // Pass ownership of the viewport screenshot to the
        // callback.
        std::move(*contextual_input_data->viewport_screenshot));
  }

  // After potentially synchronous image processing, ensure the FileInfo
  // still exists. It may have been deleted by an error callback. We re-fetch
  // it from the map here to avoid a Use-After-Free if the original file_info
  // pointer was invalidated.
  file_info = GetMutableFileInfo(file_token);
  if (!file_info) {
    return;
  }

  switch (file_info->mime_type) {
    case lens::MimeType::kPdf:
      [[fallthrough]];
    case lens::MimeType::kAnnotatedPageContent:
      [[fallthrough]];
    case lens::MimeType::kUnknown: {
      if (!contextual_input_data->context_input.has_value() ||
          contextual_input_data->context_input->empty()) {
        if (enable_viewport_images_ &&
            (contextual_input_data->viewport_screenshot_bytes.has_value() ||
             contextual_input_data->viewport_screenshot.has_value())) {
          // This is a reupload with an updated viewport but unchanged page /
          // pdf contents. Other than the viewport upload set up earlier
          // in this function, no other uploads are needed.
          break;
        }
        if (file_info->mime_type != lens::MimeType::kUnknown) {
          UpdateContextUploadStatus(
              file_info->file_token,
              contextual_search::ContextUploadStatus::kValidationFailed,
              contextual_search::ContextUploadErrorType::
                  kBrowserProcessingError);
          return;
        }
      }
      // Call CreateContentextualDataUploadPayload off the main thread to avoid
      // blocking the main thread on compression.
      size_t request_index = file_info->num_outstanding_network_requests_++;
      create_request_task_runner_->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(
              &CreateContentextualDataUploadPayload,
              // Pass ownership of the contextual input data to the callback.
              contextual_input_data->context_input.has_value()
                  ? std::move(contextual_input_data->context_input.value())
                  : std::vector<lens::ContextualInput>(),
              contextual_input_data->page_url,
              contextual_input_data->page_title,
              contextual_input_data->drive_id,
              contextual_input_data->resource_key,
              contextual_input_data->file_name,
              contextual_input_data->parsed_url),
          base::BindOnce(
              &CreateFileUploadRequestProtoWithPayloadAndContinue,
              file_info->request_id.value(), CreateClientContext(),
              base::BindOnce(
                  &ComposeboxQueryController::
                      AddLensUsageIntentToUploadRequestAndContinue,
                  weak_ptr_factory_.GetWeakPtr(), has_lens_usage_intent,
                  base::BindOnce(
                      &ComposeboxQueryController::
                          AddPageIndexToUploadRequestAndContinue,
                      weak_ptr_factory_.GetWeakPtr(),
                      contextual_input_data->pdf_current_page,
                      base::BindOnce(
                          &ComposeboxQueryController::OnUploadRequestBodyReady,
                          weak_ptr_factory_.GetWeakPtr(), file_token,
                          request_index)))));
    } break;
    case lens::MimeType::kImage:
      if (contextual_input_data->context_input.has_value() &&
          contextual_input_data->context_input->empty() &&
          contextual_input_data->viewport_screenshot.has_value()) {
        // In this case, the viewport screenshot should have already been
        // uploaded above. It does not need to be uploaded again.
        break;
      } else {
        CHECK(contextual_input_data->context_input.has_value() &&
              contextual_input_data->context_input->size() == 1);
        size_t request_index = file_info->num_outstanding_network_requests_++;
        CreateImageUploadRequest(
            file_info->request_id.value(),
            // Pass ownership of the contextual input data to the callback.
            std::move(contextual_input_data->context_input->front().bytes_),
            std::move(image_options), contextual_input_data->page_url,
            contextual_input_data->page_title, contextual_input_data->file_name,
            base::BindOnce(
                &ComposeboxQueryController::
                    AddLensUsageIntentToUploadRequestAndContinue,
                weak_ptr_factory_.GetWeakPtr(), has_lens_usage_intent,
                base::BindOnce(
                    &ComposeboxQueryController::OnUploadRequestBodyReady,
                    weak_ptr_factory_.GetWeakPtr(), file_token,
                    request_index)));
      }
      break;
    default:
      UpdateContextUploadStatus(
          file_info->file_token,
          contextual_search::ContextUploadStatus::kValidationFailed,
          contextual_search::ContextUploadErrorType::kBrowserProcessingError);
      break;
  }
}

void ComposeboxQueryController::AddLensUsageIntentToUploadRequestAndContinue(
    bool has_lens_usage_intent,
    RequestBodyProtoCreatedCallback callback,
    lens::LensOverlayServerRequest request,
    std::optional<contextual_search::ContextUploadErrorType> error_type) {
  if (!error_type.has_value()) {
    request.set_has_lens_intent(has_lens_usage_intent);
    request.set_process_image_for_aim(has_lens_usage_intent);
  }

  std::move(callback).Run(std::move(request), error_type);
}

void ComposeboxQueryController::AddPageIndexToUploadRequestAndContinue(
    std::optional<size_t> pdf_page_index,
    RequestBodyProtoCreatedCallback callback,
    lens::LensOverlayServerRequest request,
    std::optional<contextual_search::ContextUploadErrorType> error_type) {
  if (!error_type.has_value() && pdf_page_index.has_value()) {
    request.mutable_objects_request()
        ->mutable_viewport_request_context()
        ->set_pdf_page_number(pdf_page_index.value());
  }

  std::move(callback).Run(request, error_type);
}

void ComposeboxQueryController::OnUploadRequestBodyReady(
    const base::UnguessableToken& file_token,
    size_t request_index,
    lens::LensOverlayServerRequest request,
    std::optional<contextual_search::ContextUploadErrorType> error_type) {
  auto* file_info = GetMutableFileInfo(file_token);
  if (!file_info) {
    return;
  }

  if (error_type.has_value()) {
    UpdateContextUploadStatus(
        file_info->file_token,
        contextual_search::ContextUploadStatus::kValidationFailed, error_type);
    return;
  }

  // Create the upload requests if they haven't been created yet.
  while (file_info->upload_requests_.size() <= request_index) {
    file_info->upload_requests_.push_back(std::make_unique<UploadRequest>());
  }
  file_info->upload_requests_[request_index]->request_body =
      std::make_unique<lens::LensOverlayServerRequest>(request);
  MaybeSendUploadNetworkRequest(file_token, request_index);
}

void ComposeboxQueryController::OnUploadRequestHeadersReady(
    const base::UnguessableToken& file_token,
    std::vector<std::string> headers) {
  auto* file_info = GetMutableFileInfo(file_token);
  if (!file_info) {
    return;
  }

  file_info->context_upload_access_token_fetcher_.reset();
  file_info->request_headers_ =
      std::make_unique<std::vector<std::string>>(headers);
  for (size_t i = 0; i < file_info->upload_requests_.size(); ++i) {
    MaybeSendUploadNetworkRequest(file_token, i);
  }
}

void ComposeboxQueryController::MaybeSendUploadNetworkRequest(
    const base::UnguessableToken& file_token,
    size_t request_index) {
  auto* file_info = GetMutableFileInfo(file_token);
  if (!file_info) {
    return;
  }

  CHECK_LT(request_index, file_info->upload_requests_.size());
  UploadRequest* upload_request =
      file_info->upload_requests_[request_index].get();
  CHECK(upload_request);
  // Check that the request is ready to be sent and has not yet been sent.
  if (file_info->request_headers_ && upload_request->request_body &&
      upload_request->response_code == 0 &&
      !upload_request->endpoint_fetcher_ && cluster_info_.has_value()) {
    SendUploadNetworkRequest(file_info, request_index);
  }
}

void ComposeboxQueryController::SendUploadNetworkRequest(FileInfo* file_info,
                                                         size_t request_index) {
  CHECK_LT(request_index, file_info->upload_requests_.size());
  UploadRequest* upload_request =
      file_info->upload_requests_[request_index].get();
  CHECK(upload_request);
  CHECK(upload_request->request_body);
  CHECK(file_info->request_headers_);
  PerformFetchRequest(
      upload_request->request_body.get(), file_info->request_headers_.get(),
      base::Milliseconds(
          lens::features::GetLensOverlayPageContentRequestTimeoutMs()),
      base::BindOnce(&ComposeboxQueryController::OnUploadEndpointFetcherCreated,
                     weak_ptr_factory_.GetWeakPtr(), file_info->file_token,
                     request_index),
      base::BindOnce(&ComposeboxQueryController::HandleUploadResponse,
                     weak_ptr_factory_.GetWeakPtr(), file_info->file_token,
                     request_index),
      /*upload_progress_callback=*/base::DoNothing());
}

void ComposeboxQueryController::OnInteractionRequestHeadersReady(
    std::vector<std::string> headers) {
  if (latest_interaction_request_data_) {
    latest_interaction_request_data_->request_headers_ =
        std::make_unique<std::vector<std::string>>(headers);
    TrySendInteractionRequest();
  }
}

void ComposeboxQueryController::TrySendInteractionRequest() {
  bool has_interaction_request = latest_interaction_request_data_ &&
                                 latest_interaction_request_data_->request_;
  bool has_cluster_info = cluster_info_.has_value();
  bool has_request_headers = latest_interaction_request_data_ &&
                             latest_interaction_request_data_->request_headers_;
  bool has_not_sent_request = !latest_interaction_request_data_->request_sent_;
  if (has_interaction_request && has_cluster_info && has_request_headers &&
      has_not_sent_request) {
    latest_interaction_request_data_->request_sent_ = true;
    PerformFetchRequest(
        latest_interaction_request_data_->request_.get(),
        latest_interaction_request_data_->request_headers_.get(),
        base::Milliseconds(
            lens::features::GetLensOverlayPageContentRequestTimeoutMs()),
        base::BindOnce(
            &ComposeboxQueryController::OnInteractionEndpointFetcherCreated,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&ComposeboxQueryController::HandleInteractionResponse,
                       weak_ptr_factory_.GetWeakPtr()),
        /*upload_progress_callback=*/base::DoNothing());
  }
}

void ComposeboxQueryController::OnInteractionEndpointFetcherCreated(
    std::unique_ptr<EndpointFetcher> endpoint_fetcher) {
  latest_interaction_request_data_->interaction_endpoint_fetcher_ =
      std::move(endpoint_fetcher);
}

void ComposeboxQueryController::HandleInteractionResponse(
    std::unique_ptr<EndpointResponse> response) {
  latest_interaction_request_data_->interaction_endpoint_fetcher_.reset();

  if (response->http_status_code != google_apis::ApiErrorCode::HTTP_SUCCESS) {
    return;
  }

  lens::LensOverlayServerResponse server_response;
  if (!server_response.ParseFromString(response->response)) {
    return;
  }

  if (!server_response.has_interaction_response()) {
    return;
  }

  if (latest_interaction_request_data_->interaction_response_callback_) {
    std::move(latest_interaction_request_data_->interaction_response_callback_)
        .Run(server_response.interaction_response());
  }
}

void ComposeboxQueryController::OnUploadEndpointFetcherCreated(
    const base::UnguessableToken& file_token,
    size_t request_index,
    std::unique_ptr<EndpointFetcher> endpoint_fetcher) {
  auto* file_info = GetMutableFileInfo(file_token);
  if (!file_info) {
    return;
  }

  CHECK_LT(request_index, file_info->upload_requests_.size());
  UploadRequest* upload_request =
      file_info->upload_requests_[request_index].get();
  CHECK(upload_request);

  upload_request->start_time = base::Time::Now();
  upload_request->endpoint_fetcher_ = std::move(endpoint_fetcher);
  if (file_info->upload_status ==
          contextual_search::ContextUploadStatus::kProcessing ||
      file_info->upload_status == contextual_search::ContextUploadStatus::
                                      kProcessingSuggestSignalsReady) {
    UpdateContextUploadStatus(
        file_info->file_token,
        contextual_search::ContextUploadStatus::kUploadStarted, std::nullopt);
  }
}

void ComposeboxQueryController::HandleUploadResponse(
    const base::UnguessableToken& file_token,
    size_t request_index,
    std::unique_ptr<EndpointResponse> response) {
  auto* file_info = GetMutableFileInfo(file_token);
  if (!file_info) {
    return;
  }

  file_info->num_outstanding_network_requests_--;

  CHECK_LT(request_index, file_info->upload_requests_.size());
  UploadRequest* upload_request =
      file_info->upload_requests_[request_index].get();
  CHECK(upload_request);

  upload_request->response_time = base::Time::Now();
  upload_request->response_code = response->http_status_code;
  upload_request->endpoint_fetcher_.reset();

  if (response->http_status_code != google_apis::ApiErrorCode::HTTP_SUCCESS) {
    file_info->upload_error_type =
        contextual_search::ContextUploadErrorType::kServerError;
#if BUILDFLAG(IS_IOS)
    file_info->background_action.reset();
#endif
    UpdateContextUploadStatus(
        file_token, contextual_search::ContextUploadStatus::kUploadFailed,
        contextual_search::ContextUploadErrorType::kServerError);
    return;
  }

  // Store the response body for later processing.
  file_info->response_bodies.push_back(response->response);

  // If the file was still uploading and there are no more outstanding network
  // requests, update the file upload status to successful. The upload status
  // would have been set to ServerError if the response code for any prior
  // request was not successful.
  if (file_info->upload_status ==
          contextual_search::ContextUploadStatus::kUploadStarted &&
      file_info->num_outstanding_network_requests_ == 0) {
#if BUILDFLAG(IS_IOS)
    file_info->background_action.reset();
#endif
    UpdateContextUploadStatus(
        file_token, contextual_search::ContextUploadStatus::kUploadSuccessful,
        std::nullopt);
  }
}

void ComposeboxQueryController::PerformFetchRequest(
    lens::LensOverlayServerRequest* request,
    std::vector<std::string>* request_headers,
    base::TimeDelta timeout,
    base::OnceCallback<void(std::unique_ptr<endpoint_fetcher::EndpointFetcher>)>
        fetcher_created_callback,
    endpoint_fetcher::EndpointFetcherCallback response_received_callback,
    UploadProgressCallback upload_progress_callback) {
  CHECK_EQ(query_controller_state_, QueryControllerState::kClusterInfoReceived);
  CHECK(cluster_info_.has_value());

  // If the cluster info has routing info, update the request to use it.
  // This ensures that the latest routing info that corresponds with the
  // server session id is used for the request, even if the cluster info
  // has been updated since the request was created.
  if (cluster_info_->has_routing_info()) {
    if (request->has_objects_request()) {
      request->mutable_objects_request()
          ->mutable_request_context()
          ->mutable_request_id()
          ->mutable_routing_info()
          ->CopyFrom(cluster_info_->routing_info());
    } else if (request->has_interaction_request()) {
      request->mutable_interaction_request()
          ->mutable_request_context()
          ->mutable_request_id()
          ->mutable_routing_info()
          ->CopyFrom(cluster_info_->routing_info());
    }
  }

  // Get client experiment variations to include in the request.
  std::vector<std::string> cors_exempt_headers;
  // The variations client may be null in tests.
  if (variations_client_) {
    cors_exempt_headers = lens::CreateVariationsHeaders(variations_client_);
  }

  // Generate the URL to fetch to and include the server session id if present.
  GURL fetch_url = GURL(lens::features::GetLensOverlayEndpointURL());
  // The endpoint fetches should use the server session id from the cluster
  // info.
  fetch_url =
      net::AppendOrReplaceQueryParameter(fetch_url, kSessionIdQueryParameterKey,
                                         cluster_info_->server_session_id());

  std::string request_string;
  CHECK(request->SerializeToString(&request_string));

  // Create the EndpointFetcher, responsible for making the request using our
  // given params.
  std::unique_ptr<EndpointFetcher> endpoint_fetcher = CreateEndpointFetcher(
      std::move(request_string), fetch_url, HttpMethod::kPost,
      base::Milliseconds(
          lens::features::GetLensOverlayPageContentRequestTimeoutMs()),
      *request_headers, cors_exempt_headers,
      std::move(upload_progress_callback));
  EndpointFetcher* fetcher = endpoint_fetcher.get();
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(fetcher_created_callback),
                                std::move(endpoint_fetcher)));

  // Finally, perform the request.
  fetcher->PerformRequest(std::move(response_received_callback),
                          google_apis::GetAPIKey().c_str());
}

const contextual_search::FileInfo* ComposeboxQueryController::GetFileInfo(
    const base::UnguessableToken& file_token) {
  return GetMutableFileInfo(file_token);
}

std::vector<const contextual_search::FileInfo*>
ComposeboxQueryController::GetFileInfoList() {
  std::vector<const contextual_search::FileInfo*> file_infos;
  file_infos.reserve(active_files_.size());
  for (const auto& [file_token, file_info] : active_files_) {
    file_infos.push_back(file_info.get());
  }
  return file_infos;
}

base::WeakPtr<contextual_search::ContextualSearchContextController>
ComposeboxQueryController::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ComposeboxQueryController::BeforeCreateSearchUrl(
    std::unique_ptr<CreateSearchUrlRequestInfo> search_url_request_info,
    base::OnceCallback<void(GURL)> callback,
    bool failed_creation) {
  if (failed_creation) {
    std::move(callback).Run(GURL());
    return;
  }
  ComposeboxQueryController::CreateSearchUrl(std::move(search_url_request_info),
                                             std::move(callback));
}

ComposeboxQueryController::FileInfo*
ComposeboxQueryController::GetMutableFileInfo(
    const base::UnguessableToken& file_token) {
  auto it = active_files_.find(file_token);
  if (it == active_files_.end()) {
    return nullptr;
  }
  return it->second.get();
}

void ComposeboxQueryController::AddEncodedVisualSearchInteractionLogDataParam(
    const FileInfo* file_info,
    const std::optional<std::string>& query_text,
    std::optional<lens::LensOverlaySelectionType> lens_overlay_selection_type,
    std::map<std::string, std::string>& url_params_map) {
  std::optional<lens::LensOverlayVisualSearchInteractionData> interaction_data =
      ConstructVisualSearchInteractionData(
          file_info, query_text, lens_overlay_selection_type,
          /*force_include_latest_interaction_request_data=*/false);

  if (!interaction_data.has_value()) {
    return;
  }

  std::string serialized_proto;
  CHECK(interaction_data->SerializeToString(&serialized_proto));
  std::string encoded_proto;
  base::Base64UrlEncode(serialized_proto,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_proto);

  url_params_map.insert(
      {kVisualSearchInteractionQueryParameterKey, encoded_proto});
}

std::optional<lens::LensOverlayVisualSearchInteractionData>
ComposeboxQueryController::ConstructVisualSearchInteractionData(
    const FileInfo* file_info,
    const std::optional<std::string>& query_text,
    std::optional<lens::LensOverlaySelectionType> lens_overlay_selection_type,
    bool force_include_latest_interaction_request_data) {
  if (!file_info ||
      !IsValidContextUploadStatusForMultimodalRequest(
          file_info->upload_status) ||
      !file_info->request_id.has_value()) {
    return std::nullopt;
  }

  // If there was an interaction request which has not been used to create a
  // vsint yet, then set the interaction data from the request.
  bool has_interaction_request =
      latest_interaction_request_data_ &&
      latest_interaction_request_data_->request_ &&
      latest_interaction_request_data_->request_->has_interaction_request();
  bool should_include_interaction_request_data =
      has_interaction_request &&
      (!latest_interaction_request_data_->interaction_details_used_in_vsint_ ||
       force_include_latest_interaction_request_data);

  if (!should_include_interaction_request_data) {
    return std::nullopt;
  }

  auto sent_interaction_request =
      latest_interaction_request_data_->request_->interaction_request();
  if (!sent_interaction_request.has_image_crop()) {
    return std::nullopt;
  }

  latest_interaction_request_data_->interaction_details_used_in_vsint_ = true;

  // Set the interaction data based on the last file request type.
  lens::LensOverlayVisualSearchInteractionData interaction_data;
  interaction_data.mutable_log_data()->mutable_filter_data()->set_filter_type(
      lens::AUTO_FILTER);
  interaction_data.mutable_log_data()
      ->mutable_user_selection_data()
      ->set_selection_type(lens::MULTIMODAL_SEARCH);
  if (lens_overlay_selection_type.has_value()) {
    interaction_data.mutable_log_data()
        ->mutable_user_selection_data()
        ->set_selection_type(lens_overlay_selection_type.value());
  }
  interaction_data.mutable_log_data()->set_client_platform(
      lens::CLIENT_PLATFORM_LENS_OVERLAY);

  // A query that generates a url without an interaction request is a parent
  // query.
  // TODO(crbug.com/462506270): Set to false if this is not the first query.
  interaction_data.mutable_log_data()->set_is_parent_query(true);

  if (query_text.has_value()) {
    interaction_data.mutable_text_select()->set_selected_texts(
        query_text.value());
  }

  interaction_data.set_interaction_type(
      sent_interaction_request.interaction_request_metadata().type());

  interaction_data.mutable_zoomed_crop()->CopyFrom(
      sent_interaction_request.image_crop().zoomed_crop());

  return interaction_data;
}
