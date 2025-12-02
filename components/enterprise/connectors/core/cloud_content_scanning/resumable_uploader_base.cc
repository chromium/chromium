// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/resumable_uploader_base.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "components/safe_browsing/core/common/utils.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"

namespace enterprise_connectors {

namespace {

using ::safe_browsing::RecordHttpResponseOrErrorCode;
using ::safe_browsing::SafeBrowsingAuthenticatedEndpoint;
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

// Content type of the upload contents.
constexpr char kUploadContentType[] = "application/octet-stream";
// Content type of metadata.
constexpr char kMetadataContentType[] = "application/json";
// Content type of pasted images.
constexpr char kImageContentType[] = "image/png";

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

  return ConnectorDataPipeGetter::CreateResumablePipeGetter(std::move(file),
                                                            is_obfuscated);
}

}  // namespace

ResumableUploadRequestBase::ResumableUploadRequestBase(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    enterprise_connectors::ScanRequestUploadResult get_data_result,
    const base::FilePath& path,
    uint64_t file_size,
    bool is_obfuscated,
    const std::string& histogram_suffix,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    VerdictReceivedCallback verdict_received_callback,
    ContentUploadedCallback content_uploaded_callback,
    bool force_sync_upload)
    : ConnectorUploadRequest(std::move(url_loader_factory),
                             base_url,
                             metadata,
                             path,
                             file_size,
                             is_obfuscated,
                             histogram_suffix,
                             traffic_annotation,
                             base::DoNothing()),
      verdict_received_callback_(std::move(verdict_received_callback)),
      content_uploaded_callback_(std::move(content_uploaded_callback)),
      get_data_result_(get_data_result),
      is_obfuscated_(is_obfuscated),
      force_sync_upload_(force_sync_upload) {}

ResumableUploadRequestBase::ResumableUploadRequestBase(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    enterprise_connectors::ScanRequestUploadResult get_data_result,
    base::ReadOnlySharedMemoryRegion page_region,
    const std::string& histogram_suffix,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    VerdictReceivedCallback verdict_received_callback,
    ContentUploadedCallback content_uploaded_callback,
    bool force_sync_upload)
    : ConnectorUploadRequest(std::move(url_loader_factory),
                             base_url,
                             metadata,
                             std::move(page_region),
                             histogram_suffix,
                             traffic_annotation,
                             base::DoNothing()),
      verdict_received_callback_(std::move(verdict_received_callback)),
      content_uploaded_callback_(std::move(content_uploaded_callback)),
      get_data_result_(get_data_result),
      force_sync_upload_(force_sync_upload) {}

ResumableUploadRequestBase::ResumableUploadRequestBase(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    const std::string& data,
    const std::string& histogram_suffix,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    VerdictReceivedCallback verdict_received_callback,
    ContentUploadedCallback content_uploaded_callback,
    bool force_sync_upload)
    : ConnectorUploadRequest(std::move(url_loader_factory),
                             base_url,
                             metadata,
                             data,
                             histogram_suffix,
                             traffic_annotation,
                             base::DoNothing()),
      verdict_received_callback_(std::move(verdict_received_callback)),
      content_uploaded_callback_(std::move(content_uploaded_callback)),
      get_data_result_(enterprise_connectors::ScanRequestUploadResult::SUCCESS),
      force_sync_upload_(force_sync_upload) {}

ResumableUploadRequestBase::~ResumableUploadRequestBase() = default;

void ResumableUploadRequestBase::SetMetadataRequestHeaders(
    network::ResourceRequest* request) {
  CHECK(request);

  // Page, string and file requests should have non-zero `data_size_`.
  DCHECK_GT(data_size_, (uint64_t)0);

  request->headers.SetHeader(kUploadProtocolHeader, "resumable");
  request->headers.SetHeader(kUploadCommandHeader, "start");
  request->headers.SetHeader(kUploadHeaderContentLengthHeader,
                             base::NumberToString(data_size_));
  // `STRING` is only used for resumable requests for image pasting.
  request->headers.SetHeader(
      kUploadHeaderContentTypeHeader,
      data_source_ == STRING ? kImageContentType : kUploadContentType);
  if (!access_token_.empty()) {
    LogAuthenticatedCookieResets(
        *request, SafeBrowsingAuthenticatedEndpoint::kDeepScanning);
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

  return base::StrCat({"Resumable - ", scan_info});
}

std::string ResumableUploadRequestBase::GetRequestType() {
  switch (data_source_) {
    case FILE:
      return "File";
    case STRING:
      return "Text";
    case PAGE:
      return "Print";
  }
}

void ResumableUploadRequestBase::SendMetadataRequest() {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = base_url_;
  resource_request->method = "POST";
  SetMetadataRequestHeaders(resource_request.get());

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

void ResumableUploadRequestBase::Finish(
    int net_error,
    int response_code,
    std::optional<std::string> response_body) {
  if (!histogram_suffix_.empty()) {
    std::string histogram = base::StrCat(
        {"SafeBrowsing.ResumableUploader.NetworkResult.", histogram_suffix_});
    RecordHttpResponseOrErrorCode(histogram.c_str(), net_error, response_code);
  }

  // The callback may have been invoked when the metadata verdict was received
  // with the CEP header, to unblock the user initiate an async upload.
  if (!verdict_received_callback_.is_null()) {
    std::move(verdict_received_callback_)
        .Run(/*success=*/IsSuccess(net_error, response_code), response_code,
             response_body.value_or(""));
  }
  std::move(content_uploaded_callback_).Run();
}

void ResumableUploadRequestBase::SendContentSoon(
    const std::string& upload_url) {
  auto request = std::make_unique<network::ResourceRequest>();
  request->method = "POST";
  request->url = GURL(upload_url);
  // Only sends content smaller than 50MB, in a single request.
  request->headers.SetHeader(kUploadCommandHeader, "upload, finalize");
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

  if (!data_pipe_getter_) {
    url_loader_->AttachStringForUpload(data_, kImageContentType);
  }

  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&ResumableUploadRequestBase::OnSendContentCompleted,
                     weak_factory_.GetWeakPtr(), base::TimeTicks::Now()));
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
  return base::EqualsCaseInsensitiveASCII(upload_status.value_or(std::string()),
                                          "active");
}

bool ResumableUploadRequestBase::ShouldUploadEncryptedFile() {
  return base::FeatureList::IsEnabled(kEnableEncryptedFileUpload) &&
         scan_type_ == ASYNC;
}

}  // namespace enterprise_connectors
