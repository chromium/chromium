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
constexpr char kVisualRequestIdQueryParameterKey[] = "vsrid";

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

// Creates a payload for a contextual data upload request, for webpage contents
// or for uploaded pdf files.
lens::Payload CreateContentextualDataUploadPayload(
    std::vector<lens::ContextualInput> context_inputs,
    std::optional<GURL> page_url,
    std::optional<std::string> page_title) {
  lens::Payload payload;
  auto* content = payload.mutable_content();

  if (page_url.has_value() && !page_url->is_empty()) {
    content->set_webpage_url(page_url->spec());
  }
  if (page_title.has_value() && !page_title.value().empty()) {
    content->set_webpage_title(page_title.value());
  }

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

// Returns true if the file upload status is valid to include in the multimodal
// request.
bool IsValidFileUploadStatusForMultimodalRequest(
    contextual_search::FileUploadStatus upload_status) {
  return upload_status == contextual_search::FileUploadStatus::kProcessing ||
         upload_status == contextual_search::FileUploadStatus::
                              kProcessingSuggestSignalsReady ||
         upload_status == contextual_search::FileUploadStatus::kUploadStarted ||
         upload_status ==
             contextual_search::FileUploadStatus::kUploadSuccessful;
}

// Returns true if the media type has an image.
bool MediaTypeHasImage(lens::LensOverlayRequestId::MediaType media_type) {
  return media_type == lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE ||
         media_type ==
             lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE ||
         media_type == lens::LensOverlayRequestId::MEDIA_TYPE_PDF_AND_IMAGE;
}

// Returns the interaction type for the given media type.
lens::LensOverlayInteractionRequestMetadata::Type MediaTypeToInteractionType(
    lens::LensOverlayRequestId::MediaType media_type) {
  switch (media_type) {
    case lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE:
    case lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE:
      return lens::LensOverlayInteractionRequestMetadata::WEBPAGE_QUERY;
    case lens::LensOverlayRequestId::MEDIA_TYPE_PDF:
    case lens::LensOverlayRequestId::MEDIA_TYPE_PDF_AND_IMAGE:
      return lens::LensOverlayInteractionRequestMetadata::PDF_QUERY;
    default:
      return lens::LensOverlayInteractionRequestMetadata::
          CONTEXTUAL_SEARCH_QUERY;
  }
}

// Returns the LensOverlayVisualInputType for the given media type.
lens::LensOverlayVisualInputType MediaTypeToVisualInputType(
    lens::LensOverlayRequestId::MediaType media_type) {
  switch (media_type) {
    case lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE:
      return lens::LensOverlayVisualInputType::VISUAL_INPUT_TYPE_UNKNOWN;
    case lens::LensOverlayRequestId::MEDIA_TYPE_PDF:
    case lens::LensOverlayRequestId::MEDIA_TYPE_PDF_AND_IMAGE:
      return lens::LensOverlayVisualInputType::VISUAL_INPUT_TYPE_PDF;
    case lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE:
    case lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE:
      return lens::LensOverlayVisualInputType::VISUAL_INPUT_TYPE_WEBPAGE;
    default:
      return lens::LensOverlayVisualInputType::VISUAL_INPUT_TYPE_UNKNOWN;
  }
}

int64_t RandInt64() {
  int64_t number;
  base::RandBytes(base::byte_span_from_ref(number));
  return number;
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
      variations_client_(variations_client) {
  send_lns_surface_ = feature_params->send_lns_surface;
  suppress_lns_surface_param_if_no_image_ =
      feature_params->suppress_lns_surface_param_if_no_image;
  enable_multi_context_input_flow_ =
      feature_params->enable_multi_context_input_flow;
  enable_viewport_images_ = feature_params->enable_viewport_images;
  use_separate_request_ids_for_multi_context_viewport_images_ =
      feature_params
          ->use_separate_request_ids_for_multi_context_viewport_images;
  prioritize_suggestions_for_the_first_attached_document_ =
      feature_params->prioritize_suggestions_for_the_first_attached_document;

  attach_page_title_and_url_to_suggest_requests_ =
      feature_params->attach_page_title_and_url_to_suggest_requests;

  // Enable multi-context input if the contextual tasks feature is enabled.
  // This allows the query controller to behave consistently for co-browsing
  // enabled users, even if the NTP or Omnibox entrypoints have different
  // configurations.
  if (base::FeatureList::IsEnabled(contextual_tasks::kContextualTasks)) {
    enable_multi_context_input_flow_ = true;
    use_separate_request_ids_for_multi_context_viewport_images_ = false;
  }

  attach_page_title_and_url_to_suggest_requests_ =
      feature_params->attach_page_title_and_url_to_suggest_requests;
  create_request_task_runner_ = base::ThreadPool::CreateTaskRunner(
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

ComposeboxQueryController::~ComposeboxQueryController() = default;

void ComposeboxQueryController::InitializeIfNeeded() {
  if (query_controller_state_ == QueryControllerState::kOff) {
    // The query controller state starts at kOff. If it is set to any other
    // state by the call to FetchClusterInfo(), this indicates that the
    // handshake has already been initialized.
    FetchClusterInfo();
  }
}

lens::LensOverlayRequestId
ComposeboxQueryController::GetRequestIdForViewportImage(
    const base::UnguessableToken& file_token) {
  auto* file_info = GetMutableFileInfo(file_token);
  if (!file_info) {
    return lens::LensOverlayRequestId();
  }
  if (enable_multi_context_input_flow_ &&
      use_separate_request_ids_for_multi_context_viewport_images_) {
    // Create a new request id for the viewport image upload request.
    file_info->viewport_request_id_ = request_id_generator_.GetNextRequestId(
        lens::RequestIdUpdateMode::kMultiContextUploadRequest,
        lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
    return *file_info->viewport_request_id_;
  }
  return file_info->request_id;
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
  if (should_create_multimodal_url &&
      query_controller_state_ ==
          QueryControllerState::kAwaitingClusterInfoResponse) {
    pending_search_url_request_ =
        base::BindOnce(&ComposeboxQueryController::CreateSearchUrl,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(search_url_request_info), std::move(callback));
    return;
  }

  if (should_create_multimodal_url && cluster_info_.has_value()) {
    if (enable_multi_context_input_flow_) {
      std::unique_ptr<lens::LensOverlayContextualInputs> contextual_inputs =
          std::make_unique<lens::LensOverlayContextualInputs>();
      const FileInfo* last_active_file = nullptr;
      bool has_image_upload = false;
      size_t num_valid_files = 0;
      for (const auto& file_token : search_url_request_info->file_tokens) {
        auto* file_info = GetMutableFileInfo(file_token);
        if (!file_info) {
          continue;
        }
        if (IsValidFileUploadStatusForMultimodalRequest(
                file_info->upload_status)) {
          num_valid_files++;
          auto* contextual_input = contextual_inputs->add_inputs();
          contextual_input->mutable_request_id()->CopyFrom(
              file_info->request_id);
          has_image_upload |=
              MediaTypeHasImage(file_info->request_id.media_type());

          // Add the viewport request id to the contextual inputs if it exists.
          if (file_info->viewport_request_id_) {
            auto* viewport_contextual_input = contextual_inputs->add_inputs();
            viewport_contextual_input->mutable_request_id()->CopyFrom(
                *file_info->viewport_request_id_);
            has_image_upload = true;
          }
          last_active_file = file_info;
        }
      }

      if (num_valid_files > 0) {
        // Trigger the interaction request on the last file if needed.
        // TODO(crbug.com/462509148): Determine how to support interaction
        // requests for multi-context input flow.
        if (search_url_request_info->lens_overlay_selection_type.has_value()) {
          auto interaction_request_id = request_id_generator_.GetNextRequestId(
              lens::RequestIdUpdateMode::kInteractionRequest,
              search_url_request_info->image_crop.has_value()
                  ? lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE
                  : last_active_file->request_id.media_type(),
              std::make_optional<int64_t>(last_active_file->GetContextId()));
          SendInteractionRequest(
              std::move(interaction_request_id),
              search_url_request_info->query_text,
              search_url_request_info->image_crop,
              search_url_request_info->client_logs,
              search_url_request_info->lens_overlay_selection_type,
              std::move(
                  search_url_request_info->interaction_response_callback));

          auto* interaction_contextual_input = contextual_inputs->add_inputs();
          interaction_contextual_input->mutable_request_id()->CopyFrom(
              *latest_interaction_request_data_->request_id_);

          std::unique_ptr<lens::LensOverlayRequestId> search_url_request_id;
          lens::LensOverlayRequestId* request_id_for_vsrid;
          search_url_request_id = request_id_generator_.GetNextRequestId(
              lens::RequestIdUpdateMode::kSearchUrl,
              search_url_request_info->image_crop.has_value()
                  ? lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE
                  : last_active_file->request_id.media_type());
          request_id_for_vsrid = search_url_request_id.get();
          std::string serialized_request_id;
          CHECK(
              request_id_for_vsrid->SerializeToString(&serialized_request_id));
          std::string encoded_request_id;
          base::Base64UrlEncode(serialized_request_id,
                                base::Base64UrlEncodePolicy::OMIT_PADDING,
                                &encoded_request_id);
          search_url_request_info->additional_params.insert(
              {kVisualRequestIdQueryParameterKey, encoded_request_id});
        }

        AddEncodedVisualSearchInteractionLogDataParam(
            last_active_file, search_url_request_info->query_text,
            search_url_request_info->lens_overlay_selection_type,
            search_url_request_info->additional_params);
        // Get the encoded visual search interaction log data.
        bool should_send_lns_surface =
            send_lns_surface_ &&
            (!suppress_lns_surface_param_if_no_image_ || has_image_upload);
        std::move(callback).Run(GetUrlForMultimodalSearch(
            template_url_service_,
            /*is_aim_search=*/search_url_request_info->search_url_type ==
                SearchUrlType::kAim,
            search_url_request_info->aim_entry_point,
            search_url_request_info->query_start_time,
            cluster_info_->search_session_id(), std::move(contextual_inputs),
            search_url_request_info->invocation_source,
            should_send_lns_surface ? kLnsSurfaceParameterValue : std::string(),
            base::UTF8ToUTF16(search_url_request_info->query_text),
            std::move(search_url_request_info->additional_params)));
        return;
      }
    } else {
      // When multi-context input flow is not enabled, only one file is
      // supported.
      // Use the last file uploaded to determine `vit` param.
      // TODO(crbug.com/446972028): Remove this once multi-context input flow is
      // fully supported.
      auto* last_file = active_files_.rbegin()->second.get();
      if (last_file && IsValidFileUploadStatusForMultimodalRequest(
                           last_file->upload_status)) {
        // Trigger the interaction request if needed.
        // TODO(crbug.com/462509148): Determine how to support interaction
        // requests for multi-context input flow.
        if (search_url_request_info->lens_overlay_selection_type.has_value()) {
          SendInteractionRequest(
              request_id_generator_.GetNextRequestId(
                  lens::RequestIdUpdateMode::kInteractionRequest,
                  last_file->request_id.media_type()),
              search_url_request_info->query_text,
              search_url_request_info->image_crop,
              search_url_request_info->client_logs,
              search_url_request_info->lens_overlay_selection_type,
              std::move(
                  search_url_request_info->interaction_response_callback));
        }

        // Get the encoded visual search interaction log data after triggering
        // the interaction request if needed, so that interaction metadata
        // is included.
        AddEncodedVisualSearchInteractionLogDataParam(
            last_file, search_url_request_info->query_text,
            search_url_request_info->lens_overlay_selection_type,
            search_url_request_info->additional_params);
        bool should_send_lns_surface =
            send_lns_surface_ &&
            (!suppress_lns_surface_param_if_no_image_ ||
             MediaTypeHasImage(last_file->request_id.media_type()));
        std::move(callback).Run(GetUrlForMultimodalSearch(
            template_url_service_,
            /*is_aim_search=*/search_url_request_info->search_url_type ==
                SearchUrlType::kAim,
            search_url_request_info->aim_entry_point,
            search_url_request_info->query_start_time,
            cluster_info_->search_session_id(),
            request_id_generator_.GetNextRequestId(
                lens::RequestIdUpdateMode::kSearchUrl,
                last_file->request_id.media_type()),
            last_file->mime_type, search_url_request_info->invocation_source,
            should_send_lns_surface ? kLnsSurfaceParameterValue : std::string(),
            base::UTF8ToUTF16(search_url_request_info->query_text),
            std::move(search_url_request_info->additional_params)));
        return;
      }
    }
  }

  // Treat queries in which the cluster info has expired, or without valid
  // contextual inputs, as unimodal text queries.
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
  submit_query->mutable_payload()->set_use_research_agent(
      create_client_to_aim_request_info->deep_search_selected);
  submit_query->mutable_payload()->set_use_image_generation(
      create_client_to_aim_request_info->create_images_selected);

  // Add additional CGI params.
  for (const auto& param :
       create_client_to_aim_request_info->additional_cgi_params) {
    (*submit_query->mutable_payload()
          ->mutable_additional_cgi_params())[param.first] = param.second;
  }

  // Add the request id data for each file token.
  if (!active_files_.empty() && cluster_info_.has_value()) {
    for (const auto& file_token :
         create_client_to_aim_request_info->file_tokens) {
      auto* file_info = GetFileInfo(file_token);
      if (!file_info || !IsValidFileUploadStatusForMultimodalRequest(
                            file_info->upload_status)) {
        continue;
      }
      lens::LensImageQueryData* lens_image_query_data =
          submit_query->mutable_payload()->add_lens_image_query_data();
      lens_image_query_data->set_search_session_id(
          cluster_info_->search_session_id());
      lens_image_query_data->mutable_request_id()->CopyFrom(
          file_info->request_id);
      auto media_type = file_info->request_id.media_type();
      lens_image_query_data->set_visual_input_type(
          MediaTypeToVisualInputType(media_type));
    }
  }

  // Add the latest visual search interaction data to the query if it exists.
  // Only check the first file token since the interaction should be associated
  // with the a single contextual input.
  if (!create_client_to_aim_request_info->file_tokens.empty()) {
    auto* file_info =
        GetFileInfo(create_client_to_aim_request_info->file_tokens[0]);
    if (file_info &&
        IsValidFileUploadStatusForMultimodalRequest(file_info->upload_status)) {
      std::optional<lens::LensOverlayVisualSearchInteractionData>
          visual_search_interaction_data = ConstructVisualSearchInteractionData(
              static_cast<const FileInfo*>(file_info),
              create_client_to_aim_request_info->query_text, std::nullopt);
      if (visual_search_interaction_data.has_value()) {
        for (auto& lens_image_query_data :
             *submit_query->mutable_payload()
                  ->mutable_lens_image_query_data()) {
          lens_image_query_data.mutable_visual_search_interaction_data()
              ->CopyFrom(visual_search_interaction_data.value());
        }
      }
    }
  }

  return client_to_aim_message;
}

void ComposeboxQueryController::AddObserver(FileUploadStatusObserver* obs) {
  observers_.AddObserver(obs);
}

void ComposeboxQueryController::RemoveObserver(FileUploadStatusObserver* obs) {
  observers_.RemoveObserver(obs);
}

void ComposeboxQueryController::StartFileUploadFlow(
    const base::UnguessableToken& file_token,
    std::unique_ptr<lens::ContextualInputData> contextual_input_data,
    std::optional<lens::ImageEncodingOptions> image_options) {
  // Create a file info struct to hold the file upload data.
  auto file_info = std::make_unique<FileInfo>();
  file_info->file_token = file_token;
  file_info->mime_type = contextual_input_data->primary_content_type.value();
  file_info->upload_status = contextual_search::FileUploadStatus::kNotUploaded;
  file_info->tab_url = contextual_input_data->page_url;
  file_info->tab_title = contextual_input_data->page_title;
  file_info->tab_session_id = contextual_input_data->tab_session_id;
  file_info->input_data =
      std::make_unique<lens::ContextualInputData>(*contextual_input_data);

  auto [it, inserted] = active_files_.emplace(file_token, std::move(file_info));
  DCHECK(inserted);
  FileInfo& current_file_info = *it->second;

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

  // Determine the update mode based on file type and viewport.
  lens::RequestIdUpdateMode base_update_mode =
      lens::RequestIdUpdateMode::kPageContentRequest;
  if (current_file_info.mime_type == lens::MimeType::kImage) {
    base_update_mode = lens::RequestIdUpdateMode::kFullImageRequest;
  } else if (has_viewport_screenshot) {
    base_update_mode =
        lens::RequestIdUpdateMode::kPageContentWithViewportRequest;
  }

  // For the multi-context input flow, whether or not to use the _AND_IMAGE
  // media type depends on whether or not to use separate request ids for the
  // viewport image upload request.
  bool use_has_viewport_media_type =
      has_viewport_screenshot &&
      (!enable_multi_context_input_flow_ ||
       !use_separate_request_ids_for_multi_context_viewport_images_);

  std::optional<lens::LensOverlayRequestId> previous_request_id = std::nullopt;
  if (contextual_input_data->context_id.has_value()) {
    for (const auto& [token, info] : active_files_) {
      if (!info) {
        continue;
      }
      if (info->request_id.context_id() ==
          contextual_input_data->context_id.value()) {
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
  } else {
    // Unlike image uploads, PDF / page content uploads need to increment the
    // long context id instead of the image sequence id.
    int64_t context_id = contextual_input_data->context_id.has_value()
                             ? contextual_input_data->context_id.value()
                             : RandInt64();
    lens::RequestIdUpdateMode update_mode =
        enable_multi_context_input_flow_
            ? lens::RequestIdUpdateMode::kMultiContextUploadRequest
            : base_update_mode;

    current_file_info.request_id = *request_id_generator_.GetNextRequestId(
        update_mode,
        lens::MimeTypeToMediaType(current_file_info.mime_type,
                                  use_has_viewport_media_type),
        context_id);
  }

  // Update the file upload status to processing.
  UpdateFileUploadStatus(file_token,
                         contextual_search::FileUploadStatus::kProcessing,
                         std::nullopt);

  // If the cluster info is available, update the file upload status to ready
  // for suggest.
  // If the file upload later fails due to
  // validation failures, the suggest response will be empty so it is safe to
  // kick off the suggestions fetch at this point.
  if (cluster_info_.has_value()) {
    // TODO(crbug.com/452401443): Listen for this new status from the webui.
    UpdateFileUploadStatus(
        file_token,
        contextual_search::FileUploadStatus::kProcessingSuggestSignalsReady,
        std::nullopt);
  }

  // If the is_page_context_eligible is set to false, then fail early.
  if (contextual_input_data->is_page_context_eligible.has_value() &&
      !contextual_input_data->is_page_context_eligible.value()) {
    // TODO(crbug.com/444276947): Consider adding a new error type for this.
    UpdateFileUploadStatus(
        file_token, contextual_search::FileUploadStatus::kValidationFailed,
        contextual_search::FileUploadErrorType::kBrowserProcessingError);
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
  current_file_info.file_upload_access_token_fetcher_ =
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
        lens::ImageData image_data) {
  lens::LensOverlayServerRequest request;
  auto* objects_request = request.mutable_objects_request();
  objects_request->mutable_request_context()->mutable_request_id()->CopyFrom(
      request_id);
  objects_request->mutable_request_context()
      ->mutable_client_context()
      ->CopyFrom(client_context);
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
  return !!active_files_.erase(file_token);
}

void ComposeboxQueryController::ClearFiles() {
  active_files_.clear();
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

  if (!file_info) {
    return suggest_inputs;
  }

  suggest_inputs->set_encoded_request_id(
      lens::Base64EncodeRequestId(file_info->request_id));
  // TODO(crbug.com/445777189): Support multi-context input id flow for
  // suggest.
  suggest_inputs->set_contextual_visual_input_type(
      lens::VitQueryParamValueForMediaType(file_info->request_id.media_type()));

  if (attach_page_title_and_url_to_suggest_requests_) {
    suggest_inputs->set_send_page_title_and_url(true);
    suggest_inputs->set_page_title(file_info->tab_title.value_or(""));
    if (file_info->tab_url.has_value()) {
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
    signin::ScopeSet oauth_scopes;
    oauth_scopes.insert(GaiaConstants::kLensOAuth2Scope);
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
  request_id_generator_.ResetRequestId();
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
        contextual_search::FileUploadStatus::kValidationFailed) {
      UpdateFileUploadStatus(
          file_token, contextual_search::FileUploadStatus::kUploadExpired,
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
    lens::LensOverlayRequestId::MediaType media_type =
        latest_interaction_request_data_->request_id_->media_type();
    lens::LensOverlayInteractionRequestMetadata::Type interaction_type =
        MediaTypeToInteractionType(media_type);
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
  cluster_info_endpoint_fetcher_.reset();
  if (response->http_status_code != google_apis::ApiErrorCode::HTTP_SUCCESS) {
    SetQueryControllerState(QueryControllerState::kClusterInfoInvalid);
    if (pending_search_url_request_) {
      std::move(pending_search_url_request_).Run();
    }
    return;
  }

  lens::LensOverlayServerClusterInfoResponse server_response;
  if (!server_response.ParseFromString(response->response)) {
    SetQueryControllerState(QueryControllerState::kClusterInfoInvalid);
    if (pending_search_url_request_) {
      std::move(pending_search_url_request_).Run();
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
      file_info->request_id.mutable_routing_info()->CopyFrom(
          server_response.routing_info());
      if (file_info->viewport_request_id_) {
        file_info->viewport_request_id_->mutable_routing_info()->CopyFrom(
            server_response.routing_info());
      }
    }
  }
  SetQueryControllerState(QueryControllerState::kClusterInfoReceived);

  // Iterate through any existing files and send the upload requests if ready.
  for (const auto& [file_token, file_info] : active_files_) {
    // If the file is processing, set its state to suggest signals ready.
    if (file_info->upload_status ==
        contextual_search::FileUploadStatus::kProcessing) {
      UpdateFileUploadStatus(
          file_token,
          contextual_search::FileUploadStatus::kProcessingSuggestSignalsReady,
          std::nullopt);
    }

    // Trigger pending upload requests.
    for (size_t i = 0; i < file_info->upload_requests_.size(); ++i) {
      MaybeSendUploadNetworkRequest(file_token, i);
    }
  }

  if (pending_search_url_request_) {
    std::move(pending_search_url_request_).Run();
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

void ComposeboxQueryController::UpdateFileUploadStatus(
    const base::UnguessableToken& file_token,
    contextual_search::FileUploadStatus status,
    std::optional<contextual_search::FileUploadErrorType> error_type) {
  auto* file_info = GetMutableFileInfo(file_token);
  if (!file_info) {
    return;
  }

  for (auto& observer : observers_) {
    observer.OnFileUploadStatusChanged(file_token, file_info->mime_type, status,
                                       error_type);
  }
  if (!IsValidFileUploadStatusForMultimodalRequest(status) &&
      status != contextual_search::FileUploadStatus::kUploadExpired) {
    active_files_.erase(file_token);
  } else {
    file_info->upload_status = status;
  }
}

void ComposeboxQueryController::ProcessDecodedImageAndContinue(
    lens::LensOverlayRequestId request_id,
    const lens::ImageEncodingOptions& image_options,
    RequestBodyProtoCreatedCallback callback,
    const SkBitmap& bitmap) {
#if !BUILDFLAG(IS_IOS)
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  if (bitmap.isNull() || bitmap.empty()) {
    std::move(callback).Run(
        lens::LensOverlayServerRequest(),
        contextual_search::FileUploadErrorType::kImageProcessingError);
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
                     std::move(callback)));
#endif  // !BUILDFLAG(IS_IOS)
}

void ComposeboxQueryController::CreateImageUploadRequest(
    lens::LensOverlayRequestId request_id,
    const std::vector<uint8_t>& image_data,
    std::optional<lens::ImageEncodingOptions> image_options,
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
                     image_options.value(), std::move(callback)));
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

  // If there is a viewport screenshot, create the viewport upload request body.
  // TODO(crbug.com/442685171): Pass the pdf page number to the viewport
  // upload request if available.
  if (enable_viewport_images_ &&
      contextual_input_data->viewport_screenshot_bytes.has_value()) {
    CHECK(image_options.has_value());
    CreateImageUploadRequest(
        GetRequestIdForViewportImage(file_token),
        // Pass ownership of the viewport screenshot bytes to the callback.
        std::move(contextual_input_data->viewport_screenshot_bytes.value()),
        std::move(image_options),
        base::BindOnce(
            &ComposeboxQueryController::
                AddPageIndexToImageUploadRequestAndContinue,
            weak_ptr_factory_.GetWeakPtr(),
            std::move(contextual_input_data->pdf_current_page),
            base::BindOnce(&ComposeboxQueryController::OnUploadRequestBodyReady,
                           weak_ptr_factory_.GetWeakPtr(), file_token,
                           file_info->num_outstanding_network_requests_++)));
  } else if (enable_viewport_images_ &&
             contextual_input_data->viewport_screenshot.has_value()) {
    CHECK(image_options.has_value());
    ProcessDecodedImageAndContinue(
        GetRequestIdForViewportImage(file_token), image_options.value(),
        base::BindOnce(
            &ComposeboxQueryController::
                AddPageIndexToImageUploadRequestAndContinue,
            weak_ptr_factory_.GetWeakPtr(),
            std::move(contextual_input_data->pdf_current_page),
            base::BindOnce(&ComposeboxQueryController::OnUploadRequestBodyReady,
                           weak_ptr_factory_.GetWeakPtr(), file_token,
                           file_info->num_outstanding_network_requests_++)),
        // Pass ownership of the viewport screenshot to the
        // callback.
        std::move(*contextual_input_data->viewport_screenshot));
  }

  switch (file_info->mime_type) {
    case lens::MimeType::kPdf:
      [[fallthrough]];
    case lens::MimeType::kAnnotatedPageContent:
      CHECK(contextual_input_data->context_input.has_value() &&
            contextual_input_data->context_input->size() > 0);
      [[fallthrough]];
    case lens::MimeType::kUnknown:
      // Call CreateContentextualDataUploadPayload off the main thread to avoid
      // blocking the main thread on compression.
      create_request_task_runner_->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(
              &CreateContentextualDataUploadPayload,
              // Pass ownership of the contextual input data to the callback.
              std::move(contextual_input_data->context_input.value()),
              contextual_input_data->page_url,
              contextual_input_data->page_title),
          base::BindOnce(
              &CreateFileUploadRequestProtoWithPayloadAndContinue,
              file_info->request_id, CreateClientContext(),

              base::BindOnce(
                  &ComposeboxQueryController::OnUploadRequestBodyReady,
                  weak_ptr_factory_.GetWeakPtr(), file_token,
                  file_info->num_outstanding_network_requests_++)));
      break;
    case lens::MimeType::kImage:
      CHECK(contextual_input_data->context_input.has_value() &&
            contextual_input_data->context_input->size() == 1);
      // TODO(crbug.com/441142455): Support image context via SkBitmap.
      CreateImageUploadRequest(
          file_info->request_id,
          // Pass ownership of the contextual input data to the callback.
          std::move(contextual_input_data->context_input->front().bytes_),
          std::move(image_options),
          base::BindOnce(&ComposeboxQueryController::OnUploadRequestBodyReady,
                         weak_ptr_factory_.GetWeakPtr(), file_token,
                         file_info->num_outstanding_network_requests_++));
      break;
    default:
      UpdateFileUploadStatus(
          file_info->file_token,
          contextual_search::FileUploadStatus::kValidationFailed,
          contextual_search::FileUploadErrorType::kBrowserProcessingError);
      break;
  }
}

void ComposeboxQueryController::AddPageIndexToImageUploadRequestAndContinue(
    std::optional<size_t> pdf_page_index,
    RequestBodyProtoCreatedCallback callback,
    lens::LensOverlayServerRequest request,
    std::optional<contextual_search::FileUploadErrorType> error_type) {
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
    std::optional<contextual_search::FileUploadErrorType> error_type) {
  auto* file_info = GetMutableFileInfo(file_token);
  if (!file_info) {
    return;
  }

  if (error_type.has_value()) {
    UpdateFileUploadStatus(
        file_info->file_token,
        contextual_search::FileUploadStatus::kValidationFailed, error_type);
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

  file_info->file_upload_access_token_fetcher_.reset();
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
          contextual_search::FileUploadStatus::kProcessing ||
      file_info->upload_status ==
          contextual_search::FileUploadStatus::kProcessingSuggestSignalsReady) {
    UpdateFileUploadStatus(file_info->file_token,
                           contextual_search::FileUploadStatus::kUploadStarted,
                           std::nullopt);
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
        contextual_search::FileUploadErrorType::kServerError;
    UpdateFileUploadStatus(
        file_token, contextual_search::FileUploadStatus::kUploadFailed,
        contextual_search::FileUploadErrorType::kServerError);
    return;
  }

  // Store the response body for later processing.
  file_info->response_bodies.push_back(response->response);

  // If the file was still uploading and there are no more outstanding network
  // requests, update the file upload status to successful. The upload status
  // would have been set to ServerError if the response code for any prior
  // request was not successful.
  if (file_info->upload_status ==
          contextual_search::FileUploadStatus::kUploadStarted &&
      file_info->num_outstanding_network_requests_ == 0) {
    UpdateFileUploadStatus(
        file_token, contextual_search::FileUploadStatus::kUploadSuccessful,
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
      ConstructVisualSearchInteractionData(file_info, query_text,
                                           lens_overlay_selection_type);

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
    std::optional<lens::LensOverlaySelectionType> lens_overlay_selection_type) {
  if (!file_info ||
      !IsValidFileUploadStatusForMultimodalRequest(file_info->upload_status)) {
    return std::nullopt;
  }

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

  switch (file_info->mime_type) {
    case lens::MimeType::kPdf:
      interaction_data.set_interaction_type(
          lens::LensOverlayInteractionRequestMetadata::PDF_QUERY);
      break;
    case lens::MimeType::kAnnotatedPageContent:
      interaction_data.set_interaction_type(
          lens::LensOverlayInteractionRequestMetadata::WEBPAGE_QUERY);
      break;
    case lens::MimeType::kUnknown:
      [[fallthrough]];
    case lens::MimeType::kImage:
      interaction_data.set_interaction_type(
          lens::LensOverlayInteractionRequestMetadata::REGION);
      break;
    default:
      NOTREACHED();
  }

  auto media_type = file_info->request_id.media_type();
  bool use_full_region =
      media_type == lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE ||
      media_type == lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE ||
      media_type == lens::LensOverlayRequestId::MEDIA_TYPE_PDF_AND_IMAGE;

  // If there was an interaction request, then set the interaction data from
  // the request.
  if (latest_interaction_request_data_ &&
      latest_interaction_request_data_->request_ &&
      latest_interaction_request_data_->request_->has_interaction_request()) {
    auto sent_interaction_request =
        latest_interaction_request_data_->request_->interaction_request();
    interaction_data.set_interaction_type(
        sent_interaction_request.interaction_request_metadata().type());
    if (sent_interaction_request.has_image_crop()) {
      // The zoomed crop field should only be set if the object id is not set.
      interaction_data.mutable_zoomed_crop()->CopyFrom(
          sent_interaction_request.image_crop().zoomed_crop());
      use_full_region = false;
    }
  }

  // Set the zoomed crop if there is an image associated with the request.
  if (use_full_region) {
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
