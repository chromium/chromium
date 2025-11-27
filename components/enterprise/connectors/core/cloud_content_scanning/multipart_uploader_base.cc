// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/multipart_uploader_base.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "components/file_access/scoped_file_access.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/utils.h"
#include "net/base/mime_util.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace enterprise_connectors {

namespace {

using ::safe_browsing::RecordHttpResponseOrErrorCode;
using ::safe_browsing::SafeBrowsingAuthenticatedEndpoint;
using ::safe_browsing::SetAccessToken;

// Constants associated with exponential backoff. On each failure, we will
// increase the backoff by `kBackoffFactor`, starting from
// `kInitialBackoffSeconds`. If we fail after `kMaxRetryAttempts` retries, the
// upload fails.
const int kInitialBackoffSeconds = 1;
const int kBackoffFactor = 2;
const int kMaxRetryAttempts = 2;

// Content type of the metadata and file contents.
const char kDataContentType[] = "Content-Type: application/octet-stream";

// Content type of a full multipart request
const char kUploadContentType[] = "multipart/related; boundary=";

std::unique_ptr<ConnectorDataPipeGetter> CreateFileDataPipeGetterBlocking(
    const std::string& boundary,
    const std::string& metadata,
    const base::FilePath& path,
    bool is_obfuscated) {
  // FLAG_WIN_SHARE_DELETE is necessary to allow the file to be renamed by the
  // user clicking "Open Now" without causing download errors.
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                            base::File::FLAG_WIN_SHARE_DELETE);

  return ConnectorDataPipeGetter::CreateMultipartPipeGetter(
      boundary, metadata, std::move(file), is_obfuscated);
}

}  // namespace

MultipartUploadRequestBase::MultipartUploadRequestBase(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    const std::string& data,
    const std::string& histogram_suffix,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    Callback callback,
    std::unique_ptr<BrowserThreadGuard> thread_guard)
    : ConnectorUploadRequest(std::move(url_loader_factory),
                             base_url,
                             metadata,
                             data,
                             histogram_suffix,
                             traffic_annotation,
                             std::move(callback)),
      thread_guard_(std::move(thread_guard)),
      boundary_(net::GenerateMimeMultipartBoundary()),
      current_backoff_(base::Seconds(kInitialBackoffSeconds)),
      retry_count_(0) {
  thread_guard_->AssertCalledOnUIThread();
}

MultipartUploadRequestBase::MultipartUploadRequestBase(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    const base::FilePath& path,
    uint64_t file_size,
    bool is_obfuscated,
    const std::string& histogram_suffix,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    Callback callback,
    std::unique_ptr<BrowserThreadGuard> thread_guard)
    : ConnectorUploadRequest(std::move(url_loader_factory),
                             base_url,
                             metadata,
                             path,
                             file_size,
                             is_obfuscated,
                             histogram_suffix,
                             traffic_annotation,
                             std::move(callback)),
      thread_guard_(std::move(thread_guard)),
      boundary_(net::GenerateMimeMultipartBoundary()),
      current_backoff_(base::Seconds(kInitialBackoffSeconds)),
      retry_count_(0),
      is_obfuscated_(is_obfuscated) {
  thread_guard_->AssertCalledOnUIThread();
}

MultipartUploadRequestBase::MultipartUploadRequestBase(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    base::ReadOnlySharedMemoryRegion page_region,
    const std::string& histogram_suffix,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    Callback callback,
    std::unique_ptr<BrowserThreadGuard> thread_guard)
    : ConnectorUploadRequest(std::move(url_loader_factory),
                             base_url,
                             metadata,
                             std::move(page_region),
                             histogram_suffix,
                             traffic_annotation,
                             std::move(callback)),
      thread_guard_(std::move(thread_guard)),
      boundary_(net::GenerateMimeMultipartBoundary()),
      current_backoff_(base::Seconds(kInitialBackoffSeconds)),
      retry_count_(0) {
  thread_guard_->AssertCalledOnUIThread();
}

std::string MultipartUploadRequestBase::GetUploadInfo() {
  return scan_complete_ ? "Multipart - Complete" : "Multipart - Pending";
}

std::string MultipartUploadRequestBase::GenerateRequestBody(
    const std::string& metadata,
    const std::string& data) {
  return base::StrCat({"--", boundary_, "\r\n", kDataContentType, "\r\n\r\n",
                       metadata, "\r\n--", boundary_, "\r\n", kDataContentType,
                       "\r\n\r\n", data, "\r\n--", boundary_, "--\r\n"});
}

void MultipartUploadRequestBase::SetRequestHeaders(
    network::ResourceRequest* request) {
  request->headers.SetHeader("X-Goog-Upload-Protocol", "multipart");
  uint64_t data_size = 0;
  switch (data_source_) {
    case STRING:
      data_size = data_.size();
      break;
    case FILE:
    case PAGE:
      data_size = data_size_;
      break;
    default:
      NOTREACHED();
  }
  request->headers.SetHeader("X-Goog-Upload-Header-Content-Length",
                             base::NumberToString(data_size));

  if (!access_token_.empty()) {
    LogAuthenticatedCookieResets(
        *request, SafeBrowsingAuthenticatedEndpoint::kDeepScanning);
    SetAccessToken(request, access_token_);
  }
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
}

void MultipartUploadRequestBase::RetryOrFinish(
    int net_error,
    int response_code,
    std::optional<std::string> response_body) {
  std::string response = response_body.value_or("");
  if (net_error == net::OK && response_code == net::HTTP_OK) {
    std::move(callback_).Run(/*success=*/true, response_code,
                             std::move(response));
  } else {
    if (response_code < 500 || retry_count_ >= kMaxRetryAttempts) {
      std::move(callback_).Run(/*success=*/false, response_code,
                               std::move(response));
    } else {
      auto task_runner = GetTaskRunner();
      CHECK(task_runner);
      task_runner->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&MultipartUploadRequestBase::SendRequest,
                         weak_factory_.GetWeakPtr()),
          current_backoff_);
      current_backoff_ *= kBackoffFactor;
      retry_count_++;
    }
  }
}

void MultipartUploadRequestBase::MarkScanAsCompleteForTesting() {
  scan_complete_ = true;
}

MultipartUploadRequestBase::~MultipartUploadRequestBase() = default;

void MultipartUploadRequestBase::Start() {
  thread_guard_->AssertCalledOnUIThread();

  start_time_ = base::Time::Now();
  SendRequest();
}

void MultipartUploadRequestBase::SendRequest() {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = base_url_;
  resource_request->method = "POST";
  SetRequestHeaders(resource_request.get());

  switch (data_source_) {
    case STRING:
      SendStringRequest(std::move(resource_request));
      break;
    case FILE:
      SendFileRequest(std::move(resource_request));
      break;
    case PAGE:
      SendPageRequest(std::move(resource_request));
      break;
    default:
      NOTREACHED();
  }
}

void MultipartUploadRequestBase::SendStringRequest(
    std::unique_ptr<network::ResourceRequest> request) {
  DCHECK_EQ(data_source_, STRING);

  url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation_);
  url_loader_->SetAllowHttpErrorResults(true);
  std::string request_body = GenerateRequestBody(metadata_, data_);
  url_loader_->AttachStringForUpload(request_body,
                                     kUploadContentType + boundary_);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&MultipartUploadRequestBase::OnURLLoaderComplete,
                     weak_factory_.GetWeakPtr()));
}

void MultipartUploadRequestBase::SendFileRequest(
    std::unique_ptr<network::ResourceRequest> request) {
  DCHECK_EQ(data_source_, FILE);

  if (data_pipe_getter_) {
    // When `data_pipe_getter_` is already initialized, this means a
    // request has already been made and that this call is a retry, so the pipe
    // is reset to read from the beginning of the request body.
    data_pipe_getter_->Reset();
    CompleteSendRequest(std::move(request));
  } else {
    file_access::RequestFilesAccessForSystem(
        {path_},
        base::BindOnce(&MultipartUploadRequestBase::CreateDatapipe,
                       weak_factory_.GetWeakPtr(), std::move(request)));
  }
}

void MultipartUploadRequestBase::SendPageRequest(
    std::unique_ptr<network::ResourceRequest> request) {
  DCHECK_EQ(data_source_, PAGE);

  if (data_pipe_getter_) {
    // When `data_pipe_getter_` is already initialized, this means a
    // request has already been made and that this call is a retry, so the pipe
    // is reset to read from the beginning of the request body.
    data_pipe_getter_->Reset();
    CompleteSendRequest(std::move(request));
  } else {
    DataPipeCreatedCallback(std::move(request),
                            ConnectorDataPipeGetter::CreateMultipartPipeGetter(
                                boundary_, metadata_, std::move(page_region_)));
  }
}

void MultipartUploadRequestBase::DataPipeCreatedCallback(
    std::unique_ptr<network::ResourceRequest> request,
    std::unique_ptr<ConnectorDataPipeGetter> data_pipe_getter) {
  scoped_file_access_.reset();
  if (!data_pipe_getter) {
    std::move(callback_).Run(/*success=*/false, 0, "");
    return;
  }

  data_pipe_getter_ = std::move(data_pipe_getter);
  CompleteSendRequest(std::move(request));
}

void MultipartUploadRequestBase::CompleteSendRequest(
    std::unique_ptr<network::ResourceRequest> request) {
  mojo::PendingRemote<network::mojom::DataPipeGetter> data_pipe_getter;
  data_pipe_getter_->Clone(data_pipe_getter.InitWithNewPipeAndPassReceiver());
  request->request_body = new network::ResourceRequestBody();
  request->request_body->AppendDataPipe(std::move(data_pipe_getter));
  request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                             kUploadContentType + boundary_);

  url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation_);
  url_loader_->SetAllowHttpErrorResults(true);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&MultipartUploadRequestBase::OnURLLoaderComplete,
                     weak_factory_.GetWeakPtr()));
}

void MultipartUploadRequestBase::CreateDatapipe(
    std::unique_ptr<network::ResourceRequest> request,
    file_access::ScopedFileAccess file_access) {
  scoped_file_access_ =
      std::make_unique<file_access::ScopedFileAccess>(std::move(file_access));
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&CreateFileDataPipeGetterBlocking, boundary_, metadata_,
                     path_, is_obfuscated_),
      base::BindOnce(&MultipartUploadRequestBase::DataPipeCreatedCallback,
                     weak_factory_.GetWeakPtr(), std::move(request)));
}

void MultipartUploadRequestBase::OnURLLoaderComplete(
    std::optional<std::string> response_body) {
  thread_guard_->AssertCalledOnUIThread();
  scan_complete_ = true;

  int response_code = 0;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }
  if (!histogram_suffix_.empty()) {
    std::string histogram = base::StrCat(
        {"SafeBrowsing.MultipartUploader.NetworkResult.", histogram_suffix_});
    RecordHttpResponseOrErrorCode(histogram.c_str(), url_loader_->NetError(),
                                  response_code);
  }

  RetryOrFinish(url_loader_->NetError(), response_code,
                std::move(response_body));
}

}  //  namespace enterprise_connectors
