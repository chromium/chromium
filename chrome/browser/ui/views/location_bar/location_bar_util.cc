// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_bar_util.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_ripple.h"

void ConfigureInkDropForRefresh2023(views::View* const host,
                                    const ChromeColorIds hover_color_id,
                                    const ChromeColorIds ripple_color_id) {
  // TODO(crbug.com/1450984): Figure out if one of these are redundant.
  CHECK(features::IsChromeRefresh2023() ||
        OmniboxFieldTrial::IsChromeRefreshIconsEnabled());

  views::InkDrop::Get(host)->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::Get(host)->SetLayerRegion(views::LayerRegion::kAbove);

  views::InkDrop::Get(host)->SetCreateRippleCallback(base::BindRepeating(
      [](views::View* host, ChromeColorIds ripple_color_id)
          -> std::unique_ptr<views::InkDropRipple> {
        const SkColor pressed_color =
            host->GetColorProvider()->GetColor(ripple_color_id);
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
        const SkColor hover_color =
            host->GetColorProvider()->GetColor(hover_color_id);
        const float hover_alpha = SkColorGetA(hover_color);

        auto ink_drop_highlight = std::make_unique<views::InkDropHighlight>(
            gfx::SizeF(host->size()), SkColorSetA(hover_color, SK_AlphaOPAQUE));
        ink_drop_highlight->set_visible_opacity(hover_alpha / SK_AlphaOPAQUE);
        return ink_drop_highlight;
      },
      host, hover_color_id));
}
