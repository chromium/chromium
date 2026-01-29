// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_UTILS_H_

#include "ui/gfx/geometry/rect.h"

namespace views {
class View;
}

// Returns the target bounds for the provided `view` in the vertical tab strip
// hierarchy. For views managed by `TabCollectionAnimatingLayoutManager` this
// may differ from current `View::bounds()` due to animated transitions. For
// other views the current bounds will be returned.
gfx::Rect GetVerticalTabStripViewTargetBounds(const views::View* view);

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_UTILS_H_
