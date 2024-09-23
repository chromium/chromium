// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_NATIVE_TEST_TEST_URL_REQUEST_CALLBACK_H_
#define COMPONENTS_CRONET_NATIVE_TEST_TEST_URL_REQUEST_CALLBACK_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cronet_c.h"

#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"

namespace cronet {
// Various test utility functions for testing Cronet.
namespace test {

class TestUrlRequestCallback {
 public:
  enum ResponseStep {
    NOTHING,
    ON_RECEIVED_REDIRECT,
    ON_RESPONSE_STARTED,
    ON_READ_COMPLETED,
    ON_SUCCEEDED,
    ON_FAILED,
    ON_CANCELED,
  };

  enum FailureType {
    NONE,
    CANCEL_SYNC,
    CANCEL_ASYNC,
    // Same as above, but continues to advance the request after posting
    // the cancellation task.
    CANCEL_ASYNC_WITHOUT_PAUSE,
  };

  class UrlResponseInfo {
   public:
    // Construct actual response info copied from Cronet_UrlResponseInfoPtr.
    explicit UrlResponseInfo(Cronet_UrlResponseInfoPtr response_info);
    // Construct expected response info for testing.
    UrlResponseInfo(const std::vector<std::string>& urls,
                    const std::string& message,
                    int32_t status_code,
                    int64_t received_bytes,
                    std::vector<std::string> headers);
    ~UrlResponseInfo();

    // Data copied from response_info to make it available after request is
    // done.
    std::string url;
    std::vector<std::string> url_chain;
    int32_t http_status_code = 0;
    std::string http_status_text;
    std::vector<std::pair<std::string, std::string>> all_headers;
    bool was_cached = false;
    std::string negotiated_protocol;
    std::string proxy_server;
    int64_t received_byte_count = 0;
  };

  // TODO(crbug.com/41462044): Make these private with public accessors.
  std::vector<std::unique_ptr<UrlResponseInfo>> redirect_response_info_list_;
  std::vector<std::string> redirect_url_list_;
  // Owned by UrlRequest, only valid until UrlRequest is destroyed.
  Cronet_UrlResponseInfoPtr original_response_info_ = nullptr;
  // |response_info_| is copied from |original_response_info_|, valid after
  // UrlRequest is destroyed.
  std::unique_ptr<UrlResponseInfo> response_info_;
  // Owned by UrlRequest, only valid until UrlRequest is destroyed.
  Cronet_ErrorPtr last_error_ = nullptr;
  // Values copied from |last_error_| valid after UrlRequest is destroyed.
  Cronet_Error_ERROR_CODE last_error_code_ =
      Cronet_Error_ERROR_CODE_ERROR_OTHER;
  std::string last_error_message_;

  ResponseStep response_step_ = NOTHING;

  int redirect_count_ = 0;
  bool on_error_called_ = false;
  bool on_canceled_called_ = false;

  int response_data_length_ = 0;
  std::string response_as_string_;

  explicit TestUrlRequestCallback(bool direct_executor);
  virtual ~TestUrlRequestCallback();

  Cronet_ExecutorPtr GetExecutor();

  Cronet_UrlRequestCallbackPtr CreateUrlRequestCallback();

  void set_auto_advance(bool auto_advance) { auto_advance_ = auto_advance; }

  void set_accumulate_response_data(bool accuumulate) {
    accumulate_response_data_ = accuumulate;
  }

  void set_failure(FailureType failure_type, ResponseStep failure_step) {
    failure_step_ = failure_step;
    failure_type_ = failure_type;
  }

  void WaitForDone() { done_.Wait(); }

  void WaitForNextStep() {
    step_block_.Wait();
    step_block_.Reset();
  }

  void ShutdownExecutor();

  bool IsDone() { return done_.IsSignaled(); }

 protected:
  class Executor;

  virtual void OnRedirectReceived(Cronet_UrlRequestPtr request,
                                  Cronet_UrlResponseInfoPtr info,
                                  Cronet_String newLocationUrl);

  virtual void OnResponseStarted(Cronet_UrlRequestPtr request,
                                 Cronet_UrlResponseInfoPtr info);

  virtual void OnReadCompleted(Cronet_UrlRequestPtr request,
                               Cronet_UrlResponseInfoPtr info,
                               Cronet_BufferPtr buffer,
                               uint64_t bytes_read);

  virtual void OnSucceeded(Cronet_UrlRequestPtr request,
                           Cronet_UrlResponseInfoPtr info);

  virtual void OnFailed(Cronet_UrlRequestPtr request,
                        Cronet_UrlResponseInfoPtr info,
                        Cronet_ErrorPtr error);

  virtual void OnCanceled(Cronet_UrlRequestPtr request,
                          Cronet_UrlResponseInfoPtr info);

  void StartNextRead(Cronet_UrlRequestPtr request) {
    Cronet_BufferPtr buffer = Cronet_Buffer_Create();
    Cronet_Buffer_InitWithAlloc(buffer, READ_BUFFER_SIZE);

    StartNextRead(request, buffer);
  }

  void StartNextRead(Cronet_UrlRequestPtr request, Cronet_BufferPtr buffer) {
    Cronet_UrlRequest_Read(request, buffer);
  }

  void SignalDone() { done_.Signal(); }

  void CheckExecutorThread();

  /**
   * Returns false if the callback should continue to advance the
   * request.
   */
  bool MaybeCancelOrPause(Cronet_UrlRequestPtr request);

  // Implementation of Cronet_UrlRequestCallback methods.
  static TestUrlRequestCallback* GetThis(Cronet_UrlRequestCallbackPtr self);

  static void OnRedirectReceived(Cronet_UrlRequestCallbackPtr self,
                                 Cronet_UrlRequestPtr request,
                                 Cronet_UrlResponseInfoPtr info,
                                 Cronet_String newLocationUrl);

  static void OnResponseStarted(Cronet_UrlRequestCallbackPtr self,
                                Cronet_UrlRequestPtr request,
                                Cronet_UrlResponseInfoPtr info);

  static void OnReadCompleted(Cronet_UrlRequestCallbackPtr self,
                              Cronet_UrlRequestPtr request,
                              Cronet_UrlResponseInfoPtr info,
                              Cronet_BufferPtr buffer,
                              uint64_t bytesRead);

  static void OnSucceeded(Cronet_UrlRequestCallbackPtr self,
                          Cronet_UrlRequestPtr request,
                          Cronet_UrlResponseInfoPtr info);

  static void OnFailed(Cronet_UrlRequestCallbackPtr self,
                       Cronet_UrlRequestPtr request,
                       Cronet_UrlResponseInfoPtr info,
                       Cronet_ErrorPtr error);

  static void OnCanceled(Cronet_UrlRequestCallbackPtr self,
                         Cronet_UrlRequestPtr request,
                         Cronet_UrlResponseInfoPtr info);

  // Implementation of Cronet_Executor methods.
  static void Execute(Cronet_ExecutorPtr self, Cronet_RunnablePtr runnable);
  static void ExecuteDirect(Cronet_ExecutorPtr self,
                            Cronet_RunnablePtr runnable);

  const int READ_BUFFER_SIZE = 32 * 1024;

  // When false, the consumer is responsible for all calls into the request
  // that advance it.
  bool auto_advance_ = true;

  // When false response data is not accuumulated for better performance.
  bool accumulate_response_data_ = true;

  // Whether to create direct executors.
  const bool direct_executor_;

  // Conditionally fail on certain steps.
  FailureType failure_type_ = NONE;
  ResponseStep failure_step_ = NOTHING;

  // Signals when request is done either successfully or not.
  base::WaitableEvent done_;

  // Signaled on each step when |auto_advance_| is false.
  base::WaitableEvent step_block_;

  // Lock that synchronizes access to |executor_| and |executor_thread_|.
  base::Lock executor_lock_;

  // Executor that runs callback tasks.
  Cronet_ExecutorPtr executor_ = nullptr;

  // Thread on which |executor_| runs callback tasks.
  std::unique_ptr<base::Thread> executor_thread_;
};

}  // namespace test
}  // namespace cronet

#endif  // COMPONENTS_CRONET_NATIVE_TEST_TEST_URL_REQUEST_CALLBACK_H_
