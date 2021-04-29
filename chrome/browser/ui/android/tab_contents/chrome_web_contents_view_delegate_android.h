// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_ANDROID_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
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
  ~ChromeWebContentsViewDelegateAndroid() override;

  // WebContentsViewDelegate:
  void ShowContextMenu(content::RenderFrameHost* render_frame_host,
                       const content::ContextMenuParams& params) override;

  // WebContentsViewDelegate:
  content::WebDragDestDelegate* GetDragDestDelegate() override;

 private:
  // The WebContents that owns the view and this delegate transitively.
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(ChromeWebContentsViewDelegateAndroid);
};

#endif  // CHROME_BROWSER_UI_ANDROID_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_ANDROID_H_
