// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COLOR_TAB_GROUP_COLOR_IDS_H_
#define CHROME_BROWSER_UI_COLOR_TAB_GROUP_COLOR_IDS_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"

namespace gfx {

constexpr SkColor kTabGroupBlueDarkMode = SkColorSetRGB(0xA8, 0xBC, 0xFF);
constexpr SkColor kTabGroupBlueLightMode = SkColorSetRGB(0x43, 0x6B, 0xD7);
constexpr SkColor kTabGroupBlueChipLightMode =
    SkColorSetA(kTabGroupBlueDarkMode, 0x80);
constexpr SkColor kTabGroupBlueChipDarkMode =
    SkColorSetA(kTabGroupBlueLightMode, 0x80);

constexpr SkColor kTabGroupRedDarkMode = SkColorSetRGB(0xFF, 0x92, 0x8B);
constexpr SkColor kTabGroupRedLightMode = SkColorSetRGB(0xDB, 0x1B, 0x2B);
constexpr SkColor kTabGroupRedChipLightMode =
    SkColorSetA(kTabGroupRedDarkMode, 0x80);
constexpr SkColor kTabGroupRedChipDarkMode =
    SkColorSetA(kTabGroupRedLightMode, 0x80);

constexpr SkColor kTabGroupGreenDarkMode = SkColorSetRGB(0x87, 0xEB, 0x84);
constexpr SkColor kTabGroupGreenLightMode = SkColorSetRGB(0x18, 0x81, 0x29);
constexpr SkColor kTabGroupGreenChipLightMode =
    SkColorSetA(kTabGroupGreenDarkMode, 0x80);
constexpr SkColor kTabGroupGreenChipDarkMode =
    SkColorSetA(kTabGroupGreenLightMode, 0x80);

constexpr SkColor kTabGroupGreyDarkMode = SkColorSetRGB(0xDF, 0xE2, 0xE9);
constexpr SkColor kTabGroupGreyLightMode = SkColorSetRGB(0x5F, 0x63, 0x69);
constexpr SkColor kTabGroupGreyChipLightMode =
    SkColorSetA(kTabGroupGreyDarkMode, 0x80);
constexpr SkColor kTabGroupGreyChipDarkMode =
    SkColorSetA(kTabGroupGreyLightMode, 0x80);

constexpr SkColor kTabGroupOrangeDarkMode = SkColorSetRGB(0xFF, 0xB3, 0x79);
constexpr SkColor kTabGroupOrangeLightMode = SkColorSetRGB(0xFF, 0x94, 0x36);
constexpr SkColor kTabGroupOrangeChipLightMode =
    SkColorSetA(kTabGroupOrangeDarkMode, 0x80);
constexpr SkColor kTabGroupOrangeChipDarkMode =
    SkColorSetA(kTabGroupOrangeLightMode, 0x80);

constexpr SkColor kTabGroupPurpleDarkMode = SkColorSetRGB(0xCB, 0x93, 0xFF);
constexpr SkColor kTabGroupPurpleLightMode = SkColorSetRGB(0x9D, 0x39, 0xF3);
constexpr SkColor kTabGroupPurpleChipLightMode =
    SkColorSetA(kTabGroupPurpleDarkMode, 0x80);
constexpr SkColor kTabGroupPurpleChipDarkMode =
    SkColorSetA(kTabGroupPurpleLightMode, 0x80);

constexpr SkColor kTabGroupCyanDarkMode = SkColorSetRGB(0x88, 0xE3, 0xEB);
constexpr SkColor kTabGroupCyanLightMode = SkColorSetRGB(0x00, 0x7B, 0x83);
constexpr SkColor kTabGroupCyanChipLightMode =
    SkColorSetA(kTabGroupCyanDarkMode, 0x80);
constexpr SkColor kTabGroupCyanChipDarkMode =
    SkColorSetA(kTabGroupCyanLightMode, 0x80);

constexpr SkColor kTabGroupPinkDarkMode = SkColorSetRGB(0xFF, 0x96, 0xDE);
constexpr SkColor kTabGroupPinkLightMode = SkColorSetRGB(0xC8, 0x09, 0xA8);
constexpr SkColor kTabGroupPinkChipLightMode =
    SkColorSetA(kTabGroupPinkDarkMode, 0x80);
constexpr SkColor kTabGroupPinkChipDarkMode =
    SkColorSetA(kTabGroupPinkLightMode, 0x80);

constexpr SkColor kTabGroupYellowDarkMode = SkColorSetRGB(0xFF, 0xDD, 0x7A);
constexpr SkColor kTabGroupYellowLightMode = SkColorSetRGB(0xFF, 0xD0, 0x36);
constexpr SkColor kTabGroupYellowChipLightMode =
    SkColorSetA(kTabGroupYellowDarkMode, 0x80);
constexpr SkColor kTabGroupYellowChipDarkMode =
    SkColorSetA(kTabGroupYellowLightMode, 0x80);

}  // namespace gfx

#endif  // CHROME_BROWSER_UI_COLOR_TAB_GROUP_COLOR_IDS_H_
