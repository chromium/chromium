// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COLOR_TAB_GROUP_COLOR_IDS_H_
#define CHROME_BROWSER_UI_COLOR_TAB_GROUP_COLOR_IDS_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"

namespace gfx {

constexpr SkColor kTabGroupBlueDarkMode = SkColorSetRGB(0xC8, 0xD3, 0xFF);
constexpr SkColor kTabGroupBlueLightMode = SkColorSetRGB(0x32, 0x5C, 0xCD);
constexpr SkColor kTabGroupBlueChipLightMode =
    SkColorSetA(kTabGroupBlueDarkMode, 0x80);
constexpr SkColor kTabGroupBlueChipDarkMode =
    SkColorSetA(kTabGroupBlueLightMode, 0x80);

constexpr SkColor kTabGroupRedDarkMode = SkColorSetRGB(0xFF, 0xC7, 0xC1);
constexpr SkColor kTabGroupRedLightMode = SkColorSetRGB(0xDF, 0x00, 0x0C);
constexpr SkColor kTabGroupRedChipLightMode =
    SkColorSetA(kTabGroupRedDarkMode, 0x80);
constexpr SkColor kTabGroupRedChipDarkMode =
    SkColorSetA(kTabGroupRedLightMode, 0x80);

constexpr SkColor kTabGroupGreenDarkMode = SkColorSetRGB(0xC6, 0xFF, 0xC7);
constexpr SkColor kTabGroupGreenLightMode = SkColorSetRGB(0x1A, 0x77, 0x36);
constexpr SkColor kTabGroupGreenChipLightMode =
    SkColorSetA(kTabGroupGreenDarkMode, 0x80);
constexpr SkColor kTabGroupGreenChipDarkMode =
    SkColorSetA(kTabGroupGreenLightMode, 0x80);

constexpr SkColor kTabGroupGreyDarkMode = SkColorSetRGB(0xD0, 0xD5, 0xDD);
constexpr SkColor kTabGroupGreyLightMode = SkColorSetRGB(0x49, 0x4D, 0x52);
constexpr SkColor kTabGroupGreyChipLightMode =
    SkColorSetA(kTabGroupGreyDarkMode, 0x80);
constexpr SkColor kTabGroupGreyChipDarkMode =
    SkColorSetA(kTabGroupGreyLightMode, 0x80);

constexpr SkColor kTabGroupOrangeDarkMode = SkColorSetRGB(0xFF, 0xDE, 0xA7);
constexpr SkColor kTabGroupOrangeLightMode = SkColorSetRGB(0xCC, 0x4E, 0x00);
constexpr SkColor kTabGroupOrangeChipLightMode =
    SkColorSetA(kTabGroupOrangeDarkMode, 0x80);
constexpr SkColor kTabGroupOrangeChipDarkMode =
    SkColorSetA(kTabGroupOrangeLightMode, 0x80);

constexpr SkColor kTabGroupPurpleDarkMode = SkColorSetRGB(0xE0, 0xCB, 0xFF);
constexpr SkColor kTabGroupPurpleLightMode = SkColorSetRGB(0x7C, 0x31, 0xE6);
constexpr SkColor kTabGroupPurpleChipLightMode =
    SkColorSetA(kTabGroupPurpleDarkMode, 0x80);
constexpr SkColor kTabGroupPurpleChipDarkMode =
    SkColorSetA(kTabGroupPurpleLightMode, 0x80);

constexpr SkColor kTabGroupCyanDarkMode = SkColorSetRGB(0xC7, 0xFA, 0xFF);
constexpr SkColor kTabGroupCyanLightMode = SkColorSetRGB(0x05, 0x7C, 0x84);
constexpr SkColor kTabGroupCyanChipLightMode =
    SkColorSetA(kTabGroupCyanDarkMode, 0x80);
constexpr SkColor kTabGroupCyanChipDarkMode =
    SkColorSetA(kTabGroupCyanLightMode, 0x80);

constexpr SkColor kTabGroupMagentaDarkMode = SkColorSetRGB(0xFF, 0xC3, 0xE8);
constexpr SkColor kTabGroupMagentaLightMode = SkColorSetRGB(0xCC, 0x06, 0xAB);
constexpr SkColor kTabGroupMagentaChipLightMode =
    SkColorSetA(kTabGroupMagentaDarkMode, 0x80);
constexpr SkColor kTabGroupMagentaChipDarkMode =
    SkColorSetA(kTabGroupMagentaLightMode, 0x80);

constexpr SkColor kTabGroupLimeDarkMode = SkColorSetRGB(0xEE, 0xFF, 0xA5);
constexpr SkColor kTabGroupLimeLightMode = SkColorSetRGB(0x63, 0x75, 0x05);
constexpr SkColor kTabGroupLimeChipLightMode =
    SkColorSetA(kTabGroupLimeDarkMode, 0x80);
constexpr SkColor kTabGroupLimeChipDarkMode =
    SkColorSetA(kTabGroupLimeLightMode, 0x80);

}  // namespace gfx

#endif  // CHROME_BROWSER_UI_COLOR_TAB_GROUP_COLOR_IDS_H_
