// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/feature_promo_colors.h"

#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/color_palette.h"

// Button background and icon colors for in-product help promos. The first is
// the preferred color, but the selected color depends on the
// background. TODO(collinbaker): consider moving these into theme system.
constexpr SkColor kFeaturePromoHighlightDarkColor = gfx::kGoogleBlue600;
constexpr SkColor kFeaturePromoHighlightDarkExtremeColor = gfx::kGoogleBlue900;
constexpr SkColor kFeaturePromoHighlightLightColor = gfx::kGoogleGrey100;
constexpr SkColor kFeaturePromoHighlightLightExtremeColor = SK_ColorWHITE;

SkColor GetFeaturePromoHighlightColorForToolbar(
    const ui::ThemeProvider* theme_provider) {
  return ToolbarButton::AdjustHighlightColorForContrast(
      theme_provider, kFeaturePromoHighlightDarkColor,
      kFeaturePromoHighlightLightColor, kFeaturePromoHighlightDarkExtremeColor,
      kFeaturePromoHighlightLightExtremeColor);
}
