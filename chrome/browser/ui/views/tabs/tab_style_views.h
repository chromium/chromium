// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STYLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STYLE_VIEWS_H_

#include <memory>

#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/glow_hover_controller.h"
#include "ui/base/metadata/base_type_conversion.h"
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

namespace gfx {
class Canvas;
}

// Holds Views-specific logic for rendering and sizing tabs.
class TabStyleViews {
 public:
  // Factory function allows to experiment with different variations on tab
  // style at runtime or via flag.
  static std::unique_ptr<TabStyleViews> CreateForTab(Tab* tab);

  TabStyleViews();
  virtual ~TabStyleViews();

  // Gets the specific |path_type| associated with the specific |tab|.
  // If |force_active| is true, applies an active appearance on the tab (usually
  // involving painting an optional stroke) even if the tab is not the active
  //  tab.
  virtual SkPath GetPath(TabStyle::PathType path_type,
                         float scale,
                         bool force_active = false,
                         TabStyle::RenderUnits render_units =
                             TabStyle::RenderUnits::kPixels) const = 0;

  // Paints the tab.
  virtual void PaintTab(gfx::Canvas* canvas) const = 0;

  // Returns the insets to use for laying out tab contents.
  virtual gfx::Insets GetContentsInsets() const = 0;

  // Returns the z-value of the tab, which should be used to paint them in
  // ascending order. Return values are in the range (0,
  // TabStyle::GetMaximumZValue()).
  virtual float GetZValue() const = 0;

  // Returns whichever of (active, inactive) the tab appears more like given the
  // active opacity.
  virtual TabActive GetApparentActiveState() const = 0;

  // Returns the target opacity of the "active" portion of the tab's state. The
  // current opacity may be animating towards this value.
  virtual float GetTargetActiveOpacity() const = 0;

  // Returns the current opacity of the "active" portion of the tab's state.
  virtual float GetCurrentActiveOpacity() const = 0;

  // Derives and returns colors for the tab. See TabColors, above.
  virtual TabStyle::TabColors CalculateTargetColors() const = 0;

  // Sets the center of the radial highlight in the hover animation.
  virtual void SetHoverLocation(const gfx::Point& location) = 0;

  // Shows the hover animation.
  virtual void ShowHover(TabStyle::ShowHoverStyle style) = 0;

  // Hides the hover animation.
  virtual void HideHover(TabStyle::HideHoverStyle style) = 0;

  // Returns the progress (0 to 1) of the hover animation.
  virtual double GetHoverAnimationValue() const = 0;

  const TabStyle* tab_style() const { return tab_style_; }

 private:
  const raw_ptr<const TabStyle> tab_style_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STYLE_VIEWS_H_
