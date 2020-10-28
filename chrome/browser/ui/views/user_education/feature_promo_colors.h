// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_COLORS_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_COLORS_H_

#include "third_party/skia/include/core/SkColor.h"

namespace ui {
class ThemeProvider;
}  // namespace ui

SkColor GetFeaturePromoHighlightColorForToolbar(
    const ui::ThemeProvider* theme_provider);

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_COLORS_H_
