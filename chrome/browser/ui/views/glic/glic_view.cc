// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/glic/glic_view.h"

#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

namespace glic {

GlicView::GlicView(Profile* profile, const gfx::Size& initial_size) {
  auto web_view = std::make_unique<views::WebView>(profile);
  web_view->SetSize(initial_size);
  web_view->LoadInitialURL(GURL("chrome://glic"));
  web_view->GetWebContents()->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);
  AddChildView(std::move(web_view));
}

GlicView::~GlicView() = default;

// static
views::UniqueWidgetPtr GlicView::CreateWidget(Profile* profile,
                                              const gfx::Rect& initial_bounds) {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.remove_standard_frame = true;
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.bounds = initial_bounds;

  views::UniqueWidgetPtr widget =
      std::make_unique<views::Widget>(std::move(params));

  widget->SetContentsView(
      std::make_unique<GlicView>(profile, initial_bounds.size()));

  return widget;
}
}  // namespace glic
