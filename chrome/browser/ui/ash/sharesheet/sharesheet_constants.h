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
constexpr int kDefaultBubbleWidth = 416;
constexpr int kSpacing = 24;

constexpr int kFooterDefaultVerticalPadding = 20;
constexpr int kFooterNoExtensionVerticalPadding = 16;

constexpr int kExpandButtonInsideBorderInsetsVertical = 6;
constexpr int kExpandButtonInsideBorderInsetsHorizontal = 16;
constexpr int kExpandButtonBetweenChildSpacing = 8;
constexpr int kExpandButtonCaretIconSize = 20;

constexpr size_t kTextPreviewMaximumLines = 3;
constexpr size_t kImagePreviewMaxIcons = 4;
// TODO(crbug.com/40173943) |kImagePreviewHalfIconSize| value should actually be
// 19. When refactoring HoldingSpaceImage, once the DCHECK_GT(20) is removed,
// this should be set to 19. At that point |kImagePreviewFullIconSize| can be
// be removed and set to |::sharesheet::kIconSize|.
constexpr size_t kImagePreviewHalfIconSize = 21;
constexpr size_t kImagePreviewFullIconSize = 44;
constexpr gfx::Size kImagePreviewFullSize(kImagePreviewFullIconSize,
                                          kImagePreviewFullIconSize);
constexpr gfx::Size kImagePreviewHalfSize(kImagePreviewFullIconSize,
                                          kImagePreviewHalfIconSize);
constexpr gfx::Size kImagePreviewQuarterSize(kImagePreviewHalfIconSize,
                                             kImagePreviewHalfIconSize);
constexpr int kImagePreviewFileEnumerationLineHeight = 10;
constexpr int kImagePreviewBetweenChildSpacing = 2;
constexpr int kImagePreviewIconCornerRadius = 2;
constexpr int kImagePreviewPlaceholderIconContentSize = 20;
constexpr SkAlpha kImagePreviewBackgroundAlphaComponent = 0x32;

constexpr int kHeaderViewBetweenChildSpacing = 12;
constexpr int kHeaderViewNarrowInsideBorderInsets = 14;

constexpr int kTitleTextLineHeight = 24;
constexpr int kSubtitleTextLineHeight = 22;
constexpr int kPrimaryTextLineHeight = 20;

}  // namespace sharesheet
}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_CONSTANTS_H_
