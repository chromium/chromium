// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_HORIZONTAL_TAB_STYLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_HORIZONTAL_TAB_STYLE_VIEWS_H_

#include <memory>
#include <optional>

#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

class BrowserFrameView;
class GlowHoverController;
class SkPath;

class HorizontalTabStyleViews : public TabStyleViews {
 public:
  explicit HorizontalTabStyleViews(
      std::unique_ptr<TabStyleViewDelegate> delegate);
  HorizontalTabStyleViews(const HorizontalTabStyleViews&) = delete;
  HorizontalTabStyleViews& operator=(const HorizontalTabStyleViews&) = delete;
  ~HorizontalTabStyleViews() override;

  // TabStyleViews:
  int GetStrokeThickness() const override;
  SkPath GetOverlinePath(float scale) const override;
  const TabStyleViewDelegate* delegate() const override;

 protected:
  SkPath GetPath(TabStyle::PathType path_type,
                 float scale,
                 const TabPathFlags& flags) const override;
  std::optional<SkPath> GetChildClipPath(
      float paint_recording_scale) const override;
  void PaintTab(gfx::Canvas* canvas) const override;
  gfx::Insets GetContentsInsets() const override;
  bool IsApparentlyActive() const override;
  TabStyle::TabColors CalculateTargetColors() const override;

  // Returns the progress (0 to 1) of the hover animation.
  double GetHoverAnimationValue() const override;

  GlowHoverController* GetHoverControllerForTesting() override;
  TabStyle::SeparatorOpacities GetSeparatorOpacitiesForTesting() const override;

 private:
  // Returns the color for the separator.
  SkColor GetTabSeparatorColor() const;

  // Painting helper functions:
  SkColor GetCurrentTabBackgroundColor(
      TabStyle::TabSelectionState selection_state,
      bool hovered) const;

  // Returns the current opacity of the "active" portion of the tab's state.
  float GetCurrentActiveOpacity() const;

  bool ShouldPaintTabBackgroundColor(
      TabStyle::TabSelectionState selection_state,
      bool has_custom_background) const;

  gfx::RectF ScaleAndAlignBounds(const gfx::Rect& bounds,
                                 float scale,
                                 int stroke_thickness) const;

  // Given a tab of width `width`, returns the radius to use for the corners.
  float GetTopCornerRadiusForWidth(int width) const;

  // Returns a single separator's opacity based on whether it is the
  // logically `leading` separator. `for_layout` has the same meaning as in
  // GetSeparatorOpacities().
  float GetSeparatorOpacity(bool for_layout, bool leading) const;

  // Helper that returns an interpolated opacity if the tab or its neighbor
  // `other_tab` is mid-hover-animation. Used in almost all cases when a
  // separator is shown, since hovering is independent of tab state.
  // `for_layout` has the same meaning as in GetSeparatorOpacities().
  float GetHoverInterpolatedSeparatorOpacity(
      bool for_layout,
      const TabStyleViewDelegate* other_tab) const;

  TabStyle::TabSelectionState GetSelectionState() const;

  // Gets the bounds for the leading and trailing separators for a tab.
  TabStyle::SeparatorBounds GetSeparatorBounds(float scale) const;

  // Returns the opacities of the separators. If `for_layout` is true, returns
  // the "layout" opacities, which ignore the effects of surrounding tabs' hover
  // effects and consider only the current tab's state.
  TabStyle::SeparatorOpacities GetSeparatorOpacities(bool for_layout) const;

  // Returns whether the mouse is currently hovering this tab.
  bool IsHovering() const;

  // Returns whether the hover animation is being shown.
  bool IsHoverAnimationActive() const;

  // Returns the opacity of the hover effect that should be drawn, which may not
  // be the same as GetHoverAnimationValue.
  float GetHoverOpacity() const;

  // Painting helper functions:
  void PaintTabBackground(gfx::Canvas* canvas,
                          bool hovered,
                          std::optional<int> fill_id) const;
  void PaintTabBackgroundWithImages(
      gfx::Canvas* canvas,
      std::optional<int> active_tab_fill_id,
      std::optional<int> inactive_tab_fill_id) const;
  void PaintTabBackgroundFill(gfx::Canvas* canvas,
                              bool hovered,
                              std::optional<int> fill_id) const;
  void PaintBackgroundHover(gfx::Canvas* canvas, float scale) const;
  void PaintBackgroundStroke(gfx::Canvas* canvas, SkColor stroke_color) const;
  void PaintSeparators(gfx::Canvas* canvas) const;

  BrowserFrameView* GetBrowserFrameView() const;

  std::optional<SkPath> GetPinnedPath(TabStyle::PathType path_type,
                                      float scale,
                                      const TabPathFlags& flags) const;

  std::unique_ptr<TabStyleViewDelegate> delegate_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_HORIZONTAL_TAB_STYLE_VIEWS_H_
