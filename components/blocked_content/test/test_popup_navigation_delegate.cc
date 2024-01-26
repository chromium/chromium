// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blocked_content/test/test_popup_navigation_delegate.h"

namespace blocked_content {

TestPopupNavigationDelegate::ResultHolder::ResultHolder() = default;

TestPopupNavigationDelegate::ResultHolder::~ResultHolder() = default;

TestPopupNavigationDelegate::TestPopupNavigationDelegate(
    const GURL& url,
    ResultHolder* result_holder)
    : url_(url), result_holder_(result_holder) {}

content::RenderFrameHost* TestPopupNavigationDelegate::GetOpener() {
  return nullptr;
}

bool TestPopupNavigationDelegate::GetOriginalUserGesture() {
  return true;
}

GURL TestPopupNavigationDelegate::GetURL() {
  return url_;
}

PopupNavigationDelegate::NavigateResult
TestPopupNavigationDelegate::NavigateWithGesture(
    const blink::mojom::WindowFeatures& window_features,
    std::optional<WindowOpenDisposition> updated_disposition) {
  if (result_holder_) {
    result_holder_->did_navigate = true;
    result_holder_->navigation_window_features = window_features;
    result_holder_->navigation_disposition = updated_disposition;
  }
  return NavigateResult();
}

void TestPopupNavigationDelegate::OnPopupBlocked(
    content::WebContents* web_contents,
    int total_popups_blocked_on_page) {
  if (result_holder_)
    result_holder_->total_popups_blocked_on_page = total_popups_blocked_on_page;
}

}  // namespace blocked_content
