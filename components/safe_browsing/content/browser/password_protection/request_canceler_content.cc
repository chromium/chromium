// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/password_protection/request_canceler_content.h"

namespace safe_browsing {

std::unique_ptr<RequestCanceler> RequestCanceler::CreateRequestCanceler(
    base::WeakPtr<CancelableRequest> request,
    content::WebContents* web_contents) {
  return std::make_unique<RequestCancelerContent>(request, web_contents);
}

RequestCancelerContent::RequestCancelerContent(
    base::WeakPtr<CancelableRequest> request,
    content::WebContents* web_contents)
    : RequestCanceler(request), content::WebContentsObserver(web_contents) {}

void RequestCancelerContent::WebContentsDestroyed() {
  request_->Cancel(/*timed_out=*/false);
}

}  // namespace safe_browsing
