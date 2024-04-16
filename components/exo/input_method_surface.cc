// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/input_method_surface.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "components/exo/input_method_surface_manager.h"
#include "components/exo/wm_helper.h"
#include "ui/base/class_property.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/views/accessibility/view_accessibility.h"

DEFINE_UI_CLASS_PROPERTY_KEY(exo::InputMethodSurface*,
                             kInputMethodSurface,
                             nullptr)
DEFINE_UI_CLASS_PROPERTY_TYPE(exo::InputMethodSurface*)

namespace exo {

InputMethodSurface::InputMethodSurface(InputMethodSurfaceManager* manager,
                                       Surface* surface,
                                       bool default_scale_cancellation)
    : ClientControlledShellSurface(
          surface,
          true /* can_minimize */,
          ash::kShellWindowId_ArcVirtualKeyboardContainer,
          default_scale_cancellation,
          /*supports_floated_state=*/false),
      manager_(manager),
      input_method_bounds_() {
  host_window()->SetName("ExoInputMethodSurface");
  host_window()->SetProperty(kInputMethodSurface, this);
}

InputMethodSurface::~InputMethodSurface() {
  if (added_to_manager_)
    manager_->RemoveSurface(this);
}

exo::InputMethodSurface* InputMethodSurface::GetInputMethodSurface() {
  WMHelper* wm_helper = exo::WMHelper::GetInstance();
  if (!wm_helper)
    return nullptr;

  aura::Window* container = wm_helper->GetPrimaryDisplayContainer(
      ash::kShellWindowId_ArcVirtualKeyboardContainer);
  if (!container)
    return nullptr;

  // Host window of InputMethodSurface is grandchild of the container.
  if (container->children().empty())
    return nullptr;

  aura::Window* child = container->children().at(0);

  if (child->children().empty())
    return nullptr;

  aura::Window* host_window = child->children().at(0);
  return host_window->GetProperty(kInputMethodSurface);
}

void InputMethodSurface::OnSurfaceCommit() {
  ClientControlledShellSurface::OnSurfaceCommit();

  if (!added_to_manager_) {
    added_to_manager_ = true;
    manager_->AddSurface(this);
  }

  gfx::RectF new_bounds_in_dips = gfx::ConvertRectToDips(
      root_surface()->hit_test_region().bounds(), GetScale());
  // TODO(crbug.com/40150312): We should avoid dropping precision to integers
  // here if we want to know the true rectangle bounds in DIPs. If not, we
  // should use ToEnclosingRect() if we want to include DIPs that partly overlap
  // the physical pixel bounds, or ToEnclosedRect() if we do not.
  gfx::Rect int_bounds_in_dips =
      gfx::ToFlooredRectDeprecated(new_bounds_in_dips);
  if (input_method_bounds_ != int_bounds_in_dips) {
    input_method_bounds_ = int_bounds_in_dips;
    manager_->OnTouchableBoundsChanged(this);

    GetViewAccessibility().SetBounds(gfx::RectF(input_method_bounds_));
  }
}

void InputMethodSurface::SetWidgetBounds(const gfx::Rect& bounds,
                                         bool adjusted_by_server) {
  if (bounds == widget_->GetWindowBoundsInScreen())
    return;

  widget_->SetBounds(bounds);
  UpdateHostWindowOrigin();

  // Bounds change requests will be ignored in client side.
}

gfx::Rect InputMethodSurface::GetBounds() const {
  return input_method_bounds_;
}

}  // namespace exo
