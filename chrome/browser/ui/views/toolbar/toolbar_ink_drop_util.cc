// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"

#include "base/functional/bind.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_provider_utils.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/user_education/common/user_education_class_properties.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/animation/ink_drop_impl.h"
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
        views::Emphasis::kMaximum, rect.size());

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

SkColor GetToolbarInkDropBaseColor(const views::View* host_view) {
  const auto* color_provider = host_view->GetColorProvider();
  // There may be no color provider in unit tests.
  return color_provider ? color_provider->GetColor(kColorToolbarInkDrop)
                        : gfx::kPlaceholderColor;
}

void ConfigureInkDropForToolbar(
    views::Button* host,
    std::unique_ptr<views::HighlightPathGenerator> highlight_generator) {
  host->SetHasInkDropActionOnClick(true);

  if (!highlight_generator) {
    highlight_generator =
        std::make_unique<ToolbarButtonHighlightPathGenerator>();
  }
  views::HighlightPathGenerator::Install(host, std::move(highlight_generator));

  views::InkDrop::Get(host)->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::Get(host)->SetVisibleOpacity(kToolbarInkDropVisibleOpacity);
  views::InkDrop::Get(host)->SetHighlightOpacity(
      kToolbarInkDropHighlightVisibleAlpha / float{SK_AlphaOPAQUE});
  views::InkDrop::Get(host)->SetBaseColorCallback(
      base::BindRepeating(&GetToolbarInkDropBaseColor, host));
  ConfigureToolbarInkdropForRefresh2023(host, kColorToolbarInkDropHover,
                                        kColorToolbarInkDropRipple);
}

void ConfigureToolbarInkdropForRefresh2023(
    views::View* const host,
    const ChromeColorIds hover_color_id,
    const ChromeColorIds ripple_color_id) {
  views::InkDrop::Get(host)->SetLayerRegion(views::LayerRegion::kAbove);
  CreateToolbarInkdropCallbacks(host, hover_color_id, ripple_color_id);
}

void CreateToolbarInkdropCallbacks(views::View* const host,
                                   const ChromeColorIds hover_color_id,
                                   const ChromeColorIds ripple_color_id) {
  views::InkDrop::Get(host)->SetCreateRippleCallback(base::BindRepeating(
      [](views::View* host, ChromeColorIds ripple_color_id)
          -> std::unique_ptr<views::InkDropRipple> {
        const auto* color_provider = host->GetColorProvider();
        const SkColor pressed_color =
            color_provider ? color_provider->GetColor(ripple_color_id)
                           : gfx::kPlaceholderColor;
        const float pressed_alpha = SkColorGetA(pressed_color);

        return std::make_unique<views::FloodFillInkDropRipple>(
            views::InkDrop::Get(host), host->size(),
            host->GetLocalBounds().CenterPoint(),
            SkColorSetA(pressed_color, SK_AlphaOPAQUE),
            pressed_alpha / SK_AlphaOPAQUE);
      },
      host, ripple_color_id));

  views::InkDrop::Get(host)->SetCreateHighlightCallback(base::BindRepeating(
      [](views::View* host, ChromeColorIds hover_color_id) {
        const auto* color_provider = host->GetColorProvider();
        SkColor hover_color = color_provider
                                  ? color_provider->GetColor(hover_color_id)
                                  : gfx::kPlaceholderColor;

        // override the hover color if this is triggered by `user_education`.
        if (host->GetProperty(user_education::kHasInProductHelpPromoKey)) {
          hover_color = color_provider->GetColor(
              ui::kColorButtonFeatureAttentionHighlight);
        }

        const float hover_alpha = SkColorGetA(hover_color);

        auto ink_drop_highlight = std::make_unique<views::InkDropHighlight>(
            gfx::SizeF(host->size()), SkColorSetA(hover_color, SK_AlphaOPAQUE));

        ink_drop_highlight->set_visible_opacity(hover_alpha / SK_AlphaOPAQUE);
        return ink_drop_highlight;
      },
      host, hover_color_id));
}
