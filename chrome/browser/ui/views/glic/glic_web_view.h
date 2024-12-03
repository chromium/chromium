// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_WEB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_WEB_VIEW_H_

#include "ui/views/controls/webview/webview.h"

namespace glic {

class GlicWebView : public views::WebView {
  METADATA_HEADER(GlicWebView, views::WebView)
 public:
  explicit GlicWebView(content::BrowserContext* browser_context);
  ~GlicWebView() override;

  // Overridden from content::WebContentsDelegate:
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
};

}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_WEB_VIEW_H_
