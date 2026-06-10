// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/resumable_uploader_base.h"

#include <memory>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "components/safe_browsing/core/common/utils.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

#if !BUILDFLAG(IS_IOS)
#include "components/safe_browsing/content/browser/web_ui/web_ui_content_info_singleton.h"
#endif

namespace enterprise_connectors {

namespace {

using ::safe_browsing::RecordHttpResponseOrErrorCode;
using ::safe_browsing::SetAccessToken;

// HTTP headers for resumable upload requests
constexpr char kUploadProtocolHeader[] = "X-Goog-Upload-Protocol";
constexpr char kUploadCommandHeader[] = "X-Goog-Upload-Command";
constexpr char kUploadHeaderContentLengthHeader[] =
    "X-Goog-Upload-Header-Content-Length";
constexpr char kUploadHeaderContentTypeHeader[] =
    "X-Goog-Upload-Header-Content-Type";
constexpr char kUploadStatusHeader[] = "X-Goog-Upload-Status";
constexpr char kUploadUrlHeader[] = "X-Goog-Upload-Url";
constexpr char kUploadOffsetHeader[] = "X-Goog-Upload-Offset";
constexpr char kUploadIntermediateHeader[] =
    "X-Goog-Upload-Header-Cep-Response";
constexpr char kUploadHashHeader[] = "X-Goog-Upload-Header-File-Hash";

// Content type of the upload contents.
constexpr char kUploadContentType[] = "application/octet-stream";
// Content type of metadata.
constexpr char kMetadataContentType[] = "application/json";
// Content type of pasted images.
constexpr char kImageContentType[] = "image/png";

// Combined command for upload and finalize.
constexpr char kCommandUploadFinalize[] = "upload, finalize";

bool IsSuccess(int net_error, int response_code) {
  return net_error == net::OK && response_code == net::HTTP_OK;
}

std::unique_ptr<ConnectorDataPipeGetter> CreateFileDataPipeGetterBlocking(
    const base::FilePath& path,
    bool is_obfuscated) {
  // FLAG_WIN_SHARE_DELETE is necessary to allow the file to be renamed by the
  // user clicking "Open Now" without causing download errors.
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                            base::File::FLAG_WIN_SHARE_DELETE);
#if BUILDFLAG(IS_CHROMEOS)
  if (base::FilePath("/media/fuse/fusebox").IsParent(path)) {
    return ConnectorDataPipeGetter::CreateFuseboxResumablePipeGetter(
        std::move(file), is_obfuscated);
  }
#endif

  return ConnectorDataPipeGetter::CreateResumablePipeGetter(std::move(file),
                                                            is_obfuscated);
}

}  // namespace

ResumableUploadRequestBase::ResumableUploadRequestBase(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    ScanRequestUploadResult get_data_result,
    const base::FilePath& path,
    uint64_t file_size,
    bool is_obfuscated,
    const std::string& histogram_suffix,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    VerdictReceivedCallback verdict_received_callback,
    ContentUploadedCallback content_uploaded_callback,
    bool force_sync_upload,
    OnceRegisterOnGotHashCallback register_on_got_hash_callback,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : ConnectorUploadRequest(std::move(url_loader_factory),
                             base_url,
                             metadata,
                             path,
                             file_size,
                             is_obfuscated,
                             histogram_suffix,
                             traffic_annotation,
                             base::DoNothing(),
                             ui_task_runner),
      verdict_received_callback_(std::move(verdict_received_callback)),
      content_uploaded_callback_(std::move(content_uploaded_callback)),
      get_data_result_(get_data_result),
      force_sync_upload_(force_sync_upload),
      register_on_got_hash_callback_(std::move(register_on_got_hash_callback)) {
  AssertCalledOnUIThread();
  hash_computation_is_synchronous_ = register_on_got_hash_callback_.is_null();
}

ResumableUploadRequestBase::ResumableUploadRequestBase(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    ScanRequestUploadResult get_data_result,
    base::ReadOnlySharedMemoryRegion page_region,
    const std::string& histogram_suffix,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    VerdictReceivedCallback verdict_received_callback,
    ContentUploadedCallback content_uploaded_callback,
    bool force_sync_upload,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : ConnectorUploadRequest(std::move(url_loader_factory),
                             base_url,
                             metadata,
                             std::move(page_region),
                             histogram_suffix,
                             traffic_annotation,
                             base::DoNothing(),
                             ui_task_runner),
      verdict_received_callback_(std::move(verdict_received_callback)),
      content_uploaded_callback_(std::move(content_uploaded_callback)),
      get_data_result_(get_data_result),
      force_sync_upload_(force_sync_upload) {
  AssertCalledOnUIThread();
}

ResumableUploadRequestBase::ResumableUploadRequestBase(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    const std::string& data,
    DataSource data_source,
    const std::string& histogram_suffix,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    VerdictReceivedCallback verdict_received_callback,
    ContentUploadedCallback content_uploaded_callback,
    bool force_sync_upload,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : ConnectorUploadRequest(std::move(url_loader_factory),
                             base_url,
                             metadata,
                             data,
                             data_source,
                             histogram_suffix,
                             traffic_annotation,
                             base::DoNothing(),
                             ui_task_runner),
      verdict_received_callback_(std::move(verdict_received_callback)),
      content_uploaded_callback_(std::move(content_uploaded_callback)),
      get_data_result_(ScanRequestUploadResult::kSuccess),
      force_sync_upload_(force_sync_upload) {
  AssertCalledOnUIThread();
}

ResumableUploadRequestBase::~ResumableUploadRequestBase() = default;

void ResumableUploadRequestBase::OnSendContentCompleted(
    base::TimeTicks start_time,
    std::optional<std::string> response_body) {
  AssertCalledOnUIThread();

  // If this has already been called after the metadata check, that means that
  // we have set the value to ASYNC.
  if (!verdict_received_callback_.is_null()) {
    scan_type_ = FULL_CONTENT;
  }

  base::UmaHistogramCustomTimes(
      base::StrCat({"Enterprise.ResumableRequest.ContentCheck.",
                    GetRequestType(), ".Duration"}),
      base::TimeTicks::Now() - start_time, base::Milliseconds(1),
      base::Minutes(6), 50);

  int response_code = 0;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }
  MaybeSendHashAndFinish(data_size_, url_loader_->NetError(), response_code,
                         std::move(response_body));
}

void ResumableUploadRequestBase::SetMetadataRequestHeaders(
    network::ResourceRequest* request) {
  CHECK(request);

  // Page, string and file requests should have non-zero `data_size_`.
  DCHECK_GT(data_size_, (uint64_t)0);

  request->headers.SetHeader(kUploadProtocolHeader, "resumable");
  request->headers.SetHeader(kUploadCommandHeader, "start");
  if (hash_computation_is_synchronous_) {
    // When the request already has hash, let the server know the content size,
    // since there will not need to be an empty final file upload.
    // TODO(b/496284950): Remove this header entirely once webprotect accepts
    // the size in the ContentMetadata proto.
    request->headers.SetHeader(kUploadHeaderContentLengthHeader,
                               base::NumberToString(data_size_));
  }
  request->headers.SetHeader(
      kUploadHeaderContentTypeHeader,
      data_source_ == IMAGE ? kImageContentType : kUploadContentType);
  if (!access_token_.empty()) {
    SetAccessToken(request, access_token_);
  }
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
}

std::string ResumableUploadRequestBase::GetUploadInfo() {
  std::string scan_info;
  switch (scan_type_) {
    case PENDING:
      scan_info = "Pending";
      break;
    case FULL_CONTENT:
      scan_info = "Full content scan";
      break;
    case METADATA_ONLY:
      scan_info = "Metadata only scan";
      break;
    case ASYNC:
      scan_info = "Async content upload";
      break;
  }

  return base::StrCat(
      {"Resumable - ", scan_info,
       hash_computation_is_synchronous_ ? "" : ", hash in final call"});
}

void ResumableUploadRequestBase::Start() {
  AssertCalledOnUIThread();
  SendMetadataRequest();
}

std::string ResumableUploadRequestBase::GetRequestType() {
  switch (data_source_) {
    case FILE:
      return "File";
    case STRING:
      return "Text";
    case PAGE:
      return "Print";
    case IMAGE:
      return "Image";
  }
}

void ResumableUploadRequestBase::SendMetadataRequest() {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = base_url_;
  resource_request->method = "POST";
  SetMetadataRequestHeaders(resource_request.get());
#if !BUILDFLAG(IS_IOS)
  safe_browsing::WebUIContentInfoSingleton::GetInstance()
      ->AddHeadersToDeepScanRequests(request_token_, resource_request->headers);
#endif

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation_);
  url_loader_->SetAllowHttpErrorResults(true);
  url_loader_->AttachStringForUpload(base::StrCat({metadata_, "\r\n"}),
                                     kMetadataContentType);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&ResumableUploadRequestBase::OnMetadataUploadCompleted,
                     weak_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

void ResumableUploadRequestBase::MaybeSendHashAndFinish(
    size_t upload_offset,
    int net_error,
    int response_code,
    std::optional<std::string> response_body) {
  if (hash_computation_is_synchronous_) {
    Finish(net_error, response_code, std::move(response_body));
  } else {
    CHECK(!upload_url_.empty());
    MaybeRunVerdictReceivedCallback(net_error, response_code,
                                    std::move(response_body));

    auto request = std::make_unique<network::ResourceRequest>();
    request->method = "POST";
    request->url = GURL(upload_url_);
    // Sending hash is always the final request.
    request->headers.SetHeader(kUploadCommandHeader, kCommandUploadFinalize);
    request->headers.SetHeader(kUploadOffsetHeader,
                               base::NumberToString(upload_offset));
    std::move(register_on_got_hash_callback_)
        .Run(base::BindOnce(&ResumableUploadRequestBase::SendHashNow,
                            weak_factory_.GetWeakPtr(), std::move(request)));
  }
}

void ResumableUploadRequestBase::Finish(
    int net_error,
    int response_code,
    std::optional<std::string> response_body) {
  AssertCalledOnUIThread();
  if (!histogram_suffix_.empty()) {
    std::string histogram = base::StrCat(
        {"SafeBrowsing.ResumableUploader.NetworkResult.", histogram_suffix_});
    RecordHttpResponseOrErrorCode(histogram.c_str(), net_error, response_code);
  }

  // The callback may have been invoked when the metadata verdict was received
  // with the CEP header, to unblock the user initiate an async upload.
  MaybeRunVerdictReceivedCallback(net_error, response_code,
                                  response_body.value_or(""));

  // If no other codepath has, ensure content_uploaded_callback_ isn't called
  // until the hash is ready, since the object may be destroyed when the
  // uploaded callback is run.
  if (register_on_got_hash_callback_) {
    std::move(register_on_got_hash_callback_)
        .Run(base::IgnoreArgs<std::string>(
            std::move(content_uploaded_callback_)));
  } else {
    std::move(content_uploaded_callback_).Run();
  }
}

void ResumableUploadRequestBase::SendHashNow(
    std::unique_ptr<network::ResourceRequest> request,
    std::string hash) {
  if (hash.empty()) {
    // Hash computation failed, so finish by indicating no upload occurred.
    Finish(url_loader_->NetError(), 0, std::nullopt);
    return;
  }
  DCHECK(data_source_ == FILE && !hash.empty() &&
         std::ranges::all_of(hash, base::IsHexDigit<char>));
  request->headers.SetHeader(kUploadHashHeader, hash);
  url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation_);
  url_loader_->SetAllowHttpErrorResults(true);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&ResumableUploadRequestBase::OnSendHashCompleted,
                     weak_factory_.GetWeakPtr()));
}

void ResumableUploadRequestBase::OnSendHashCompleted(
    std::optional<std::string> response_body) {
  AssertCalledOnUIThread();
  int response_code = 0;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }
  Finish(url_loader_->NetError(), response_code, std::move(response_body));
}

void ResumableUploadRequestBase::SendContentSoon() {
  CHECK(!upload_url_.empty());
  auto request = std::make_unique<network::ResourceRequest>();
  request->method = "POST";
  request->url = GURL(upload_url_);
  // If hash computation is synchronous, this is the final request.
  request->headers.SetHeader(
      kUploadCommandHeader,
      hash_computation_is_synchronous_ ? kCommandUploadFinalize : "upload");
  request->headers.SetHeader(kUploadOffsetHeader, "0");

  // TODO(crbug.com/322005992): Add retry logics.
  switch (data_source_) {
    case FILE:
      file_access::RequestFilesAccessForSystem(
          {path_},
          base::BindOnce(&ResumableUploadRequestBase::CreateDatapipe,
                         weak_factory_.GetWeakPtr(), std::move(request)));
      break;
    case PAGE:
      OnDataPipeCreated(std::move(request),
                        ConnectorDataPipeGetter::CreateResumablePipeGetter(
                            std::move(page_region_)));
      break;
    // Resumable uploads are used for pasted images, which are handled as string
    // data. Using resumable uploads for pasted images is enabled by the
    // `enterprise_connectors::kDlpScanPastedImages` feature flag. Text pastes
    // use multipart uploads.
    case IMAGE:
    case STRING:
      SendContentNow(std::move(request));
      break;
    default:
      NOTREACHED();
  }
}

// TODO(crbug.com/328415950): Move the data pipe creation logics to
// connector_upload_request.
void ResumableUploadRequestBase::CreateDatapipe(
    std::unique_ptr<network::ResourceRequest> request,
    file_access::ScopedFileAccess file_access) {
  scoped_file_access_ =
      std::make_unique<file_access::ScopedFileAccess>(std::move(file_access));
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&CreateFileDataPipeGetterBlocking, path_, is_obfuscated_),
      base::BindOnce(&ResumableUploadRequestBase::OnDataPipeCreated,
                     weak_factory_.GetWeakPtr(), std::move(request)));
}

void ResumableUploadRequestBase::OnDataPipeCreated(
    std::unique_ptr<network::ResourceRequest> request,
    std::unique_ptr<ConnectorDataPipeGetter> data_pipe_getter) {
  scoped_file_access_.reset();
  if (!data_pipe_getter) {
    // TODO(329293309): Replace with meaningful net_error value since 0 does not
    // indicate an error.
    Finish(0, 0, std::nullopt);
    return;
  }

  data_pipe_getter_ = std::move(data_pipe_getter);
  SendContentNow(std::move(request));
}

void ResumableUploadRequestBase::SendContentNow(
    std::unique_ptr<network::ResourceRequest> request) {
  // `data_pipe_getter_` is null for STRING requests, which are handled by
  // attaching the string data directly to the URL loader. For FILE and PAGE
  // requests, `data_pipe_getter_` will be non-null.
  if (data_pipe_getter_) {
    mojo::PendingRemote<network::mojom::DataPipeGetter> data_pipe_getter;
    data_pipe_getter_->Clone(data_pipe_getter.InitWithNewPipeAndPassReceiver());
    request->request_body = new network::ResourceRequestBody();
    request->request_body->AppendDataPipe(std::move(data_pipe_getter));
  }

  url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation_);
  url_loader_->SetAllowHttpErrorResults(true);

  // No data pipe and no hash callback indicates data_ is an image.
  if (!data_pipe_getter_ && hash_computation_is_synchronous_) {
    url_loader_->AttachStringForUpload(data_, kImageContentType);
  }

  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&ResumableUploadRequestBase::OnSendContentCompleted,
                     weak_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

void ResumableUploadRequestBase::OnMetadataUploadCompleted(
    base::TimeTicks start_time,
    std::optional<std::string> response_body) {
  AssertCalledOnUIThread();
  scan_type_ = METADATA_ONLY;
  base::UmaHistogramCustomTimes(
      base::StrCat({"Enterprise.ResumableRequest.MetadataCheck.",
                    GetRequestType(), ".Duration"}),
      base::TimeTicks::Now() - start_time, base::Milliseconds(1),
      base::Minutes(6), 50);
  int response_code = 0;
  if (!url_loader_->ResponseInfo() || !url_loader_->ResponseInfo()->headers) {
    // TODO(b/322005992): Add retry logics.
    Finish(url_loader_->NetError(), response_code, std::move(response_body));
    return;
  }

  auto headers = url_loader_->ResponseInfo()->headers;
  // If there is an error or if no content upload is required,
  // CanUploadContent() returns false. Otherwise, it sets upload_url_.
  response_code = headers->response_code();
  if (!CanUploadContent(headers)) {
    Finish(url_loader_->NetError(), response_code, std::move(response_body));
    return;
  }

  if (!force_sync_upload_ && headers->HasHeader(kUploadIntermediateHeader)) {
    response_body = headers->GetNormalizedHeader(kUploadIntermediateHeader);

    std::string output;
    bool is_decoded = base::Base64Decode(response_body.value(), &output);

    if (output.empty() || !is_decoded) {
      MaybeSendHashAndFinish(/*upload_offset=*/0, net::ERR_FAILED,
                             net::HTTP_BAD_REQUEST, std::nullopt);
      return;
    }

    scan_type_ = ASYNC;
    MaybeRunVerdictReceivedCallback(url_loader_->NetError(), response_code,
                                    std::move(output));
  }

  // If chrome is being told to upload the content but the content is too large
  // or is encrypted and encrypted file upload is not enabled, stop now and
  // maybe upload the hash. If the verdict was delivered from an intermediate
  // header, it had already been callbacked above and this FAILED result will
  // not be used.
  if (get_data_result_ == ScanRequestUploadResult::kFileTooLarge ||
      (get_data_result_ == ScanRequestUploadResult::kFileEncrypted &&
       !ShouldUploadEncryptedFile())) {
    MaybeSendHashAndFinish(/*upload_offset=*/0, net::ERR_FAILED,
                           net::HTTP_BAD_REQUEST, std::move(response_body));
    return;
  }

  SendContentSoon();
}

void ResumableUploadRequestBase::MaybeRunVerdictReceivedCallback(
    int net_error,
    int response_code,
    std::optional<std::string> response_body) {
  if (!verdict_received_callback_.is_null()) {
    std::move(verdict_received_callback_)
        .Run(/*success=*/IsSuccess(net_error, response_code), response_code,
             response_body.value_or(""));
  }
}

bool ResumableUploadRequestBase::CanUploadContent(
    const scoped_refptr<net::HttpResponseHeaders>& headers) {
  if (headers->response_code() != net::HTTP_OK) {
    return false;
  }
  std::optional<std::string> upload_status =
      headers->GetNormalizedHeader(kUploadStatusHeader);
  if (!upload_status || !headers->HasHeader(kUploadUrlHeader)) {
    return false;
  }

  bool is_active = base::EqualsCaseInsensitiveASCII(
      upload_status.value_or(std::string()), "active");
  if (is_active && upload_url_.empty()) {
    upload_url_ = headers->GetNormalizedHeader(kUploadUrlHeader).value();
  }
  return is_active;
}

bool ResumableUploadRequestBase::ShouldUploadEncryptedFile() {
  return base::FeatureList::IsEnabled(kEnableEncryptedFileUpload) &&
         scan_type_ == ASYNC;
}

}  // namespace enterprise_connectors
