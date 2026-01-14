// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TOOLBAR_WEB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TOOLBAR_WEB_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/views/toolbar/webui_reload_control.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class WebUIToolbarUI;
class BrowserWindowInterface;

namespace views {
class WebView;
}  // namespace views

// A view that displays the toolbar as a WebView.
class WebUIToolbarWebView : public views::View,
                            public content::WebContentsDelegate,
                            public content::WebContentsObserver {
  METADATA_HEADER(WebUIToolbarWebView, views::View)

 public:
  WebUIToolbarWebView(BrowserWindowInterface* browser,
                      chrome::BrowserCommandController* controller);
  WebUIToolbarWebView(const WebUIToolbarWebView&) = delete;
  WebUIToolbarWebView& operator=(const WebUIToolbarWebView&) = delete;
  ~WebUIToolbarWebView() override;

  ReloadControl* GetReloadControl();

  // views::View:
  void AddedToWidget() override;

  // content::WebContentsDelegate:
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;

  // content::WebContentsObserver:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DidFirstVisuallyNonEmptyPaint() override;

  void SetDidFirstNonEmptyPaintCallbackForTesting(base::OnceClosure callback);

 private:
  friend WebUIReloadControl;

  chrome::BrowserCommandController* controller() { return controller_; }
  WebUIToolbarUI* GetWebUIToolbarUI();

  raw_ptr<views::WebView> web_view_ = nullptr;
  const raw_ptr<BrowserWindowInterface> browser_;
  const raw_ptr<chrome::BrowserCommandController> controller_;
  WebUIReloadControl reload_control_;
  base::OnceClosure did_first_non_empty_paint_callback_;
  bool has_finished_first_non_empty_paint_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TOOLBAR_WEB_VIEW_H_
