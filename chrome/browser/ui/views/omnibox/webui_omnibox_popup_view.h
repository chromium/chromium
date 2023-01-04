// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_WEBUI_OMNIBOX_POPUP_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_WEBUI_OMNIBOX_POPUP_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/webview/webview.h"

class RealboxHandler;

// A WebView to display WebUI suggestions in the Views OmniboxPopupViewViews.
class WebUIOmniboxPopupView : public views::WebView {
 public:
  METADATA_HEADER(WebUIOmniboxPopupView);
  explicit WebUIOmniboxPopupView(content::BrowserContext* browser_context);
  WebUIOmniboxPopupView(const WebUIOmniboxPopupView&) = delete;
  WebUIOmniboxPopupView& operator=(const WebUIOmniboxPopupView&) = delete;
  ~WebUIOmniboxPopupView() override = default;

  // TODO(crbug.com/1396174): OmniboxPopupView and WebUIOmniboxPopupView could
  //  potentially implement the same interface which is better suited for WebUI.
  void OnSelectedLineChanged(size_t old_selected_line,
                             size_t new_selected_line);

 private:
  RealboxHandler* GetWebUIHandler();
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_WEBUI_OMNIBOX_POPUP_VIEW_H_
