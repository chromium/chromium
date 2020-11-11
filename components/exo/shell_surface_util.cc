// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/shell_surface_util.h"

#include <memory>

#include "ash/public/cpp/app_types.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "components/exo/permission.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_targeter.h"
#include "ui/events/event.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

#if defined(OS_CHROMEOS)
#include "chromeos/ui/base/window_properties.h"
#endif  // defined(OS_CHROMEOS)

DEFINE_UI_CLASS_PROPERTY_TYPE(exo::Permission*)

namespace exo {

namespace {

DEFINE_UI_CLASS_PROPERTY_KEY(Surface*, kMainSurfaceKey, nullptr)

// Application Id set by the client. For example:
// "org.chromium.arc.<task-id>" for ARC++ shell surfaces.
// "org.chromium.lacros.<window-id>" for Lacros browser shell surfaces.
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kApplicationIdKey, nullptr)

// Startup Id set by the client.
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kStartupIdKey, nullptr)

// Accessibility Id set by the client.
DEFINE_UI_CLASS_PROPERTY_KEY(int32_t, kClientAccessibilityIdKey, -1)

// Permission object allowing this window to activate itself.
DEFINE_UI_CLASS_PROPERTY_KEY(exo::Permission*, kPermissionKey, nullptr)

// Returns true if the component for a located event should be taken care of
// by the window system.
bool ShouldHTComponentBlocked(int component) {
  switch (component) {
    case HTCAPTION:
    case HTCLOSE:
    case HTMAXBUTTON:
    case HTMINBUTTON:
    case HTMENU:
    case HTSYSMENU:
      return true;
    default:
      return false;
  }
}

// Find the lowest targeter in the parent chain.
aura::WindowTargeter* FindTargeter(ui::EventTarget* target) {
  do {
    ui::EventTargeter* targeter = target->GetEventTargeter();
    if (targeter)
      return static_cast<aura::WindowTargeter*>(targeter);
    target = target->GetParentTarget();
  } while (target);

  return nullptr;
}

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

void SetArcAppType(aura::Window* window) {
  window->SetProperty(aura::client::kAppType,
                      static_cast<int>(ash::AppType::ARC_APP));
}

void SetLacrosAppType(aura::Window* window) {
  window->SetProperty(aura::client::kAppType,
                      static_cast<int>(ash::AppType::LACROS));
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

void SetShellUseImmersiveForFullscreen(aura::Window* window, bool value) {
#if defined(OS_CHROMEOS)
  window->SetProperty(chromeos::kImmersiveImpliedByFullscreen, value);

  // Ensure the shelf is fully hidden in plain fullscreen, but shown
  // (auto-hides based on mouse movement) when in immersive fullscreen.
  window->SetProperty(chromeos::kHideShelfWhenFullscreenKey, !value);
#endif  // defined(OS_CHROMEOS)
}

void SetShellClientAccessibilityId(aura::Window* window,
                                   const base::Optional<int32_t>& id) {
  TRACE_EVENT1("exo", "SetClientAccessibilityId", "id",
               id ? base::NumberToString(*id) : "null");

  if (id)
    window->SetProperty(kClientAccessibilityIdKey, *id);
  else
    window->ClearProperty(kClientAccessibilityIdKey);
}

const base::Optional<int32_t> GetShellClientAccessibilityId(
    aura::Window* window) {
  auto id = window->GetProperty(kClientAccessibilityIdKey);
  if (id < 0)
    return base::nullopt;
  else
    return id;
}

bool IsShellMainSurfaceKey(const void* key) {
  return kMainSurfaceKey == key;
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

Surface* GetTargetSurfaceForLocatedEvent(
    const ui::LocatedEvent* original_event) {
  aura::Window* window =
      WMHelper::GetInstance()->GetCaptureClient()->GetCaptureWindow();
  if (!window) {
    return Surface::AsSurface(
        static_cast<aura::Window*>(original_event->target()));
  }

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

  // Create a clone of the event as targeter may update it during the
  // search.
  auto cloned = ui::Event::Clone(*original_event);
  ui::LocatedEvent* event = cloned->AsLocatedEvent();

  while (true) {
    gfx::PointF location_in_target_f = event->location_f();
    gfx::Point location_in_target = event->location();
    ui::EventTarget* event_target = window;
    aura::WindowTargeter* targeter = FindTargeter(event_target);
    DCHECK(targeter);

    aura::Window* focused =
        static_cast<aura::Window*>(targeter->FindTargetForEvent(window, event));

    if (focused) {
      Surface* surface = Surface::AsSurface(focused);
      if (focused != window)
        return surface;
      else if (surface && surface->HitTest(location_in_target)) {
        // If the targeting fallback to the root (first) window, test the
        // hit region again.
        return surface;
      }
    }

    // If the event falls into the place where the window system should care
    // about (i.e. window caption), do not check the transient parent but just
    // return nullptr. See b/149517682.
    if (window->delegate() &&
        ShouldHTComponentBlocked(
            window->delegate()->GetNonClientComponent(location_in_target))) {
      return nullptr;
    }

    aura::Window* parent_window = wm::GetTransientParent(window);

    if (!parent_window)
      return main_surface;

    event->set_location_f(location_in_target_f);
    event_target->ConvertEventToTarget(parent_window, event);
    window = parent_window;
  }
}

namespace {

// An activation-permission object whose lifetime is tied to a window property.
class ScopedWindowActivationPermission : public Permission {
 public:
  ScopedWindowActivationPermission(aura::Window* window,
                                   base::TimeDelta timeout)
      : Permission(Permission::Capability::kActivate, timeout),
        window_(window) {
    Permission* other = window_->GetProperty(kPermissionKey);
    if (other) {
      other->Revoke();
    }
    window_->SetProperty(kPermissionKey, reinterpret_cast<Permission*>(this));
  }

  ~ScopedWindowActivationPermission() override {
    if (window_->GetProperty(kPermissionKey) == this)
      window_->ClearProperty(kPermissionKey);
  }

 private:
  aura::Window* window_;
};

}  // namespace

std::unique_ptr<Permission> GrantPermissionToActivate(aura::Window* window,
                                                      base::TimeDelta timeout) {
  return std::make_unique<ScopedWindowActivationPermission>(window, timeout);
}

bool HasPermissionToActivate(aura::Window* window) {
  Permission* permission = window->GetProperty(kPermissionKey);
  return permission && permission->Check(Permission::Capability::kActivate);
}

}  // namespace exo
