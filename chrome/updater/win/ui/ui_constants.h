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
inline constexpr COLORREF kProgressEmptyFrameColor = RGB(0xF2, 0xF2, 0xEF);

// Time-related constants for defining durations.
inline constexpr int kMsPerSec = 1000;
inline constexpr int kSecPerMin = 60;
inline constexpr int kSecondsPerHour = 60 * 60;

inline constexpr wchar_t kLegacyUiDisplayedEventEnvironmentVariableName[] =
    L"GOOGLE_UPDATE_UI_DISPLAYED_EVENT_NAME";

inline constexpr wchar_t kDialogFont[] = L"Segoe UI";

}  // namespace updater::ui

#endif  // CHROME_UPDATER_WIN_UI_UI_CONSTANTS_H_
