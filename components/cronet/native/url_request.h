// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_NATIVE_URL_REQUEST_H_
#define COMPONENTS_CRONET_NATIVE_URL_REQUEST_H_

#include <memory>
#include <string>
#include <unordered_set>

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/thread_annotations.h"
#include "components/cronet/cronet_context.h"
#include "components/cronet/cronet_url_request.h"
#include "components/cronet/native/generated/cronet.idl_impl_interface.h"

namespace net {
enum LoadState;
}  // namespace net

namespace cronet {

class Cronet_EngineImpl;
class Cronet_UploadDataSinkImpl;

// Implementation of Cronet_UrlRequest that uses CronetContext.
class Cronet_UrlRequestImpl : public Cronet_UrlRequest {
 public:
  Cronet_UrlRequestImpl();

  Cronet_UrlRequestImpl(const Cronet_UrlRequestImpl&) = delete;
  Cronet_UrlRequestImpl& operator=(const Cronet_UrlRequestImpl&) = delete;

  ~Cronet_UrlRequestImpl() override;

  // Cronet_UrlRequest
  Cronet_RESULT InitWithParams(Cronet_EnginePtr engine,
                               Cronet_String url,
                               Cronet_UrlRequestParamsPtr params,
                               Cronet_UrlRequestCallbackPtr callback,
                               Cronet_ExecutorPtr executor) override;
  Cronet_RESULT Start() override;
  Cronet_RESULT FollowRedirect() override;
  Cronet_RESULT Read(Cronet_BufferPtr buffer) override;
  void Cancel() override;
  bool IsDone() override;
  void GetStatus(Cronet_UrlRequestStatusListenerPtr listener) override;

  // Upload data provider has reported error while reading or rewinding
  // so request must fail.
  void OnUploadDataProviderError(const std::string& error_message);

 private:
  class NetworkTasks;

  // Return |true| if request has started and is now done.
  // Must be called under |lock_| held.
  bool IsDoneLocked() const SHARED_LOCKS_REQUIRED(lock_);

  // Helper method to set final status of CronetUrlRequest and clean up the
  // native request adapter. Returns true if request is already done, false
  // request is not done and is destroyed.
  bool DestroyRequestUnlessDone(
      Cronet_RequestFinishedInfo_FINISHED_REASON finished_reason);

  // Helper method to set final status of CronetUrlRequest and clean up the
  // native request adapter. Returns true if request is already done, false
  // request is not done and is destroyed. Must be called under |lock_| held.
  bool DestroyRequestUnlessDoneLocked(
      Cronet_RequestFinishedInfo_FINISHED_REASON finished_reason)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Helper method to post |task| to the |executor_|.
  void PostTaskToExecutor(base::OnceClosure task);

  // Helper methods to invoke application |callback_|.
  void InvokeCallbackOnRedirectReceived(const std::string& new_location);
  void InvokeCallbackOnResponseStarted();
  void InvokeCallbackOnReadCompleted(
      std::unique_ptr<Cronet_Buffer> cronet_buffer,
      int bytes_read);
  void InvokeCallbackOnSucceeded();
  void InvokeCallbackOnFailed();
  void InvokeCallbackOnCanceled();

  // Runs InvokeCallbackOnFailed() on the client executor.
  void PostCallbackOnFailedToExecutor();

  // Invoke all members of |status_listeners_|. Should be called prior to
  // invoking a final callback. Once a final callback has been called, |this|
  // and |executor_| may be deleted and so the callbacks cannot be issued.
  void InvokeAllStatusListeners();

  // Reports metrics if metrics were collected, otherwise does nothing. This
  // method should only be called once on Callback's executor thread and before
  // Callback's OnSucceeded, OnFailed and OnCanceled.
  //
  // Adds |finished_reason| to the reported RequestFinishedInfo. Also passes
  // pointers to |response_info_| and |error_|.
  //
  // Also, the field |annotations_| is moved into the RequestFinishedInfo.
  //
  // |finished_reason|: Success / fail / cancel status of request.
  void MaybeReportMetrics(
      Cronet_RequestFinishedInfo_FINISHED_REASON finished_reason);

  // Synchronize access to |request_| and other objects below from different
  // threads.
  base::Lock lock_;
  // NetworkTask object lives on the network thread. Owned by |request_|.
  // Outlives this.
  raw_ptr<NetworkTasks, AcrossTasksDanglingUntriaged> network_tasks_
      GUARDED_BY(lock_) = nullptr;
  // Cronet URLRequest used for this operation.
  raw_ptr<CronetURLRequest, AcrossTasksDanglingUntriaged> request_
      GUARDED_BY(lock_) = nullptr;
  bool started_ GUARDED_BY(lock_) = false;
  bool waiting_on_redirect_ GUARDED_BY(lock_) = false;
  bool waiting_on_read_ GUARDED_BY(lock_) = false;
  // Set of status_listeners_ that have not yet been called back.
  std::unordered_multiset<Cronet_UrlRequestStatusListenerPtr> status_listeners_
      GUARDED_BY(lock_);

  // Report containing metrics and other information to send to attached
  // RequestFinishedListener(s). A nullptr value indicates that metrics haven't
  // been collected.
  //
  // Ownership is shared since we guarantee that the RequestFinishedInfo will
  // be valid if its UrlRequest isn't destroyed. We also guarantee that it's
  // valid in RequestFinishedListener.OnRequestFinished() even if the
  // UrlRequest is destroyed (and furthermore, each listener finishes at
  // different times).
  //
  // NOTE: this field isn't protected by |lock_| since we pass this field as a
  // unowned pointer to OnRequestFinished(). The pointee of this field cannot
  // be updated after that call is made.
  scoped_refptr<base::RefCountedData<Cronet_RequestFinishedInfo>>
      request_finished_info_;

  // Annotations passed via UrlRequestParams.annotations. These annotations
  // aren't used by Cronet itself -- they're just moved into the
  // RequestFinishedInfo passed to RequestFinishedInfoListener instances.
  std::vector<Cronet_RawDataPtr> annotations_;

  // Optional; allows a listener to receive request info and stats.
  //
  // A nullptr value indicates that there is no RequestFinishedInfo listener
  // specified for the request (however, the Engine may have additional
  // listeners -- Engine listeners apply to all its UrlRequests).
  //
  // Owned by the app -- must outlive this UrlRequest.
  Cronet_RequestFinishedInfoListenerPtr request_finished_listener_ = nullptr;

  // Executor upon which |request_finished_listener_| will run. If
  // |request_finished_listener_| is not nullptr, this won't be nullptr either.
  //
  // Owned by the app -- must outlive this UrlRequest.
  Cronet_ExecutorPtr request_finished_executor_ = nullptr;

  // Response info updated by callback with number of bytes received. May be
  // nullptr, if no response has been received.
  //
  // Ownership is shared since we guarantee that the UrlResponseInfo will
  // be valid if its UrlRequest isn't destroyed. We also guarantee that it's
  // valid in RequestFinishedListener.OnRequestFinished() even if the
  // UrlRequest is destroyed (and furthermore, each listener finishes at
  // different times).
  //
  // NOTE: the synchronization of this field is complex -- it can't be
  // completely protected by |lock_| since we pass this field as a unowned
  // pointer to OnSucceed(), OnFailed(), and OnCanceled(). The pointee of this
  // field cannot be updated after one of those callback calls is made.
  scoped_refptr<base::RefCountedData<Cronet_UrlResponseInfo>> response_info_;

  // The error reported by request. May be nullptr if no error has occurred.
  //
  // Ownership is shared since we guarantee that the Error will be valid if its
  // UrlRequest isn't destroyed. We also guarantee that it's valid in
  // RequestFinishedListener.OnRequestFinished() even if the UrlRequest is
  // destroyed (and furthermore, each listener finishes at different times).
  //
  // NOTE: the synchronization of this field is complex -- it can't be
  // completely protected by |lock_| since we pass this field as an unowned
  // pointer to OnSucceed(), OnFailed(), and OnCanceled(). The pointee of this
  // field cannot be updated after one of those callback calls is made.
  scoped_refptr<base::RefCountedData<Cronet_Error>> error_;

  // The upload data stream if specified.
  std::unique_ptr<Cronet_UploadDataSinkImpl> upload_data_sink_;

  // Application callback interface, used, but not owned, by |this|.
  Cronet_UrlRequestCallbackPtr callback_ = nullptr;
  // Executor for application callback, used, but not owned, by |this|.
  Cronet_ExecutorPtr executor_ = nullptr;

  // Cronet Engine used to run network operations. Not owned, accessed from
  // client thread. Must outlive this request.
  raw_ptr<Cronet_EngineImpl> engine_ = nullptr;

#if DCHECK_IS_ON()
  // Event indicating Executor is properly destroying Runnables.
  base::WaitableEvent runnable_destroyed_;
#endif  // DCHECK_IS_ON()
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_NATIVE_URL_REQUEST_H_
