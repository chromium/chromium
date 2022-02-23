// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copied with modifications from ash/accessibility, refactored for use in
// chromecast.

#ifndef CHROMECAST_GRAPHICS_ACCESSIBILITY_FOCUS_RING_CONTROLLER_H_
#define CHROMECAST_GRAPHICS_ACCESSIBILITY_FOCUS_RING_CONTROLLER_H_

#include <memory>

#include "chromecast/graphics/accessibility/focus_ring_layer.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/focus/widget_focus_manager.h"
#include "ui/views/widget/widget_observer.h"

namespace wm {
class ActivationClient;
}  // namespace wm

namespace views {
class View;
class Widget;
}  // namespace views

namespace chromecast {

// FocusRingController manages the focus ring around the focused view. It
// follows widget focus change and update the focus ring layer when the focused
// view of the widget changes.
class FocusRingController : public AccessibilityLayerDelegate,
                            public views::WidgetObserver,
                            public views::WidgetFocusChangeListener,
                            public views::FocusChangeListener {
 public:
  explicit FocusRingController(aura::Window* root_window,
                               wm::ActivationClient* activation_client);

  FocusRingController(const FocusRingController&) = delete;
  FocusRingController& operator=(const FocusRingController&) = delete;

  ~FocusRingController() override;

  // Turns on/off the focus ring.
  void SetVisible(bool visible);

 private:
  // AccessibilityLayerDelegate.
  void OnDeviceScaleFactorChanged() override;
  void OnAnimationStep(base::TimeTicks timestamp) override;

  // Sets the focused |widget|.
  void SetWidget(views::Widget* widget);

  // Updates the focus ring to the focused view of |widget_|. If |widget_| is
  // NULL or has no focused view, removes the focus ring. Otherwise, draws it.
  void UpdateFocusRing();

  // views::WidgetObserver overrides:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

  // views::WidgetFocusChangeListener overrides:
  void OnNativeFocusChanged(gfx::NativeView focused_now) override;

  // views::FocusChangeListener overrides:
  void OnWillChangeFocus(views::View* focused_before,
                         views::View* focused_now) override;
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override;

  aura::Window* root_window_;
  wm::ActivationClient* activation_client_;

  bool visible_;
  views::Widget* widget_;
  std::unique_ptr<FocusRingLayer> focus_ring_layer_;
};

}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_ACCESSIBILITY_FOCUS_RING_CONTROLLER_H_
