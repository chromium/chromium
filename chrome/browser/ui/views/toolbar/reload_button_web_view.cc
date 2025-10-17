// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/reload_button_web_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"

ReloadButtonWebView::ReloadButtonWebView(Profile* profile) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  auto web_view = std::make_unique<views::WebView>(profile);
  web_view->LoadInitialURL(GURL(chrome::kChromeUIReloadButtonURL));
  const int size = GetLayoutConstant(LayoutConstant::TOOLBAR_BUTTON_HEIGHT);
  web_view->SetPreferredSize(gfx::Size(size, size));
  web_view->GetWebContents()->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);
  AddChildView(std::move(web_view));
}

ReloadButtonWebView::~ReloadButtonWebView() = default;

BEGIN_METADATA(ReloadButtonWebView)
END_METADATA
