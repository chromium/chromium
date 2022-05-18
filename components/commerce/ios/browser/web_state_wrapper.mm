// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/ios/browser/web_state_wrapper.h"

#include "ios/web/public/browser_state.h"
#include "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace commerce {

WebStateWrapper::WebStateWrapper(web::WebState* web_state)
    : web_state_(web_state) {}

const GURL& WebStateWrapper::GetLastCommittedURL() {
  if (!web_state_)
    return std::move(GURL());

  return web_state_->GetLastCommittedURL();
}

bool WebStateWrapper::IsOffTheRecord() {
  if (!web_state_ || !web_state_->GetBrowserState())
    return false;

  return web_state_->GetBrowserState()->IsOffTheRecord();
}

void WebStateWrapper::ClearWebStatePointer() {
  web_state_ = nullptr;
}

}  // namespace commerce
