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

namespace ui {
class ColorProvider;
}

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

  // The states the tab can be in throughout its selection lifecycle.
  enum class TabSelectionState {
    kActive,
    kSelected,
    kInactive,
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

  // Returns the preferred width of a single Tab, assuming space is
  // available.
  virtual int GetStandardWidth() const = 0;

  // Returns the width for pinned tabs. Pinned tabs always have this width.
  virtual int GetPinnedWidth() const = 0;

  // Returns the minimum possible width of an active Tab. Active tabs must
  // always show a close button, and thus have a larger minimum size than
  // inactive tabs.
  virtual int GetMinimumActiveWidth() const = 0;

  // Returns the minimum possible width of a single inactive Tab.
  virtual int GetMinimumInactiveWidth() const = 0;

  // Returns the overlap between adjacent tabs.
  virtual int GetTabOverlap() const = 0;

  // Gets the size of the separator drawn between tabs, if any.
  virtual gfx::Size GetSeparatorSize() const = 0;

  // Gets the distance between the separator and tab, if any.
  virtual gfx::Insets GetSeparatorMargins() const = 0;

  // Gets the radius of the rounded rect used to draw the separator.
  virtual int GetSeparatorCornerRadius() const = 0;

  // Returns, for a tab of height |height|, how far the window top drag handle
  // can extend down into inactive tabs or the new tab button. This behavior
  // is not used in all cases.
  virtual int GetDragHandleExtension(int height) const = 0;

  // Gets the preferred size for tab previews, which could be screencaps, hero
  // or og:image images, etc.
  virtual gfx::Size GetPreviewImageSize() const = 0;

  // Returns the radius of the outer corners of the tab shape.
  virtual int GetTopCornerRadius() const = 0;

  // Returns the radius of the outer corners of the tab shape.
  virtual int GetBottomCornerRadius() const = 0;

  // Returns the background color of a tab with selection state `state`.
  // `frame_active` is whether the tab's widget is painted as active or not.
  // TODO(tbergquist): Non-Tab callers of this should probably be refactored to
  // use their own color ids.
  virtual SkColor GetTabBackgroundColor(
      TabSelectionState state,
      bool hovered,
      bool frame_active,
      const ui::ColorProvider& color_provider) const = 0;

  // Opacity of the active tab background painted over inactive selected tabs.
  virtual float GetSelectedTabOpacity() const = 0;

  // Returns how far from the leading and trailing edges of a tab the contents
  // should actually be laid out.
  virtual gfx::Insets GetContentsInsets() const = 0;

  // The largest valid value of TabStyle::GetZValue(). Currently,
  // GM2TabStyle::GetZValue is the only implementation, and it can't return
  // values larger than 7.
  static constexpr float kMaximumZValue = 7.0f;

  static constexpr float kDefaultSelectedTabOpacity = 0.75f;

  static const TabStyle* Get();

 protected:
  // Avoid implicitly-deleted constructor.
  TabStyle() = default;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STYLE_H_
