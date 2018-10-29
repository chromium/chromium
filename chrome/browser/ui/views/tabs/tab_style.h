// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STYLE_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STYLE_H_

#include "base/macros.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/path.h"

namespace gfx {
class Canvas;
}

class Tab;

// Holds all of the logic for rendering tabs, including preferred sizes, paths,
// etc.
class TabStyle {
 public:
  // The different types of path GetPath() can return. Different paths are used
  // in different situations, but most (excluding |kClip|) are roughly the same
  // shape.
  enum class PathType {
    // Interior fill outline. Extends halfway into the border so there are no
    // gaps between border and fill.
    kFill,
    // Center of the border path. The path is guaranteed to fit into the tab
    // bounds, including the stroke thickness.
    kBorder,
    // The hit test region. May be extended into a rectangle that touches the
    // top of the bounding box when the window is maximized, for Fitts' Law.
    kHitTest,
    // The area inside the tab where children can be rendered, used to clip
    // child views. Does not have to be the same shape as the border.
    kInteriorClip,
    // The outline of the tab, used for occlusion in certain special situations.
    kExteriorClip,
    // The path used for focus rings.
    kHighlight,
  };

  // How we want the resulting path scaled.
  enum class RenderUnits {
    // The path is in pixels, and should have its internal area nicely aligned
    // to pixel boundaries.
    kPixels,
    // The path is in DIPs. It will likely be calculated in pixels and then
    // scaled back down.
    kDips
  };

  // If we want to draw vertical separators between tabs, these are the leading
  // and trailing separator stroke rectangles.
  struct SeparatorBounds {
    gfx::RectF leading;
    gfx::RectF trailing;
  };

  // Contains values 0..1 representing the opacity of the corresponding
  // separators.  These are physical and not logical, so "left" is the left
  // separator in both LTR and RTL.
  struct SeparatorOpacities {
    float left = 0, right = 0;
  };

  // Creates an appropriate TabStyle instance for a particular tab.
  // Caller is responsibly for the TabStyle object's lifespan and should delete
  // it when finished.
  //
  // We've implemented this as a factory function so that when we're playing
  // with new variatons on tab shapes we can have a few possible implementations
  // and switch them in one place.
  static TabStyle* CreateForTab(const Tab* tab);

  virtual ~TabStyle();

  // Gets the specific |path_type| associated with the specific |tab|.
  // If |force_active| is true, applies an active appearance on the tab (usually
  // involving painting an optional stroke) even if the tab is not the active
  // tab.
  virtual gfx::Path GetPath(
      PathType path_type,
      float scale,
      bool force_active = false,
      RenderUnits render_units = RenderUnits::kPixels) const = 0;

  // Gets the bounds for the leading and trailing separators for a tab.
  virtual SeparatorBounds GetSeparatorBounds(float scale) const = 0;

  // Returns the thickness of the stroke drawn around the top and sides of the
  // tab.  Only active tabs may have a stroke, and not in all cases.  If there
  // is no stroke, returns 0.  If |should_paint_as_active| is true, the tab is
  // treated as an active tab regardless of its true current state.
  virtual int GetStrokeThickness(bool should_paint_as_active = false) const = 0;

  // Paint the tab.
  virtual void PaintTab(gfx::Canvas* canvas, const gfx::Path& clip) const = 0;

  // Returns the opacities of the separators.  If |for_layout| is true, returns
  // the "layout" opacities, which ignore the effects of surrounding tabs' hover
  // effects and consider only the current tab's state.
  virtual SeparatorOpacities GetSeparatorOpacities(bool for_layout) const = 0;

  // Returns the insets to use for laying out tab contents.
  virtual gfx::Insets GetContentsInsets() const = 0;

  // Returns the minimum possible width of a selected Tab. Selected tabs must
  // always show a close button, and thus have a larger minimum size than
  // unselected tabs.
  static int GetMinimumActiveWidth();

  // Returns the minimum possible width of a single unselected Tab.
  static int GetMinimumInactiveWidth();

  // Returns the preferred width of a single Tab, assuming space is
  // available.
  static int GetStandardWidth();

  // Returns the width for pinned tabs. Pinned tabs always have this width.
  static int GetPinnedWidth();

  // Returns the overlap between adjacent tabs.
  static int GetTabOverlap();

  // Returns, for a tab of height |height|, how far the window top drag handle
  // can extend down into inactive tabs or the new tab button. This behavior
  // is not used in all cases.
  static int GetDragHandleExtension(int height);

  // Get the space only partially occupied by a tab that we should
  // consider to be padding rather than part of the body of the tab for
  // interaction purposes.
  static gfx::Insets GetTabInternalPadding();

  // Gets the size of the separator drawn between tabs, if any.
  // TODO(dfried): Move any logic that needs this inside tab_style.cc.
  static gfx::Size GetSeparatorSize();

 protected:
  // Avoid implicitly-deleted constructor.
  TabStyle() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabStyle);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STYLE_H_
