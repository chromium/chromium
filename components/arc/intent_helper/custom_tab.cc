// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/intent_helper/custom_tab.h"

#include <memory>
#include <string>
#include <utility>

#include "base/task/sequenced_task_runner.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace arc {

CustomTab::CustomTab(aura::Window* arc_app_window)
    : arc_app_window_(arc_app_window) {
  arc_app_window_observation_.Observe(arc_app_window_.get());
  host_->set_owned_by_client();
  auto* const widget = views::Widget::GetWidgetForNativeWindow(arc_app_window_);
  DCHECK(widget);
  widget->GetContentsView()->AddChildView(host_.get());
}

CustomTab::~CustomTab() {
  if (host_->GetWidget()) {
    host_->GetWidget()->GetContentsView()->RemoveChildView(host_.get());
  }
}

void CustomTab::Attach(gfx::NativeView view) {
  DCHECK(view);
  DCHECK(!GetHostView());
  host_->Attach(view);
  aura::Window* const container = host_->GetNativeViewContainer();
  container->SetEventTargeter(std::make_unique<aura::WindowTargeter>());
  other_windows_observation_.Observe(container);
  EnsureWindowOrders();
  UpdateHostBounds(arc_app_window_);
}

gfx::NativeView CustomTab::GetHostView() {
  return host_->native_view();
}

void CustomTab::OnWindowBoundsChanged(aura::Window* window,
                                      const gfx::Rect& old_bounds,
                                      const gfx::Rect& new_bounds,
                                      ui::PropertyChangeReason reason) {
  if (arc_app_window_observation_.IsObservingSource(window) &&
      old_bounds.size() != new_bounds.size()) {
    UpdateHostBounds(window);
  }
}

void CustomTab::OnWindowStackingChanged(aura::Window* window) {
  if (window == host_->GetNativeViewContainer() &&
      !weak_ptr_factory_.HasWeakPtrs()) {
    // Reordering should happen asynchronously -- some entity (like
    // views::WindowReorderer) changes the window orders, and then ensures layer
    // orders later. Changing order here synchronously leads to inconsistent
    // window/layer ordering and causes weird graphical effects.
    // TODO(hashimoto): fix the views ordering and remove this handling.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&CustomTab::EnsureWindowOrders,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

void CustomTab::OnWindowDestroying(aura::Window* window) {
  arc_app_window_observation_.Reset();
  other_windows_observation_.Reset();
}

void CustomTab::UpdateHostBounds(aura::Window* arc_app_window) {
  DCHECK(arc_app_window);
  auto* surface = exo::GetShellRootSurface(arc_app_window);
  if (!surface) {
    return;
  }

  aura::Window* surface_window = surface->window();
  gfx::Point origin(0, 0);
  gfx::Point bottom_right(surface_window->bounds().width(),
                          surface_window->bounds().height());
  ConvertPointFromWindow(surface_window, &origin);
  ConvertPointFromWindow(surface_window, &bottom_right);
  host_->SetBounds(origin.x(), origin.y(), bottom_right.x() - origin.x(),
                   bottom_right.y() - origin.y());
}

void CustomTab::EnsureWindowOrders() {
  aura::Window* const container = host_->GetNativeViewContainer();
  if (container) {
    container->parent()->StackChildAtTop(container);
  }
}

void CustomTab::ConvertPointFromWindow(aura::Window* window,
                                       gfx::Point* point) {
  views::Widget* const widget = host_->GetWidget();
  aura::Window::ConvertPointToTarget(window, widget->GetNativeWindow(), point);
  views::View::ConvertPointFromWidget(widget->GetContentsView(), point);
}

}  // namespace arc
