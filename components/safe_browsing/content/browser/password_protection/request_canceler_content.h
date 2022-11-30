// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_PASSWORD_PROTECTION_REQUEST_CANCELER_CONTENT_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_PASSWORD_PROTECTION_REQUEST_CANCELER_CONTENT_H_

#include "components/safe_browsing/core/browser/password_protection/request_canceler.h"
#include "content/public/browser/web_contents_observer.h"

namespace safe_browsing {

class RequestCancelerContent : public RequestCanceler,
                               public content::WebContentsObserver {
 public:
  RequestCancelerContent(base::WeakPtr<CancelableRequest> request,
                         content::WebContents* web_contents);
  ~RequestCancelerContent() override = default;

 private:
  // WebContentsObserver implementation
  void WebContentsDestroyed() override;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_PASSWORD_PROTECTION_REQUEST_CANCELER_CONTENT_H_
