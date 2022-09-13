// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check.h"
#include "base/synchronization/waitable_event.h"
#include "cronet_c.h"

#ifndef COMPONENTS_CRONET_NATIVE_TEST_TEST_REQUEST_FINISHED_INFO_LISTENER_H_
#define COMPONENTS_CRONET_NATIVE_TEST_TEST_REQUEST_FINISHED_INFO_LISTENER_H_

namespace cronet {
namespace test {

// A RequestFinishedInfoListener implementation that allows waiting for and
// accessing callback results from tests.
//
// Note that the RequestFinishedInfo for a request is shared-owned by its
// UrlRequest and the code calling the listeners.
class TestRequestFinishedInfoListener {
 public:
  // Create a listener that can be registered with Cronet.
  //
  // The listener deletes itself when OnRequestFinished() is run.
  Cronet_RequestFinishedInfoListenerPtr CreateRequestFinishedListener();

  // Wait until a listener created with CreateRequestFinishedListener() runs
  // OnRequestFinished().
  void WaitForDone();

  Cronet_RequestFinishedInfoPtr request_finished_info() {
    CHECK(done_.IsSignaled());
    return request_finished_info_;
  }

  Cronet_UrlResponseInfoPtr url_response_info() {
    CHECK(done_.IsSignaled());
    return url_response_info_;
  }

  Cronet_ErrorPtr error() {
    CHECK(done_.IsSignaled());
    return error_;
  }

 private:
  static TestRequestFinishedInfoListener* GetThis(
      Cronet_RequestFinishedInfoListenerPtr self);

  // Implementation of Cronet_RequestFinishedInfoListener methods.
  static void OnRequestFinished(
      Cronet_RequestFinishedInfoListenerPtr self,
      Cronet_RequestFinishedInfoPtr request_finished_info,
      Cronet_UrlResponseInfoPtr url_response_info,
      Cronet_ErrorPtr error);

  virtual void OnRequestFinished(
      Cronet_RequestFinishedInfoPtr request_finished_info,
      Cronet_UrlResponseInfoPtr url_response_info,
      Cronet_ErrorPtr error);

  // RequestFinishedInfo from the request -- will be set when the listener is
  // called, which only happens if there are metrics to report. Won't be
  // destroyed if the UrlRequest object hasn't been destroyed.
  Cronet_RequestFinishedInfoPtr request_finished_info_ = nullptr;

  // UrlResponseInfo from the request -- will be set when the listener is
  // called, which only happens if there are metrics to report. Won't be
  // destroyed if the UrlRequest object hasn't been destroyed.
  Cronet_UrlResponseInfoPtr url_response_info_ = nullptr;

  // Error from the request -- will be will be set when the listener is called,
  // which only happens if there are metrics to report. Won't be destroyed if
  // the UrlRequest object hasn't been destroyed.
  Cronet_ErrorPtr error_ = nullptr;

  // Signaled by OnRequestFinished() on a listener created by
  // CreateRequestFinishedListener().
  base::WaitableEvent done_;
};

}  // namespace test
}  // namespace cronet

#endif  // COMPONENTS_CRONET_NATIVE_TEST_TEST_REQUEST_FINISHED_INFO_LISTENER_H_
