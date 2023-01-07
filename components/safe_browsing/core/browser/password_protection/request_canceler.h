// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_PASSWORD_PROTECTION_REQUEST_CANCELER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_PASSWORD_PROTECTION_REQUEST_CANCELER_H_

#include "base/memory/weak_ptr.h"

namespace content {
class WebContents;
}

namespace web {
class WebState;
}

namespace safe_browsing {

// An interface-only pure abstract base class for a request that can be
// canceled.
class CancelableRequest {
 public:
  virtual ~CancelableRequest() = 0;

  // Cancels this request. |timed_out| indicates if this cancellation is
  // due to timeout.
  virtual void Cancel(bool timed_out) = 0;
};

// An object that cancels a request when it is no longer valid.
class RequestCanceler {
 public:
  static std::unique_ptr<RequestCanceler> CreateRequestCanceler(
      base::WeakPtr<CancelableRequest> request,
      content::WebContents* web_contents);
  static std::unique_ptr<RequestCanceler> CreateRequestCanceler(
      base::WeakPtr<CancelableRequest> request,
      web::WebState* web_state);

  RequestCanceler(const RequestCanceler&) = delete;
  RequestCanceler& operator=(const RequestCanceler&) = delete;
  virtual ~RequestCanceler() = 0;

 protected:
  explicit RequestCanceler(base::WeakPtr<CancelableRequest> request);
  base::WeakPtr<CancelableRequest> request_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_PASSWORD_PROTECTION_REQUEST_CANCELER_H_
