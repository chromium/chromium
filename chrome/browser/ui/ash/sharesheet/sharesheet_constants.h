// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_CONSTANTS_H_
#define CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_CONSTANTS_H_

#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"

namespace ash {
namespace sharesheet {

// TODO(crbug.com/2904756) Replace the below values with constants from
// LayoutProvider.

// Sizes are in px.
inline constexpr int kDefaultBubbleWidth = 416;
inline constexpr int kSpacing = 24;

inline constexpr int kFooterDefaultVerticalPadding = 20;
inline constexpr int kFooterNoExtensionVerticalPadding = 16;

inline constexpr int kExpandButtonInsideBorderInsetsVertical = 6;
inline constexpr int kExpandButtonInsideBorderInsetsHorizontal = 16;
inline constexpr int kExpandButtonBetweenChildSpacing = 8;
inline constexpr int kExpandButtonCaretIconSize = 20;

inline constexpr size_t kTextPreviewMaximumLines = 3;
inline constexpr size_t kImagePreviewMaxIcons = 4;
// TODO(crbug.com/40173943) |kImagePreviewHalfIconSize| value should actually be
// 19. When refactoring HoldingSpaceImage, once the DCHECK_GT(20) is removed,
// this should be set to 19. At that point |kImagePreviewFullIconSize| can be
// be removed and set to |::sharesheet::kIconSize|.
inline constexpr size_t kImagePreviewHalfIconSize = 21;
inline constexpr size_t kImagePreviewFullIconSize = 44;
inline constexpr gfx::Size kImagePreviewFullSize(kImagePreviewFullIconSize,
                                                 kImagePreviewFullIconSize);
inline constexpr gfx::Size kImagePreviewHalfSize(kImagePreviewFullIconSize,
                                                 kImagePreviewHalfIconSize);
inline constexpr gfx::Size kImagePreviewQuarterSize(kImagePreviewHalfIconSize,
                                                    kImagePreviewHalfIconSize);
inline constexpr int kImagePreviewFileEnumerationLineHeight = 10;
inline constexpr int kImagePreviewBetweenChildSpacing = 2;
inline constexpr int kImagePreviewIconCornerRadius = 2;
inline constexpr int kImagePreviewPlaceholderIconContentSize = 20;
inline constexpr SkAlpha kImagePreviewBackgroundAlphaComponent = 0x32;

inline constexpr int kHeaderViewBetweenChildSpacing = 12;
inline constexpr int kHeaderViewNarrowInsideBorderInsets = 14;

inline constexpr int kTitleTextLineHeight = 24;
inline constexpr int kSubtitleTextLineHeight = 22;
inline constexpr int kPrimaryTextLineHeight = 20;

}  // namespace sharesheet
}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_CONSTANTS_H_
