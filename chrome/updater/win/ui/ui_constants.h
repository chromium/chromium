// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UI_UI_CONSTANTS_H_
#define CHROME_UPDATER_WIN_UI_UI_CONSTANTS_H_

#include <windows.h>

namespace updater::ui {

inline constexpr COLORREF kBkColor = RGB(0XFB, 0XFB, 0XFB);
inline constexpr COLORREF kTextColor = RGB(0x29, 0x29, 0x29);

inline constexpr COLORREF kCaptionBkHover = RGB(0xE9, 0xE9, 0xE9);
inline constexpr COLORREF kCaptionForegroundColor = RGB(0x00, 0x00, 0x00);
inline constexpr COLORREF kCaptionFrameColor = RGB(0xC1, 0xC1, 0xC1);

inline constexpr COLORREF kProgressBarDarkColor = RGB(0x40, 0x86, 0xfd);
inline constexpr COLORREF kProgressBarLightColor = RGB(0x4d, 0xa4, 0xfd);
inline constexpr COLORREF kProgressEmptyFillColor = RGB(0xb6, 0xb6, 0xb6);
inline constexpr COLORREF kProgressEmptyFrameColor = RGB(0xad, 0xad, 0xad);
inline constexpr COLORREF kProgressInnerFrameDark = RGB(0x44, 0x90, 0xfc);
inline constexpr COLORREF kProgressInnerFrameLight = RGB(0x6e, 0xc2, 0xfe);
inline constexpr COLORREF kProgressLeftHighlightColor = RGB(0xbd, 0xbd, 0xbd);
inline constexpr COLORREF kProgressOuterFrameDark = RGB(0x23, 0x6d, 0xd6);
inline constexpr COLORREF kProgressOuterFrameLight = RGB(0x3c, 0x86, 0xf0);
inline constexpr COLORREF kProgressShadowLightColor = RGB(0xbd, 0xbd, 0xbd);
inline constexpr COLORREF kProgressShadowDarkColor = RGB(0xa5, 0xa5, 0xa5);

// Time-related constants for defining durations.
inline constexpr int kMsPerSec = 1000;
inline constexpr int kSecPerMin = 60;
inline constexpr int kSecondsPerHour = 60 * 60;

extern const wchar_t kLegacyUiDisplayedEventEnvironmentVariableName[];

extern const wchar_t kDialogFont[];

}  // namespace updater::ui

#endif  // CHROME_UPDATER_WIN_UI_UI_CONSTANTS_H_
