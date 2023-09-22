// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_DICE_WEB_SIGNIN_INTERCEPTION_BACKDROP_LAYER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_DICE_WEB_SIGNIN_INTERCEPTION_BACKDROP_LAYER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/layer_observer.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget_observer.h"

namespace gfx {
class Canvas;
}

namespace views {
class View;
class Widget;
}  // namespace views

class Browser;

// Semi transparent layer covering the whole browser window.
// The layer has a "hole" so that the `highlighted_button` is peeking through
// it. This class assumes that the button is shaped as a "pill" or circle, and
// only relies on the size and position of the button to draw it.
// The backdrop supports window resizes and animation of the button, in
// particular the animation where the circular avatar expands into a pill.
// Does not cover bubbles that use a different widget on top of the browser.
class DiceWebSigninInterceptionBackdropLayer : public ui::LayerDelegate,
                                               public views::ViewObserver,
                                               public views::WidgetObserver,
                                               public ui::LayerObserver {
 public:
  DiceWebSigninInterceptionBackdropLayer(Browser& browser,
                                         views::View& highlighted_button);
  ~DiceWebSigninInterceptionBackdropLayer() override;

  DiceWebSigninInterceptionBackdropLayer(
      const DiceWebSigninInterceptionBackdropLayer&) = delete;
  DiceWebSigninInterceptionBackdropLayer& operator=(
      const DiceWebSigninInterceptionBackdropLayer&) = delete;

 private:
  // Updates the layer bounds based on the browser window size.
  void UpdateLayerBounds();

  // Updates the layer.
  void SchedulePaint();

  // Draws the dark backdrop. The backdrop does not always covers the whole
  // layer, as the layer may be larger than the browser window.
  void DrawDarkBackdrop(gfx::Canvas* canvas);

  // Draws the highlighted button in transparent color. This should be called
  // after `DrawDarkBackdrop()`.
  void DrawHighlightedButton(gfx::Canvas* canvas);

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  // ui::LayerObserver:
  void LayerDestroyed(ui::Layer* layer) override;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;
  void OnViewIsDeleting(views::View* observed_view) override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

  // The top-level layer that is superimposed over the browser window's root
  // layer while screen capture mode is active.
  ui::Layer backdrop_layer_;

  base::WeakPtr<Browser> browser_;
  raw_ptr<ui::Layer> browser_layer_;
  raw_ptr<views::View> highlighted_button_;

  base::ScopedObservation<views::View, views::ViewObserver>
      highlighted_button_observation_{this};
  base::ScopedObservation<ui::Layer, ui::LayerObserver>
      browser_layer_observation_{this};
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      browser_frame_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_DICE_WEB_SIGNIN_INTERCEPTION_BACKDROP_LAYER_H_
