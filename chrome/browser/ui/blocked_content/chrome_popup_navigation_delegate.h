// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLOCKED_CONTENT_CHROME_POPUP_NAVIGATION_DELEGATE_H_
#define CHROME_BROWSER_UI_BLOCKED_CONTENT_CHROME_POPUP_NAVIGATION_DELEGATE_H_

#include "chrome/browser/ui/browser_navigator_params.h"
#include "components/blocked_content/popup_navigation_delegate.h"

class ChromePopupNavigationDelegate
    : public blocked_content::PopupNavigationDelegate {
 public:
  explicit ChromePopupNavigationDelegate(NavigateParams params);

  // blocked_content::PopupNavigationDelegate:
  content::RenderFrameHost* GetOpener() override;
  bool GetOriginalUserGesture() override;
  GURL GetURL() override;
  NavigateResult NavigateWithGesture(
      const blink::mojom::WindowFeatures& window_features,
      std::optional<WindowOpenDisposition> updated_disposition) override;
  void OnPopupBlocked(content::WebContents* web_contents,
                      int total_popups_blocked_on_page) override;

  NavigateParams* nav_params() { return &params_; }

 private:
  NavigateParams params_;
  bool original_user_gesture_;
};

#endif  // CHROME_BROWSER_UI_BLOCKED_CONTENT_CHROME_POPUP_NAVIGATION_DELEGATE_H_
