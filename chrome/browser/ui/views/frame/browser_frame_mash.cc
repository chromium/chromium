// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_mash.h"

#include <stdint.h>

#include <memory>

#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/cpp/window_state_type.h"
#include "ash/public/interfaces/window_properties.mojom.h"
#include "chrome/browser/ui/browser_window_state.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_frame_ash.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_ash.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/extensions/extension_constants.h"
#include "services/ws/public/cpp/property_type_converters.h"
#include "services/ws/public/mojom/window_manager.mojom.h"
#include "services/ws/public/mojom/window_tree.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/mus/window_tree_host_mus_init_params.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/mus/desktop_window_tree_host_mus.h"
#include "ui/views/mus/mus_client.h"

BrowserFrameMash::BrowserFrameMash(BrowserFrame* browser_frame,
                                   BrowserView* browser_view)
    : views::DesktopNativeWidgetAura(browser_frame),
      browser_frame_(browser_frame),
      browser_view_(browser_view) {
  DCHECK(browser_frame_);
  DCHECK(browser_view_);
  DCHECK(features::IsUsingWindowService());
}

BrowserFrameMash::~BrowserFrameMash() {}

void BrowserFrameMash::OnWindowTargetVisibilityChanged(bool visible) {
  if (visible && !browser_view_->browser()->is_type_popup()) {
    // Once the window has been shown we know the requested bounds
    // (if provided) have been honored and we can switch on window management.
    GetNativeWindow()->GetRootWindow()->SetProperty(
        ash::kWindowPositionManagedTypeKey, true);
  }
  views::DesktopNativeWidgetAura::OnWindowTargetVisibilityChanged(visible);
}

views::Widget::InitParams BrowserFrameMash::GetWidgetParams() {
  views::Widget::InitParams params;
  params.name = "BrowserFrame";
  params.native_widget = this;
  chrome::GetSavedWindowBoundsAndShowState(browser_view_->browser(),
                                           &params.bounds, &params.show_state);
  params.delegate = browser_view_;
  // The client will draw the frame.
  params.remove_standard_frame = true;

  std::map<std::string, std::vector<uint8_t>> properties =
      views::MusClient::ConfigurePropertiesFromParams(params);

  // ChromeLauncherController manages the browser shortcut shelf item; set the
  // window's shelf item type property to be ignored by ash::ShelfWindowWatcher.
  properties[ws::mojom::WindowManager::kShelfItemType_Property] =
      mojo::ConvertTo<std::vector<uint8_t>>(
          static_cast<int64_t>(ash::TYPE_BROWSER_SHORTCUT));

  Browser* browser = browser_view_->browser();
  properties[ash::mojom::kWindowPositionManaged_Property] =
      mojo::ConvertTo<std::vector<uint8_t>>(static_cast<int64_t>(
          !browser->bounds_overridden() && !browser->is_session_restore() &&
          !browser->is_type_popup()));
  properties[ash::mojom::kCanConsumeSystemKeys_Property] =
      mojo::ConvertTo<std::vector<uint8_t>>(
          static_cast<int64_t>(browser->is_app()));

  aura::WindowTreeHostMusInitParams window_tree_host_init_params =
      aura::CreateInitParamsForTopLevel(
          views::MusClient::Get()->window_tree_client(), std::move(properties));
  std::unique_ptr<views::DesktopWindowTreeHostMus> desktop_window_tree_host =
      std::make_unique<views::DesktopWindowTreeHostMus>(
          std::move(window_tree_host_init_params), browser_frame_, this);
  // BrowserNonClientFrameViewAsh::OnBoundsChanged() takes care of updating
  // the insets.
  desktop_window_tree_host->set_auto_update_client_area(false);
  SetDesktopWindowTreeHost(std::move(desktop_window_tree_host));
  return params;
}

bool BrowserFrameMash::UseCustomFrame() const {
  return true;
}

bool BrowserFrameMash::UsesNativeSystemMenu() const {
  return false;
}

bool BrowserFrameMash::ShouldSaveWindowPlacement() const {
  return nullptr == GetWidget()->GetNativeWindow()->GetProperty(
                        ash::kRestoreBoundsOverrideKey);
}

void BrowserFrameMash::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  DesktopNativeWidgetAura::GetWindowPlacement(bounds, show_state);

  gfx::Rect* override_bounds = GetWidget()->GetNativeWindow()->GetProperty(
      ash::kRestoreBoundsOverrideKey);
  if (override_bounds && !override_bounds->IsEmpty()) {
    *bounds = *override_bounds;
    *show_state =
        ash::ToWindowShowState(GetWidget()->GetNativeWindow()->GetProperty(
            ash::kRestoreWindowStateTypeOverrideKey));
  }

  // Session restore might be unable to correctly restore other states.
  // For the record, https://crbug.com/396272
  if (*show_state != ui::SHOW_STATE_MAXIMIZED &&
      *show_state != ui::SHOW_STATE_MINIMIZED) {
    *show_state = ui::SHOW_STATE_NORMAL;
  }
}

content::KeyboardEventProcessingResult BrowserFrameMash::PreHandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}

bool BrowserFrameMash::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  return false;
}

int BrowserFrameMash::GetMinimizeButtonOffset() const {
  return 0;
}
