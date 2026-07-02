// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/private_insights/fcp_http_client.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "components/metrics/private_metrics/private_insights/fcp_utils.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/filter/source_stream_type.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/federated_compute/src/fcp/client/http/http_client.h"
#include "url/gurl.h"

namespace private_insights {

namespace {

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("private_insights_fcp_http", R"(
      semantics {
        sender: "Private Insights Service"
        description:
          "Uploads encrypted metrics to a confidential compute service to be "
          "processed by a TEE-hosted workloads pipeline, which uses "
          "differential privacy to ensure that only anonymized, aggregate "
          "results are released."
        trigger:
          "Reports are automatically generated at intervals while Chrome is "
          "running."
        data:
          "Encrypted metrics about the usage of the contextual cueing feature, "
          "including the information about the user context, and the cue "
          "suggestion. The encryption is bound to the TEE-hosted binary which "
          "applies differential privacy, ensuring that Google has access to "
          "anonymized aggregates only."
        destination: GOOGLE_OWNED_SERVICE
        internal {
          contacts {
            owners: "//components/metrics/private_metrics/OWNERS"
          }
        }
        user_data {
          type: WEB_CONTENT
          type: USER_CONTENT
          type: SENSITIVE_URL
          type: USAGE_AND_PERFORMANCE_METRICS
        }
        last_reviewed: "2026-06-26"
      }
      policy {
        cookies_allowed: NO
        setting:
          "Users can enable or disable this feature via "
          "\"Help improve Chrome's features and performance\" in Chrome "
          "settings under Sync and Google services > Other Google services. "
          "The feature is enabled by default."
        chrome_policy {
          MetricsReportingEnabled {
            policy_options { mode: MANDATORY }
            MetricsReportingEnabled: false
          }
        }
      })");

class FcpHttpResponse : public fcp::client::http::HttpResponse {
 public:
  FcpHttpResponse(int code, fcp::client::http::HeaderList headers)
      : code_(code), headers_(std::move(headers)) {}
  ~FcpHttpResponse() override = default;

  int code() const override { return code_; }

  const fcp::client::http::HeaderList& headers() const override {
    return headers_;
  }

 private:
  int code_;
  fcp::client::http::HeaderList headers_;
};

}  // namespace

FcpHttpRequestRunner::FcpHttpRequestRunner(
    network::mojom::URLLoaderFactory* url_loader_factory,
    FcpHttpRequestHandle* handle,
    std::string upload_body,
    fcp::client::http::HttpRequestCallback* callback)
    : handle_(handle),
      request_(handle->request()),
      callback_(callback),
      sent_bytes_ptr_(handle->sent_bytes()),
      received_bytes_ptr_(handle->received_bytes()),
      start_time_(base::TimeTicks::Now()) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(request_->uri());
  resource_request->method = GetMethodString(request_->method());

  ProcessedRequestHeaders processed_headers =
      ProcessFcpRequestHeaders(request_->extra_headers());
  resource_request->headers = std::move(processed_headers.headers);
  has_explicit_accept_encoding_ =
      processed_headers.has_explicit_accept_encoding;
  std::string upload_content_type =
      std::move(processed_headers.upload_content_type);

  resource_request->mode = network::mojom::RequestMode::kNoCors;

  // FCP requires the HTTP client to follow redirects (see "Redirects" on
  // fcp::client::http::HttpClient).
  resource_request->redirect_mode = network::mojom::RedirectMode::kFollow;

  // FCP requires no credentials (see "Cookies" on
  // fcp::client::http::HttpClient).
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  // FCP requires no cache (see "Caching" on fcp::client::http::HttpClient).
  resource_request->load_flags =
      net::LOAD_DISABLE_CACHE | net::LOAD_BYPASS_CACHE;

  if (has_explicit_accept_encoding_) {
    // Prevent Chrome from automatically decompressing the response body (see
    // "Response body decompression & decoding" on
    // fcp::client::http::HttpClient).
    resource_request->devtools_accepted_stream_types =
        std::vector<net::SourceStreamType>{};
  }

  loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                             kTrafficAnnotation);

  // For getting/logging error response bodies.
  loader_->SetAllowHttpErrorResults(true);

  if (!upload_body.empty()) {
    loader_->AttachStringForUpload(std::move(upload_body), upload_content_type);
  }

  loader_->SetOnResponseStartedCallback(base::BindOnce(
      &FcpHttpRequestRunner::OnResponseStarted, base::Unretained(this)));
  loader_->SetOnUploadProgressCallback(base::BindRepeating(
      &FcpHttpRequestRunner::OnUploadProgress, base::Unretained(this)));
  loader_->SetOnDownloadProgressCallback(base::BindRepeating(
      &FcpHttpRequestRunner::OnDownloadProgress, base::Unretained(this)));

  loader_->DownloadAsStream(url_loader_factory, this);
}

FcpHttpRequestRunner::~FcpHttpRequestRunner() {
  ClearReferences();
}

void FcpHttpRequestRunner::set_on_complete_callback(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_complete_callback_ = std::move(callback);
}

void FcpHttpRequestRunner::OnDataReceived(std::string_view string_piece,
                                          base::OnceClosure resume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (response_) {
    if (response_->code() >= 400) {
      VLOG(1) << "FCP HTTP error response (code " << response_->code()
              << "): " << string_piece;
    }
    absl::Status status =
        callback_->OnResponseBody(*request_, *response_, string_piece);
    if (!status.ok()) {
      VLOG(1) << "FCP callback OnResponseBody failed: " << status;
      AbortInternal();
      return;
    }
  }
  std::move(resume).Run();
}

void FcpHttpRequestRunner::OnComplete(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!loader_) {
    return;
  }
  int net_error = loader_->NetError();
  loader_.reset();

  base::UmaHistogramSparse(kFcpHttpClientNetErrorHistogram, -net_error);
  base::UmaHistogramMediumTimes(kFcpHttpClientRequestDurationHistogram,
                                base::TimeTicks::Now() - start_time_);
  if (sent_bytes_ptr_) {
    base::UmaHistogramCounts10M(kFcpHttpClientBytesSentHistogram,
                                sent_bytes_ptr_->load());
  }
  if (received_bytes_ptr_) {
    base::UmaHistogramCounts10M(kFcpHttpClientBytesReceivedHistogram,
                                received_bytes_ptr_->load());
  }

  if (success) {
    if (response_) {
      callback_->OnResponseCompleted(*request_, *response_);
    } else {
      callback_->OnResponseError(
          *request_, absl::InternalError("No response headers received"));
    }
  } else {
    absl::Status status = ConvertNetErrorToFcpStatus(net_error);
    if (response_) {
      callback_->OnResponseBodyError(*request_, *response_, status);
    } else {
      callback_->OnResponseError(*request_, status);
    }
  }
  ClearReferences();
  if (on_complete_callback_) {
    std::move(on_complete_callback_).Run();
  }
}

void FcpHttpRequestRunner::OnRetry(base::OnceClosure start_retry) {
  // Retries are not enabled.
  NOTREACHED();
}

void FcpHttpRequestRunner::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (loader_) {
    loader_.reset();
    auto err = absl::CancelledError("Request canceled");
    if (response_) {
      callback_->OnResponseBodyError(*request_, *response_, err);
    } else {
      callback_->OnResponseError(*request_, err);
    }
    ClearReferences();
    if (on_complete_callback_) {
      std::move(on_complete_callback_).Run();
    }
  }
}

void FcpHttpRequestRunner::AbortInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (loader_) {
    loader_.reset();
    ClearReferences();
    if (on_complete_callback_) {
      std::move(on_complete_callback_).Run();
    }
  }
}

void FcpHttpRequestRunner::ClearReferences() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  handle_ = nullptr;
  request_ = nullptr;
  callback_ = nullptr;
  response_ = nullptr;
  sent_bytes_ptr_ = nullptr;
  received_bytes_ptr_ = nullptr;
}

void FcpHttpRequestRunner::OnResponseStarted(
    const GURL& final_url,
    const network::mojom::URLResponseHead& response_head) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int code = 0;
  fcp::client::http::HeaderList headers;
  if (response_head.headers) {
    code = response_head.headers->response_code();
    headers = ConvertResponseHeadersToFcp(response_head.headers.get(),
                                          has_explicit_accept_encoding_);
  }
  base::UmaHistogramSparse(kFcpHttpClientHttpResponseCodeHistogram, code);
  auto response = std::make_unique<FcpHttpResponse>(code, std::move(headers));
  response_ = response.get();
  if (handle_) {
    handle_->set_response(std::move(response));
  }
  absl::Status status = callback_->OnResponseStarted(*request_, *response_);
  if (!status.ok()) {
    VLOG(1) << "FCP callback OnResponseStarted failed: " << status;
    AbortInternal();
  }
}

void FcpHttpRequestRunner::OnUploadProgress(uint64_t position, uint64_t total) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (sent_bytes_ptr_) {
    sent_bytes_ptr_->store(static_cast<int64_t>(position));
  }
}

void FcpHttpRequestRunner::OnDownloadProgress(uint64_t current) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (received_bytes_ptr_) {
    received_bytes_ptr_->store(static_cast<int64_t>(current));
  }
}

FcpHttpRequestHandle::FcpHttpRequestHandle(
    FcpHttpRequestManager* manager,
    uint64_t request_id,
    std::unique_ptr<fcp::client::http::HttpRequest> request)
    : manager_(manager),
      request_id_(request_id),
      request_(std::move(request)) {}

FcpHttpRequestHandle::~FcpHttpRequestHandle() = default;

fcp::client::http::HttpRequestHandle::SentReceivedBytes
FcpHttpRequestHandle::TotalSentReceivedBytes() const {
  return {sent_bytes_.load(), received_bytes_.load()};
}

void FcpHttpRequestHandle::Cancel() {
  if (was_canceled_.exchange(true)) {
    return;
  }
  if (manager_) {
    manager_->CancelRequest(request_id_);
  }
}

FcpHttpRequestManager::FcpHttpRequestManager(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : ui_task_runner_(std::move(ui_task_runner)),
      url_loader_factory_(std::move(url_loader_factory)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
}

FcpHttpRequestManager::~FcpHttpRequestManager() {
  if (url_loader_factory_ || !runners_.empty()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](scoped_refptr<network::SharedURLLoaderFactory> factory,
               std::map<uint64_t, std::unique_ptr<FcpHttpRequestRunner>>
                   runners) {
              // Body is intentionally empty; factory and active runners will be
              // destructed here on the UI thread sequence.
            },
            std::move(url_loader_factory_), std::move(runners_)));
  }
}

void FcpHttpRequestManager::StartRequest(
    FcpHttpRequestHandle* handle,
    std::string upload_body,
    fcp::client::http::HttpRequestCallback* callback,
    CountdownLatch* latch) {
  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&FcpHttpRequestManager::StartRequestOnUI,
                                // Note: using base::Unretained(this) is safe
                                // because the manager outlives all network
                                // requests initiated through it.
                                base::Unretained(this), handle,
                                std::move(upload_body), callback, latch));
}

void FcpHttpRequestManager::CancelRequest(uint64_t request_id) {
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](FcpHttpRequestManager* manager, uint64_t req_id) {
            DCHECK_CALLED_ON_VALID_SEQUENCE(manager->ui_sequence_checker_);
            auto it = manager->runners_.find(req_id);
            if (it != manager->runners_.end()) {
              it->second->Cancel();
            }
          },
          // Note: using base::Unretained(this) is safe because the manager
          // outlives all network requests initiated through it.
          base::Unretained(this), request_id));
}

void FcpHttpRequestManager::StartRequestOnUI(
    FcpHttpRequestHandle* handle,
    std::string upload_body,
    fcp::client::http::HttpRequestCallback* callback,
    CountdownLatch* latch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
  uint64_t request_id = handle->request_id();
  auto runner = std::make_unique<FcpHttpRequestRunner>(
      url_loader_factory_.get(), handle, std::move(upload_body), callback);
  FcpHttpRequestRunner* runner_ptr = runner.get();
  runner->set_on_complete_callback(base::BindOnce(
      &FcpHttpRequestManager::OnRequestComplete,
      // Note: using base::Unretained(this) is safe because the manager outlives
      // all network requests initiated through it.
      base::Unretained(this), request_id, runner_ptr, latch));
  runners_[request_id] = std::move(runner);
}

void FcpHttpRequestManager::OnRequestComplete(uint64_t request_id,
                                              FcpHttpRequestRunner* runner,
                                              CountdownLatch* latch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
  DCHECK(runner);

  latch->CountDown();

  auto it = runners_.find(request_id);
  if (it != runners_.end()) {
    ui_task_runner_->DeleteSoon(FROM_HERE, std::move(it->second));
    runners_.erase(it);
  }
}

FcpHttpClient::FcpHttpClient(FcpHttpRequestManager* request_manager)
    : request_manager_(request_manager) {}

FcpHttpClient::~FcpHttpClient() = default;

std::unique_ptr<fcp::client::http::HttpRequestHandle>
FcpHttpClient::EnqueueRequest(
    std::unique_ptr<fcp::client::http::HttpRequest> request) {
  return std::make_unique<FcpHttpRequestHandle>(
      request_manager_, request_manager_->GetNextRequestId(),
      std::move(request));
}

absl::Status FcpHttpClient::PerformRequests(
    std::vector<std::pair<fcp::client::http::HttpRequestHandle*,
                          fcp::client::http::HttpRequestCallback*>> requests) {
  base::UmaHistogramCounts100(kFcpHttpClientBatchSizeHistogram,
                              requests.size());

  if (requests.empty()) {
    return absl::OkStatus();
  }

  for (auto& pair : requests) {
    FcpHttpRequestHandle* fcp_handle =
        static_cast<FcpHttpRequestHandle*>(pair.first);
    if (fcp_handle->was_performed() || fcp_handle->was_canceled()) {
      return absl::InvalidArgumentError(
          "HttpRequestHandle was already performed or canceled");
    }
  }

  CountdownLatch latch(requests.size());

  for (auto& pair : requests) {
    FcpHttpRequestHandle* fcp_handle =
        static_cast<FcpHttpRequestHandle*>(pair.first);
    fcp_handle->set_was_performed(true);
    fcp::client::http::HttpRequestCallback* callback = pair.second;
    fcp::client::http::HttpRequest* request = fcp_handle->request();

    // TODO(b/525314527): Implement proper streaming. The current implementation
    // does not conform to "Request body compression & encoding" from FCP's
    // requirements.

    auto upload_body_or = ReadRequestBody(*request);
    if (!upload_body_or.ok()) {
      callback->OnResponseError(*request, upload_body_or.status());
      latch.CountDown();
      continue;
    }

    request_manager_->StartRequest(fcp_handle, std::move(*upload_body_or),
                                   callback, &latch);
  }

  latch.Wait();
  return absl::OkStatus();
}

}  // namespace private_insights
