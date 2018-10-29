// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wm_helper.h"

#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/memory/singleton.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/client/focus_client.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/wm/public/activation_client.h"

namespace exo {
namespace {
WMHelper* g_instance = nullptr;

aura::Window* GetPrimaryRoot() {
  return ash::Shell::Get()->GetPrimaryRootWindow();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// WMHelper, public:

WMHelper::WMHelper(aura::Env* env)
    : vsync_manager_(
          GetPrimaryRoot()->layer()->GetCompositor()->vsync_manager()),
      env_(env) {}

WMHelper::~WMHelper() {}

// static
void WMHelper::SetInstance(WMHelper* helper) {
  DCHECK_NE(!!helper, !!g_instance);
  g_instance = helper;
}

// static
WMHelper* WMHelper::GetInstance() {
  DCHECK(g_instance);
  return g_instance;
}

// static
bool WMHelper::HasInstance() {
  return !!g_instance;
}

void WMHelper::AddActivationObserver(wm::ActivationChangeObserver* observer) {
  ash::Shell::Get()->activation_client()->AddObserver(observer);
}

void WMHelper::RemoveActivationObserver(
    wm::ActivationChangeObserver* observer) {
  ash::Shell::Get()->activation_client()->RemoveObserver(observer);
}

void WMHelper::AddFocusObserver(aura::client::FocusChangeObserver* observer) {
  aura::client::GetFocusClient(GetPrimaryRoot())->AddObserver(observer);
}

void WMHelper::RemoveFocusObserver(
    aura::client::FocusChangeObserver* observer) {
  aura::client::GetFocusClient(GetPrimaryRoot())->RemoveObserver(observer);
}

void WMHelper::AddTabletModeObserver(ash::TabletModeObserver* observer) {
  ash::Shell::Get()->tablet_mode_controller()->AddObserver(observer);
}

void WMHelper::RemoveTabletModeObserver(ash::TabletModeObserver* observer) {
  ash::Shell::Get()->tablet_mode_controller()->RemoveObserver(observer);
}

void WMHelper::AddDisplayConfigurationObserver(
    ash::WindowTreeHostManager::Observer* observer) {
  ash::Shell::Get()->window_tree_host_manager()->AddObserver(observer);
}

void WMHelper::RemoveDisplayConfigurationObserver(
    ash::WindowTreeHostManager::Observer* observer) {
  ash::Shell::Get()->window_tree_host_manager()->RemoveObserver(observer);
}

void WMHelper::AddDragDropObserver(DragDropObserver* observer) {
  drag_drop_observers_.AddObserver(observer);
}

void WMHelper::RemoveDragDropObserver(DragDropObserver* observer) {
  drag_drop_observers_.RemoveObserver(observer);
}

void WMHelper::SetDragDropDelegate(aura::Window* window) {
  aura::client::SetDragDropDelegate(window, this);
}

void WMHelper::ResetDragDropDelegate(aura::Window* window) {
  aura::client::SetDragDropDelegate(window, nullptr);
}

void WMHelper::AddVSyncObserver(
    ui::CompositorVSyncManager::Observer* observer) {
  vsync_manager_->AddObserver(observer);
}

void WMHelper::RemoveVSyncObserver(
    ui::CompositorVSyncManager::Observer* observer) {
  vsync_manager_->RemoveObserver(observer);
}

void WMHelper::OnDragEntered(const ui::DropTargetEvent& event) {
  for (DragDropObserver& observer : drag_drop_observers_)
    observer.OnDragEntered(event);
}

int WMHelper::OnDragUpdated(const ui::DropTargetEvent& event) {
  int valid_operation = ui::DragDropTypes::DRAG_NONE;
  for (DragDropObserver& observer : drag_drop_observers_)
    valid_operation = valid_operation | observer.OnDragUpdated(event);
  return valid_operation;
}

void WMHelper::OnDragExited() {
  for (DragDropObserver& observer : drag_drop_observers_)
    observer.OnDragExited();
}

int WMHelper::OnPerformDrop(const ui::DropTargetEvent& event) {
  for (DragDropObserver& observer : drag_drop_observers_)
    observer.OnPerformDrop(event);
  // TODO(hirono): Return the correct result instead of always returning
  // DRAG_MOVE.
  return ui::DragDropTypes::DRAG_MOVE;
}

const display::ManagedDisplayInfo& WMHelper::GetDisplayInfo(
    int64_t display_id) const {
  return ash::Shell::Get()->display_manager()->GetDisplayInfo(display_id);
}

const std::vector<uint8_t>& WMHelper::GetDisplayIdentificationData(
    int64_t display_id) const {
  const auto& displays = ash::Shell::Get()
                             ->window_tree_host_manager()
                             ->display_configurator()
                             ->cached_displays();

  for (display::DisplaySnapshot* display : displays)
    if (display->display_id() == display_id)
      return display->edid();

  static std::vector<uint8_t> no_data;
  return no_data;
}

aura::Window* WMHelper::GetPrimaryDisplayContainer(int container_id) {
  return ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                                  container_id);
}

aura::Window* WMHelper::GetActiveWindow() const {
  return ash::Shell::Get()->activation_client()->GetActiveWindow();
}

aura::Window* WMHelper::GetFocusedWindow() const {
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  return focus_client->GetFocusedWindow();
}

aura::client::CursorClient* WMHelper::GetCursorClient() {
  return aura::client::GetCursorClient(ash::Shell::GetPrimaryRootWindow());
}

void WMHelper::AddPreTargetHandler(ui::EventHandler* handler) {
  ash::Shell::Get()->AddPreTargetHandler(handler);
}

void WMHelper::PrependPreTargetHandler(ui::EventHandler* handler) {
  ash::Shell::Get()->AddPreTargetHandler(
      handler, ui::EventTarget::Priority::kAccessibility);
}

void WMHelper::RemovePreTargetHandler(ui::EventHandler* handler) {
  ash::Shell::Get()->RemovePreTargetHandler(handler);
}

void WMHelper::AddPostTargetHandler(ui::EventHandler* handler) {
  ash::Shell::Get()->AddPostTargetHandler(handler);
}

void WMHelper::RemovePostTargetHandler(ui::EventHandler* handler) {
  ash::Shell::Get()->RemovePostTargetHandler(handler);
}

bool WMHelper::IsTabletModeWindowManagerEnabled() const {
  return ash::Shell::Get()
      ->tablet_mode_controller()
      ->IsTabletModeWindowManagerEnabled();
}

double WMHelper::GetDefaultDeviceScaleFactor() const {
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

}  // namespace exo
