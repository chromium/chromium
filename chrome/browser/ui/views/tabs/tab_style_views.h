// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STYLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STYLE_VIEWS_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "components/split_tabs/split_tab_id.h"
#include "components/tab_groups/tab_group_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/base_type_conversion.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

template <>
struct ui::metadata::TypeConverter<TabStyle::TabColors>
    : ui::metadata::BaseTypeConverter<true> {
  static std::u16string ToString(
      ui::metadata::ArgType<TabStyle::TabColors> source_value);
  static std::optional<TabStyle::TabColors> FromString(
      const std::u16string& source_value);
  static ui::metadata::ValidStrings GetValidStrings();
};

class Tab;
class BrowserFrameView;
class BrowserWindowInterface;
class GlowHoverController;
class SkPath;
struct TabPathFlags;

namespace views {
class View;
}

class TabStyleViewDelegate {
 public:
  virtual ~TabStyleViewDelegate() = default;

  virtual const views::View* GetView() const = 0;

  // Tab state
  virtual bool IsActive() const = 0;
  virtual bool IsSelected() const = 0;
  virtual bool IsHovering() const = 0;
  virtual bool IsClosing() const = 0;
  virtual bool IsDragging() const = 0;
  virtual std::optional<tab_groups::TabGroupId> GetGroup() const = 0;
  virtual std::optional<SkColor> GetGroupColor() const = 0;
  virtual bool IsInFocusedGroup() const = 0;
  virtual bool IsGroupCollapsed() const = 0;
  virtual bool IsSplit() const = 0;
  virtual std::optional<split_tabs::SplitTabId> GetSplit() const = 0;

  // Returns true if the tab is the leftmost in a set of split tabs. In RTL,
  // this will still return the leftmost tab which is needed when setting
  // left/right insets and positioning.
  virtual bool IsLeftSplitTab() const = 0;
  // Returns true if the tab is the rightmost in a set of split tabs. In RTL,
  // this will still return the rightmost tab which is needed when setting
  // left/right insets and positioning.
  virtual bool IsRightSplitTab() const = 0;
  virtual int GetTabCount() const = 0;

  virtual const TabStyleViewDelegate* GetAdjacentTab(bool leading) const = 0;
  virtual float GetHoverAnimationValue() const = 0;
  virtual float GetHoverOpacity() const = 0;
  virtual bool IsHoverAnimationActive() const = 0;
  virtual BrowserFrameView* GetBrowserFrameView() const = 0;
  virtual BrowserWindowInterface* GetBrowserWindowInterface() const = 0;
  virtual bool IsGlassFrame() const = 0;
  virtual bool IsPinned() const = 0;
  virtual bool ShouldPaintTabBackgroundColor() const = 0;
  virtual int GetStrokeThickness() const = 0;

  virtual GlowHoverController* GetHoverControllerForTesting() = 0;
};

namespace gfx {
class Canvas;
}

struct TabPathFlags {
  TabStyle::RenderUnits render_units = TabStyle::RenderUnits::kPixels;
  bool should_paint_extension = true;
};

// Holds Views-specific logic for rendering and sizing tabs.
class TabStyleViews {
 public:
  // Factory function allows to experiment with different variations on tab
  // style at runtime or via flag.
  static std::unique_ptr<TabStyleViews> Create(
      std::unique_ptr<TabStyleViewDelegate> delegate,
      TabStripOrientation orientation);

  TabStyleViews();
  virtual ~TabStyleViews();

  // Gets the specific `path_type` associated with the specific `tab`.
  // If `force_active` is true, applies an active appearance on the tab (usually
  // involving painting an optional stroke) even if the tab is not the active
  // tab.
  virtual SkPath GetPath(TabStyle::PathType path_type,
                         float scale,
                         const TabPathFlags& flags) const = 0;

  // Gets the path that children should be clipped to. Returns `nullopt` if
  // children shouldn't be clipped.
  virtual std::optional<SkPath> GetChildClipPath(
      float paint_recording_scale) const = 0;

  // Paints the tab.
  virtual void PaintTab(gfx::Canvas* canvas) const = 0;

  // Returns the insets to use for laying out tab contents.
  virtual gfx::Insets GetContentsInsets() const = 0;

  // Returns the thickness of the stroke drawn around the top and sides of the
  // tab. Only active tabs may have a stroke, and not in all cases. If there
  // is no stroke, returns 0.
  virtual int GetStrokeThickness() const = 0;

  // Returns the path of the overline of the active tab.
  virtual SkPath GetOverlinePath(float scale) const = 0;

  // Returns whether the tab appears more like the active opacity than the
  // inactive opacity.
  virtual bool IsApparentlyActive() const = 0;

  // Derives and returns colors for the tab. See TabColors, above.
  virtual TabStyle::TabColors CalculateTargetColors() const = 0;

  virtual const TabStyleViewDelegate* delegate() const = 0;

  // Returns the progress (0 to 1) of the hover animation.
  virtual double GetHoverAnimationValue() const = 0;

  virtual GlowHoverController* GetHoverControllerForTesting() = 0;

  virtual TabStyle::SeparatorOpacities GetSeparatorOpacitiesForTesting()
      const = 0;

  const TabStyle* tab_style() const { return tab_style_; }

 private:
  const raw_ptr<const TabStyle> tab_style_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STYLE_VIEWS_H_
