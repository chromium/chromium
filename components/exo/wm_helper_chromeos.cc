// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wm_helper_chromeos.h"

#include "ash/frame_throttler/frame_throttling_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/singleton.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/client/focus_client.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/util/display_util.h"
#include "ui/views/corewm/tooltip_controller.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/public/activation_client.h"
#include "ui/wm/public/tooltip_observer.h"

namespace exo {
namespace {

aura::Window* GetPrimaryRoot() {
  return ash::Shell::Get()->GetPrimaryRootWindow();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// WMHelperChromeOS, public:

WMHelperChromeOS::WMHelperChromeOS() : vsync_timing_manager_(this) {
  auto* power_manager = chromeos::PowerManagerClient::Get();
  // May be null in tests
  if (power_manager)
    power_manager->AddObserver(this);
}

WMHelperChromeOS::~WMHelperChromeOS() {
  auto* power_manager = chromeos::PowerManagerClient::Get();
  if (power_manager)
    power_manager->RemoveObserver(this);
}

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

void WMHelperChromeOS::AddFrameThrottlingObserver() {
  ash::FrameThrottlingController* controller =
      ash::Shell::Get()->frame_throttling_controller();
  if (!controller->HasArcObserver(&vsync_timing_manager_)) {
    ash::Shell::Get()->frame_throttling_controller()->AddArcObserver(
        &vsync_timing_manager_);
  }
}

void WMHelperChromeOS::RemoveFrameThrottlingObserver() {
  ash::Shell::Get()->frame_throttling_controller()->RemoveArcObserver(
      &vsync_timing_manager_);
}

void WMHelperChromeOS::AddPowerObserver(WMHelper::PowerObserver* observer) {
  power_observers_.AddObserver(observer);
}

void WMHelperChromeOS::RemovePowerObserver(WMHelper::PowerObserver* observer) {
  power_observers_.RemoveObserver(observer);
}

void WMHelperChromeOS::AddActivationObserver(
    wm::ActivationChangeObserver* observer) {
  ash::Shell::Get()->activation_client()->AddObserver(observer);
}

void WMHelperChromeOS::RemoveActivationObserver(
    wm::ActivationChangeObserver* observer) {
  ash::Shell::Get()->activation_client()->RemoveObserver(observer);
}

void WMHelperChromeOS::AddTooltipObserver(wm::TooltipObserver* observer) {
  ash::Shell::Get()->tooltip_controller()->AddObserver(observer);
}

void WMHelperChromeOS::RemoveTooltipObserver(wm::TooltipObserver* observer) {
  ash::Shell::Get()->tooltip_controller()->RemoveObserver(observer);
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

aura::client::DragUpdateInfo WMHelperChromeOS::OnDragUpdated(
    const ui::DropTargetEvent& event) {
  aura::client::DragUpdateInfo drag_info(
      ui::DragDropTypes::DRAG_NONE,
      ui::DataTransferEndpoint(ui::EndpointType::kUnknownVm));

  for (DragDropObserver& observer : drag_drop_observers_) {
    auto observer_drag_info = observer.OnDragUpdated(event);
    drag_info.drag_operation =
        drag_info.drag_operation | observer_drag_info.drag_operation;
    if (observer_drag_info.data_endpoint.type() !=
        drag_info.data_endpoint.type())
      drag_info.data_endpoint = observer_drag_info.data_endpoint;
  }
  return drag_info;
}

void WMHelperChromeOS::OnDragExited() {
  for (DragDropObserver& observer : drag_drop_observers_)
    observer.OnDragExited();
}

aura::client::DragDropDelegate::DropCallback WMHelperChromeOS::GetDropCallback(
    const ui::DropTargetEvent& event) {
  std::vector<WMHelper::DragDropObserver::DropCallback> drop_callbacks;
  for (DragDropObserver& observer : drag_drop_observers_) {
    WMHelper::DragDropObserver::DropCallback drop_cb =
        observer.GetDropCallback();
    if (!drop_cb.is_null()) {
      drop_callbacks.push_back(std::move(drop_cb));
    }
  }
  return base::BindOnce(&WMHelperChromeOS::PerformDrop,
                        weak_ptr_factory_.GetWeakPtr(),
                        std::move(drop_callbacks));
}

void WMHelperChromeOS::SuspendDone(base::TimeDelta sleep_duration) {
  for (PowerObserver& observer : power_observers_)
    observer.SuspendDone();
}

void WMHelperChromeOS::ScreenBrightnessChanged(
    const power_manager::BacklightBrightnessChange& change) {
  for (PowerObserver& observer : power_observers_)
    observer.ScreenBrightnessChanged(change.percent());
}

void WMHelperChromeOS::LidEventReceived(
    chromeos::PowerManagerClient::LidState state,
    base::TimeTicks timestamp) {
  for (PowerObserver& observer : power_observers_)
    observer.LidEventReceived(state ==
                              chromeos::PowerManagerClient::LidState::OPEN);
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

aura::client::DragDropClient* WMHelperChromeOS::GetDragDropClient() {
  return aura::client::GetDragDropClient(ash::Shell::GetPrimaryRootWindow());
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
  return exo::GetDefaultDeviceScaleFactor();
}

double WMHelperChromeOS::GetDeviceScaleFactorForWindow(
    aura::Window* window) const {
  if (default_scale_cancellation_)
    return exo::GetDefaultDeviceScaleFactor();
  const display::Screen* screen = display::Screen::GetScreen();
  display::Display display = screen->GetDisplayNearestWindow(window);
  return display.device_scale_factor();
}

void WMHelperChromeOS::SetDefaultScaleCancellation(
    bool default_scale_cancellation) {
  default_scale_cancellation_ = default_scale_cancellation;
}

WMHelper::LifetimeManager* WMHelperChromeOS::GetLifetimeManager() {
  return &lifetime_manager_;
}

aura::client::CaptureClient* WMHelperChromeOS::GetCaptureClient() {
  return wm::CaptureController::Get();
}

float GetDefaultDeviceScaleFactor() {
  if (!display::HasInternalDisplay())
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

void WMHelperChromeOS::PerformDrop(
    std::vector<WMHelper::DragDropObserver::DropCallback> drop_callbacks,
    std::unique_ptr<ui::OSExchangeData> data,
    ui::mojom::DragOperation& output_drag_op) {
  for (auto& drop_cb : drop_callbacks) {
    auto operation = ui::mojom::DragOperation::kNone;
    std::move(drop_cb).Run(operation);
    if (operation != ui::mojom::DragOperation::kNone)
      output_drag_op = operation;
  }
}

}  // namespace exo
