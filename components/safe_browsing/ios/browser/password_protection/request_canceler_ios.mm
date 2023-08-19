// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/ios/browser/password_protection/request_canceler_ios.h"

#import "ios/web/public/web_state.h"

namespace safe_browsing {

std::unique_ptr<RequestCanceler> RequestCanceler::CreateRequestCanceler(
    base::WeakPtr<CancelableRequest> request,
    web::WebState* web_state) {
  return std::make_unique<RequestCancelerIOS>(request, web_state);
}

RequestCancelerIOS::RequestCancelerIOS(base::WeakPtr<CancelableRequest> request,
                                       web::WebState* web_state)
    : RequestCanceler(request), web_state_(web_state) {
  web_state_->AddObserver(this);
}

RequestCancelerIOS::~RequestCancelerIOS() {
  if (web_state_)
    web_state_->RemoveObserver(this);
}

void RequestCancelerIOS::WebStateDestroyed(web::WebState* web_state) {
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
  request_->Cancel(/*timed_out=*/false);
}

}  // namespace safe_browsing
