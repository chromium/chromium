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
inline constexpr COLORREF kCaptionForegroundColor = RGB(0x01, 0x57, 0xDE);
inline constexpr COLORREF kCaptionFrameColor = RGB(0xC1, 0xC1, 0xC1);

inline constexpr COLORREF kProgressBarFillColor = RGB(0x01, 0x57, 0xDE);
inline constexpr COLORREF kProgressEmptyFillColor = RGB(0xE1, 0xE3, 0xE1);
// Color drawn outside the rounded pill in light mode. Because the pill is
// rounded, the four corners expose this color and it becomes the pill's
// visible frame against the dialog background.
inline constexpr COLORREF kProgressEmptyFrameColor = kBkColor;

// GDS Button Color Tokens (Light Mode)
inline constexpr COLORREF kPrimaryButtonBg = RGB(0x1A, 0x73, 0xE8);
inline constexpr COLORREF kPrimaryButtonBgHover = RGB(0x15, 0x57, 0xB0);
inline constexpr COLORREF kPrimaryButtonBgPressed = RGB(0x0F, 0x3D, 0x80);
inline constexpr COLORREF kPrimaryButtonFg = RGB(0xFF, 0xFF, 0xFF);

inline constexpr COLORREF kSecondaryButtonBg = RGB(0xFF, 0xFF, 0xFF);
inline constexpr COLORREF kSecondaryButtonBgHover = RGB(0xF6, 0xF9, 0xFE);
inline constexpr COLORREF kSecondaryButtonBgPressed = RGB(0xED, 0xF3, 0xFD);
inline constexpr COLORREF kSecondaryButtonFg = RGB(0x1A, 0x73, 0xE8);
inline constexpr COLORREF kSecondaryButtonBorder = RGB(0xDA, 0xDC, 0xE0);

inline constexpr COLORREF kButtonBgDisabled = RGB(0xF1, 0xF3, 0xF4);
inline constexpr COLORREF kButtonFgDisabled = RGB(0x9E, 0x9E, 0x9E);

// GDS Button Color Tokens (Dark Mode)
inline constexpr COLORREF kPrimaryButtonBgDark = RGB(0x8A, 0xB4, 0xF8);
inline constexpr COLORREF kPrimaryButtonBgDarkHover = RGB(0x7A, 0xAA, 0xF7);
inline constexpr COLORREF kPrimaryButtonBgDarkPressed = RGB(0x66, 0x9D, 0xF2);
inline constexpr COLORREF kPrimaryButtonFgDark = RGB(0x20, 0x21, 0x24);

inline constexpr COLORREF kSecondaryButtonBgDark = RGB(0x20, 0x20, 0x20);
inline constexpr COLORREF kSecondaryButtonBgDarkHover = RGB(0x30, 0x30, 0x30);
inline constexpr COLORREF kSecondaryButtonBgDarkPressed = RGB(0x3C, 0x40, 0x43);
inline constexpr COLORREF kSecondaryButtonFgDark = RGB(0x8A, 0xB4, 0xF8);
inline constexpr COLORREF kSecondaryButtonBorderDark = RGB(0x5F, 0x63, 0x68);

inline constexpr COLORREF kButtonBgDisabledDark = RGB(0x3C, 0x40, 0x43);
inline constexpr COLORREF kButtonFgDisabledDark = RGB(0x80, 0x86, 0x8B);

// Time-related constants for defining durations.
inline constexpr int kMsPerSec = 1000;
inline constexpr int kSecPerMin = 60;
inline constexpr int kSecondsPerHour = 60 * 60;

inline constexpr wchar_t kLegacyUiDisplayedEventEnvironmentVariableName[] =
    L"GOOGLE_UPDATE_UI_DISPLAYED_EVENT_NAME";

inline constexpr wchar_t kDialogFont[] = L"Segoe UI";

}  // namespace updater::ui

#endif  // CHROME_UPDATER_WIN_UI_UI_CONSTANTS_H_
