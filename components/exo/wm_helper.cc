// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wm_helper.h"

#include "ash/frame_throttler/frame_throttling_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/singleton.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "ui/aura/client/drag_drop_client.h"
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

WMHelper* g_instance = nullptr;

aura::Window* GetPrimaryRoot() {
  return ash::Shell::Get()->GetPrimaryRootWindow();
}

}  // namespace

WMHelper::LifetimeManager::LifetimeManager() = default;

WMHelper::LifetimeManager::~LifetimeManager() = default;

void WMHelper::LifetimeManager::NotifyDestroyed() {
  for (Observer& observer : observers_)
    observer.OnDestroyed();
}

void WMHelper::LifetimeManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void WMHelper::LifetimeManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

WMHelper::WMHelper() : vsync_timing_manager_(this) {
  DCHECK(!g_instance);
  g_instance = this;

  auto* power_manager = chromeos::PowerManagerClient::Get();
  // May be null in tests
  if (power_manager) {
    power_manager->AddObserver(this);
  }
}

WMHelper::~WMHelper() {
  auto* power_manager = chromeos::PowerManagerClient::Get();
  if (power_manager) {
    power_manager->RemoveObserver(this);
  }
  // Notify the user of lifetime manager first as
  // observers may access the instance.
  lifetime_manager_.NotifyDestroyed();
  DCHECK(g_instance);
  g_instance = nullptr;
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

////////////////////////////////////////////////////////////////////////////////
// WMHelper, public:

aura::Window* WMHelper::GetPrimaryDisplayContainer(int container_id) {
  return ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                                  container_id);
}

void WMHelper::RegisterAppPropertyResolver(
    std::unique_ptr<AppPropertyResolver> resolver) {
  resolver_list_.push_back(std::move(resolver));
}

void WMHelper::PopulateAppProperties(
    const AppPropertyResolver::Params& params,
    ui::PropertyHandler& out_properties_container) {
  for (auto& resolver : resolver_list_) {
    resolver->PopulateProperties(params, out_properties_container);
  }
}

void WMHelper::AddExoWindowObserver(ExoWindowObserver* observer) {
  exo_window_observers_.AddObserver(observer);
}

void WMHelper::NotifyExoWindowCreated(aura::Window* window) {
  for (auto& obs : exo_window_observers_) {
    obs.OnExoWindowCreated(window);
  }
}

void WMHelper::AddActivationObserver(wm::ActivationChangeObserver* observer) {
  ash::Shell::Get()->activation_client()->AddObserver(observer);
}

void WMHelper::RemoveActivationObserver(
    wm::ActivationChangeObserver* observer) {
  ash::Shell::Get()->activation_client()->RemoveObserver(observer);
}

void WMHelper::AddTooltipObserver(wm::TooltipObserver* observer) {
  ash::Shell::Get()->tooltip_controller()->AddObserver(observer);
}

void WMHelper::RemoveTooltipObserver(wm::TooltipObserver* observer) {
  ash::Shell::Get()->tooltip_controller()->RemoveObserver(observer);
}

void WMHelper::AddFocusObserver(aura::client::FocusChangeObserver* observer) {
  aura::client::GetFocusClient(GetPrimaryRoot())->AddObserver(observer);
}

void WMHelper::RemoveFocusObserver(
    aura::client::FocusChangeObserver* observer) {
  aura::client::GetFocusClient(GetPrimaryRoot())->RemoveObserver(observer);
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

void WMHelper::AddPowerObserver(WMHelper::PowerObserver* observer) {
  power_observers_.AddObserver(observer);
}

void WMHelper::RemovePowerObserver(WMHelper::PowerObserver* observer) {
  power_observers_.RemoveObserver(observer);
}

VSyncTimingManager& WMHelper::GetVSyncTimingManager() {
  return vsync_timing_manager_;
}

const display::ManagedDisplayInfo& WMHelper::GetDisplayInfo(
    int64_t display_id) const {
  return ash::Shell::Get()->display_manager()->GetDisplayInfo(display_id);
}

const std::vector<uint8_t>& WMHelper::GetDisplayIdentificationData(
    int64_t display_id) const {
  const auto& displays =
      ash::Shell::Get()->display_configurator()->cached_displays();

  for (display::DisplaySnapshot* display : displays) {
    if (display->display_id() == display_id) {
      return display->edid();
    }
  }

  static std::vector<uint8_t> no_data;
  return no_data;
}

bool WMHelper::GetActiveModeForDisplayId(
    int64_t display_id,
    display::ManagedDisplayMode* mode) const {
  return ash::Shell::Get()->display_manager()->GetActiveModeForDisplayId(
      display_id, mode);
}

aura::Window* WMHelper::GetActiveWindow() const {
  return ash::Shell::Get()->activation_client()->GetActiveWindow();
}

aura::Window* WMHelper::GetFocusedWindow() const {
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  return focus_client->GetFocusedWindow();
}

aura::Window* WMHelper::GetRootWindowForNewWindows() const {
  return ash::Shell::GetRootWindowForNewWindows();
}

aura::client::CursorClient* WMHelper::GetCursorClient() {
  return aura::client::GetCursorClient(ash::Shell::GetPrimaryRootWindow());
}

aura::client::DragDropClient* WMHelper::GetDragDropClient() {
  return aura::client::GetDragDropClient(ash::Shell::GetPrimaryRootWindow());
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

bool WMHelper::InTabletMode() const {
  return ash::Shell::Get()->tablet_mode_controller()->InTabletMode();
}

double WMHelper::GetDeviceScaleFactorForWindow(aura::Window* window) const {
  if (default_scale_cancellation_) {
    return GetDefaultDeviceScaleFactor();
  }
  const display::Screen* screen = display::Screen::GetScreen();
  display::Display display = screen->GetDisplayNearestWindow(window);
  return display.device_scale_factor();
}

void WMHelper::SetDefaultScaleCancellation(bool default_scale_cancellation) {
  default_scale_cancellation_ = default_scale_cancellation;
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

void WMHelper::AddFrameThrottlingObserver() {
  ash::FrameThrottlingController* controller =
      ash::Shell::Get()->frame_throttling_controller();
  if (!controller->HasArcObserver(&vsync_timing_manager_)) {
    ash::Shell::Get()->frame_throttling_controller()->AddArcObserver(
        &vsync_timing_manager_);
  }
}

void WMHelper::RemoveFrameThrottlingObserver() {
  ash::Shell::Get()->frame_throttling_controller()->RemoveArcObserver(
      &vsync_timing_manager_);
}

WMHelper::LifetimeManager* WMHelper::GetLifetimeManager() {
  return &lifetime_manager_;
}

aura::client::CaptureClient* WMHelper::GetCaptureClient() {
  return wm::CaptureController::Get();
}

void WMHelper::OnDragEntered(const ui::DropTargetEvent& event) {
  for (DragDropObserver& observer : drag_drop_observers_) {
    observer.OnDragEntered(event);
  }
}

aura::client::DragUpdateInfo WMHelper::OnDragUpdated(
    const ui::DropTargetEvent& event) {
  aura::client::DragUpdateInfo drag_info(
      ui::DragDropTypes::DRAG_NONE,
      ui::DataTransferEndpoint(ui::EndpointType::kUnknownVm));

  for (DragDropObserver& observer : drag_drop_observers_) {
    auto observer_drag_info = observer.OnDragUpdated(event);
    drag_info.drag_operation =
        drag_info.drag_operation | observer_drag_info.drag_operation;
    if (observer_drag_info.data_endpoint.type() !=
        drag_info.data_endpoint.type()) {
      drag_info.data_endpoint = observer_drag_info.data_endpoint;
    }
  }
  return drag_info;
}

void WMHelper::OnDragExited() {
  for (DragDropObserver& observer : drag_drop_observers_) {
    observer.OnDragExited();
  }
}

aura::client::DragDropDelegate::DropCallback WMHelper::GetDropCallback(
    const ui::DropTargetEvent& event) {
  std::vector<WMHelper::DragDropObserver::DropCallback> drop_callbacks;
  for (DragDropObserver& observer : drag_drop_observers_) {
    WMHelper::DragDropObserver::DropCallback drop_cb =
        observer.GetDropCallback();
    if (!drop_cb.is_null()) {
      drop_callbacks.push_back(std::move(drop_cb));
    }
  }
  return base::BindOnce(&WMHelper::PerformDrop, weak_ptr_factory_.GetWeakPtr(),
                        std::move(drop_callbacks));
}

void WMHelper::SuspendDone(base::TimeDelta sleep_duration) {
  for (PowerObserver& observer : power_observers_) {
    observer.SuspendDone();
  }
}

void WMHelper::ScreenBrightnessChanged(
    const power_manager::BacklightBrightnessChange& change) {
  for (PowerObserver& observer : power_observers_) {
    observer.ScreenBrightnessChanged(change.percent());
  }
}

void WMHelper::LidEventReceived(chromeos::PowerManagerClient::LidState state,
                                base::TimeTicks timestamp) {
  for (PowerObserver& observer : power_observers_) {
    observer.LidEventReceived(state ==
                              chromeos::PowerManagerClient::LidState::OPEN);
  }
}

void WMHelper::AddVSyncParameterObserver(
    mojo::PendingRemote<viz::mojom::VSyncParameterObserver> observer) {
  GetPrimaryRoot()->layer()->GetCompositor()->AddVSyncParameterObserver(
      std::move(observer));
}

void WMHelper::RemoveExoWindowObserver(ExoWindowObserver* observer) {
  exo_window_observers_.RemoveObserver(observer);
}

void WMHelper::PerformDrop(
    std::vector<WMHelper::DragDropObserver::DropCallback> drop_callbacks,
    std::unique_ptr<ui::OSExchangeData> data,
    ui::mojom::DragOperation& output_drag_op) {
  for (auto& drop_cb : drop_callbacks) {
    auto operation = ui::mojom::DragOperation::kNone;
    std::move(drop_cb).Run(operation);
    if (operation != ui::mojom::DragOperation::kNone) {
      output_drag_op = operation;
    }
  }
}

float GetDefaultDeviceScaleFactor() {
  if (!display::HasInternalDisplay()) {
    return 1.0;
  }

  if (display::Display::HasForceDeviceScaleFactor()) {
    return display::Display::GetForcedDeviceScaleFactor();
  }

  display::DisplayManager* display_manager =
      ash::Shell::Get()->display_manager();
  const display::ManagedDisplayInfo& display_info =
      display_manager->GetDisplayInfo(display::Display::InternalDisplayId());
  DCHECK(display_info.display_modes().size());
  return display_info.display_modes()[0].device_scale_factor();
}

}  // namespace exo
