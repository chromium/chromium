// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STYLE_H_
#define CHROME_BROWSER_UI_TABS_TAB_STYLE_H_

#include <tuple>

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/tab_types.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class Canvas;
}

class SkPath;

// Holds the basic logic for rendering tabs, including preferred sizes, paths,
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

  enum class ShowHoverStyle { kSubtle, kPronounced };

  enum class HideHoverStyle {
    kGradual,    // The hover should fade out.
    kImmediate,  // The hover should cut off, with no fade out.
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

  // Colors for various parts of the tab derived by TabStyle.
  struct TabColors {
    SkColor foreground_color = gfx::kPlaceholderColor;
    SkColor background_color = gfx::kPlaceholderColor;
    ui::ColorId focus_ring_color = kColorTabFocusRingInactive;
    ui::ColorId close_button_focus_ring_color =
        kColorTabCloseButtonFocusRingInactive;

    TabColors() = default;
    TabColors(SkColor foreground_color,
              SkColor background_color,
              ui::ColorId focus_ring_color,
              ui::ColorId close_button_focus_ring_color)
        : foreground_color(foreground_color),
          background_color(background_color),
          focus_ring_color(focus_ring_color),
          close_button_focus_ring_color(close_button_focus_ring_color) {}
    bool operator==(const TabColors& other) const {
      return std::tie(foreground_color, background_color, focus_ring_color,
                      close_button_focus_ring_color) ==
             std::tie(other.foreground_color, other.background_color,
                      other.focus_ring_color,
                      other.close_button_focus_ring_color);
    }
  };

  TabStyle(const TabStyle&) = delete;
  TabStyle& operator=(const TabStyle&) = delete;
  virtual ~TabStyle();

  // Gets the specific |path_type| associated with the specific |tab|.
  // If |force_active| is true, applies an active appearance on the tab (usually
  // involving painting an optional stroke) even if the tab is not the active
  // tab.
  virtual SkPath GetPath(
      PathType path_type,
      float scale,
      bool force_active = false,
      RenderUnits render_units = RenderUnits::kPixels) const = 0;

  // Returns the insets to use for laying out tab contents.
  virtual gfx::Insets GetContentsInsets() const = 0;

  // Returns the z-value of the tab, which should be used to paint them in
  // ascending order. Return values are in the range (0,
  // TabStyle::GetMaximumZValue()).
  virtual float GetZValue() const = 0;

  // Returns the current opacity of the "active" portion of the tab's state.
  virtual float GetActiveOpacity() const = 0;

  // Returns whichever of (active, inactive) the tab appears more like given the
  // active opacity.
  virtual TabActive GetApparentActiveState() const = 0;

  // Derives and returns colors for the tab. See TabColors, above.
  virtual TabColors CalculateColors() const = 0;

  // Paints the tab.
  virtual void PaintTab(gfx::Canvas* canvas) const = 0;

  // Sets the center of the radial highlight in the hover animation.
  virtual void SetHoverLocation(const gfx::Point& location) = 0;

  // Shows the hover animation.
  virtual void ShowHover(ShowHoverStyle style) = 0;

  // Hides the hover animation.
  virtual void HideHover(HideHoverStyle style) = 0;

  // Opacity of the active tab background painted over inactive selected tabs.
  static constexpr float kSelectedTabOpacity = 0.75f;

  // Returns the preferred width of a single Tab, assuming space is
  // available.
  static int GetStandardWidth();

  // Returns the width for pinned tabs. Pinned tabs always have this width.
  static int GetPinnedWidth();

  // Returns the overlap between adjacent tabs.
  static int GetTabOverlap();

  // Get the space only partially occupied by a tab that we should
  // consider to be padding rather than part of the body of the tab for
  // interaction purposes.
  static gfx::Insets GetTabInternalPadding();

  // Gets the size of the separator drawn between tabs, if any.
  static gfx::Size GetSeparatorSize();

  // Returns, for a tab of height |height|, how far the window top drag handle
  // can extend down into inactive tabs or the new tab button. This behavior
  // is not used in all cases.
  static int GetDragHandleExtension(int height);

  // Gets the preferred size for tab previews, which could be screencaps, hero
  // or og:image images, etc.
  static gfx::Size GetPreviewImageSize();

  // Returns the radius of the outer corners of the tab shape.
  static int GetCornerRadius();

  // The largest valid value of TabStyle::GetZValue(). Currently,
  // GM2TabStyle::GetZValue is the only implementation, and it can't return
  // values larger than 7.
  static constexpr float kMaximumZValue = 7.0f;

 protected:
  // Avoid implicitly-deleted constructor.
  TabStyle() = default;

  // Returns how far from the leading and trailing edges of a tab the contents
  // should actually be laid out.
  static int GetContentsHorizontalInsetSize();
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STYLE_H_
