// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/shell_surface_util.h"

#include "base/trace_event/trace_event.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace exo {

namespace {

DEFINE_UI_CLASS_PROPERTY_KEY(Surface*, kMainSurfaceKey, nullptr)

// Application Id set by the client.
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kApplicationIdKey, nullptr)

// Application Id set by the client.
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kStartupIdKey, nullptr)

}  // namespace

void SetShellApplicationId(aura::Window* window,
                           const base::Optional<std::string>& id) {
  TRACE_EVENT1("exo", "SetApplicationId", "application_id", id ? *id : "null");

  if (id)
    window->SetProperty(kApplicationIdKey, *id);
  else
    window->ClearProperty(kApplicationIdKey);
}

const std::string* GetShellApplicationId(const aura::Window* window) {
  return window->GetProperty(kApplicationIdKey);
}

void SetShellStartupId(aura::Window* window,
                       const base::Optional<std::string>& id) {
  TRACE_EVENT1("exo", "SetStartupId", "startup_id", id ? *id : "null");

  if (id)
    window->SetProperty(kStartupIdKey, *id);
  else
    window->ClearProperty(kStartupIdKey);
}

const std::string* GetShellStartupId(aura::Window* window) {
  return window->GetProperty(kStartupIdKey);
}

void SetShellMainSurface(aura::Window* window, Surface* surface) {
  window->SetProperty(kMainSurfaceKey, surface);
}

Surface* GetShellMainSurface(const aura::Window* window) {
  return window->GetProperty(kMainSurfaceKey);
}

ShellSurfaceBase* GetShellSurfaceBaseForWindow(aura::Window* window) {
  // Only windows with a surface can have a shell surface.
  if (!GetShellMainSurface(window))
    return nullptr;
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  if (!widget)
    return nullptr;
  return static_cast<ShellSurfaceBase*>(widget->widget_delegate());
}

Surface* GetTargetSurfaceForLocatedEvent(ui::LocatedEvent* event) {
  aura::Window* window =
      WMHelper::GetInstance()->GetCaptureClient()->GetCaptureWindow();
  gfx::PointF location_in_target = event->location_f();

  if (!window)
    return Surface::AsSurface(static_cast<aura::Window*>(event->target()));

  Surface* main_surface = GetShellMainSurface(window);
  // Skip if the event is captured by non exo windows.
  if (!main_surface) {
    auto* widget = views::Widget::GetTopLevelWidgetForNativeView(window);
    if (!widget)
      return nullptr;
    main_surface = GetShellMainSurface(widget->GetNativeWindow());
    if (!main_surface)
      return nullptr;
  }

  while (true) {
    aura::Window* focused = window->GetEventHandlerForPoint(
        gfx::ToFlooredPoint(location_in_target));

    if (focused) {
      aura::Window::ConvertPointToTarget(window, focused, &location_in_target);
      return Surface::AsSurface(focused);
    }

    aura::Window* parent_window = wm::GetTransientParent(window);

    if (!parent_window) {
      location_in_target = event->location_f();
      return main_surface;
    }
    aura::Window::ConvertPointToTarget(window, parent_window,
                                       &location_in_target);
    window = parent_window;
  }
}

}  // namespace exo
