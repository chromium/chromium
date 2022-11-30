// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_ANDROID_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_view_delegate.h"

namespace content {
class WebContents;
}

// A Chrome specific class that extends WebContentsViewAndroid with features
// like context menus.
class ChromeWebContentsViewDelegateAndroid
    : public content::WebContentsViewDelegate {
 public:
  explicit ChromeWebContentsViewDelegateAndroid(
      content::WebContents* web_contents);

  ChromeWebContentsViewDelegateAndroid(
      const ChromeWebContentsViewDelegateAndroid&) = delete;
  ChromeWebContentsViewDelegateAndroid& operator=(
      const ChromeWebContentsViewDelegateAndroid&) = delete;

  ~ChromeWebContentsViewDelegateAndroid() override;

  // WebContentsViewDelegate:
  void ShowContextMenu(content::RenderFrameHost& render_frame_host,
                       const content::ContextMenuParams& params) override;

  // WebContentsViewDelegate:
  void DismissContextMenu() override;

  // WebContentsViewDelegate:
  content::WebDragDestDelegate* GetDragDestDelegate() override;

 private:
  // The WebContents that owns the view and this delegate transitively.
  raw_ptr<content::WebContents> web_contents_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_ANDROID_H_
