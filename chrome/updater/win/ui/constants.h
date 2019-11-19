// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UI_CONSTANTS_H_
#define CHROME_UPDATER_WIN_UI_CONSTANTS_H_

#include <windows.h>

#include "base/strings/string16.h"

namespace updater {
namespace ui {

constexpr COLORREF kBkColor = RGB(0XFB, 0XFB, 0XFB);
constexpr COLORREF kTextColor = RGB(0x29, 0x29, 0x29);

constexpr COLORREF kCaptionBkHover = RGB(0xE9, 0xE9, 0xE9);
constexpr COLORREF kCaptionForegroundColor = RGB(0x00, 0x00, 0x00);
constexpr COLORREF kCaptionFrameColor = RGB(0xC1, 0xC1, 0xC1);

constexpr COLORREF kProgressBarDarkColor = RGB(0x40, 0x86, 0xfd);
constexpr COLORREF kProgressBarLightColor = RGB(0x4d, 0xa4, 0xfd);
constexpr COLORREF kProgressEmptyFillColor = RGB(0xb6, 0xb6, 0xb6);
constexpr COLORREF kProgressEmptyFrameColor = RGB(0xad, 0xad, 0xad);
constexpr COLORREF kProgressInnerFrameDark = RGB(0x44, 0x90, 0xfc);
constexpr COLORREF kProgressInnerFrameLight = RGB(0x6e, 0xc2, 0xfe);
constexpr COLORREF kProgressLeftHighlightColor = RGB(0xbd, 0xbd, 0xbd);
constexpr COLORREF kProgressOuterFrameDark = RGB(0x23, 0x6d, 0xd6);
constexpr COLORREF kProgressOuterFrameLight = RGB(0x3c, 0x86, 0xf0);
constexpr COLORREF kProgressShadowLightColor = RGB(0xbd, 0xbd, 0xbd);
constexpr COLORREF kProgressShadowDarkColor = RGB(0xa5, 0xa5, 0xa5);

// Time-related constants for defining durations.
constexpr int kMsPerSec = 1000;
constexpr int kSecPerMin = 60;
constexpr int kSecondsPerHour = 60 * 60;

extern const base::char16 kLegacyUiDisplayedEventEnvironmentVariableName[];

extern const base::char16 kDialogFont[];

}  // namespace ui
}  // namespace updater

#endif  // CHROME_UPDATER_WIN_UI_CONSTANTS_H_
