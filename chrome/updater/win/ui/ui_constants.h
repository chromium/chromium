// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UI_UI_CONSTANTS_H_
#define CHROME_UPDATER_WIN_UI_UI_CONSTANTS_H_

#include <windows.h>

namespace updater::ui {

inline constexpr COLORREF kBkColor = RGB(0XFB, 0XFB, 0XFB);
inline constexpr COLORREF kTextColor = RGB(0x29, 0x29, 0x29);
inline constexpr COLORREF kTextColorDark = RGB(0xFF, 0xFF, 0xFF);
inline constexpr COLORREF kBgColorDark = RGB(0x20, 0x20, 0x20);
inline constexpr COLORREF kBgColorLight = RGB(0xFF, 0xFF, 0xFF);

// Product accent color per Figma.
inline constexpr COLORREF kAccentColor = RGB(0x01, 0x57, 0xDE);
inline constexpr COLORREF kAccentColorDark = RGB(0xA8, 0xC7, 0xFA);

// Caption button glyph color. Named separately from the accent token so the
// caption can diverge from it without touching the other accent consumers.
inline constexpr COLORREF kCaptionForegroundColor = kAccentColor;
inline constexpr COLORREF kCaptionForegroundColorDark = kAccentColorDark;

// Glyph color for a disabled caption button. Currently matches the GDS
// disabled button tokens (`kButtonFgDisabled[Dark]`) because the caption glyph
// is a button foreground, but spelled out rather than aliased so the caption
// can be retuned without moving the dialog buttons.
//
// The light value is 2.7:1 against `kBgColorLight`, under the 3:1 of WCAG
// 1.4.11. That is intentional: the criterion exempts inactive controls, and
// dimming below the enabled accent is the point. The dark value is 4.4:1
// against `kBgColorDark`.
// TODO(crbug.com/409590312): Confirm with design whether caption glyphs and
// GDS buttons should share the disabled token.
inline constexpr COLORREF kCaptionForegroundColorDisabled =
    RGB(0x9E, 0x9E, 0x9E);
inline constexpr COLORREF kCaptionForegroundColorDisabledDark =
    RGB(0x80, 0x86, 0x8B);

// Focused caption button border color.
// TODO(crbug.com/409590312): Revisit light mode contrast with design.
inline constexpr COLORREF kCaptionFrameColor = RGB(0xC1, 0xC1, 0xC1);
// Raised from kSecondaryButtonBorderDark (0x5F6368, 2.7:1) to reach the 3:1
// WCAG 1.4.11 minimum against kBgColorDark, because this frame is the button's
// only keyboard focus indicator.
inline constexpr COLORREF kCaptionFrameColorDark = RGB(0x6A, 0x6A, 0x6A);

// Caption button hover background color.
inline constexpr COLORREF kCaptionBkHover = RGB(0xE9, 0xE9, 0xE9);
inline constexpr COLORREF kCaptionBkHoverDark = RGB(0x30, 0x30, 0x30);

inline constexpr COLORREF kWindowBorderColor = RGB(0x3C, 0x40, 0x43);

inline constexpr COLORREF kProgressBarFillColor = kAccentColor;
inline constexpr COLORREF kProgressBarFillColorDark = kAccentColorDark;
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

// Text color for the standard dialog buttons in dark mode. Deliberately
// independent from `kAccentColorDark` (button text token vs accent fill),
// though currently matching its value.
// TODO(crbug.com/409590312): Confirm with design whether this should match
// `kSecondaryButtonFgDark` (0x8AB4F8).
inline constexpr COLORREF kDialogButtonTextDark = RGB(0xA8, 0xC7, 0xFA);

// Time-related constants for defining durations.
inline constexpr int kMsPerSec = 1000;
inline constexpr int kSecPerMin = 60;
inline constexpr int kSecondsPerHour = 60 * 60;

inline constexpr wchar_t kLegacyUiDisplayedEventEnvironmentVariableName[] =
    L"GOOGLE_UPDATE_UI_DISPLAYED_EVENT_NAME";

inline constexpr wchar_t kDialogFont[] = L"Segoe UI";

}  // namespace updater::ui

#endif  // CHROME_UPDATER_WIN_UI_UI_CONSTANTS_H_
