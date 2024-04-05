// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_WM_CONSTANTS_H_
#define CHROMEOS_UI_WM_CONSTANTS_H_

#include "base/component_export.h"

namespace chromeos::wm {

// The distance from the edge of the floated window to the edge of the work
// area when it is floated.
COMPONENT_EXPORT(CHROMEOS_UI_WM) constexpr int kFloatedWindowPaddingDp = 8;

// The ideal dimensions of a floated window before factoring in its minimum size
// (if any) is the available work area multiplied by these ratios.
constexpr float kFloatedWindowTabletWidthRatio = 1.f / 3.f;
constexpr float kFloatedWindowTabletHeightRatio = 0.8f;

// The thickness of the divider when it is not being dragged.
COMPONENT_EXPORT(CHROMEOS_UI_WM)
constexpr int kSplitviewDividerShortSideLength = 6;

// Extra padding for the browser window in tablet mode since the minimum size
// returned by the browser makes the omnibox untappable in several cases.
COMPONENT_EXPORT(CHROMEOS_UI_WM)
constexpr int kBrowserExtraPaddingDp = 48;

}  // namespace chromeos::wm

#endif  // CHROMEOS_UI_WM_CONSTANTS_H_
