// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sample_url_request_callback.h"

#include <iostream>

SampleUrlRequestCallback::SampleUrlRequestCallback()
    : callback_(Cronet_UrlRequestCallback_CreateWith(
          SampleUrlRequestCallback::OnRedirectReceived,
          SampleUrlRequestCallback::OnResponseStarted,
          SampleUrlRequestCallback::OnReadCompleted,
          SampleUrlRequestCallback::OnSucceeded,
          SampleUrlRequestCallback::OnFailed,
          SampleUrlRequestCallback::OnCanceled)) {
  Cronet_UrlRequestCallback_SetClientContext(callback_, this);
}

SampleUrlRequestCallback::~SampleUrlRequestCallback() {
  Cronet_UrlRequestCallback_Destroy(callback_);
}

Cronet_UrlRequestCallbackPtr SampleUrlRequestCallback::GetUrlRequestCallback() {
  return callback_;
}

void SampleUrlRequestCallback::OnRedirectReceived(
    Cronet_UrlRequestPtr request,
    Cronet_UrlResponseInfoPtr info,
    Cronet_String newLocationUrl) {
  std::cout << "OnRedirectReceived called: " << newLocationUrl << std::endl;
  Cronet_UrlRequest_FollowRedirect(request);
}

void SampleUrlRequestCallback::OnResponseStarted(
    Cronet_UrlRequestPtr request,
    Cronet_UrlResponseInfoPtr info) {
  std::cout << "OnResponseStarted called." << std::endl;
  std::cout << "HTTP Status: "
            << Cronet_UrlResponseInfo_http_status_code_get(info) << " "
            << Cronet_UrlResponseInfo_http_status_text_get(info) << std::endl;
  // Create and allocate 32kb buffer.
  Cronet_BufferPtr buffer = Cronet_Buffer_Create();
  Cronet_Buffer_InitWithAlloc(buffer, 32 * 1024);
  // Started reading the response.
  Cronet_UrlRequest_Read(request, buffer);
}

void SampleUrlRequestCallback::OnReadCompleted(Cronet_UrlRequestPtr request,
                                               Cronet_UrlResponseInfoPtr info,
                                               Cronet_BufferPtr buffer,
                                               uint64_t bytes_read) {
  std::cout << "OnReadCompleted called: " << bytes_read << " bytes read."
            << std::endl;
  std::string last_read_data(
      reinterpret_cast<char*>(Cronet_Buffer_GetData(buffer)), bytes_read);
  response_as_string_ += last_read_data;
  // Continue reading the response.
  Cronet_UrlRequest_Read(request, buffer);
}

void SampleUrlRequestCallback::OnSucceeded(Cronet_UrlRequestPtr request,
                                           Cronet_UrlResponseInfoPtr info) {
  std::cout << "OnSucceeded called." << std::endl;
  SignalDone(true);
}

void SampleUrlRequestCallback::OnFailed(Cronet_UrlRequestPtr request,
                                        Cronet_UrlResponseInfoPtr info,
                                        Cronet_ErrorPtr error) {
  std::cout << "OnFailed called: " << Cronet_Error_message_get(error)
            << std::endl;
  last_error_message_ = Cronet_Error_message_get(error);
  SignalDone(false);
}

void SampleUrlRequestCallback::OnCanceled(Cronet_UrlRequestPtr request,
                                          Cronet_UrlResponseInfoPtr info) {
  std::cout << "OnCanceled called." << std::endl;
  SignalDone(false);
}

/* static */
SampleUrlRequestCallback* SampleUrlRequestCallback::GetThis(
    Cronet_UrlRequestCallbackPtr self) {
  return static_cast<SampleUrlRequestCallback*>(
      Cronet_UrlRequestCallback_GetClientContext(self));
}

/* static */
void SampleUrlRequestCallback::OnRedirectReceived(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_UrlRequestPtr request,
    Cronet_UrlResponseInfoPtr info,
    Cronet_String newLocationUrl) {
  GetThis(self)->OnRedirectReceived(request, info, newLocationUrl);
}

/* static */
void SampleUrlRequestCallback::OnResponseStarted(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_UrlRequestPtr request,
    Cronet_UrlResponseInfoPtr info) {
  GetThis(self)->OnResponseStarted(request, info);
}

/* static */
void SampleUrlRequestCallback::OnReadCompleted(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_UrlRequestPtr request,
    Cronet_UrlResponseInfoPtr info,
    Cronet_BufferPtr buffer,
    uint64_t bytesRead) {
  GetThis(self)->OnReadCompleted(request, info, buffer, bytesRead);
}

/* static */
void SampleUrlRequestCallback::OnSucceeded(Cronet_UrlRequestCallbackPtr self,
                                           Cronet_UrlRequestPtr request,
                                           Cronet_UrlResponseInfoPtr info) {
  GetThis(self)->OnSucceeded(request, info);
}

/* static */
void SampleUrlRequestCallback::OnFailed(Cronet_UrlRequestCallbackPtr self,
                                        Cronet_UrlRequestPtr request,
                                        Cronet_UrlResponseInfoPtr info,
                                        Cronet_ErrorPtr error) {
  GetThis(self)->OnFailed(request, info, error);
}

/* static */
void SampleUrlRequestCallback::OnCanceled(Cronet_UrlRequestCallbackPtr self,
                                          Cronet_UrlRequestPtr request,
                                          Cronet_UrlResponseInfoPtr info) {
  GetThis(self)->OnCanceled(request, info);
}
