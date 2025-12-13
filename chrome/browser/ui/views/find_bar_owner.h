// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FIND_BAR_OWNER_H_
#define CHROME_BROWSER_UI_VIEWS_FIND_BAR_OWNER_H_

#include <string>

#include "ui/gfx/geometry/rect.h"

namespace views {
class View;
class Widget;
}  // namespace views

// An interface implemented by a class that owns a FindBar. This is used to
// abstract the FindBar's owner for testing and to decouple FindBarHost from
// BrowserView.
class FindBarOwner {
 public:
  virtual ~FindBarOwner() = default;

  // Returns the owner's widget.
  virtual views::Widget* GetOwnerWidget() = 0;

  // Find Bar Layout
  // ---------------------------------------------------------------------------
  // The find bar layout is determined by the following steps:
  // 1. The find bar is positioned within the bounds returned by
  //    GetFindBarBoundingBox(). This is only a soft limit, see below.
  // 2. The positioning is adjusted to avoid overlapping the matched text in the
  //    WebContents. This might result in the find bar being positioned outside
  //    of the bounds returned by GetFindBarBoundingBox().
  // 3. The positioning is further adjusted so that it remains within the
  //    bounds returned by GetFindBarClippingBox().

  // Returns the bounding box for the find bar, in the owner widget's
  // coordinates. This is _not_ the size of the find bar, just the bounding
  // box it should be laid out within. This is only a soft limit, see "Find Bar
  // Layout" above.
  virtual gfx::Rect GetFindBarBoundingBox() = 0;

  // Returns the clipping box for the find bar, in the owner widget's
  // coordinates.
  virtual gfx::Rect GetFindBarClippingBox() = 0;

  // Returns true if the owner is using an off-the-record profile.
  virtual bool IsOffTheRecord() const = 0;

  // Returns the widget that the find bar widget should be anchored to.
  virtual views::Widget* GetWidgetForAnchoring() = 0;

  // Returns the accessible window title for the find bar widget.
  virtual std::u16string GetFindBarAccessibleWindowTitle() = 0;

  // Called when the find bar's visibility changes. `visible_bounds` is the
  // find bar's bounds if it's visible, or an empty rect if it's not.
  virtual void OnFindBarVisibilityChanged(gfx::Rect visible_bounds) = 0;

  // Closes any overlapping bubbles, such as the translate bubble.
  virtual void CloseOverlappingBubbles() = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FIND_BAR_OWNER_H_
