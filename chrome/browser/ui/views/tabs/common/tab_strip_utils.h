// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_STRIP_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_STRIP_UTILS_H_

#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "ui/gfx/geometry/rect.h"

class TabStripView;

namespace ui {
class GestureEvent;
}

namespace views {
class View;
}

// Returns the target bounds for the provided `view` in the tab strip
// hierarchy. For views managed by `TabCollectionAnimatingLayoutManager` this
// may differ from current `View::bounds()` due to animated transitions. For
// other views the current bounds will be returned.
gfx::Rect GetTabStripViewTargetBounds(const views::View* view);

// Returns the tab strip view for the provided `view`. Iterates
// through the parent hierarchy until a `TabStripView` is found.
TabStripView* GetTabStripView(views::View* view);

// Returns true if the `ScrollView` ancestor of `view` is scrollable along the
// primary direction of the given `kGestureScrollBegin` `event`.
bool CanScrollAlongAxis(const views::View* view,
                        TabStripOrientation orientation,
                        const ui::GestureEvent& event);

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_STRIP_UTILS_H_
