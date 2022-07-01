// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_HIGHLIGHT_BORDER_OVERLAY_CHROMEOS_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_HIGHLIGHT_BORDER_OVERLAY_CHROMEOS_H_

#include "ui/compositor/layer.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/widget/widget_observer.h"

namespace gfx {
class Canvas;
}  // namespace gfx

namespace views {
class Widget;
}  // namespace views

// `HighlightBorderOverlay` is mainly used to add highlight border on the
// windows whose client area spans the entire width of the widget. To prevent
// the border from being covered by the client view, the class creates a nine
// patch layer painted with a highlight border and overlay on the widget. The
// inner border covers on the window contents and outer border is outside the
// window. It uses `kHighlightBorder3` as its border type which has low opacity
// of outer border.
class HighlightBorderOverlay : public views::WidgetObserver,
                               public display::DisplayObserver {
 public:
  HighlightBorderOverlay(views::Widget* widget,
                         const gfx::RoundedCornersF& rounded_corner);
  HighlightBorderOverlay(const HighlightBorderOverlay&) = delete;
  HighlightBorderOverlay& operator=(const HighlightBorderOverlay&) = delete;
  ~HighlightBorderOverlay() override;

  // Paint a highlight border on the canvas.
  void PaintBorder(gfx::Canvas* canvas);
  // Calculate image source size according to rounded corner radius and border
  // thickness.
  gfx::Size CalculateImageSourceSize() const;

  // WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;
  void OnWidgetThemeChanged(views::Widget* widget) override;

  // display::DisplayObserver
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

 private:
  // Calculate the painting region of border.
  gfx::Insets CalculateBorderRegion() const;

  // Update layer visibility according to current window bounds and work area.
  void UpdateLayerVisibility();
  // Update layer bounds according to current widget bounds.
  void UpdateLayerBounds();
  // Update the nine patch layer with current highlight border settings.
  void UpdateNinePatchLayer();

  ui::Layer layer_;
  base::raw_ptr<views::Widget> widget_;
  gfx::RoundedCornersF rounded_corner_;

  display::ScopedDisplayObserver display_observer_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_HIGHLIGHT_BORDER_OVERLAY_CHROMEOS_H_
