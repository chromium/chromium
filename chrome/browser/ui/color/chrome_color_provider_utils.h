// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COLOR_CHROME_COLOR_PROVIDER_UTILS_H_
#define CHROME_BROWSER_UI_COLOR_CHROME_COLOR_PROVIDER_UTILS_H_

#include <string>

#include "ui/color/color_id.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_utils.h"

inline constexpr SkAlpha kToolbarInkDropHighlightVisibleAlpha = 0x14;

// Converts ColorId if |color_id| is in CHROME_COLOR_IDS.
std::string ChromeColorIdName(ui::ColorId color_id);

// Returns the tint associated with the given ID either from the custom theme or
// the default from ThemeProperties::GetDefaultTint().
color_utils::HSL GetThemeTint(int id, const ui::ColorProviderKey& key);

// Computes the "toolbar top separator" color.  This color is drawn atop the
// frame to separate it from tabs, the toolbar, and the new tab button, as well
// as atop background tabs to separate them from other tabs or the toolbar.  We
// use semitransparent black or white so as to darken or lighten the frame, with
// the goal of contrasting with both the frame color and the active tab (i.e.
// toolbar) color.  (It's too difficult to try to find colors that will contrast
// with both of these as well as the background tab color, and contrasting with
// the foreground tab is the most important).
SkColor GetToolbarTopSeparatorColor(SkColor toolbar_color, SkColor frame_color);

// Adjusts the desired highlight color for a toolbar control `fg` for contrast
// against the `bg` color.
ui::ColorTransform AdjustHighlightColorForContrast(ui::ColorTransform fg,
                                                   ui::ColorTransform bg);

// Returns true if we should apply chrome high contrast colors for the `key`.
bool ShouldApplyHighContrastColors(const ui::ColorProviderKey& key);

// Returns true if material color overrides should be applied over the top of
// chrome color mixer definitions. If false color recipes from the old design
// system should be honored.
bool ShouldApplyChromeMaterialOverrides(const ui::ColorProviderKey& key);

#endif  // CHROME_BROWSER_UI_COLOR_CHROME_COLOR_PROVIDER_UTILS_H_
