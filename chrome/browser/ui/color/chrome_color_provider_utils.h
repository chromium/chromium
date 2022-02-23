// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COLOR_CHROME_COLOR_PROVIDER_UTILS_H_
#define CHROME_BROWSER_UI_COLOR_CHROME_COLOR_PROVIDER_UTILS_H_

#include <string>

#include "ui/color/color_id.h"
#include "ui/color/color_provider_manager.h"
#include "ui/gfx/color_utils.h"

// Converts ColorId if |color_id| is in CHROME_COLOR_IDS.
std::string ChromeColorIdName(ui::ColorId color_id);

// Returns the tint associated with the given ID either from the custom theme or
// the default from ThemeProperties::GetDefaultTint().
color_utils::HSL GetThemeTint(int id, const ui::ColorProviderManager::Key& key);

#endif  // CHROME_BROWSER_UI_COLOR_CHROME_COLOR_PROVIDER_UTILS_H_
