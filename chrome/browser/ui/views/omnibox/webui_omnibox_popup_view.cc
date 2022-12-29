// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/webui_omnibox_popup_view.h"

#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/realbox/realbox_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"

WebUIOmniboxPopupView::WebUIOmniboxPopupView(
    content::BrowserContext* browser_context)
    : views::WebView(browser_context) {
  // TODO(crbug.com/1396174): Should be dynamically sized based on WebContents.
  SetPreferredSize(gfx::Size(640, 480));
  LoadInitialURL(GURL(chrome::kChromeUIOmniboxPopupURL));
}

void WebUIOmniboxPopupView::OnSelectedLineChanged(size_t old_selected_line,
                                                  size_t new_selected_line) {
  RealboxHandler* handler = GetWebUIHandler();
  if (handler) {
    handler->SelectMatchAtLine(old_selected_line, new_selected_line);
  }
}

RealboxHandler* WebUIOmniboxPopupView::GetWebUIHandler() {
  OmniboxPopupUI* omnibox_popup_ui = static_cast<OmniboxPopupUI*>(
      GetWebContents()->GetWebUI()->GetController());
  return omnibox_popup_ui->webui_handler();
}

BEGIN_METADATA(WebUIOmniboxPopupView, views::WebView)
END_METADATA
