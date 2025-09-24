// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_native_widget_aura.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_desktop_window_tree_host.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_observer.h"
#include "ui/base/hit_test.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/gfx/font.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/visibility_controller.h"

using aura::Window;

///////////////////////////////////////////////////////////////////////////////
// BrowserNativeWidgetAura, public:

BrowserNativeWidgetAura::BrowserNativeWidgetAura(BrowserWidget* browser_widget,
                                                 BrowserView* browser_view)
    : views::DesktopNativeWidgetAura(browser_widget),
      browser_view_(browser_view),
      browser_widget_(browser_widget),
      browser_desktop_window_tree_host_(nullptr) {
  GetNativeWindow()->SetName("BrowserFrameAura");
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNativeWidgetAura, protected:

BrowserNativeWidgetAura::~BrowserNativeWidgetAura() = default;

///////////////////////////////////////////////////////////////////////////////
// BrowserNativeWidgetAura, views::DesktopNativeWidgetAura overrides:

void BrowserNativeWidgetAura::OnHostClosed() {
  browser_widget_ = nullptr;
  browser_view_ = nullptr;
  aura::client::SetVisibilityClient(GetNativeView()->GetRootWindow(), nullptr);
  DesktopNativeWidgetAura::OnHostClosed();
}

void BrowserNativeWidgetAura::InitNativeWidget(
    views::Widget::InitParams params) {
  browser_desktop_window_tree_host_ =
      BrowserDesktopWindowTreeHost::CreateBrowserDesktopWindowTreeHost(
          browser_widget_, this, browser_view_, browser_widget_);
  params.desktop_window_tree_host =
      browser_desktop_window_tree_host_->AsDesktopWindowTreeHost();
  DesktopNativeWidgetAura::InitNativeWidget(std::move(params));

  visibility_controller_ = std::make_unique<wm::VisibilityController>();
  aura::client::SetVisibilityClient(GetNativeView()->GetRootWindow(),
                                    visibility_controller_.get());
  wm::SetChildWindowVisibilityChangesAnimated(GetNativeView()->GetRootWindow());
}

void BrowserNativeWidgetAura::OnOcclusionStateChanged(
    aura::WindowTreeHost* host,
    aura::Window::OcclusionState new_state,
    const SkRegion& occluded_region) {
  if (browser_view_) {
    browser_view_->UpdateLoadingAnimations(
        new_state == aura::Window::OcclusionState::VISIBLE);
  }
}

////////////////////////////////////////////////////////////////////////////////
// BrowserNativeWidgetAura, BrowserNativeWidget implementation:

views::Widget::InitParams BrowserNativeWidgetAura::GetWidgetParams(
    views::Widget::InitParams::Ownership ownership) {
  views::Widget::InitParams params(ownership);
  params.native_widget = this;
  return params;
}

bool BrowserNativeWidgetAura::UseCustomFrame() const {
  return true;
}

bool BrowserNativeWidgetAura::UsesNativeSystemMenu() const {
  return browser_desktop_window_tree_host_->UsesNativeSystemMenu();
}

bool BrowserNativeWidgetAura::ShouldSaveWindowPlacement() const {
  // The placement can always be stored.
  return true;
}

void BrowserNativeWidgetAura::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::mojom::WindowShowState* show_state) const {
  *bounds = GetWidget()->GetRestoredBounds();
  if (IsMaximized()) {
    *show_state = ui::mojom::WindowShowState::kMaximized;
  } else if (IsMinimized()) {
    *show_state = ui::mojom::WindowShowState::kMinimized;
  } else {
    *show_state = ui::mojom::WindowShowState::kNormal;
  }
}

content::KeyboardEventProcessingResult
BrowserNativeWidgetAura::PreHandleKeyboardEvent(
    const input::NativeWebKeyboardEvent& event) {
  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}

bool BrowserNativeWidgetAura::HandleKeyboardEvent(
    const input::NativeWebKeyboardEvent& event) {
  return false;
}

bool BrowserNativeWidgetAura::ShouldRestorePreviousBrowserWidgetState() const {
  return true;
}

bool BrowserNativeWidgetAura::ShouldUseInitialVisibleOnAllWorkspaces() const {
  return true;
}

void BrowserNativeWidgetAura::ClientDestroyedWidget() {
  browser_widget_ = nullptr;
  browser_view_ = nullptr;
  DesktopNativeWidgetAura::ClientDestroyedWidget();
}
