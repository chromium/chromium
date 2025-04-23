// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/new_tab_footer/footer_web_view.h"

#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/background.h"

namespace {
constexpr int kFooterHeight = 56;
}

namespace new_tab_footer {

NewTabFooterWebView::NewTabFooterWebView(
    content::BrowserContext* browser_context,
    views::View* base_view)
    : views::WebView(browser_context), base_view_(base_view) {
  base_view_->AddChildView(this);
  SetPaintToLayer();
  // TODO(crbug.com/409056427): Add conditions for visibility.
  SetVisible(true);
  // TODO(crbug.com/409054648): Set background color in the hosted WebUI
  // instead.
  SetBackground(views::CreateSolidBackground(SK_ColorBLACK));
}

void NewTabFooterWebView::Reposition() {
  gfx::Rect base_view_bounds = base_view_->GetLocalBounds();
  // TODO(crbug.com/409058788): Resize the base_view_ so that the footer view
  // can show inside the base view, then set y equal to kFooterHeight -
  // base_view_bounds.height().
  SetBounds(base_view_bounds.x(), base_view_bounds.height(),
            base_view_bounds.width(), kFooterHeight);
}

NewTabFooterWebView::~NewTabFooterWebView() = default;

BEGIN_METADATA(NewTabFooterWebView)
END_METADATA

}  // namespace new_tab_footer
