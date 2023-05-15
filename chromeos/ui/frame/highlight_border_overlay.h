// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_HIGHLIGHT_BORDER_OVERLAY_H_
#define CHROMEOS_UI_FRAME_HIGHLIGHT_BORDER_OVERLAY_H_

#include "ui/aura/window_observer.h"
#include "ui/compositor/layer.h"
#include "ui/display/display_observer.h"
#include "ui/display/tablet_state.h"

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
class HighlightBorderOverlay : public aura::WindowObserver,
                               public display::DisplayObserver {
 public:
  explicit HighlightBorderOverlay(views::Widget* widget);
  HighlightBorderOverlay(const HighlightBorderOverlay&) = delete;
  HighlightBorderOverlay& operator=(const HighlightBorderOverlay&) = delete;
  ~HighlightBorderOverlay() override;

  // Calculate image source size according to rounded corner radius and border
  // thickness.
  gfx::Size CalculateImageSourceSize() const;

  // aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowDestroying(aura::Window* window) override;

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;

 private:
  // Calculate the painting region of border.
  gfx::Insets CalculateBorderRegion() const;

  // Update layer visibility and bounds according to current window bounds and
  // work area.
  void UpdateLayerVisibilityAndBounds();

  // Update the nine patch layer with current highlight border settings.
  void UpdateNinePatchLayer();

  ui::Layer layer_;
  raw_ptr<views::Widget> widget_;
  raw_ptr<aura::Window> window_;
  int rounded_corner_radius_ = 0;

  display::ScopedDisplayObserver display_observer_{this};
};

#endif  // CHROMEOS_UI_FRAME_HIGHLIGHT_BORDER_OVERLAY_H_
