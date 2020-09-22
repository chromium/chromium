// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"

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
#include "ui/views/animation/installable_ink_drop_config.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {
class ToolbarButtonHighlightPathGenerator
    : public views::HighlightPathGenerator {
 public:
  // HighlightPathGenerator:
  SkPath GetHighlightPath(const views::View* view) override {
    gfx::Rect rect(view->size());
    rect.Inset(GetToolbarInkDropInsets(view));

    const int radii = ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
        views::EMPHASIS_MAXIMUM, rect.size());

    SkPath path;
    path.addRoundRect(gfx::RectToSkRect(rect), radii, radii);
    return path;
  }
};
}  // namespace

gfx::Insets GetToolbarInkDropInsets(const views::View* host_view) {
  gfx::Insets margin_insets;
  gfx::Insets* const internal_padding =
      host_view->GetProperty(views::kInternalPaddingKey);
  if (internal_padding)
    margin_insets = *internal_padding;

  // Inset the inkdrop insets so that the end result matches the target inkdrop
  // dimensions.
  const gfx::Size host_size = host_view->size();
  const int inkdrop_dimensions = GetLayoutConstant(LOCATION_BAR_HEIGHT);
  gfx::Insets inkdrop_insets =
      margin_insets +
      gfx::Insets(std::max(0, (host_size.height() - inkdrop_dimensions) / 2));

  return inkdrop_insets;
}

std::unique_ptr<views::InkDropHighlight> CreateToolbarInkDropHighlight(
    const views::InkDropHostView* host_view) {
  auto highlight = host_view->views::InkDropHostView::CreateInkDropHighlight();
  highlight->set_visible_opacity(kToolbarInkDropHighlightVisibleOpacity);
  return highlight;
}

SkColor GetToolbarInkDropBaseColor(const views::View* host_view) {
  const auto* theme_provider = host_view->GetThemeProvider();
  // There may be no theme provider in unit tests.
  return theme_provider
             ? theme_provider->GetColor(ThemeProperties::COLOR_TOOLBAR_INK_DROP)
             : gfx::kPlaceholderColor;
}

views::InstallableInkDropConfig GetToolbarInstallableInkDropConfig(
    const views::View* host_view) {
  views::InstallableInkDropConfig config;
  config.base_color = GetToolbarInkDropBaseColor(host_view);
  config.ripple_opacity = kToolbarInkDropVisibleOpacity;
  config.highlight_opacity = kToolbarInkDropHighlightVisibleOpacity;
  return config;
}

void InstallToolbarButtonHighlightPathGenerator(views::View* host) {
  views::HighlightPathGenerator::Install(
      host, std::make_unique<ToolbarButtonHighlightPathGenerator>());
}

void ConfigureInkDropForToolbar(views::Button* host) {
  host->SetHasInkDropActionOnClick(true);
  InstallToolbarButtonHighlightPathGenerator(host);
  host->SetInkDropMode(views::InkDropHostView::InkDropMode::ON);
  host->SetInkDropVisibleOpacity(kToolbarInkDropVisibleOpacity);
  host->SetInkDropHighlightOpacity(kToolbarInkDropHighlightVisibleOpacity);
  host->SetInkDropBaseColor(GetToolbarInkDropBaseColor(host));
}
