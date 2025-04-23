// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_NEW_TAB_FOOTER_FOOTER_WEB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_NEW_TAB_FOOTER_FOOTER_WEB_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/webview/webview.h"

namespace views {
class View;
}  // namespace views

namespace new_tab_footer {

// NewTabFooterWebView is used to present the WebContents of the New Tab Footer.
// TODO(crbug.com/409054648): Embed footer WebContents.
class NewTabFooterWebView : public views::WebView {
  METADATA_HEADER(NewTabFooterWebView, views::WebView)

 public:
  explicit NewTabFooterWebView(content::BrowserContext* browser_context,
                               views::View* base_view);
  NewTabFooterWebView(const NewTabFooterWebView&) = delete;
  NewTabFooterWebView& operator=(const NewTabFooterWebView&) = delete;
  ~NewTabFooterWebView() override;

  void Reposition();

 private:
  raw_ptr<views::View> base_view_;
};

}  // namespace new_tab_footer

#endif  // CHROME_BROWSER_UI_VIEWS_NEW_TAB_FOOTER_FOOTER_WEB_VIEW_H_
