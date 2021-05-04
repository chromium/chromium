// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_CONSTANTS_H_
#define CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_CONSTANTS_H_

#include "third_party/skia/include/core/SkColor.h"

namespace ash {
namespace sharesheet {

// Sizes are in px.
constexpr int kSpacing = 24;

constexpr size_t kTextPreviewMaximumLines = 3;
constexpr int kImagePreviewCornerRadius = 4;
constexpr SkColor kImagePreviewPlaceholderBackgroundColor = gfx::kGoogleBlue050;

constexpr int kHeaderViewBetweenChildSpacing = 12;
constexpr int kHeaderViewNarrowInsideBorderInsets = 14;

constexpr int kTitleTextLineHeight = 24;
constexpr int kSubtitleTextLineHeight = 22;
constexpr int kPrimaryTextLineHeight = 20;

constexpr SkColor kTitleTextColor = gfx::kGoogleGrey900;
constexpr SkColor kPrimaryTextColor = gfx::kGoogleGrey700;
constexpr SkColor kSecondaryTextColor = gfx::kGoogleGrey600;
constexpr SkColor kButtonTextColor = gfx::kGoogleBlue600;

}  // namespace sharesheet
}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_CONSTANTS_H_
