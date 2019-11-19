// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/native/url_request.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "components/cronet/cronet_upload_data_stream.h"
#include "components/cronet/native/engine.h"
#include "components/cronet/native/generated/cronet.idl_impl_struct.h"
#include "components/cronet/native/include/cronet_c.h"
#include "components/cronet/native/io_buffer_with_cronet_buffer.h"
#include "components/cronet/native/native_metrics_util.h"
#include "components/cronet/native/runnables.h"
#include "components/cronet/native/upload_data_sink.h"
#include "net/base/io_buffer.h"
#include "net/base/load_states.h"

namespace {

using RequestFinishedInfo = base::RefCountedData<Cronet_RequestFinishedInfo>;
using UrlResponseInfo = base::RefCountedData<Cronet_UrlResponseInfo>;
using CronetError = base::RefCountedData<Cronet_Error>;

template <typename T>
T* GetData(scoped_refptr<base::RefCountedData<T>> ptr) {
  return ptr == nullptr ? nullptr : &ptr->data;
}

net::RequestPriority ConvertRequestPriority(
    Cronet_UrlRequestParams_REQUEST_PRIORITY priority) {
  switch (priority) {
    case Cronet_UrlRequestParams_REQUEST_PRIORITY_REQUEST_PRIORITY_IDLE:
      return net::IDLE;
    case Cronet_UrlRequestParams_REQUEST_PRIORITY_REQUEST_PRIORITY_LOWEST:
      return net::LOWEST;
    case Cronet_UrlRequestParams_REQUEST_PRIORITY_REQUEST_PRIORITY_LOW:
      return net::LOW;
    case Cronet_UrlRequestParams_REQUEST_PRIORITY_REQUEST_PRIORITY_MEDIUM:
      return net::MEDIUM;
    case Cronet_UrlRequestParams_REQUEST_PRIORITY_REQUEST_PRIORITY_HIGHEST:
      return net::HIGHEST;
  }
  return net::DEFAULT_PRIORITY;
}

scoped_refptr<UrlResponseInfo> CreateCronet_UrlResponseInfo(
    const std::vector<std::string>& url_chain,
    int http_status_code,
    const std::string& http_status_text,
    const net::HttpResponseHeaders* headers,
    bool was_cached,
    const std::string& negotiated_protocol,
    const std::string& proxy_server,
    int64_t received_byte_count) {
  auto response_info = base::MakeRefCounted<UrlResponseInfo>();
  response_info->data.url = url_chain.back();
  response_info->data.url_chain = url_chain;
  response_info->data.http_status_code = http_status_code;
  response_info->data.http_status_text = http_status_text;
  // |headers| could be nullptr.
  if (headers != nullptr) {
    size_t iter = 0;
    std::string header_name;
    std::string header_value;
    while (headers->EnumerateHeaderLines(&iter, &header_name, &header_value)) {
      Cronet_HttpHeader header;
      header.name = header_name;
      header.value = header_value;
      response_info->data.all_headers_list.push_back(std::move(header));
    }
  }
  response_info->data.was_cached = was_cached;
  response_info->data.negotiated_protocol = negotiated_protocol;
  response_info->data.proxy_server = proxy_server;
  response_info->data.received_byte_count = received_byte_count;
  return response_info;
}

Cronet_Error_ERROR_CODE NetErrorToCronetErrorCode(int net_error) {
  switch (net_error) {
    case net::ERR_NAME_NOT_RESOLVED:
      return Cronet_Error_ERROR_CODE_ERROR_HOSTNAME_NOT_RESOLVED;
    case net::ERR_INTERNET_DISCONNECTED:
      return Cronet_Error_ERROR_CODE_ERROR_INTERNET_DISCONNECTED;
    case net::ERR_NETWORK_CHANGED:
      return Cronet_Error_ERROR_CODE_ERROR_NETWORK_CHANGED;
    case net::ERR_TIMED_OUT:
      return Cronet_Error_ERROR_CODE_ERROR_TIMED_OUT;
    case net::ERR_CONNECTION_CLOSED:
      return Cronet_Error_ERROR_CODE_ERROR_CONNECTION_CLOSED;
    case net::ERR_CONNECTION_TIMED_OUT:
      return Cronet_Error_ERROR_CODE_ERROR_CONNECTION_TIMED_OUT;
    case net::ERR_CONNECTION_REFUSED:
      return Cronet_Error_ERROR_CODE_ERROR_CONNECTION_REFUSED;
    case net::ERR_CONNECTION_RESET:
      return Cronet_Error_ERROR_CODE_ERROR_CONNECTION_RESET;
    case net::ERR_ADDRESS_UNREACHABLE:
      return Cronet_Error_ERROR_CODE_ERROR_ADDRESS_UNREACHABLE;
    case net::ERR_QUIC_PROTOCOL_ERROR:
      return Cronet_Error_ERROR_CODE_ERROR_QUIC_PROTOCOL_FAILED;
    default:
      return Cronet_Error_ERROR_CODE_ERROR_OTHER;
  }
}

bool IsCronetErrorImmediatelyRetryable(Cronet_Error_ERROR_CODE error_code) {
  switch (error_code) {
    case Cronet_Error_ERROR_CODE_ERROR_HOSTNAME_NOT_RESOLVED:
    case Cronet_Error_ERROR_CODE_ERROR_INTERNET_DISCONNECTED:
    case Cronet_Error_ERROR_CODE_ERROR_CONNECTION_REFUSED:
    case Cronet_Error_ERROR_CODE_ERROR_ADDRESS_UNREACHABLE:
    case Cronet_Error_ERROR_CODE_ERROR_OTHER:
    default:
      return false;
    case Cronet_Error_ERROR_CODE_ERROR_NETWORK_CHANGED:
    case Cronet_Error_ERROR_CODE_ERROR_TIMED_OUT:
    case Cronet_Error_ERROR_CODE_ERROR_CONNECTION_CLOSED:
    case Cronet_Error_ERROR_CODE_ERROR_CONNECTION_TIMED_OUT:
    case Cronet_Error_ERROR_CODE_ERROR_CONNECTION_RESET:
      return true;
  }
}

scoped_refptr<CronetError> CreateCronet_Error(int net_error,
                                              int quic_error,
                                              const std::string& error_string) {
  auto error = base::MakeRefCounted<CronetError>();
  error->data.error_code = NetErrorToCronetErrorCode(net_error);
  error->data.message = error_string;
  error->data.internal_error_code = net_error;
  error->data.quic_detailed_error_code = quic_error;
  error->data.immediately_retryable =
      IsCronetErrorImmediatelyRetryable(error->data.error_code);
  return error;
}

#if DCHECK_IS_ON()
// Runnable used to verify that Executor calls Cronet_Runnable_Destroy().
class VerifyDestructionRunnable : public Cronet_Runnable {
 public:
  VerifyDestructionRunnable(base::WaitableEvent* destroyed)
      : destroyed_(destroyed) {}
  // Signal event indicating Runnable was properly Destroyed.
  ~VerifyDestructionRunnable() override { destroyed_->Signal(); }

  void Run() override {}

 private:
  // Event indicating destructor is called.
  base::WaitableEvent* const destroyed_;

  DISALLOW_COPY_AND_ASSIGN(VerifyDestructionRunnable);
};
#endif  // DCHECK_IS_ON()

// Convert net::LoadState to Cronet_UrlRequestStatusListener_Status.
Cronet_UrlRequestStatusListener_Status ConvertLoadState(
    net::LoadState load_state) {
  switch (load_state) {
    case net::LOAD_STATE_IDLE:
      return Cronet_UrlRequestStatusListener_Status_IDLE;

    case net::LOAD_STATE_WAITING_FOR_STALLED_SOCKET_POOL:
      return Cronet_UrlRequestStatusListener_Status_WAITING_FOR_STALLED_SOCKET_POOL;

    case net::LOAD_STATE_WAITING_FOR_AVAILABLE_SOCKET:
      return Cronet_UrlRequestStatusListener_Status_WAITING_FOR_AVAILABLE_SOCKET;

    case net::LOAD_STATE_WAITING_FOR_DELEGATE:
      return Cronet_UrlRequestStatusListener_Status_WAITING_FOR_DELEGATE;

    case net::LOAD_STATE_WAITING_FOR_CACHE:
      return Cronet_UrlRequestStatusListener_Status_WAITING_FOR_CACHE;

    case net::LOAD_STATE_DOWNLOADING_PAC_FILE:
      return Cronet_UrlRequestStatusListener_Status_DOWNLOADING_PAC_FILE;

    case net::LOAD_STATE_RESOLVING_PROXY_FOR_URL:
      return Cronet_UrlRequestStatusListener_Status_RESOLVING_PROXY_FOR_URL;

    case net::LOAD_STATE_RESOLVING_HOST_IN_PAC_FILE:
      return Cronet_UrlRequestStatusListener_Status_RESOLVING_HOST_IN_PAC_FILE;

    case net::LOAD_STATE_ESTABLISHING_PROXY_TUNNEL:
      return Cronet_UrlRequestStatusListener_Status_ESTABLISHING_PROXY_TUNNEL;

    case net::LOAD_STATE_RESOLVING_HOST:
      return Cronet_UrlRequestStatusListener_Status_RESOLVING_HOST;

    case net::LOAD_STATE_CONNECTING:
      return Cronet_UrlRequestStatusListener_Status_CONNECTING;

    case net::LOAD_STATE_SSL_HANDSHAKE:
      return Cronet_UrlRequestStatusListener_Status_SSL_HANDSHAKE;

    case net::LOAD_STATE_SENDING_REQUEST:
      return Cronet_UrlRequestStatusListener_Status_SENDING_REQUEST;

    case net::LOAD_STATE_WAITING_FOR_RESPONSE:
      return Cronet_UrlRequestStatusListener_Status_WAITING_FOR_RESPONSE;

    case net::LOAD_STATE_READING_RESPONSE:
      return Cronet_UrlRequestStatusListener_Status_READING_RESPONSE;

    default:
      // A load state is retrieved but there is no corresponding
      // request status. This most likely means that the mapping is
      // incorrect.
      CHECK(false);
      return Cronet_UrlRequestStatusListener_Status_INVALID;
  }
}

}  // namespace

namespace cronet {

// NetworkTasks is owned by CronetURLRequest. It is constructed on client
// thread, but invoked and deleted on the network thread.
class Cronet_UrlRequestImpl::NetworkTasks : public CronetURLRequest::Callback {
 public:
  NetworkTasks(const std::string& url, Cronet_UrlRequestImpl* url_request);
  ~NetworkTasks() override = default;

  // Callback function used for GetStatus().
  void OnStatus(Cronet_UrlRequestStatusListenerPtr listener,
                net::LoadState load_state);

 private:
  // CronetURLRequest::Callback implementation:
  void OnReceivedRedirect(const std::string& new_location,
                          int http_status_code,
                          const std::string& http_status_text,
                          const net::HttpResponseHeaders* headers,
                          bool was_cached,
                          const std::string& negotiated_protocol,
                          const std::string& proxy_server,
                          int64_t received_byte_count) override;
  void OnResponseStarted(int http_status_code,
                         const std::string& http_status_text,
                         const net::HttpResponseHeaders* headers,
                         bool was_cached,
                         const std::string& negotiated_protocol,
                         const std::string& proxy_server,
                         int64_t received_byte_count) override;
  void OnReadCompleted(scoped_refptr<net::IOBuffer> buffer,
                       int bytes_read,
                       int64_t received_byte_count) override;
  void OnSucceeded(int64_t received_byte_count) override;
  void OnError(int net_error,
               int quic_error,
               const std::string& error_string,
               int64_t received_byte_count) override;
  void OnCanceled() override;
  void OnDestroyed() override;
  void OnMetricsCollected(const base::Time& request_start_time,
                          const base::TimeTicks& request_start,
                          const base::TimeTicks& dns_start,
                          const base::TimeTicks& dns_end,
                          const base::TimeTicks& connect_start,
                          const base::TimeTicks& connect_end,
                          const base::TimeTicks& ssl_start,
                          const base::TimeTicks& ssl_end,
                          const base::TimeTicks& send_start,
                          const base::TimeTicks& send_end,
                          const base::TimeTicks& push_start,
                          const base::TimeTicks& push_end,
                          const base::TimeTicks& receive_headers_end,
                          const base::TimeTicks& request_end,
                          bool socket_reused,
                          int64_t sent_bytes_count,
                          int64_t received_bytes_count)
      LOCKS_EXCLUDED(url_request_->lock_) override;

  // The UrlRequest which owns context that owns the callback.
  Cronet_UrlRequestImpl* const url_request_ = nullptr;

  // URL chain contains the URL currently being requested, and
  // all URLs previously requested. New URLs are added before
  // Cronet_UrlRequestCallback::OnRedirectReceived is called.
  std::vector<std::string> url_chain_;

  // Set to true when OnCanceled/OnSucceeded/OnFailed is posted.
  // When true it is unsafe to attempt to post other callbacks
  // like OnStatus because the request may be destroyed.
  bool final_callback_posted_ = false;

  // All methods except constructor are invoked on the network thread.
  THREAD_CHECKER(network_thread_checker_);
  DISALLOW_COPY_AND_ASSIGN(NetworkTasks);
};

Cronet_UrlRequestImpl::Cronet_UrlRequestImpl() = default;

Cronet_UrlRequestImpl::~Cronet_UrlRequestImpl() {
  base::AutoLock lock(lock_);
  // Only request that has never started is allowed to exist at this point.
  // The app must wait for OnSucceeded / OnFailed / OnCanceled  callback before
  // destroying |this|.
  if (request_) {
    CHECK(!started_);
    DestroyRequestUnlessDoneLocked(
        Cronet_RequestFinishedInfo_FINISHED_REASON_SUCCEEDED);
  }
}

Cronet_RESULT Cronet_UrlRequestImpl::InitWithParams(
    Cronet_EnginePtr engine,
    Cronet_String url,
    Cronet_UrlRequestParamsPtr params,
    Cronet_UrlRequestCallbackPtr callback,
    Cronet_ExecutorPtr executor) {
  CHECK(engine);
  engine_ = reinterpret_cast<Cronet_EngineImpl*>(engine);
  if (!url || std::string(url).empty())
    return engine_->CheckResult(Cronet_RESULT_NULL_POINTER_URL);
  if (!params)
    return engine_->CheckResult(Cronet_RESULT_NULL_POINTER_PARAMS);
  if (!callback)
    return engine_->CheckResult(Cronet_RESULT_NULL_POINTER_CALLBACK);
  if (!executor)
    return engine_->CheckResult(Cronet_RESULT_NULL_POINTER_EXECUTOR);

  VLOG(1) << "New Cronet_UrlRequest: " << url;

  base::AutoLock lock(lock_);
  if (request_) {
    return engine_->CheckResult(
        Cronet_RESULT_ILLEGAL_STATE_REQUEST_ALREADY_INITIALIZED);
  }

  callback_ = callback;
  executor_ = executor;

  if (params->request_finished_listener != nullptr &&
      params->request_finished_executor == nullptr) {
    return engine_->CheckResult(
        Cronet_RESULT_NULL_POINTER_REQUEST_FINISHED_INFO_LISTENER_EXECUTOR);
  }

  request_finished_listener_ = params->request_finished_listener;
  request_finished_executor_ = params->request_finished_executor;
  // Copy, don't move -- this function isn't allowed to change |params|.
  annotations_ = params->annotations;

  auto network_tasks = std::make_unique<NetworkTasks>(url, this);
  network_tasks_ = network_tasks.get();

  request_ = new CronetURLRequest(
      engine_->cronet_url_request_context(), std::move(network_tasks),
      GURL(url), ConvertRequestPriority(params->priority),
      params->disable_cache, true /* params->disableConnectionMigration */,
      request_finished_listener_ != nullptr ||
          engine_->HasRequestFinishedListener() /* params->enableMetrics */,
      // TODO(pauljensen): Consider exposing TrafficStats API via C++ API.
      false /* traffic_stats_tag_set */, 0 /* traffic_stats_tag */,
      false /* traffic_stats_uid_set */, 0 /* traffic_stats_uid */);

  if (params->upload_data_provider) {
    upload_data_sink_ = std::make_unique<Cronet_UploadDataSinkImpl>(
        this, params->upload_data_provider,
        params->upload_data_provider_executor
            ? params->upload_data_provider_executor
            : executor);
    upload_data_sink_->InitRequest(request_);
    request_->SetHttpMethod("POST");
  }

  if (!params->http_method.empty() &&
      !request_->SetHttpMethod(params->http_method)) {
    return engine_->CheckResult(
        Cronet_RESULT_ILLEGAL_ARGUMENT_INVALID_HTTP_METHOD);
  }

  for (const auto& request_header : params->request_headers) {
    if (request_header.name.empty())
      return engine_->CheckResult(Cronet_RESULT_NULL_POINTER_HEADER_NAME);
    if (request_header.value.empty())
      return engine_->CheckResult(Cronet_RESULT_NULL_POINTER_HEADER_VALUE);
    if (!request_->AddRequestHeader(request_header.name,
                                    request_header.value)) {
      return engine_->CheckResult(
          Cronet_RESULT_ILLEGAL_ARGUMENT_INVALID_HTTP_HEADER);
    }
  }
  return engine_->CheckResult(Cronet_RESULT_SUCCESS);
}

Cronet_RESULT Cronet_UrlRequestImpl::Start() {
  base::AutoLock lock(lock_);
  if (started_) {
    return engine_->CheckResult(
        Cronet_RESULT_ILLEGAL_STATE_REQUEST_ALREADY_STARTED);
  }
  if (!request_) {
    return engine_->CheckResult(
        Cronet_RESULT_ILLEGAL_STATE_REQUEST_NOT_INITIALIZED);
  }
#if DCHECK_IS_ON()
  Cronet_Executor_Execute(executor_,
                          new VerifyDestructionRunnable(&runnable_destroyed_));
#endif  // DCHECK_IS_ON()
  request_->Start();
  started_ = true;
  return engine_->CheckResult(Cronet_RESULT_SUCCESS);
}

Cronet_RESULT Cronet_UrlRequestImpl::FollowRedirect() {
  base::AutoLock lock(lock_);
  if (!waiting_on_redirect_) {
    return engine_->CheckResult(
        Cronet_RESULT_ILLEGAL_STATE_UNEXPECTED_REDIRECT);
  }
  waiting_on_redirect_ = false;
  if (!IsDoneLocked())
    request_->FollowDeferredRedirect();
  return engine_->CheckResult(Cronet_RESULT_SUCCESS);
}

Cronet_RESULT Cronet_UrlRequestImpl::Read(Cronet_BufferPtr buffer) {
  base::AutoLock lock(lock_);
  if (!waiting_on_read_)
    return engine_->CheckResult(Cronet_RESULT_ILLEGAL_STATE_UNEXPECTED_READ);
  waiting_on_read_ = false;
  if (IsDoneLocked()) {
    Cronet_Buffer_Destroy(buffer);
    return engine_->CheckResult(Cronet_RESULT_SUCCESS);
  }
  // Create IOBuffer that will own |buffer| while it is used by |request_|.
  net::IOBuffer* io_buffer = new IOBufferWithCronet_Buffer(buffer);
  if (request_->ReadData(io_buffer, Cronet_Buffer_GetSize(buffer)))
    return engine_->CheckResult(Cronet_RESULT_SUCCESS);
  return engine_->CheckResult(Cronet_RESULT_ILLEGAL_STATE_READ_FAILED);
}

void Cronet_UrlRequestImpl::Cancel() {
  base::AutoLock lock(lock_);
  if (started_) {
    // If request has posted callbacks to client executor, then it is possible
    // that |request_| will be destroyed before callback is executed. The
    // callback runnable uses IsDone() to avoid calling client callback in this
    // case.
    DestroyRequestUnlessDoneLocked(
        Cronet_RequestFinishedInfo_FINISHED_REASON_CANCELED);
  }
}

bool Cronet_UrlRequestImpl::IsDone() {
  base::AutoLock lock(lock_);
  return IsDoneLocked();
}

bool Cronet_UrlRequestImpl::IsDoneLocked() const {
  lock_.AssertAcquired();
  return started_ && request_ == nullptr;
}

bool Cronet_UrlRequestImpl::DestroyRequestUnlessDone(
    Cronet_RequestFinishedInfo_FINISHED_REASON finished_reason) {
  base::AutoLock lock(lock_);
  return DestroyRequestUnlessDoneLocked(finished_reason);
}

bool Cronet_UrlRequestImpl::DestroyRequestUnlessDoneLocked(
    Cronet_RequestFinishedInfo_FINISHED_REASON finished_reason) {
  lock_.AssertAcquired();
  if (request_ == nullptr)
    return true;
  DCHECK(error_ == nullptr ||
         finished_reason == Cronet_RequestFinishedInfo_FINISHED_REASON_FAILED);
  request_->Destroy(finished_reason ==
                    Cronet_RequestFinishedInfo_FINISHED_REASON_CANCELED);
  // Request can no longer be used as CronetURLRequest::Destroy() will
  // eventually delete |request_| from the network thread, so setting |request_|
  // to nullptr doesn't introduce a memory leak.
  request_ = nullptr;
  return false;
}

void Cronet_UrlRequestImpl::GetStatus(
    Cronet_UrlRequestStatusListenerPtr listener) {
  {
    base::AutoLock lock(lock_);
    if (started_ && request_) {
      status_listeners_.insert(listener);
      request_->GetStatus(
          base::BindOnce(&Cronet_UrlRequestImpl::NetworkTasks::OnStatus,
                         base::Unretained(network_tasks_), listener));
      return;
    }
  }
  PostTaskToExecutor(
      base::BindOnce(Cronet_UrlRequestStatusListener_OnStatus, listener,
                     Cronet_UrlRequestStatusListener_Status_INVALID));
}

void Cronet_UrlRequestImpl::PostCallbackOnFailedToExecutor() {
  PostTaskToExecutor(base::BindOnce(
      &Cronet_UrlRequestImpl::InvokeCallbackOnFailed, base::Unretained(this)));
}

void Cronet_UrlRequestImpl::OnUploadDataProviderError(
    const std::string& error_message) {
  base::AutoLock lock(lock_);
  // If |error_| is not nullptr, that means that another network error is
  // already reported.
  if (error_)
    return;
  error_ = CreateCronet_Error(
      0, 0, "Failure from UploadDataProvider: " + error_message);
  error_->data.error_code = Cronet_Error_ERROR_CODE_ERROR_CALLBACK;

  request_->MaybeReportMetricsAndRunCallback(
      base::BindOnce(&Cronet_UrlRequestImpl::PostCallbackOnFailedToExecutor,
                     base::Unretained(this)));
}

void Cronet_UrlRequestImpl::PostTaskToExecutor(base::OnceClosure task) {
  Cronet_RunnablePtr runnable =
      new cronet::OnceClosureRunnable(std::move(task));
  // |runnable| is passed to executor, which destroys it after execution.
  Cronet_Executor_Execute(executor_, runnable);
}

void Cronet_UrlRequestImpl::InvokeCallbackOnRedirectReceived(
    const std::string& new_location) {
  if (IsDone())
    return;
  Cronet_UrlRequestCallback_OnRedirectReceived(
      callback_, this, GetData(response_info_), new_location.c_str());
}

void Cronet_UrlRequestImpl::InvokeCallbackOnResponseStarted() {
  if (IsDone())
    return;
#if DCHECK_IS_ON()
  // Verify that Executor calls Cronet_Runnable_Destroy().
  if (!runnable_destroyed_.TimedWait(base::TimeDelta::FromSeconds(5))) {
    LOG(ERROR) << "Cronet Executor didn't call Cronet_Runnable_Destroy() in "
                  "5s; still waiting.";
    runnable_destroyed_.Wait();
  }
#endif  // DCHECK_IS_ON()
  Cronet_UrlRequestCallback_OnResponseStarted(callback_, this,
                                              GetData(response_info_));
}

void Cronet_UrlRequestImpl::InvokeCallbackOnReadCompleted(
    std::unique_ptr<Cronet_Buffer> cronet_buffer,
    int bytes_read) {
  if (IsDone())
    return;
  Cronet_UrlRequestCallback_OnReadCompleted(
      callback_, this, GetData(response_info_), cronet_buffer.release(),
      bytes_read);
}

void Cronet_UrlRequestImpl::InvokeCallbackOnSucceeded() {
  if (DestroyRequestUnlessDone(
          Cronet_RequestFinishedInfo_FINISHED_REASON_SUCCEEDED)) {
    return;
  }
  InvokeAllStatusListeners();
  MaybeReportMetrics(Cronet_RequestFinishedInfo_FINISHED_REASON_SUCCEEDED);
  Cronet_UrlRequestCallback_OnSucceeded(callback_, this,
                                        GetData(response_info_));
  // |this| may have been deleted here.
}

void Cronet_UrlRequestImpl::InvokeCallbackOnFailed() {
  if (DestroyRequestUnlessDone(
          Cronet_RequestFinishedInfo_FINISHED_REASON_FAILED)) {
    return;
  }
  InvokeAllStatusListeners();
  MaybeReportMetrics(Cronet_RequestFinishedInfo_FINISHED_REASON_FAILED);
  Cronet_UrlRequestCallback_OnFailed(callback_, this, GetData(response_info_),
                                     GetData(error_));
  // |this| may have been deleted here.
}

void Cronet_UrlRequestImpl::InvokeCallbackOnCanceled() {
  InvokeAllStatusListeners();
  MaybeReportMetrics(Cronet_RequestFinishedInfo_FINISHED_REASON_CANCELED);
  Cronet_UrlRequestCallback_OnCanceled(callback_, this,
                                       GetData(response_info_));
  // |this| may have been deleted here.
}

void Cronet_UrlRequestImpl::InvokeAllStatusListeners() {
  std::unordered_multiset<Cronet_UrlRequestStatusListenerPtr> status_listeners;
  {
    base::AutoLock lock(lock_);
    // Verify the request has already been destroyed, which ensures no more
    // status listeners can be added.
    DCHECK(!request_);
    status_listeners.swap(status_listeners_);
  }
  for (Cronet_UrlRequestStatusListener* status_listener : status_listeners) {
    Cronet_UrlRequestStatusListener_OnStatus(
        status_listener, Cronet_UrlRequestStatusListener_Status_INVALID);
  }
#if DCHECK_IS_ON()
  // Verify no status listeners added during OnStatus() callbacks.
  base::AutoLock lock(lock_);
  DCHECK(status_listeners_.empty());
#endif  // DCHECK_IS_ON()
}

void Cronet_UrlRequestImpl::MaybeReportMetrics(
    Cronet_RequestFinishedInfo_FINISHED_REASON finished_reason) {
  if (request_finished_info_ == nullptr)
    return;
  request_finished_info_->data.annotations = std::move(annotations_);
  request_finished_info_->data.finished_reason = finished_reason;

  engine_->ReportRequestFinished(request_finished_info_, response_info_,
                                 error_);
  if (request_finished_listener_ != nullptr) {
    DCHECK(request_finished_executor_ != nullptr);
    // Execute() owns and deletes the runnable.
    request_finished_executor_->Execute(
        new cronet::OnceClosureRunnable(base::BindOnce(
            [](Cronet_RequestFinishedInfoListenerPtr request_finished_listener,
               scoped_refptr<RequestFinishedInfo> request_finished_info,
               scoped_refptr<UrlResponseInfo> response_info,
               scoped_refptr<CronetError> error) {
              request_finished_listener->OnRequestFinished(
                  GetData(request_finished_info), GetData(response_info),
                  GetData(error));
            },
            request_finished_listener_, request_finished_info_, response_info_,
            error_)));
  }
}

Cronet_UrlRequestImpl::NetworkTasks::NetworkTasks(
    const std::string& url,
    Cronet_UrlRequestImpl* url_request)
    : url_request_(url_request), url_chain_({url}) {
  DETACH_FROM_THREAD(network_thread_checker_);
  DCHECK(url_request);
}

void Cronet_UrlRequestImpl::NetworkTasks::OnReceivedRedirect(
    const std::string& new_location,
    int http_status_code,
    const std::string& http_status_text,
    const net::HttpResponseHeaders* headers,
    bool was_cached,
    const std::string& negotiated_protocol,
    const std::string& proxy_server,
    int64_t received_byte_count) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  {
    base::AutoLock lock(url_request_->lock_);
    url_request_->waiting_on_redirect_ = true;
    url_request_->response_info_ = CreateCronet_UrlResponseInfo(
        url_chain_, http_status_code, http_status_text, headers, was_cached,
        negotiated_protocol, proxy_server, received_byte_count);
  }

  // Have to do this after creating responseInfo.
  url_chain_.push_back(new_location);

  // Invoke Cronet_UrlRequestCallback_OnRedirectReceived on client executor.
  url_request_->PostTaskToExecutor(
      base::BindOnce(&Cronet_UrlRequestImpl::InvokeCallbackOnRedirectReceived,
                     base::Unretained(url_request_), new_location));
}

void Cronet_UrlRequestImpl::NetworkTasks::OnResponseStarted(
    int http_status_code,
    const std::string& http_status_text,
    const net::HttpResponseHeaders* headers,
    bool was_cached,
    const std::string& negotiated_protocol,
    const std::string& proxy_server,
    int64_t received_byte_count) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  {
    base::AutoLock lock(url_request_->lock_);
    url_request_->waiting_on_read_ = true;
    url_request_->response_info_ = CreateCronet_UrlResponseInfo(
        url_chain_, http_status_code, http_status_text, headers, was_cached,
        negotiated_protocol, proxy_server, received_byte_count);
  }

  if (url_request_->upload_data_sink_)
    url_request_->upload_data_sink_->PostCloseToExecutor();

  // Invoke Cronet_UrlRequestCallback_OnResponseStarted on client executor.
  url_request_->PostTaskToExecutor(
      base::BindOnce(&Cronet_UrlRequestImpl::InvokeCallbackOnResponseStarted,
                     base::Unretained(url_request_)));
}

void Cronet_UrlRequestImpl::NetworkTasks::OnReadCompleted(
    scoped_refptr<net::IOBuffer> buffer,
    int bytes_read,
    int64_t received_byte_count) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  IOBufferWithCronet_Buffer* io_buffer =
      reinterpret_cast<IOBufferWithCronet_Buffer*>(buffer.get());
  std::unique_ptr<Cronet_Buffer> cronet_buffer(io_buffer->Release());
  {
    base::AutoLock lock(url_request_->lock_);
    url_request_->waiting_on_read_ = true;
    url_request_->response_info_->data.received_byte_count =
        received_byte_count;
  }

  // Invoke Cronet_UrlRequestCallback_OnReadCompleted on client executor.
  url_request_->PostTaskToExecutor(base::BindOnce(
      &Cronet_UrlRequestImpl::InvokeCallbackOnReadCompleted,
      base::Unretained(url_request_), std::move(cronet_buffer), bytes_read));
}

void Cronet_UrlRequestImpl::NetworkTasks::OnSucceeded(
    int64_t received_byte_count) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  {
    base::AutoLock lock(url_request_->lock_);
    url_request_->response_info_->data.received_byte_count =
        received_byte_count;
  }

  // Invoke Cronet_UrlRequestCallback_OnSucceeded on client executor.
  url_request_->PostTaskToExecutor(
      base::BindOnce(&Cronet_UrlRequestImpl::InvokeCallbackOnSucceeded,
                     base::Unretained(url_request_)));
  final_callback_posted_ = true;
}

void Cronet_UrlRequestImpl::NetworkTasks::OnError(
    int net_error,
    int quic_error,
    const std::string& error_string,
    int64_t received_byte_count) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  {
    base::AutoLock lock(url_request_->lock_);
    if (url_request_->response_info_)
      url_request_->response_info_->data.received_byte_count =
          received_byte_count;
    url_request_->error_ =
        CreateCronet_Error(net_error, quic_error, error_string);
  }

  if (url_request_->upload_data_sink_)
    url_request_->upload_data_sink_->PostCloseToExecutor();

  // Invoke Cronet_UrlRequestCallback_OnFailed on client executor.
  url_request_->PostTaskToExecutor(
      base::BindOnce(&Cronet_UrlRequestImpl::InvokeCallbackOnFailed,
                     base::Unretained(url_request_)));
  final_callback_posted_ = true;
}

void Cronet_UrlRequestImpl::NetworkTasks::OnCanceled() {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  if (url_request_->upload_data_sink_)
    url_request_->upload_data_sink_->PostCloseToExecutor();

  // Invoke Cronet_UrlRequestCallback_OnCanceled on client executor.
  url_request_->PostTaskToExecutor(
      base::BindOnce(&Cronet_UrlRequestImpl::InvokeCallbackOnCanceled,
                     base::Unretained(url_request_)));
  final_callback_posted_ = true;
}

void Cronet_UrlRequestImpl::NetworkTasks::OnDestroyed() {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  DCHECK(url_request_);
}

void Cronet_UrlRequestImpl::NetworkTasks::OnMetricsCollected(
    const base::Time& request_start_time,
    const base::TimeTicks& request_start,
    const base::TimeTicks& dns_start,
    const base::TimeTicks& dns_end,
    const base::TimeTicks& connect_start,
    const base::TimeTicks& connect_end,
    const base::TimeTicks& ssl_start,
    const base::TimeTicks& ssl_end,
    const base::TimeTicks& send_start,
    const base::TimeTicks& send_end,
    const base::TimeTicks& push_start,
    const base::TimeTicks& push_end,
    const base::TimeTicks& receive_headers_end,
    const base::TimeTicks& request_end,
    bool socket_reused,
    int64_t sent_bytes_count,
    int64_t received_bytes_count) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  base::AutoLock lock(url_request_->lock_);
  DCHECK_EQ(url_request_->request_finished_info_, nullptr)
      << "Metrics collection should only happen once.";
  url_request_->request_finished_info_ =
      base::MakeRefCounted<RequestFinishedInfo>();
  auto& metrics = url_request_->request_finished_info_->data.metrics;
  metrics.emplace();
  using native_metrics_util::ConvertTime;
  ConvertTime(request_start, request_start, request_start_time,
              &metrics->request_start);
  ConvertTime(dns_start, request_start, request_start_time,
              &metrics->dns_start);
  ConvertTime(dns_end, request_start, request_start_time, &metrics->dns_end);
  ConvertTime(connect_start, request_start, request_start_time,
              &metrics->connect_start);
  ConvertTime(connect_end, request_start, request_start_time,
              &metrics->connect_end);
  ConvertTime(ssl_start, request_start, request_start_time,
              &metrics->ssl_start);
  ConvertTime(ssl_end, request_start, request_start_time, &metrics->ssl_end);
  ConvertTime(send_start, request_start, request_start_time,
              &metrics->sending_start);
  ConvertTime(send_end, request_start, request_start_time,
              &metrics->sending_end);
  ConvertTime(push_start, request_start, request_start_time,
              &metrics->push_start);
  ConvertTime(push_end, request_start, request_start_time, &metrics->push_end);
  ConvertTime(receive_headers_end, request_start, request_start_time,
              &metrics->response_start);
  ConvertTime(request_end, request_start, request_start_time,
              &metrics->request_end);
  metrics->socket_reused = socket_reused;
  metrics->sent_byte_count = sent_bytes_count;
  metrics->received_byte_count = received_bytes_count;
}

void Cronet_UrlRequestImpl::NetworkTasks::OnStatus(
    Cronet_UrlRequestStatusListenerPtr listener,
    net::LoadState load_state) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  if (final_callback_posted_)
    return;
  {
    base::AutoLock lock(url_request_->lock_);
    auto element = url_request_->status_listeners_.find(listener);
    CHECK(element != url_request_->status_listeners_.end());
    url_request_->status_listeners_.erase(element);
  }

  // Invoke Cronet_UrlRequestCallback_OnCanceled on client executor.
  url_request_->PostTaskToExecutor(
      base::BindOnce(&Cronet_UrlRequestStatusListener_OnStatus, listener,
                     ConvertLoadState(load_state)));
}

}  // namespace cronet

CRONET_EXPORT Cronet_UrlRequestPtr Cronet_UrlRequest_Create() {
  return new cronet::Cronet_UrlRequestImpl();
}
