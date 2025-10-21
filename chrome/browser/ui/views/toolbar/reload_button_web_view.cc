// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/reload_button_web_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/webui/reload_button/reload_button_ui.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"

ReloadButtonWebView::ReloadButtonWebView(BrowserWindowInterface* browser) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  auto web_view = std::make_unique<views::WebView>(browser->GetProfile());
  web_view->LoadInitialURL(GURL(chrome::kChromeUIReloadButtonURL));
  const int size = GetLayoutConstant(LayoutConstant::TOOLBAR_BUTTON_HEIGHT);
  web_view->SetPreferredSize(gfx::Size(size, size));
  auto* web_contents = web_view->GetWebContents();
  webui::SetBrowserWindowInterface(web_contents, browser);
  web_contents->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);
  reload_button_ui_ =
      web_contents->GetWebUI()->GetController()->GetAs<ReloadButtonUI>();
  AddChildView(std::move(web_view));
}

ReloadButtonWebView::~ReloadButtonWebView() = default;

void ReloadButtonWebView::ChangeMode(ReloadControl::Mode mode, bool force) {
  CHECK(reload_button_ui_);
  reload_button_ui_->SetLoadingState(mode == ReloadControl::Mode::kStop, force);
}

views::View* ReloadButtonWebView::GetAsViewClassForTesting() {
  return this;
}

BEGIN_METADATA(ReloadButtonWebView)
END_METADATA
