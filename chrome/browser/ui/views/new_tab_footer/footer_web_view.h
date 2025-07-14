// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_NEW_TAB_FOOTER_FOOTER_WEB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_NEW_TAB_FOOTER_FOOTER_WEB_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"

namespace ui {
class MenuModel;
}  // namespace ui

namespace views {
class MenuRunner;
}  // namespace views

class BrowserWindowInterface;
class WebUIContentsWrapper;

DECLARE_ELEMENT_IDENTIFIER_VALUE(kNtpFooterId);

namespace new_tab_footer {

// NewTabFooterWebView is used to present the WebContents of the New Tab Footer.
class NewTabFooterWebView : public views::WebView,
                            public WebUIContentsWrapper::Host {
  METADATA_HEADER(NewTabFooterWebView, views::WebView)

 public:
  explicit NewTabFooterWebView(BrowserWindowInterface* browser);
  NewTabFooterWebView(const NewTabFooterWebView&) = delete;
  NewTabFooterWebView& operator=(const NewTabFooterWebView&) = delete;
  ~NewTabFooterWebView() override;

  void ShowUI(base::TimeTicks load_start);

  // WebUIContentsWrapper::Host:
  void ShowUI() override;
  void CloseUI() override;
  void ShowCustomContextMenu(
      gfx::Point point,
      std::unique_ptr<ui::MenuModel> menu_model) override;
  void HideCustomContextMenu() override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;

 private:
  std::unique_ptr<views::MenuRunner> context_menu_runner_;
  std::unique_ptr<ui::MenuModel> context_menu_model_;
  // Processes keyboard events not handled by the renderer.
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;
  raw_ptr<BrowserWindowInterface> browser_;
  std::unique_ptr<WebUIContentsWrapper> contents_wrapper_;

  base::WeakPtrFactory<NewTabFooterWebView> weak_factory_{this};
};

}  // namespace new_tab_footer

#endif  // CHROME_BROWSER_UI_VIEWS_NEW_TAB_FOOTER_FOOTER_WEB_VIEW_H_
