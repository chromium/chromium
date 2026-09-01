// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_TAB_STYLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_TAB_STYLE_VIEWS_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"

class VerticalTabStyleViews : public TabStyleViews {
 public:
  explicit VerticalTabStyleViews(
      std::unique_ptr<TabStyleViewDelegate> delegate);
  VerticalTabStyleViews(const VerticalTabStyleViews&) = delete;
  VerticalTabStyleViews& operator=(const VerticalTabStyleViews&) = delete;
  ~VerticalTabStyleViews() override;

  // TabStyleViews:
  SkPath GetPath(TabStyle::PathType path_type,
                 float scale,
                 const TabPathFlags& flags) const override;
  std::optional<SkPath> GetChildClipPath(
      float paint_recording_scale) const override;
  void PaintTab(gfx::Canvas* canvas) const override;

  gfx::Insets GetContentsInsets() const override;
  int GetStrokeThickness() const override;
  SkPath GetOverlinePath(float scale) const override;

  bool IsApparentlyActive() const override;
  TabStyle::TabColors CalculateTargetColors() const override;
  const TabStyleViewDelegate* delegate() const override;
  double GetHoverAnimationValue() const override;
  GlowHoverController* GetHoverControllerForTesting() override;
  TabStyle::SeparatorOpacities GetSeparatorOpacitiesForTesting() const override;

 private:
  SkScalar GetCornerRadius() const;
  float GetCurrentActiveOpacity() const;
  void PaintTabBackgroundWithImages(
      gfx::Canvas* canvas,
      std::optional<int> active_tab_fill_id,
      std::optional<int> inactive_tab_fill_id) const;
  void PaintTabBackgroundFill(gfx::Canvas* canvas,
                              TabStyle::TabSelectionState selection_state,
                              bool hovered,
                              std::optional<int> fill_id) const;
  bool ShouldPaintTabBackgroundColor(
      TabStyle::TabSelectionState selection_state,
      bool has_custom_background,
      bool hovered) const;
  SkColor GetCurrentTabBackgroundColor(
      TabStyle::TabSelectionState selection_state) const;
  TabStyle::TabSelectionState GetSelectionState() const;

  std::unique_ptr<TabStyleViewDelegate> delegate_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_TAB_STYLE_VIEWS_H_
