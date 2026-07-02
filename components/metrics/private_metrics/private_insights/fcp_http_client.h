// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_FCP_HTTP_CLIENT_H_
#define COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_FCP_HTTP_CLIENT_H_

#include <atomic>
#include <map>
#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "components/metrics/private_metrics/private_insights/fcp_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/federated_compute/src/fcp/client/http/http_client.h"
#include "url/gurl.h"

namespace base {
class SequencedTaskRunner;
}

namespace private_insights {

inline constexpr char kFcpHttpClientBatchSizeHistogram[] =
    "PrivateMetrics.PrivateInsights.FcpHttpClient.BatchSize";
inline constexpr char kFcpHttpClientBytesReceivedHistogram[] =
    "PrivateMetrics.PrivateInsights.FcpHttpClient.BytesReceived";
inline constexpr char kFcpHttpClientBytesSentHistogram[] =
    "PrivateMetrics.PrivateInsights.FcpHttpClient.BytesSent";
inline constexpr char kFcpHttpClientHttpResponseCodeHistogram[] =
    "PrivateMetrics.PrivateInsights.FcpHttpClient.HttpResponseCode";
inline constexpr char kFcpHttpClientNetErrorHistogram[] =
    "PrivateMetrics.PrivateInsights.FcpHttpClient.NetError";
inline constexpr char kFcpHttpClientRequestDurationHistogram[] =
    "PrivateMetrics.PrivateInsights.FcpHttpClient.RequestDuration";

class FcpHttpRequestHandle;
class FcpHttpRequestManager;

// Manages the execution of an individual FCP HTTP request using
// SimpleURLLoader.
class FcpHttpRequestRunner : public network::SimpleURLLoaderStreamConsumer {
 public:
  FcpHttpRequestRunner(network::mojom::URLLoaderFactory* url_loader_factory,
                       FcpHttpRequestHandle* handle,
                       std::string upload_body,
                       fcp::client::http::HttpRequestCallback* callback);
  ~FcpHttpRequestRunner() override;

  FcpHttpRequestRunner(const FcpHttpRequestRunner&) = delete;
  FcpHttpRequestRunner& operator=(const FcpHttpRequestRunner&) = delete;

  void set_on_complete_callback(base::OnceClosure callback);

  // SimpleURLLoaderStreamConsumer:
  void OnDataReceived(std::string_view string_piece,
                      base::OnceClosure resume) override;
  void OnComplete(bool success) override;
  void OnRetry(base::OnceClosure start_retry) override;

  void Cancel();

 private:
  void AbortInternal();
  void ClearReferences();
  void OnResponseStarted(const GURL& final_url,
                         const network::mojom::URLResponseHead& response_head);
  void OnUploadProgress(uint64_t position, uint64_t total);
  void OnDownloadProgress(uint64_t current);

  raw_ptr<FcpHttpRequestHandle> handle_ GUARDED_BY_CONTEXT(sequence_checker_);
  raw_ptr<fcp::client::http::HttpRequest> request_
      GUARDED_BY_CONTEXT(sequence_checker_);
  raw_ptr<fcp::client::http::HttpRequestCallback> callback_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::OnceClosure on_complete_callback_ GUARDED_BY_CONTEXT(sequence_checker_);
  raw_ptr<std::atomic<int64_t>> sent_bytes_ptr_
      GUARDED_BY_CONTEXT(sequence_checker_);
  raw_ptr<std::atomic<int64_t>> received_bytes_ptr_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // FCP has logic dependent on whether there was an Accept-Encoding header
  // present or not (see "Response body decompression & decoding" on
  // fcp::client::http::HttpClient).
  bool has_explicit_accept_encoding_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;

  std::unique_ptr<network::SimpleURLLoader> loader_
      GUARDED_BY_CONTEXT(sequence_checker_);
  raw_ptr<fcp::client::http::HttpResponse> response_
      GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;
  base::TimeTicks start_time_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

// Manages and routes asynchronous FCP HTTP requests between background worker
// threads and Chrome's UI thread network stack.
//
// Wraps scoped_refptr<network::SharedURLLoaderFactory> to ensure that network
// requests and factory destruction occur on the UI sequence, while managing
// active request runners (FcpHttpRequestRunner) and request cancelation.
//
// WARNING: The caller must ensure that an instance of this class outlives all
// network requests initiated through it, including any posted UI tasks.
class FcpHttpRequestManager {
 public:
  FcpHttpRequestManager(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  FcpHttpRequestManager(const FcpHttpRequestManager&) = delete;
  FcpHttpRequestManager& operator=(const FcpHttpRequestManager&) = delete;

  ~FcpHttpRequestManager();

  // Starts the request on the UI thread. Can be called from any thread.
  // `latch` must remain valid until all requests complete and `latch->Wait()`
  // unblocks.
  void StartRequest(FcpHttpRequestHandle* handle,
                    std::string upload_body,
                    fcp::client::http::HttpRequestCallback* callback,
                    CountdownLatch* latch);

  uint64_t GetNextRequestId() { return next_request_id_.fetch_add(1); }

  // Cancels the request on the UI thread. Can be called from any thread.
  void CancelRequest(uint64_t request_id);

 private:
  void StartRequestOnUI(FcpHttpRequestHandle* handle,
                        std::string upload_body,
                        fcp::client::http::HttpRequestCallback* callback,
                        CountdownLatch* latch);
  void OnRequestComplete(uint64_t request_id,
                         FcpHttpRequestRunner* runner,
                         CountdownLatch* latch);

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_
      GUARDED_BY_CONTEXT(ui_sequence_checker_);
  std::atomic<uint64_t> next_request_id_{1};
  std::map<uint64_t, std::unique_ptr<FcpHttpRequestRunner>> runners_
      GUARDED_BY_CONTEXT(ui_sequence_checker_);

  SEQUENCE_CHECKER(ui_sequence_checker_);
};

// Represents an enqueued FCP HTTP request handle.
//
// This handle holds a reference to the manager rather than FcpHttpClient, so it
// does not depend on FcpHttpClient's lifetime.
class FcpHttpRequestHandle : public fcp::client::http::HttpRequestHandle {
 public:
  // `manager` must outlive `this`.
  FcpHttpRequestHandle(FcpHttpRequestManager* manager,
                       uint64_t request_id,
                       std::unique_ptr<fcp::client::http::HttpRequest> request);
  ~FcpHttpRequestHandle() override;

  FcpHttpRequestHandle(const FcpHttpRequestHandle&) = delete;
  FcpHttpRequestHandle& operator=(const FcpHttpRequestHandle&) = delete;

  fcp::client::http::HttpRequest* request() const { return request_.get(); }

  uint64_t request_id() const { return request_id_; }

  bool was_performed() const { return was_performed_.load(); }
  void set_was_performed(bool was_performed) {
    was_performed_.store(was_performed);
  }

  bool was_canceled() const { return was_canceled_.load(); }

  std::atomic<int64_t>* sent_bytes() { return &sent_bytes_; }
  std::atomic<int64_t>* received_bytes() { return &received_bytes_; }

  void set_response(std::unique_ptr<fcp::client::http::HttpResponse> response) {
    response_ = std::move(response);
  }

  // HttpRequestHandle overrides:
  SentReceivedBytes TotalSentReceivedBytes() const override;
  void Cancel() override;

 private:
  raw_ptr<FcpHttpRequestManager> manager_;
  uint64_t request_id_;
  std::unique_ptr<fcp::client::http::HttpRequest> request_;
  std::unique_ptr<fcp::client::http::HttpResponse> response_;
  std::atomic<bool> was_performed_{false};
  std::atomic<bool> was_canceled_{false};
  std::atomic<int64_t> sent_bytes_{0};
  std::atomic<int64_t> received_bytes_{0};
};

class FcpHttpClient : public fcp::client::http::HttpClient {
 public:
  // The caller must ensure that `request_manager` outlives `this`.
  explicit FcpHttpClient(FcpHttpRequestManager* request_manager);
  ~FcpHttpClient() override;

  FcpHttpClient(const FcpHttpClient&) = delete;
  FcpHttpClient& operator=(const FcpHttpClient&) = delete;

  // HttpClient overrides:
  std::unique_ptr<fcp::client::http::HttpRequestHandle> EnqueueRequest(
      std::unique_ptr<fcp::client::http::HttpRequest> request) override;

  absl::Status PerformRequests(
      std::vector<std::pair<fcp::client::http::HttpRequestHandle*,
                            fcp::client::http::HttpRequestCallback*>> requests)
      override;

 private:
  raw_ptr<FcpHttpRequestManager> request_manager_;
};

}  // namespace private_insights

#endif  // COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_FCP_HTTP_CLIENT_H_
