// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"

#include "chrome/browser/page_load_metrics/page_load_metrics_initialize.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/waap/initial_web_ui_manager.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

WebUIToolbarWebView::WebUIToolbarWebView(
    BrowserWindowInterface* browser,
    chrome::BrowserCommandController* controller)
    : browser_(browser), controller_(controller), reload_control_(this) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  auto web_view = std::make_unique<views::WebView>(browser->GetProfile());
  const GURL kUrl(chrome::kChromeUIWebUIToolbarURL);
  auto* web_contents = web_view->GetWebContents(kUrl);
  // PLM has to be initialized before loading the URL.
  InitializePageLoadMetricsForWebContents(web_contents);

  const int size = GetLayoutConstant(LayoutConstant::kToolbarButtonHeight);
  web_view->SetPreferredSize(gfx::Size(size, size));
  web_contents->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);
  web_view->SetID(VIEW_ID_RELOAD_BUTTON);

  // We must save the pointer to the WebView so we can load the URL after the
  // view is added to a widget.
  web_view_ = AddChildView(std::move(web_view));
  web_contents->SetDelegate(this);
  Observe(web_contents);

  // The accessibility and tooltip attributes are handled by the WebUI.
  SetProperty(views::kElementIdentifierKey, kReloadButtonElementId);
}

WebUIToolbarWebView::~WebUIToolbarWebView() = default;

void WebUIToolbarWebView::AddedToWidget() {
  CHECK(web_view_);
  if (webui_toolbar_ui_) {
    return;
  }
  // Ensure the browser window interface is associated with the WebContents
  // before the WebUI acts on it.
  webui::SetBrowserWindowInterface(web_view_->GetWebContents(), browser_);
  web_view_->LoadInitialURL(GURL(chrome::kChromeUIWebUIToolbarURL));
  webui_toolbar_ui_ = web_view_->GetWebContents()
                          ->GetWebUI()
                          ->GetController()
                          ->GetAs<WebUIToolbarUI>()
                          ->GetWeakPtr();
  reload_control_.Init();
}

bool WebUIToolbarWebView::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  gfx::Point screen_location = GetBoundsInScreen().origin();
  screen_location.Offset(params.x, params.y);

  // TODO(crbug.com/470955454): Dispatch context menu based on which context
  // menu was triggered.
  return reload_control_.HandleContextMenu(GetWidget(), screen_location,
                                           params);
}

void WebUIToolbarWebView::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  InitialWebUIManager::From(browser_)->OnWebUIToolbarLoaded();
}

ReloadControl* WebUIToolbarWebView::GetReloadControl() {
  return &reload_control_;
}

void WebUIToolbarWebView::DidFirstVisuallyNonEmptyPaint() {
  has_finished_first_non_empty_paint_ = true;
  if (did_first_non_empty_paint_callback_) {
    std::move(did_first_non_empty_paint_callback_).Run();
  }
}

void WebUIToolbarWebView::SetDidFirstNonEmptyPaintCallbackForTesting(
    base::OnceClosure callback) {
  if (callback.is_null()) {
    return;
  }
  if (has_finished_first_non_empty_paint_) {
    std::move(callback).Run();
    return;
  }
  did_first_non_empty_paint_callback_ = std::move(callback);
}

BEGIN_METADATA(WebUIToolbarWebView)
END_METADATA
