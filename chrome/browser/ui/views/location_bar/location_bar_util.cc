// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_bar_util.h"
#include "base/feature_list.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/user_education/common/user_education_class_properties.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_ripple.h"

void ConfigureInkDropForRefresh2023(views::View* const view,
                                    const ChromeColorIds hover_color_id,
                                    const ChromeColorIds ripple_color_id) {

  views::InkDrop::Get(view)->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::Get(view)->SetLayerRegion(views::LayerRegion::kAbove);

  views::InkDrop::Get(view)->SetCreateRippleCallback(base::BindRepeating(
      [](views::View* view, ChromeColorIds ripple_color_id)
          -> std::unique_ptr<views::InkDropRipple> {
        const SkColor pressed_color =
            view->GetColorProvider()->GetColor(ripple_color_id);
        const float pressed_alpha = SkColorGetA(pressed_color);

        return std::make_unique<views::FloodFillInkDropRipple>(
            views::InkDrop::Get(view), view->size(),
            views::InkDrop::Get(view)->GetInkDropCenterBasedOnLastEvent(),
            SkColorSetA(pressed_color, SK_AlphaOPAQUE),
            pressed_alpha / SK_AlphaOPAQUE);
      },
      view, ripple_color_id));

  views::InkDrop::Get(view)->SetCreateHighlightCallback(base::BindRepeating(
      [](views::View* view, ChromeColorIds hover_color_id) {
        const auto* color_provider = view->GetColorProvider();
        SkColor hover_color = color_provider->GetColor(hover_color_id);

        // override the hover color if this is triggered by `user_education`.
        if (view->GetProperty(user_education::kHasInProductHelpPromoKey)) {
          hover_color = color_provider->GetColor(
              ui::kColorButtonFeatureAttentionHighlight);
        }

        const float hover_alpha = SkColorGetA(hover_color);

        auto ink_drop_highlight = std::make_unique<views::InkDropHighlight>(
            gfx::SizeF(view->size()), SkColorSetA(hover_color, SK_AlphaOPAQUE));
        ink_drop_highlight->set_visible_opacity(hover_alpha / SK_AlphaOPAQUE);
        return ink_drop_highlight;
      },
      view, hover_color_id));
}
