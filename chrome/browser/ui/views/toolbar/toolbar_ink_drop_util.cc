// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"

#include "base/i18n/rtl.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view.h"
#include "ui/views/view_properties.h"

gfx::Insets GetToolbarInkDropInsets(const views::View* host_view,
                                    const gfx::Insets& margin_insets) {
  // TODO(pbos): Inkdrop masks and layers should be flipped with RTL. Fix this
  // and remove RTL handling from here.
  gfx::Insets inkdrop_insets =
      base::i18n::IsRTL()
          ? gfx::Insets(margin_insets.top(), margin_insets.right(),
                        margin_insets.bottom(), margin_insets.left())
          : margin_insets;

  // Inset the inkdrop insets so that the end result matches the target inkdrop
  // dimensions.
  const gfx::Size host_size = host_view->size();
  const int inkdrop_dimensions = GetLayoutConstant(LOCATION_BAR_HEIGHT);
  inkdrop_insets += gfx::Insets((host_size.height() - inkdrop_dimensions) / 2);

  return inkdrop_insets;
}

void SetToolbarButtonHighlightPath(views::View* host_view,
                                   const gfx::Insets& margin_insets) {
  gfx::Rect rect(host_view->size());
  rect.Inset(GetToolbarInkDropInsets(host_view, margin_insets));

  const int radii = ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
      views::EMPHASIS_MAXIMUM, rect.size());

  auto path = std::make_unique<SkPath>();
  path->addRoundRect(gfx::RectToSkRect(rect), radii, radii);
  host_view->SetProperty(views::kHighlightPathKey, path.release());
}

std::unique_ptr<views::InkDrop> CreateToolbarInkDrop(
    views::InkDropHostView* host_view) {
  auto ink_drop =
      std::make_unique<views::InkDropImpl>(host_view, host_view->size());
  ink_drop->SetAutoHighlightMode(
      views::InkDropImpl::AutoHighlightMode::SHOW_ON_RIPPLE);
  ink_drop->SetShowHighlightOnHover(true);
  ink_drop->SetShowHighlightOnFocus(!views::PlatformStyle::kPreferFocusRings);
  return ink_drop;
}

std::unique_ptr<views::InkDropHighlight> CreateToolbarInkDropHighlight(
    const views::InkDropHostView* host_view) {
  constexpr float kToolbarInkDropHighlightVisibleOpacity = 0.08f;
  auto highlight = host_view->views::InkDropHostView::CreateInkDropHighlight();
  highlight->set_visible_opacity(kToolbarInkDropHighlightVisibleOpacity);
  return highlight;
}

SkColor GetToolbarInkDropBaseColor(const views::View* host_view) {
  const auto* theme_provider = host_view->GetThemeProvider();
  // There may be no theme provider in unit tests.
  if (theme_provider) {
    return color_utils::BlendTowardOppositeLuma(
        theme_provider->GetColor(ThemeProperties::COLOR_TOOLBAR),
        SK_AlphaOPAQUE);
  }

  return gfx::kPlaceholderColor;
}
