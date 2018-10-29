// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_NATIVE_SAMPLE_SAMPLE_URL_REQUEST_CALLBACK_H_
#define COMPONENTS_CRONET_NATIVE_SAMPLE_SAMPLE_URL_REQUEST_CALLBACK_H_

// Cronet sample is expected to be used outside of Chromium infrastructure,
// and as such has to rely on STL directly instead of //base alternatives.
#include <future>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cronet_c.h"

// Sample implementation of Cronet_UrlRequestCallback interface using static
// methods to map C API into instance of C++ class.
class SampleUrlRequestCallback {
 public:
  SampleUrlRequestCallback();
  ~SampleUrlRequestCallback();

  // Gets Cronet_UrlRequestCallbackPtr implemented by |this|.
  Cronet_UrlRequestCallbackPtr GetUrlRequestCallback();

  // Waits until request is done.
  void WaitForDone() { is_done_.wait(); }

  // Returns error message if OnFailed callback is invoked.
  std::string last_error_message() const { return last_error_message_; }
  // Returns string representation of the received response.
  std::string response_as_string() const { return response_as_string_; }

 protected:
  void OnRedirectReceived(Cronet_UrlRequestPtr request,
                          Cronet_UrlResponseInfoPtr info,
                          Cronet_String newLocationUrl);

  void OnResponseStarted(Cronet_UrlRequestPtr request,
                         Cronet_UrlResponseInfoPtr info);

  void OnReadCompleted(Cronet_UrlRequestPtr request,
                       Cronet_UrlResponseInfoPtr info,
                       Cronet_BufferPtr buffer,
                       uint64_t bytes_read);

  void OnSucceeded(Cronet_UrlRequestPtr request,
                   Cronet_UrlResponseInfoPtr info);

  void OnFailed(Cronet_UrlRequestPtr request,
                Cronet_UrlResponseInfoPtr info,
                Cronet_ErrorPtr error);

  void OnCanceled(Cronet_UrlRequestPtr request, Cronet_UrlResponseInfoPtr info);

  void SignalDone(bool success) { done_with_success_.set_value(success); }

  static SampleUrlRequestCallback* GetThis(Cronet_UrlRequestCallbackPtr self);

  // Implementation of Cronet_UrlRequestCallback methods.
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

  // Error message copied from |error| if OnFailed callback is invoked.
  std::string last_error_message_;
  // Accumulated string representation of the received response.
  std::string response_as_string_;
  // Promise that is set when request is done.
  std::promise<bool> done_with_success_;
  // Future that is signalled when request is done.
  std::future<bool> is_done_ = done_with_success_.get_future();

  Cronet_UrlRequestCallbackPtr const callback_;
};

#endif  // COMPONENTS_CRONET_NATIVE_SAMPLE_SAMPLE_URL_REQUEST_CALLBACK_H_
