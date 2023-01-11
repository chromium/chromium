// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/native/test/test_url_request_callback.h"

#include "base/functional/bind.h"
#include "components/cronet/native/test/test_util.h"

namespace cronet {
namespace test {

TestUrlRequestCallback::UrlResponseInfo::UrlResponseInfo(
    Cronet_UrlResponseInfoPtr response_info)
    : url(Cronet_UrlResponseInfo_url_get(response_info)),
      http_status_code(
          Cronet_UrlResponseInfo_http_status_code_get(response_info)),
      http_status_text(
          Cronet_UrlResponseInfo_http_status_text_get(response_info)),
      was_cached(Cronet_UrlResponseInfo_was_cached_get(response_info)),
      negotiated_protocol(
          Cronet_UrlResponseInfo_negotiated_protocol_get(response_info)),
      proxy_server(Cronet_UrlResponseInfo_proxy_server_get(response_info)),
      received_byte_count(
          Cronet_UrlResponseInfo_received_byte_count_get(response_info)) {
  for (uint32_t url_id = 0;
       url_id < Cronet_UrlResponseInfo_url_chain_size(response_info);
       ++url_id) {
    url_chain.push_back(
        Cronet_UrlResponseInfo_url_chain_at(response_info, url_id));
  }
  for (uint32_t i = 0;
       i < Cronet_UrlResponseInfo_all_headers_list_size(response_info); ++i) {
    Cronet_HttpHeaderPtr header =
        Cronet_UrlResponseInfo_all_headers_list_at(response_info, i);
    all_headers.push_back(std::pair<std::string, std::string>(
        Cronet_HttpHeader_name_get(header),
        Cronet_HttpHeader_value_get(header)));
  }
}

TestUrlRequestCallback::UrlResponseInfo::UrlResponseInfo(
    const std::vector<std::string>& urls,
    const std::string& message,
    int32_t status_code,
    int64_t received_bytes,
    std::vector<std::string> headers)
    : url(urls.back()),
      url_chain(urls),
      http_status_code(status_code),
      http_status_text(message),
      negotiated_protocol("unknown"),
      proxy_server(":0"),
      received_byte_count(received_bytes) {
  for (uint32_t i = 0; i < headers.size(); i += 2) {
    all_headers.push_back(
        std::pair<std::string, std::string>(headers[i], headers[i + 1]));
  }
}

TestUrlRequestCallback::UrlResponseInfo::~UrlResponseInfo() = default;

TestUrlRequestCallback::TestUrlRequestCallback(bool direct_executor)
    : direct_executor_(direct_executor),
      done_(base::WaitableEvent::ResetPolicy::MANUAL,
            base::WaitableEvent::InitialState::NOT_SIGNALED),
      step_block_(base::WaitableEvent::ResetPolicy::MANUAL,
                  base::WaitableEvent::InitialState::NOT_SIGNALED) {}

TestUrlRequestCallback::~TestUrlRequestCallback() {
  ShutdownExecutor();
}

Cronet_ExecutorPtr TestUrlRequestCallback::GetExecutor() {
  if (executor_)
    return executor_;
  if (direct_executor_) {
    executor_ =
        Cronet_Executor_CreateWith(TestUrlRequestCallback::ExecuteDirect);
  } else {
    executor_thread_ =
        std::make_unique<base::Thread>("TestUrlRequestCallback executor");
    executor_thread_->Start();
    executor_ = Cronet_Executor_CreateWith(TestUrlRequestCallback::Execute);
    Cronet_Executor_SetClientContext(executor_, this);
  }
  return executor_;
}

Cronet_UrlRequestCallbackPtr
TestUrlRequestCallback::CreateUrlRequestCallback() {
  Cronet_UrlRequestCallbackPtr callback = Cronet_UrlRequestCallback_CreateWith(
      TestUrlRequestCallback::OnRedirectReceived,
      TestUrlRequestCallback::OnResponseStarted,
      TestUrlRequestCallback::OnReadCompleted,
      TestUrlRequestCallback::OnSucceeded, TestUrlRequestCallback::OnFailed,
      TestUrlRequestCallback::OnCanceled);
  Cronet_UrlRequestCallback_SetClientContext(callback, this);
  return callback;
}

void TestUrlRequestCallback::OnRedirectReceived(Cronet_UrlRequestPtr request,
                                                Cronet_UrlResponseInfoPtr info,
                                                Cronet_String newLocationUrl) {
  CheckExecutorThread();

  CHECK(!Cronet_UrlRequest_IsDone(request));
  CHECK(response_step_ == NOTHING || response_step_ == ON_RECEIVED_REDIRECT);
  CHECK(!last_error_);

  response_step_ = ON_RECEIVED_REDIRECT;
  redirect_url_list_.push_back(newLocationUrl);
  redirect_response_info_list_.push_back(
      std::make_unique<UrlResponseInfo>(info));
  ++redirect_count_;
  if (MaybeCancelOrPause(request)) {
    return;
  }
  Cronet_UrlRequest_FollowRedirect(request);
}

void TestUrlRequestCallback::OnResponseStarted(Cronet_UrlRequestPtr request,
                                               Cronet_UrlResponseInfoPtr info) {
  CheckExecutorThread();
  CHECK(!Cronet_UrlRequest_IsDone(request));
  CHECK(response_step_ == NOTHING || response_step_ == ON_RECEIVED_REDIRECT);
  CHECK(!last_error_);
  response_step_ = ON_RESPONSE_STARTED;
  original_response_info_ = info;
  response_info_ = std::make_unique<UrlResponseInfo>(info);
  if (MaybeCancelOrPause(request)) {
    return;
  }
  StartNextRead(request);
}

void TestUrlRequestCallback::OnReadCompleted(Cronet_UrlRequestPtr request,
                                             Cronet_UrlResponseInfoPtr info,
                                             Cronet_BufferPtr buffer,
                                             uint64_t bytes_read) {
  CheckExecutorThread();
  CHECK(!Cronet_UrlRequest_IsDone(request));
  CHECK(response_step_ == ON_RESPONSE_STARTED ||
        response_step_ == ON_READ_COMPLETED);
  CHECK(!last_error_);
  response_step_ = ON_READ_COMPLETED;
  original_response_info_ = info;
  response_info_ = std::make_unique<UrlResponseInfo>(info);
  response_data_length_ += bytes_read;

  if (accumulate_response_data_) {
    std::string last_read_data(
        reinterpret_cast<char*>(Cronet_Buffer_GetData(buffer)), bytes_read);
    response_as_string_ += last_read_data;
  }

  if (MaybeCancelOrPause(request)) {
    Cronet_Buffer_Destroy(buffer);
    return;
  }
  StartNextRead(request, buffer);
}

void TestUrlRequestCallback::OnSucceeded(Cronet_UrlRequestPtr request,
                                         Cronet_UrlResponseInfoPtr info) {
  CheckExecutorThread();
  CHECK(Cronet_UrlRequest_IsDone(request));
  CHECK(response_step_ == ON_RESPONSE_STARTED ||
        response_step_ == ON_READ_COMPLETED);
  CHECK(!on_error_called_);
  CHECK(!on_canceled_called_);
  CHECK(!last_error_);
  response_step_ = ON_SUCCEEDED;
  original_response_info_ = info;
  response_info_ = std::make_unique<UrlResponseInfo>(info);

  MaybeCancelOrPause(request);
  SignalDone();
}

void TestUrlRequestCallback::OnFailed(Cronet_UrlRequestPtr request,
                                      Cronet_UrlResponseInfoPtr info,
                                      Cronet_ErrorPtr error) {
  CheckExecutorThread();
  CHECK(Cronet_UrlRequest_IsDone(request));
  // Shouldn't happen after success.
  CHECK(response_step_ != ON_SUCCEEDED);
  // Should happen at most once for a single request.
  CHECK(!on_error_called_);
  CHECK(!on_canceled_called_);
  CHECK(!last_error_);

  response_step_ = ON_FAILED;
  on_error_called_ = true;
  // It is possible that |info| is nullptr if response has not started.
  if (info) {
    original_response_info_ = info;
    response_info_ = std::make_unique<UrlResponseInfo>(info);
  }
  last_error_ = error;
  last_error_code_ = Cronet_Error_error_code_get(error);
  last_error_message_ = Cronet_Error_message_get(error);
  MaybeCancelOrPause(request);
  SignalDone();
}

void TestUrlRequestCallback::OnCanceled(Cronet_UrlRequestPtr request,
                                        Cronet_UrlResponseInfoPtr info) {
  CheckExecutorThread();
  CHECK(Cronet_UrlRequest_IsDone(request));
  CHECK(!on_error_called_);
  // Should happen at most once for a single request.
  CHECK(!on_canceled_called_);
  CHECK(!last_error_);

  response_step_ = ON_CANCELED;
  on_canceled_called_ = true;
  // It is possible |info| is nullptr if the response has not started.
  if (info) {
    original_response_info_ = info;
    response_info_ = std::make_unique<UrlResponseInfo>(info);
  }
  MaybeCancelOrPause(request);
  SignalDone();
}

void TestUrlRequestCallback::ShutdownExecutor() {
  base::AutoLock lock(executor_lock_);
  if (executor_ == nullptr)
    return;
  Cronet_Executor_Destroy(executor_);
  executor_ = nullptr;
  // Stop executor thread outside of lock to allow runnables to complete.
  auto executor_thread(std::move(executor_thread_));
  executor_lock_.Release();
  executor_thread.reset();
  executor_lock_.Acquire();
}

void TestUrlRequestCallback::CheckExecutorThread() {
  base::AutoLock lock(executor_lock_);
  if (executor_thread_ && !direct_executor_)
    CHECK(executor_thread_->task_runner()->BelongsToCurrentThread());
}

bool TestUrlRequestCallback::MaybeCancelOrPause(Cronet_UrlRequestPtr request) {
  CheckExecutorThread();
  if (response_step_ != failure_step_ || failure_type_ == NONE) {
    if (!auto_advance_) {
      step_block_.Signal();
      return true;
    }
    return false;
  }

  if (failure_type_ == CANCEL_SYNC) {
    Cronet_UrlRequest_Cancel(request);
  }
  if (failure_type_ == CANCEL_ASYNC ||
      failure_type_ == CANCEL_ASYNC_WITHOUT_PAUSE) {
    if (direct_executor_) {
      Cronet_UrlRequest_Cancel(request);
    } else {
      base::AutoLock lock(executor_lock_);
      CHECK(executor_thread_);
      executor_thread_->task_runner()->PostTask(
          FROM_HERE, base::BindOnce(&Cronet_UrlRequest_Cancel, request));
    }
  }
  return failure_type_ != CANCEL_ASYNC_WITHOUT_PAUSE;
}

/* static */
TestUrlRequestCallback* TestUrlRequestCallback::GetThis(
    Cronet_UrlRequestCallbackPtr self) {
  return static_cast<TestUrlRequestCallback*>(
      Cronet_UrlRequestCallback_GetClientContext(self));
}

/* static */
void TestUrlRequestCallback::OnRedirectReceived(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_UrlRequestPtr request,
    Cronet_UrlResponseInfoPtr info,
    Cronet_String newLocationUrl) {
  GetThis(self)->OnRedirectReceived(request, info, newLocationUrl);
}

/* static */
void TestUrlRequestCallback::OnResponseStarted(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_UrlRequestPtr request,
    Cronet_UrlResponseInfoPtr info) {
  GetThis(self)->OnResponseStarted(request, info);
}

/* static */
void TestUrlRequestCallback::OnReadCompleted(Cronet_UrlRequestCallbackPtr self,
                                             Cronet_UrlRequestPtr request,
                                             Cronet_UrlResponseInfoPtr info,
                                             Cronet_BufferPtr buffer,
                                             uint64_t bytesRead) {
  GetThis(self)->OnReadCompleted(request, info, buffer, bytesRead);
}

/* static */
void TestUrlRequestCallback::OnSucceeded(Cronet_UrlRequestCallbackPtr self,
                                         Cronet_UrlRequestPtr request,
                                         Cronet_UrlResponseInfoPtr info) {
  GetThis(self)->OnSucceeded(request, info);
}

/* static */
void TestUrlRequestCallback::OnFailed(Cronet_UrlRequestCallbackPtr self,
                                      Cronet_UrlRequestPtr request,
                                      Cronet_UrlResponseInfoPtr info,
                                      Cronet_ErrorPtr error) {
  GetThis(self)->OnFailed(request, info, error);
}

/* static */
void TestUrlRequestCallback::OnCanceled(Cronet_UrlRequestCallbackPtr self,
                                        Cronet_UrlRequestPtr request,
                                        Cronet_UrlResponseInfoPtr info) {
  GetThis(self)->OnCanceled(request, info);
}

/* static */
void TestUrlRequestCallback::Execute(Cronet_ExecutorPtr self,
                                     Cronet_RunnablePtr runnable) {
  CHECK(self);
  auto* callback = static_cast<TestUrlRequestCallback*>(
      Cronet_Executor_GetClientContext(self));
  CHECK(callback);
  base::AutoLock lock(callback->executor_lock_);
  CHECK(callback->executor_thread_);
  // Post |runnable| onto executor thread.
  callback->executor_thread_->task_runner()->PostTask(
      FROM_HERE, RunnableWrapper::CreateOnceClosure(runnable));
}

/* static */
void TestUrlRequestCallback::ExecuteDirect(Cronet_ExecutorPtr self,
                                           Cronet_RunnablePtr runnable) {
  // Run |runnable| directly.
  Cronet_Runnable_Run(runnable);
  Cronet_Runnable_Destroy(runnable);
}

}  // namespace test
}  // namespace cronet
