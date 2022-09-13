// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/native/test/test_request_finished_info_listener.h"

#include "base/check.h"

namespace cronet {
namespace test {

Cronet_RequestFinishedInfoListenerPtr
TestRequestFinishedInfoListener::CreateRequestFinishedListener() {
  auto* listener = Cronet_RequestFinishedInfoListener_CreateWith(
      TestRequestFinishedInfoListener::OnRequestFinished);
  Cronet_RequestFinishedInfoListener_SetClientContext(listener, this);
  return listener;
}

void TestRequestFinishedInfoListener::WaitForDone() {
  done_.Wait();
}

/* static */
TestRequestFinishedInfoListener* TestRequestFinishedInfoListener::GetThis(
    Cronet_RequestFinishedInfoListenerPtr self) {
  CHECK(self);
  return static_cast<TestRequestFinishedInfoListener*>(
      Cronet_RequestFinishedInfoListener_GetClientContext(self));
}

/* static */
void TestRequestFinishedInfoListener::OnRequestFinished(
    Cronet_RequestFinishedInfoListenerPtr self,
    Cronet_RequestFinishedInfoPtr request_finished_info,
    Cronet_UrlResponseInfoPtr url_response_info,
    Cronet_ErrorPtr error) {
  GetThis(self)->OnRequestFinished(request_finished_info, url_response_info,
                                   error);
  Cronet_RequestFinishedInfoListener_Destroy(self);
}

void TestRequestFinishedInfoListener::OnRequestFinished(
    Cronet_RequestFinishedInfoPtr request_finished_info,
    Cronet_UrlResponseInfoPtr url_response_info,
    Cronet_ErrorPtr error) {
  request_finished_info_ = request_finished_info;
  url_response_info_ = url_response_info;
  error_ = error;
  done_.Signal();
}

}  // namespace test
}  // namespace cronet
