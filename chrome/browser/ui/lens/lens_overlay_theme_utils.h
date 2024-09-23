// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_THEME_UTILS_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_THEME_UTILS_H_

class ThemeService;

namespace lens {

// Returns true if the lens overlay results and searchbox in the
// side panel should use dark mode.
bool LensOverlayShouldUseDarkMode(ThemeService* theme_service);

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_THEME_UTILS_H_
