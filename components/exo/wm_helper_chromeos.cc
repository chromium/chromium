// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wm_helper_chromeos.h"
#include "components/exo/wm_helper.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/memory/singleton.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/client/focus_client.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/public/activation_client.h"

namespace exo {
namespace {

aura::Window* GetPrimaryRoot() {
  return ash::Shell::Get()->GetPrimaryRootWindow();
}

// A property key to store whether IME should be blocked for the surface.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kImeBlockedKey, false)

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// WMHelperChromeOS, public:

WMHelperChromeOS::WMHelperChromeOS() : vsync_timing_manager_(this) {}

WMHelperChromeOS::~WMHelperChromeOS() {}

WMHelperChromeOS* WMHelperChromeOS::GetInstance() {
  return static_cast<WMHelperChromeOS*>(WMHelper::GetInstance());
}

void WMHelperChromeOS::AddTabletModeObserver(
    ash::TabletModeObserver* observer) {
  ash::Shell::Get()->tablet_mode_controller()->AddObserver(observer);
}

void WMHelperChromeOS::RemoveTabletModeObserver(
    ash::TabletModeObserver* observer) {
  ash::Shell::Get()->tablet_mode_controller()->RemoveObserver(observer);
}

void WMHelperChromeOS::AddDisplayConfigurationObserver(
    ash::WindowTreeHostManager::Observer* observer) {
  ash::Shell::Get()->window_tree_host_manager()->AddObserver(observer);
}

void WMHelperChromeOS::RemoveDisplayConfigurationObserver(
    ash::WindowTreeHostManager::Observer* observer) {
  ash::Shell::Get()->window_tree_host_manager()->RemoveObserver(observer);
}

void WMHelperChromeOS::AddActivationObserver(
    wm::ActivationChangeObserver* observer) {
  ash::Shell::Get()->activation_client()->AddObserver(observer);
}

void WMHelperChromeOS::RemoveActivationObserver(
    wm::ActivationChangeObserver* observer) {
  ash::Shell::Get()->activation_client()->RemoveObserver(observer);
}

void WMHelperChromeOS::AddFocusObserver(
    aura::client::FocusChangeObserver* observer) {
  aura::client::GetFocusClient(GetPrimaryRoot())->AddObserver(observer);
}

void WMHelperChromeOS::RemoveFocusObserver(
    aura::client::FocusChangeObserver* observer) {
  aura::client::GetFocusClient(GetPrimaryRoot())->RemoveObserver(observer);
}

void WMHelperChromeOS::AddDragDropObserver(DragDropObserver* observer) {
  drag_drop_observers_.AddObserver(observer);
}

void WMHelperChromeOS::RemoveDragDropObserver(DragDropObserver* observer) {
  drag_drop_observers_.RemoveObserver(observer);
}

void WMHelperChromeOS::SetDragDropDelegate(aura::Window* window) {
  aura::client::SetDragDropDelegate(window, this);
}

void WMHelperChromeOS::ResetDragDropDelegate(aura::Window* window) {
  aura::client::SetDragDropDelegate(window, nullptr);
}

VSyncTimingManager& WMHelperChromeOS::GetVSyncTimingManager() {
  return vsync_timing_manager_;
}

void WMHelperChromeOS::OnDragEntered(const ui::DropTargetEvent& event) {
  for (DragDropObserver& observer : drag_drop_observers_)
    observer.OnDragEntered(event);
}

int WMHelperChromeOS::OnDragUpdated(const ui::DropTargetEvent& event) {
  int valid_operation = ui::DragDropTypes::DRAG_NONE;
  for (DragDropObserver& observer : drag_drop_observers_)
    valid_operation = valid_operation | observer.OnDragUpdated(event);
  return valid_operation;
}

void WMHelperChromeOS::OnDragExited() {
  for (DragDropObserver& observer : drag_drop_observers_)
    observer.OnDragExited();
}

int WMHelperChromeOS::OnPerformDrop(const ui::DropTargetEvent& event,
                                    std::unique_ptr<ui::OSExchangeData> data) {
  for (DragDropObserver& observer : drag_drop_observers_)
    observer.OnPerformDrop(event);
  // TODO(hirono): Return the correct result instead of always returning
  // DRAG_MOVE.
  return ui::DragDropTypes::DRAG_MOVE;
}

void WMHelperChromeOS::AddVSyncParameterObserver(
    mojo::PendingRemote<viz::mojom::VSyncParameterObserver> observer) {
  GetPrimaryRoot()->layer()->GetCompositor()->AddVSyncParameterObserver(
      std::move(observer));
}

const display::ManagedDisplayInfo& WMHelperChromeOS::GetDisplayInfo(
    int64_t display_id) const {
  return ash::Shell::Get()->display_manager()->GetDisplayInfo(display_id);
}

const std::vector<uint8_t>& WMHelperChromeOS::GetDisplayIdentificationData(
    int64_t display_id) const {
  const auto& displays =
      ash::Shell::Get()->display_configurator()->cached_displays();

  for (display::DisplaySnapshot* display : displays)
    if (display->display_id() == display_id)
      return display->edid();

  static std::vector<uint8_t> no_data;
  return no_data;
}

bool WMHelperChromeOS::GetActiveModeForDisplayId(
    int64_t display_id,
    display::ManagedDisplayMode* mode) const {
  return ash::Shell::Get()->display_manager()->GetActiveModeForDisplayId(
      display_id, mode);
}

aura::Window* WMHelperChromeOS::GetPrimaryDisplayContainer(int container_id) {
  return ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                                  container_id);
}

aura::Window* WMHelperChromeOS::GetActiveWindow() const {
  return ash::Shell::Get()->activation_client()->GetActiveWindow();
}

aura::Window* WMHelperChromeOS::GetFocusedWindow() const {
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  return focus_client->GetFocusedWindow();
}

aura::Window* WMHelperChromeOS::GetRootWindowForNewWindows() const {
  return ash::Shell::GetRootWindowForNewWindows();
}

aura::client::CursorClient* WMHelperChromeOS::GetCursorClient() {
  return aura::client::GetCursorClient(ash::Shell::GetPrimaryRootWindow());
}

void WMHelperChromeOS::AddPreTargetHandler(ui::EventHandler* handler) {
  ash::Shell::Get()->AddPreTargetHandler(handler);
}

void WMHelperChromeOS::PrependPreTargetHandler(ui::EventHandler* handler) {
  ash::Shell::Get()->AddPreTargetHandler(
      handler, ui::EventTarget::Priority::kAccessibility);
}

void WMHelperChromeOS::RemovePreTargetHandler(ui::EventHandler* handler) {
  ash::Shell::Get()->RemovePreTargetHandler(handler);
}

void WMHelperChromeOS::AddPostTargetHandler(ui::EventHandler* handler) {
  ash::Shell::Get()->AddPostTargetHandler(handler);
}

void WMHelperChromeOS::RemovePostTargetHandler(ui::EventHandler* handler) {
  ash::Shell::Get()->RemovePostTargetHandler(handler);
}

bool WMHelperChromeOS::InTabletMode() const {
  return ash::Shell::Get()->tablet_mode_controller()->InTabletMode();
}

double WMHelperChromeOS::GetDefaultDeviceScaleFactor() const {
  if (!display::Display::HasInternalDisplay())
    return 1.0;

  if (display::Display::HasForceDeviceScaleFactor())
    return display::Display::GetForcedDeviceScaleFactor();

  display::DisplayManager* display_manager =
      ash::Shell::Get()->display_manager();
  const display::ManagedDisplayInfo& display_info =
      display_manager->GetDisplayInfo(display::Display::InternalDisplayId());
  DCHECK(display_info.display_modes().size());
  return display_info.display_modes()[0].device_scale_factor();
}

void WMHelperChromeOS::SetImeBlocked(aura::Window* window, bool ime_blocked) {
  DCHECK_EQ(window, window->GetToplevelWindow());
  window->SetProperty(kImeBlockedKey, ime_blocked);
}

bool WMHelperChromeOS::IsImeBlocked(aura::Window* window) const {
  return window && window->GetToplevelWindow()->GetProperty(kImeBlockedKey);
}

WMHelper::LifetimeManager* WMHelperChromeOS::GetLifetimeManager() {
  return &lifetime_manager_;
}

aura::client::CaptureClient* WMHelperChromeOS::GetCaptureClient() {
  return wm::CaptureController::Get();
}

}  // namespace exo
