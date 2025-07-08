// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/composebox/composebox_query_controller.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/base64url.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_request_construction.h"
#include "components/lens/ref_counted_lens_overlay_client_logs.h"
#include "components/search_engines/util.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
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
#include "third_party/lens_server_proto/lens_overlay_payload.pb.h"
#include "third_party/lens_server_proto/lens_overlay_platform.pb.h"
#include "third_party/lens_server_proto/lens_overlay_service_deps.pb.h"
#include "third_party/lens_server_proto/lens_overlay_surface.pb.h"

#if !BUILDFLAG(IS_IOS)
#include "components/omnibox/composebox/composebox_image_helper.h"
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
constexpr char kOAuthConsumerName[] = "ComposeboxQueryController";
constexpr char kSessionIdQueryParameterKey[] = "gsessionid";
constexpr char kEntrypointParameterValue[] = "42";

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

ComposeboxQueryController::FileInfo::FileInfo() = default;
ComposeboxQueryController::FileInfo::~FileInfo() = default;

namespace {
// Creates a pdf file upload request payload.
lens::Payload CreatePDFFileUploadPayload(
    scoped_refptr<base::RefCountedBytes> file_data) {
  lens::Payload payload;
  auto* content = payload.mutable_content();
  auto* content_data = content->add_content_data();
  content_data->set_content_type(lens::ContentData::CONTENT_TYPE_PDF);

  // TODO(crbug.com/427618282): Add compression for PDF bytes.
  auto bytes = file_data->as_vector();
  content_data->mutable_data()->assign(bytes.begin(), bytes.end());
  return payload;
}

// Creates the server request proto for the pdf file upload request. Called
// on the main thread after the payload is ready.
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
  std::move(callback).Run(request);
}

#if !BUILDFLAG(IS_IOS)
// Creates the server request proto for the image file upload request. Called
// on the main thread after the image data is ready.
void CreateFileUploadRequestProtoWithImageDataAndContinue(
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
  std::move(callback).Run(request);
}
#endif  // !BUILDFLAG(IS_IOS)

}  // namespace

ComposeboxQueryController::ComposeboxQueryController(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    version_info::Channel channel,
    std::string locale,
    TemplateURLService* template_url_service)
    : identity_manager_(identity_manager),
      url_loader_factory_(url_loader_factory),
      channel_(channel),
      locale_(locale),
      template_url_service_(template_url_service) {
  create_request_task_runner_ = base::ThreadPool::CreateTaskRunner(
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

ComposeboxQueryController::~ComposeboxQueryController() {
  // Ensure NTP exits are tracked. i.e. The user starts a composebox session,
  // and closes the NTP without explicitly exiting the session or submitting a
  // query.
  // TODO(420701010): Add unittest coverage, e.g. ensuring abandoned metrics
  // are correctly emitted.
  if (session_state() == SessionState::kSessionStarted) {
    NotifySessionAbandoned();
  }
}

void ComposeboxQueryController::NotifySessionStarted() {
  DCHECK_EQ(session_state_, SessionState::kNone);
  DCHECK_EQ(query_controller_state_, QueryControllerState::kOff);
  session_state_ = SessionState::kSessionStarted;
  session_start_time_ = base::Time::Now();
  FetchClusterInfo();
}

void ComposeboxQueryController::NotifySessionAbandoned() {
  session_state_ = SessionState::kSessionAbandoned;
  SetQueryControllerState(QueryControllerState::kOff);
  cluster_info_access_token_fetcher_.reset();
  cluster_info_endpoint_fetcher_.reset();
}

GURL ComposeboxQueryController::CreateAimUrl(const std::string& query_text) {
  session_state_ = SessionState::kQuerySubmitted;
  if (!active_files_.empty()) {
    // Since multiple file upload isn't supported right now, use the last file
    // uploaded to determine `vit` param.
    // TODO(crbug.com/428967670): Support multiple file upload.
    const std::unique_ptr<FileInfo>& last_file = active_files_.rbegin()->second;
    return GetUrlForMultimodalAim(
        template_url_service_, kEntrypointParameterValue,
        request_id_generator_.GetNextRequestId(
            lens::RequestIdUpdateMode::kSearchUrl),
        last_file->mime_type_, base::UTF8ToUTF16(query_text));
  }
  return GetUrlForAim(template_url_service_, kEntrypointParameterValue,
                      base::UTF8ToUTF16(query_text));
}

void ComposeboxQueryController::AddObserver(FileUploadStatusObserver* obs) {
  observers_.AddObserver(obs);
}

void ComposeboxQueryController::RemoveObserver(FileUploadStatusObserver* obs) {
  observers_.RemoveObserver(obs);
}

void ComposeboxQueryController::StartFileUploadFlow(
    std::unique_ptr<FileInfo> file_info,
    scoped_refptr<base::RefCountedBytes> file_data) {
  CHECK_EQ(file_info->upload_status_, FileUploadStatus::kNotUploaded);
  const base::UnguessableToken& file_token = file_info->file_token_;

  auto [it, inserted] = active_files_.emplace(file_token, std::move(file_info));
  DCHECK(inserted);
  FileInfo& current_file_info = *it->second;

  UpdateFileUploadStatus(file_token, FileUploadStatus::kProcessing,
                         std::nullopt);
  // Increment the request id sequence and image sequence id, regardless of
  // whether this is an image or pdf upload.
  // TODO(crbug.com/426855057): Update the request id generator with more
  // customized logic for the composebox use case.
  current_file_info.request_id_ = request_id_generator_.GetNextRequestId(
      lens::RequestIdUpdateMode::kFullImageRequest);

  // Preparing for the file upload request requires multiple async flows to
  // complete before the request is ready to be send to the server. Start the
  // required flows here, and each flow completes by calling the ready method,
  // i.e., OnUploadFileRequestBodyReady(). The ready method will handle waiting
  // for all the necessary flows to complete before performing the request.
  // Async Flow 1: Fetching the cluster info, which is shared across.
  // This flow only occurs once per session and occurs in
  // NotifySessionStarted().
  // Async Flow 2: Creating the file upload request.
  CreateFileUploadRequestBodyAndContinue(
      file_token, std::move(file_data),
      base::BindOnce(&ComposeboxQueryController::OnUploadFileRequestBodyReady,
                     weak_ptr_factory_.GetWeakPtr(), file_token));

  // Async Flow 3: Retrieve the OAuth headers.
  current_file_info.file_upload_access_token_fetcher_ =
      CreateOAuthHeadersAndContinue(base::BindOnce(
          &ComposeboxQueryController::OnUploadFileRequestHeadersReady,
          weak_ptr_factory_.GetWeakPtr(), file_token));
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
      /*url_loader_factory=*/url_loader_factory_,
      /*url=*/fetch_url,
      /*content_type=*/kContentType,
      /*timeout=*/timeout,
      /*post_data=*/std::move(request_string),
      /*headers=*/request_headers,
      /*cors_exempt_headers=*/cors_exempt_headers,
      /*channel=*/channel_,
      /*request_params=*/
      EndpointFetcher::RequestParams::Builder(http_method,
                                              kTrafficAnnotationTag)
          .SetCredentialsMode(CredentialsMode::kInclude)
          .SetSetSiteForCookies(true)
          .SetUploadProgressCallback(std::move(upload_progress_callback))
          .Build());
}

lens::LensOverlayClientContext ComposeboxQueryController::CreateClientContext()
    const {
  lens::LensOverlayClientContext context;
  context.set_surface(lens::SURFACE_CHROME_NTP);
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
        kOAuthConsumerName, identity_manager_, oauth_scopes,
        std::move(token_callback),
        signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
        signin::ConsentLevel::kSignin);
  }

  // Fall back to fetching the endpoint directly using API key.
  std::move(callback).Run(std::vector<std::string>());
  return nullptr;
}

void ComposeboxQueryController::FetchClusterInfo() {
  SetQueryControllerState(QueryControllerState::kAwaitingClusterInfoResponse);

  // There should not be any in-flight cluster info access token request.
  CHECK(!cluster_info_access_token_fetcher_);
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
  // TODO(crbug.com/425396482): Attach variations header.
  std::vector<std::string> cors_exempt_headers;

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
    return;
  }

  lens::LensOverlayServerClusterInfoResponse server_response;
  const std::string response_string = response->response;
  bool parse_successful = server_response.ParseFromArray(
      response_string.data(), response_string.size());
  if (!parse_successful) {
    SetQueryControllerState(QueryControllerState::kClusterInfoInvalid);
    return;
  }

  // Store the cluster info.
  // TODO(crbug.com/425377511): Add TTL timer for the cluster info.
  cluster_info_ = std::make_optional<lens::LensOverlayClusterInfo>();
  cluster_info_->set_server_session_id(server_response.server_session_id());
  cluster_info_->set_search_session_id(server_response.search_session_id());
  SetQueryControllerState(QueryControllerState::kClusterInfoReceived);

  // Iterate through any existing files and send the upload requests if ready.
  for (const auto& [file_token, file_info] : active_files_) {
    MaybeSendFileUploadNetworkRequest(file_token);
  }
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
    FileUploadStatus status,
    std::optional<FileUploadErrorType> error_type) {
  FileInfo* file_info = GetFileInfo(file_token);
  if (!file_info) {
    return;
  }

  file_info->upload_status_ = status;
  for (auto& observer : observers_) {
    observer.OnFileUploadStatusChanged(file_token, status, error_type);
  }
}

#if !BUILDFLAG(IS_IOS)
void ComposeboxQueryController::ProcessDecodedImageAndContinue(
    lens::LensOverlayRequestId request_id,
    RequestBodyProtoCreatedCallback callback,
    const SkBitmap& bitmap) {
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  // TODO(crbug.com/430053361): Add error handling for when the bitmap is null
  // or empty.
  if (!bitmap.isNull() && !bitmap.empty()) {
    // Downscaling and encoding is done on a background thread to avoid blocking
    // the main thread.
    create_request_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&composebox::DownscaleAndEncodeBitmap, bitmap,
                       ref_counted_logs),
        base::BindOnce(&CreateFileUploadRequestProtoWithImageDataAndContinue,
                       request_id, CreateClientContext(), ref_counted_logs,
                       std::move(callback)));
  }
}
#endif  // !BUILDFLAG(IS_IOS)

void ComposeboxQueryController::CreateFileUploadRequestBodyAndContinue(
    const base::UnguessableToken& file_token,
    scoped_refptr<base::RefCountedBytes> file_data,
    RequestBodyProtoCreatedCallback callback) {
  FileInfo* file_info = GetFileInfo(file_token);
  if (!file_info) {
    return;
  }

  switch (file_info->mime_type_) {
    case lens::MimeType::kPdf:
      // Call CreatePDFFileUploadPayload off the main thread to avoid blocking
      // the main thread on compression.
      create_request_task_runner_->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&CreatePDFFileUploadPayload, std::move(file_data)),
          base::BindOnce(&CreateFileUploadRequestProtoWithPayloadAndContinue,
                         *file_info->request_id_, CreateClientContext(),
                         std::move(callback)));
      break;
    case lens::MimeType::kImage:
#if !BUILDFLAG(IS_IOS)
      data_decoder::DecodeImageIsolated(
          file_data->as_vector(), data_decoder::mojom::ImageCodec::kDefault,
          /*shrink_to_fit=*/false,
          /*max_size_in_bytes=*/std::numeric_limits<int64_t>::max(),
          /*desired_image_frame_size=*/gfx::Size(),
          base::BindOnce(
              &ComposeboxQueryController::ProcessDecodedImageAndContinue,
              weak_ptr_factory_.GetWeakPtr(), *file_info->request_id_,
              std::move(callback)));
#endif  // !BUILDFLAG(IS_IOS)
      break;
    default:
      // TODO(crbug.com/430053361): Add error handling for unsupported file
      // types.
      break;
  }
}

void ComposeboxQueryController::OnUploadFileRequestBodyReady(
    const base::UnguessableToken& file_token,
    lens::LensOverlayServerRequest request) {
  FileInfo* file_info = GetFileInfo(file_token);
  if (!file_info) {
    return;
  }

  file_info->request_body_ =
      std::make_unique<lens::LensOverlayServerRequest>(request);
  MaybeSendFileUploadNetworkRequest(file_token);
}

void ComposeboxQueryController::OnUploadFileRequestHeadersReady(
    const base::UnguessableToken& file_token,
    std::vector<std::string> headers) {
  FileInfo* file_info = GetFileInfo(file_token);
  if (!file_info) {
    return;
  }

  file_info->file_upload_access_token_fetcher_.reset();
  file_info->request_headers_ =
      std::make_unique<std::vector<std::string>>(headers);
  MaybeSendFileUploadNetworkRequest(file_token);
}

void ComposeboxQueryController::MaybeSendFileUploadNetworkRequest(
    const base::UnguessableToken& file_token) {
  FileInfo* file_info = GetFileInfo(file_token);
  if (!file_info) {
    return;
  }

  if (file_info->request_headers_ && file_info->request_body_ &&
      cluster_info_.has_value()) {
    SendFileUploadNetworkRequest(file_info);
  }
}

void ComposeboxQueryController::SendFileUploadNetworkRequest(
    FileInfo* file_info) {
  CHECK(file_info->request_body_);
  CHECK(file_info->request_headers_);
  CHECK_EQ(query_controller_state_, QueryControllerState::kClusterInfoReceived);
  CHECK(cluster_info_.has_value());

  // Get client experiment variations to include in the request.
  // TODO(crbug.com/425396482): Attach variations header.
  std::vector<std::string> cors_exempt_headers;

  // Generate the URL to fetch to and include the server session id if present.
  GURL fetch_url = GURL(lens::features::GetLensOverlayEndpointURL());
  // The endpoint fetches should use the server session id from the cluster
  // info.
  fetch_url =
      net::AppendOrReplaceQueryParameter(fetch_url, kSessionIdQueryParameterKey,
                                         cluster_info_->server_session_id());

  std::string request_string;
  CHECK(file_info->request_body_->SerializeToString(&request_string));

  // Create the EndpointFetcher, responsible for making the request using our
  // given params.
  file_info->file_upload_endpoint_fetcher_ = CreateEndpointFetcher(
      std::move(request_string), fetch_url, HttpMethod::kPost,
      base::Milliseconds(
          lens::features::GetLensOverlayPageContentRequestTimeoutMs()),
      *file_info->request_headers_, cors_exempt_headers,
      /*upload_progress_callback=*/base::DoNothing());
  file_info->upload_network_request_start_time_ = base::Time::Now();
  UpdateFileUploadStatus(file_info->file_token_,
                         FileUploadStatus::kUploadStarted, std::nullopt);

  // Finally, perform the request.
  file_info->file_upload_endpoint_fetcher_->PerformRequest(
      base::BindOnce(&ComposeboxQueryController::HandleFileUploadResponse,
                     weak_ptr_factory_.GetWeakPtr(), file_info->file_token_),
      google_apis::GetAPIKey().c_str());
}

void ComposeboxQueryController::HandleFileUploadResponse(
    const base::UnguessableToken& file_token,
    std::unique_ptr<EndpointResponse> response) {
  FileInfo* file_info = GetFileInfo(file_token);
  if (!file_info) {
    return;
  }

  file_info->server_response_time_ = base::Time::Now();
  file_info->response_code_ = response->http_status_code;
  file_info->file_upload_endpoint_fetcher_.reset();

  if (response->http_status_code != google_apis::ApiErrorCode::HTTP_SUCCESS) {
    file_info->upload_error_type_ = FileUploadErrorType::kServerError;
    UpdateFileUploadStatus(file_token, FileUploadStatus::kUploadFailed,
                           FileUploadErrorType::kServerError);
    return;
  }

  UpdateFileUploadStatus(file_token, FileUploadStatus::kUploadSuccessful,
                         std::nullopt);
}

ComposeboxQueryController::FileInfo* ComposeboxQueryController::GetFileInfo(
    const base::UnguessableToken& file_token) {
  auto it = active_files_.find(file_token);
  if (it == active_files_.end()) {
    return nullptr;
  }
  return it->second.get();
}
