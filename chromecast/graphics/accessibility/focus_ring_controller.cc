// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copied with modifications from ash/accessibility, refactored for use in
// chromecast.

#include "chromecast/graphics/accessibility/focus_ring_controller.h"

#include "chromecast/graphics/accessibility/focus_ring_layer.h"
#include "ui/aura/window.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/activation_client.h"

namespace chromecast {

FocusRingController::FocusRingController(
    aura::Window* root_window,
    wm::ActivationClient* activation_client)
    : root_window_(root_window),
      activation_client_(activation_client),
      visible_(false),
      widget_(nullptr) {
  DCHECK(root_window_);
  DCHECK(activation_client);
}

FocusRingController::~FocusRingController() {
  SetVisible(false);
  CHECK(!IsInObserverList());
}

void FocusRingController::SetVisible(bool visible) {
  if (visible_ == visible)
    return;

  visible_ = visible;

  if (visible_) {
    views::WidgetFocusManager::GetInstance()->AddFocusChangeListener(this);
    aura::Window* active_window = activation_client_->GetActiveWindow();
    if (active_window)
      SetWidget(views::Widget::GetWidgetForNativeWindow(active_window));
  } else {
    views::WidgetFocusManager::GetInstance()->RemoveFocusChangeListener(this);
    SetWidget(NULL);
  }
}

void FocusRingController::UpdateFocusRing() {
  views::View* view = NULL;
  if (widget_ && widget_->GetFocusManager())
    view = widget_->GetFocusManager()->GetFocusedView();

  // No focus ring if no focused view or the focused view covers the whole
  // widget content area (such as RenderWidgetHostWidgetAura).
  if (!view || view->ConvertRectToWidget(view->bounds()) ==
                   widget_->GetContentsView()->bounds()) {
    focus_ring_layer_.reset();
    return;
  }

  gfx::Rect view_bounds = view->GetContentsBounds();

  // Workarounds that attempts to pick a better bounds.
  if (view->GetClassName() == views::LabelButton::kViewClassName) {
    view_bounds = view->GetLocalBounds();
    view_bounds.Inset(2);
  }

  // Convert view bounds to widget/window coordinates.
  view_bounds = view->ConvertRectToWidget(view_bounds);

  // Translate window coordinates to root window coordinates.
  DCHECK(view->GetWidget());
  aura::Window* window = view->GetWidget()->GetNativeWindow();
  aura::Window* root_window = window->GetRootWindow();
  gfx::Point origin = view_bounds.origin();
  aura::Window::ConvertPointToTarget(window, root_window, &origin);
  view_bounds.set_origin(origin);

  // Update the focus ring layer.
  if (!focus_ring_layer_)
    focus_ring_layer_.reset(new FocusRingLayer(root_window_, this));
  focus_ring_layer_->Set(root_window, view_bounds);
}

void FocusRingController::OnDeviceScaleFactorChanged() {
  UpdateFocusRing();
}

void FocusRingController::OnAnimationStep(base::TimeTicks timestamp) {}

void FocusRingController::SetWidget(views::Widget* widget) {
  if (widget_) {
    widget_->RemoveObserver(this);
    if (widget_->GetFocusManager())
      widget_->GetFocusManager()->RemoveFocusChangeListener(this);
  }

  widget_ = widget;

  if (widget_) {
    widget_->AddObserver(this);
    if (widget_->GetFocusManager())
      widget_->GetFocusManager()->AddFocusChangeListener(this);
  }

  UpdateFocusRing();
}

void FocusRingController::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(widget_, widget);
  SetWidget(NULL);
}

void FocusRingController::OnWidgetBoundsChanged(views::Widget* widget,
                                                const gfx::Rect& new_bounds) {
  DCHECK_EQ(widget_, widget);
  UpdateFocusRing();
}

void FocusRingController::OnNativeFocusChanged(gfx::NativeView focused_now) {
  views::Widget* widget =
      focused_now ? views::Widget::GetWidgetForNativeWindow(focused_now) : NULL;
  SetWidget(widget);
}

void FocusRingController::OnWillChangeFocus(views::View* focused_before,
                                            views::View* focused_now) {}

void FocusRingController::OnDidChangeFocus(views::View* focused_before,
                                           views::View* focused_now) {
  DCHECK_EQ(focused_now, widget_->GetFocusManager()->GetFocusedView());
  UpdateFocusRing();
}

}  // namespace chromecast
