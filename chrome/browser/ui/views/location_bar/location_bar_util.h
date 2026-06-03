// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_UTIL_H_

#include <string>
#include <string_view>

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/views/view.h"

// Sets the highlight color callback and ripple color callback for inkdrop when
// the chrome refresh flag is on.
void ConfigureInkDropForRefresh2023(views::View* view,
                                    ChromeColorIds hover_color_id,
                                    ChromeColorIds ripple_color_id);

// Given a raw value of additional text from autocompletion (e.g.
// "example.com") it combines it with the appropriate separator in the current
// local for painting it  after the regular location bar text (e.g.
// " - example.com"), as well as whatever BiDi control characters are
// appropriate for providing best results for the locale's preferred
// direction.
std::u16string FormatOmniboxAdditionalText(std::u16string_view text);

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_UTIL_H_
