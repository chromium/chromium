// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_bounds_observer.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "chrome/browser/apps/platform_apps/app_window_registry_util.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/render_widget_host_view.h"
#include "extensions/browser/app_window/app_window.h"
#include "ui/aura/window.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/mojom/virtual_keyboard_types.mojom.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

// Returns the RenderWidgetHostView contained by |window|.
content::RenderWidgetHostView* GetHostViewForWindow(aura::Window* window) {
  std::unique_ptr<content::RenderWidgetHostIterator> hosts(
      content::RenderWidgetHost::GetRenderWidgetHosts());
  while (content::RenderWidgetHost* host = hosts->GetNextHost()) {
    content::RenderWidgetHostView* view = host->GetView();
    if (view && window->Contains(view->GetNativeView()))
      return view;
  }
  return nullptr;
}

ui::InputMethod* GetCurrentInputMethod() {
  ash::IMEBridge* bridge = ash::IMEBridge::Get();
  if (bridge && bridge->GetInputContextHandler())
    return bridge->GetInputContextHandler()->GetInputMethod();
  return nullptr;
}

}  // namespace

ChromeKeyboardBoundsObserver::ChromeKeyboardBoundsObserver(
    aura::Window* keyboard_window)
    : keyboard_window_(keyboard_window) {
  DCHECK(keyboard_window_);
  ChromeKeyboardControllerClient::Get()->AddObserver(this);
}

ChromeKeyboardBoundsObserver::~ChromeKeyboardBoundsObserver() {
  UpdateOccludedBounds(gfx::Rect());

  RemoveAllObservedWindows();

  ChromeKeyboardControllerClient::Get()->RemoveObserver(this);
  CHECK(!views::WidgetObserver::IsInObserverList());
}

void ChromeKeyboardBoundsObserver::OnKeyboardVisibleBoundsChanged(
    const gfx::Rect& screen_bounds) {
  std::unique_ptr<content::RenderWidgetHostIterator> hosts(
      content::RenderWidgetHost::GetRenderWidgetHosts());

  while (content::RenderWidgetHost* host = hosts->GetNextHost()) {
    content::RenderWidgetHostView* view = host->GetView();
    if (view)
      view->NotifyVirtualKeyboardOverlayRect(screen_bounds);
  }
}

void ChromeKeyboardBoundsObserver::OnKeyboardOccludedBoundsChanged(
    const gfx::Rect& screen_bounds) {
  DVLOG(1) << "OnKeyboardOccludedBoundsChanged: " << screen_bounds.ToString();
  UpdateOccludedBounds(
      ChromeKeyboardControllerClient::Get()->IsKeyboardOverscrollEnabled()
          ? screen_bounds
          : gfx::Rect());
}

void ChromeKeyboardBoundsObserver::UpdateOccludedBounds(
    const gfx::Rect& screen_bounds) {
  DVLOG(1) << "UpdateOccludedBounds: " << screen_bounds.ToString();
  occluded_bounds_in_screen_ = screen_bounds;

  std::unique_ptr<content::RenderWidgetHostIterator> hosts(
      content::RenderWidgetHost::GetRenderWidgetHosts());

  // If the keyboard is hidden or floating then reset the insets for all
  // RenderWidgetHosts and remove observers.
  if (occluded_bounds_in_screen_.IsEmpty()) {
    while (content::RenderWidgetHost* host = hosts->GetNextHost()) {
      content::RenderWidgetHostView* view = host->GetView();
      if (view)
        view->SetInsets(gfx::Insets());
    }
    RemoveAllObservedWindows();
    return;
  }

  // Adjust the height of the viewport for visible windows on the primary
  // display. TODO(kevers): Add EnvObserver to properly initialize insets if a
  // window is created while the keyboard is visible.
  while (content::RenderWidgetHost* host = hosts->GetNextHost()) {
    content::RenderWidgetHostView* view = host->GetView();
    // Can be null, e.g. if the RenderWidget is being destroyed or
    // the render process crashed.
    if (!view)
      continue;

    aura::Window* window = view->GetNativeView();
    // Added while we determine if RenderWidgetHostViewChildFrame can be
    // changed to always return a non-null value: https://crbug.com/644726.
    // If we cannot guarantee a non-null value, then this may need to stay.
    if (!window)
      continue;

    if (!ShouldWindowOverscroll(window))
      continue;

    UpdateInsets(window, view);
    AddObservedWindow(window);
  }

  // Window reshape can race with the IME trying to keep the text input caret
  // visible. Do this here because the widget bounds change happens before the
  // occluded bounds are updated. https://crbug.com/937722
  ui::InputMethod* ime = GetCurrentInputMethod();
  if (ime && ime->GetTextInputClient())
    ime->GetTextInputClient()->EnsureCaretNotInRect(occluded_bounds_in_screen_);
}

void ChromeKeyboardBoundsObserver::AddObservedWindow(aura::Window* window) {
  // Only observe top level widget.
  views::Widget* widget =
      views::Widget::GetWidgetForNativeView(window->GetToplevelWindow());
  if (!widget || widget->HasObserver(this))
    return;

  widget->AddObserver(this);
  observed_widgets_.insert(widget);
}

void ChromeKeyboardBoundsObserver::RemoveAllObservedWindows() {
  for (views::Widget* widget : observed_widgets_)
    widget->RemoveObserver(this);
  observed_widgets_.clear();
}

void ChromeKeyboardBoundsObserver::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  DVLOG(1) << "OnWidgetBoundsChanged: " << widget->GetName() << " "
           << new_bounds.ToString();

  aura::Window* window = widget->GetNativeView();
  if (!ShouldWindowOverscroll(window))
    return;

  content::RenderWidgetHostView* host_view = GetHostViewForWindow(window);
  if (!host_view)
    return;  // Transition edge case

  UpdateInsets(window, host_view);
}

void ChromeKeyboardBoundsObserver::OnWidgetDestroying(views::Widget* widget) {
  if (widget->HasObserver(this))
    widget->RemoveObserver(this);
  observed_widgets_.erase(widget);
}

void ChromeKeyboardBoundsObserver::UpdateInsets(
    aura::Window* window,
    content::RenderWidgetHostView* view) {
  if (view->GetVirtualKeyboardMode() ==
      ui::mojom::VirtualKeyboardMode::kOverlaysContent) {
    view->SetInsets(gfx::Insets());
    return;
  }
  gfx::Rect view_bounds_in_screen = view->GetViewBounds();
  if (!ShouldEnableInsets(window)) {
    DVLOG(2) << "ResetInsets: " << window->GetName()
             << " Bounds: " << view_bounds_in_screen.ToString();
    view->SetInsets(gfx::Insets());
    return;
  }
  gfx::Rect intersect =
      gfx::IntersectRects(view_bounds_in_screen, occluded_bounds_in_screen_);
  int overlap = intersect.height();
  DVLOG(2) << "SetInsets: " << window->GetName()
           << " Bounds: " << view_bounds_in_screen.ToString()
           << " Overlap: " << overlap;
  if (overlap > 0 && overlap < view_bounds_in_screen.height())
    view->SetInsets(gfx::Insets::TLBR(0, 0, overlap, 0));
  else
    view->SetInsets(gfx::Insets());
}

bool ChromeKeyboardBoundsObserver::ShouldWindowOverscroll(
    aura::Window* window) {
  // The virtual keyboard should not overscroll.
  if (window->GetToplevelWindow() == keyboard_window_->GetToplevelWindow())
    return false;

  // IME windows should not overscroll.
  extensions::AppWindow* app_window =
      AppWindowRegistryUtil::GetAppWindowForNativeWindowAnyProfile(
          window->GetToplevelWindow());
  if (app_window && app_window->is_ime_window())
    return false;

  return true;
}

bool ChromeKeyboardBoundsObserver::ShouldEnableInsets(aura::Window* window) {
  if (!keyboard_window_->IsVisible() ||
      !ChromeKeyboardControllerClient::Get()->IsKeyboardOverscrollEnabled()) {
    return false;
  }
  const auto* screen = display::Screen::GetScreen();
  return screen->GetDisplayNearestWindow(window).id() ==
         screen->GetDisplayNearestWindow(keyboard_window_).id();
}
