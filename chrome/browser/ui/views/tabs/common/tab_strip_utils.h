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

// Ref-counted layer ownership for views in the tab strip.
//
// Several independent features paint the same child views to layers for the
// duration of an operation: tab dragging, layout animations, and focus
// transitions. Each feature acquires a reference while it needs the layer and
// releases it when done. The view paints to a layer while at least one
// reference is held, so no feature needs to know which others are in flight.
//
// The count is stored as a property on the view, so it travels with the view
// if it is reparented (for example when a tab moves into or out of a group).

// Increments the layer reference count of `view`. When the count goes from
// zero to one, `view` is made to paint to a layer that does not fill its bounds
// opaquely.
void AcquireViewLayer(views::View* view);

// Decrements the layer reference count of `view`. When the count reaches zero,
// the layer is destroyed. Must be balanced with a prior AcquireViewLayer().
void ReleaseViewLayer(views::View* view);

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_STRIP_UTILS_H_
