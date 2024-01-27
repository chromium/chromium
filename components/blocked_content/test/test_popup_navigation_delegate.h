// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BLOCKED_CONTENT_TEST_TEST_POPUP_NAVIGATION_DELEGATE_H_
#define COMPONENTS_BLOCKED_CONTENT_TEST_TEST_POPUP_NAVIGATION_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "components/blocked_content/popup_navigation_delegate.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "url/gurl.h"

namespace blocked_content {

// Test delegate which stores results of calls in a ResultHolder.
class TestPopupNavigationDelegate : public PopupNavigationDelegate {
 public:
  // Struct used to hold results from calls on TestPopupNavigationDelegate since
  // the delegate will be destroyed on calls to ShowBlockedPopup().
  struct ResultHolder {
    ResultHolder();
    ~ResultHolder();

    bool did_navigate = false;
    blink::mojom::WindowFeatures navigation_window_features;
    std::optional<WindowOpenDisposition> navigation_disposition;
    int total_popups_blocked_on_page = 0;
  };

  TestPopupNavigationDelegate(const GURL& url, ResultHolder* result_holder);

  // PopupNavigationDelegate:
  content::RenderFrameHost* GetOpener() override;
  bool GetOriginalUserGesture() override;
  GURL GetURL() override;
  NavigateResult NavigateWithGesture(
      const blink::mojom::WindowFeatures& window_features,
      std::optional<WindowOpenDisposition> updated_disposition) override;
  void OnPopupBlocked(content::WebContents* web_contents,
                      int total_popups_blocked_on_page) override;

 private:
  const GURL url_;
  raw_ptr<ResultHolder> result_holder_;
};

}  // namespace blocked_content

#endif  // COMPONENTS_BLOCKED_CONTENT_TEST_TEST_POPUP_NAVIGATION_DELEGATE_H_
