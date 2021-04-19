// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_VIEW_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_VIEW_UTILS_H_

#include "ui/gfx/geometry/rect.h"

// Sets the |x| and |width| components of |popup_bounds| as the x-coordinate
// of the starting point and the width of the popup, taking into account the
// direction it's supposed to grow (either to the left or to the right).
// Components |y| and |height| of |popup_bounds| are not changed.
void CalculatePopupXAndWidth(int popup_preferred_width,
                             const gfx::Rect& window_bounds,
                             const gfx::Rect& element_bounds,
                             bool is_rtl,
                             gfx::Rect* popup_bounds);

// Sets the |y| and |height| components of |popup_bounds| as the y-coordinate
// of the starting point and the height of the popup, taking into account the
// direction it's supposed to grow (either up or down). Components |x| and
// |width| of |popup_bounds| are not changed.
void CalculatePopupYAndHeight(int popup_preferred_height,
                              const gfx::Rect& window_bounds,
                              const gfx::Rect& element_bounds,
                              gfx::Rect* popup_bounds);

// Convenience method which handles both the vertical and horizontal bounds
// and returns a new Rect.
gfx::Rect CalculatePopupBounds(const gfx::Size& desired_size,
                               const gfx::Rect& window_bounds,
                               const gfx::Rect& element_bounds,
                               bool is_rtl);

// Returns whether there is enough height within |content_area_bounds| above or
// below |element_bounds| to display |item_height|, and that the first dropdown
// item will actually be visible within the bounds of the content area.
bool CanShowDropdownHere(int item_height,
                         const gfx::Rect& content_area_bounds,
                         const gfx::Rect& element_bounds);

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_VIEW_UTILS_H_
