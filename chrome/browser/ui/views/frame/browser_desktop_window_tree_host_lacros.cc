// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_desktop_window_tree_host_lacros.h"

#include <optional>

#include "base/check.h"
#include "chrome/browser/ui/lacros/window_properties.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/desktop_browser_frame_lacros.h"
#include "chrome/browser/ui/views/tabs/tab_drag_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chromeos/ui/base/window_pin_type.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/frame/frame_utils.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/platform_window/extensions/wayland_extension.h"
#include "ui/platform_window/platform_window.h"

namespace {

// Returns the event source for the active tab drag session.
std::optional<ui::mojom::DragEventSource> GetCurrentTabDragEventSource() {
  if (auto* source_context = TabDragController::GetSourceContext()) {
    if (auto* drag_controller = source_context->GetDragController()) {
      return drag_controller->event_source();
    }
  }
  return std::nullopt;
}

bool IsPinned(ui::PlatformWindowState state) {
  return state == ui::PlatformWindowState::kPinnedFullscreen ||
         state == ui::PlatformWindowState::kTrustedPinnedFullscreen;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostLacros, public:

BrowserDesktopWindowTreeHostLacros::BrowserDesktopWindowTreeHostLacros(
    views::internal::NativeWidgetDelegate* native_widget_delegate,
    views::DesktopNativeWidgetAura* desktop_native_widget_aura,
    BrowserView* browser_view,
    BrowserFrame* browser_frame)
    : DesktopWindowTreeHostLacros(native_widget_delegate,
                                  desktop_native_widget_aura),
      browser_view_(browser_view),
      desktop_native_widget_aura_(desktop_native_widget_aura) {
  native_frame_ = static_cast<DesktopBrowserFrameLacros*>(
      browser_frame->native_browser_frame());
  native_frame_->set_host(this);

  // Lacros receives occlusion information from exo via aura-shell.
  SetNativeWindowOcclusionEnabled(true);
}

BrowserDesktopWindowTreeHostLacros::~BrowserDesktopWindowTreeHostLacros() {
  native_frame_->set_host(nullptr);
}


////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostLacros,
//     BrowserDesktopWindowTreeHost implementation:

views::DesktopWindowTreeHost*
BrowserDesktopWindowTreeHostLacros::AsDesktopWindowTreeHost() {
  return this;
}

int BrowserDesktopWindowTreeHostLacros::GetMinimizeButtonOffset() const {
  return 0;
}

bool BrowserDesktopWindowTreeHostLacros::UsesNativeSystemMenu() const {
  return false;
}

void BrowserDesktopWindowTreeHostLacros::TabDraggingKindChanged(
    TabDragKind tab_drag_kind) {
  // If there's no tabs left, the browser window is about to close, so don't
  // call SetOverrideRedirect() to prevent the window from flashing.
  if (!browser_view_->tabstrip()->GetModelCount() ||
      tab_drag_kind == TabDragKind::kNone)
    return;

  auto* wayland_extension = ui::GetWaylandExtension(*platform_window());
  if (auto event_source = GetCurrentTabDragEventSource()) {
    const auto allow_system_drag = base::FeatureList::IsEnabled(
        features::kAllowWindowDragUsingSystemDragDrop);
    wayland_extension->StartWindowDraggingSessionIfNeeded(*event_source,
                                                          allow_system_drag);
  }
}

bool BrowserDesktopWindowTreeHostLacros::SupportsMouseLock() {
  auto* wayland_extension = ui::GetWaylandExtension(*platform_window());
  return wayland_extension->SupportsPointerLock();
}

void BrowserDesktopWindowTreeHostLacros::LockMouse(aura::Window* window) {
  DesktopWindowTreeHostLacros::LockMouse(window);

  if (SupportsMouseLock()) {
    auto* wayland_extension = ui::GetWaylandExtension(*platform_window());
    wayland_extension->LockPointer(true /*enabled*/);
  }
}

void BrowserDesktopWindowTreeHostLacros::UnlockMouse(aura::Window* window) {
  DesktopWindowTreeHostLacros::UnlockMouse(window);

  if (SupportsMouseLock()) {
    auto* wayland_extension = ui::GetWaylandExtension(*platform_window());
    wayland_extension->LockPointer(false /*enabled*/);
  }
}

void BrowserDesktopWindowTreeHostLacros::OnWindowStateChanged(
    ui::PlatformWindowState old_state,
    ui::PlatformWindowState new_state) {
  DesktopWindowTreeHostLacros::OnWindowStateChanged(old_state, new_state);

  bool fullscreen_changed = ui::IsPlatformWindowStateFullscreen(new_state) ||
                            ui::IsPlatformWindowStateFullscreen(old_state);
  if (old_state != new_state && fullscreen_changed) {
    // Update WindowPinTypeKey before triggering BrowserView::ProcessFullscreen.
    if (IsPinned(old_state)) {
      CHECK(!IsPinned(new_state));
      desktop_native_widget_aura_->GetNativeWindow()->SetProperty(
          lacros::kWindowPinTypeKey, chromeos::WindowPinType::kNone);
    } else if (IsPinned(new_state)) {
      CHECK(!IsPinned(old_state));
      desktop_native_widget_aura_->GetNativeWindow()->SetProperty(
          lacros::kWindowPinTypeKey,
          new_state == ui::PlatformWindowState::kPinnedFullscreen
              ? chromeos::WindowPinType::kPinned
              : chromeos::WindowPinType::kTrustedPinned);
    }

    // If the browser view initiated this state change,
    // BrowserView::ProcessFullscreen will no-op, so this call is harmless.
    browser_view_->FullscreenStateChanging();
  }
}

void BrowserDesktopWindowTreeHostLacros::OnFullscreenTypeChanged(
    ui::PlatformFullscreenType old_type,
    ui::PlatformFullscreenType new_type) {
  DesktopWindowTreeHostLacros::OnFullscreenTypeChanged(old_type, new_type);

  // Finalizing full screen mode transition after Ash has also asynchronously
  // entered the full screen mode state for this window.
  browser_view_->FullscreenStateChanged();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostLacros,
//     DesktopWindowTreeHostPlatform implementation:

SkPath BrowserDesktopWindowTreeHostLacros::GetWindowMaskForClipping() const {
  // Lacros doesn't need to request clipping since it is already
  // done in views, so returns empty window mask.
  return SkPath();
}

void BrowserDesktopWindowTreeHostLacros::OnSurfaceFrameLockingChanged(
    bool lock) {
  aura::Window* window = desktop_native_widget_aura_->GetNativeWindow();
  DCHECK(window);
  window->SetProperty(chromeos::kFrameRestoreLookKey, lock);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHost, public:

// static
BrowserDesktopWindowTreeHost*
BrowserDesktopWindowTreeHost::CreateBrowserDesktopWindowTreeHost(
    views::internal::NativeWidgetDelegate* native_widget_delegate,
    views::DesktopNativeWidgetAura* desktop_native_widget_aura,
    BrowserView* browser_view,
    BrowserFrame* browser_frame) {
  return new BrowserDesktopWindowTreeHostLacros(native_widget_delegate,
                                                desktop_native_widget_aura,
                                                browser_view, browser_frame);
}
