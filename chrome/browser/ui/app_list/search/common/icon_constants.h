// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_COMMON_ICON_CONSTANTS_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_COMMON_ICON_CONSTANTS_H_

#include "ui/gfx/color_palette.h"

namespace app_list {

// Result icon dimension constants.
constexpr int kFaviconDimension = 18;
constexpr int kThumbnailDimension = 28;
constexpr int kSystemIconDimension = 20;

// The following dimensions depend on whether the productivity launcher is
// enabled or not.
int GetAnswerCardIconDimension();
int GetAppIconDimension();
int GetImageIconDimension();
SkColor GetGenericIconColor();

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_COMMON_ICON_CONSTANTS_H_
