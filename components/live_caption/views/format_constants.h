// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_VIEWS_FORMAT_CONSTANTS_H_
#define COMPONENTS_LIVE_CAPTION_VIEWS_FORMAT_CONSTANTS_H_

#include "ui/gfx/geometry/insets.h"

namespace captions {

inline constexpr int kLineHeightDip = 24;
inline constexpr int kLiveTranslateLabelLineHeightDip = 18;
inline constexpr int kLiveTranslateImageWidthDip = 16;
inline constexpr int kLanguageButtonImageLabelSpacing = 4;
inline constexpr auto kLanguageButtonInsets = gfx::Insets::TLBR(2, 8, 2, 6);
inline constexpr int kNumLinesCollapsed = 2;
inline constexpr int kNumLinesExpanded = 8;
inline constexpr int kCornerRadiusDip = 4;
inline constexpr int kSidePaddingDip = 18;
inline constexpr int kButtonDip = 16;
inline constexpr int kButtonCircleHighlightPaddingDip = 2;
inline constexpr int kMaxWidthDip = 536;
// Margin of the bubble with respect to the context window.
inline constexpr int kMinAnchorMarginDip = 20;
inline constexpr char kPrimaryFont[] = "Roboto";
inline constexpr char kSecondaryFont[] = "Arial";
inline constexpr char kTertiaryFont[] = "sans-serif";
inline constexpr int kFontSizePx = 16;
inline constexpr int kLiveTranslateLabelFontSizePx = 11;
inline constexpr double kDefaultRatioInParentX = 0.5;
inline constexpr double kDefaultRatioInParentY = 1;
inline constexpr int kErrorImageSizeDip = 20;
inline constexpr int kErrorMessageBetweenChildSpacingDip = 16;
inline constexpr double kContextSufficientOverlapRatio = .4;

}  // namespace captions

#endif  // COMPONENTS_LIVE_CAPTION_VIEWS_FORMAT_CONSTANTS_H_
