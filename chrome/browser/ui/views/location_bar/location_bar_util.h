// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_UTIL_H_

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/views/view.h"

// Sets the highlight color callback and ripple color callback for inkdrop when
// the chrome refresh flag is on.
void ConfigureInkDropForRefresh2023(views::View* view,
                                    ChromeColorIds hover_color_id,
                                    ChromeColorIds ripple_color_id);

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_UTIL_H_
