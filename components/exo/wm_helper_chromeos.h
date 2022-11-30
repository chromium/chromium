// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WM_HELPER_CHROMEOS_H_
#define COMPONENTS_EXO_WM_HELPER_CHROMEOS_H_

#include <vector>

#include "ash/display/window_tree_host_manager.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/exo/vsync_timing_manager.h"
#include "components/exo/wm_helper.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/base/cursor/cursor.h"

namespace ash {
class TabletModeObserver;
}

namespace aura {
class Window;
namespace client {
class CursorClient;
class FocusChangeObserver;
}  // namespace client
}  // namespace aura

namespace wm {
class ActivationChangeObserver;
}

namespace display {
class ManagedDisplayInfo;
}

namespace ui {
class EventHandler;
class DropTargetEvent;
}  // namespace ui

namespace wm {
class ActivationChangeObserver;
}

namespace exo {

// A ChromeOS-specific helper class for accessing WindowManager related
// features.
class WMHelperChromeOS : public WMHelper,
                         public chromeos::PowerManagerClient::Observer,
                         public VSyncTimingManager::Delegate {
 public:
  WMHelperChromeOS();

  WMHelperChromeOS(const WMHelperChromeOS&) = delete;
  WMHelperChromeOS& operator=(const WMHelperChromeOS&) = delete;

  ~WMHelperChromeOS() override;
  static WMHelperChromeOS* GetInstance();
  void AddTabletModeObserver(ash::TabletModeObserver* observer);
  void RemoveTabletModeObserver(ash::TabletModeObserver* observer);
  void AddDisplayConfigurationObserver(
      ash::WindowTreeHostManager::Observer* observer);
  void RemoveDisplayConfigurationObserver(
      ash::WindowTreeHostManager::Observer* observer);
  void AddFrameThrottlingObserver();
  void RemoveFrameThrottlingObserver();

  // Overridden from WMHelper
  void AddActivationObserver(wm::ActivationChangeObserver* observer) override;
  void RemoveActivationObserver(
      wm::ActivationChangeObserver* observer) override;
  void AddTooltipObserver(wm::TooltipObserver* observer) override;
  void RemoveTooltipObserver(wm::TooltipObserver* observer) override;
  void AddFocusObserver(aura::client::FocusChangeObserver* observer) override;
  void RemoveFocusObserver(
      aura::client::FocusChangeObserver* observer) override;
  void AddDragDropObserver(DragDropObserver* observer) override;
  void RemoveDragDropObserver(DragDropObserver* observer) override;
  void AddPowerObserver(WMHelper::PowerObserver* observer) override;
  void RemovePowerObserver(WMHelper::PowerObserver* observer) override;
  void SetDragDropDelegate(aura::Window*) override;
  void ResetDragDropDelegate(aura::Window*) override;
  VSyncTimingManager& GetVSyncTimingManager() override;

  const display::ManagedDisplayInfo& GetDisplayInfo(
      int64_t display_id) const override;
  const std::vector<uint8_t>& GetDisplayIdentificationData(
      int64_t display_id) const override;
  bool GetActiveModeForDisplayId(
      int64_t display_id,
      display::ManagedDisplayMode* mode) const override;

  aura::Window* GetPrimaryDisplayContainer(int container_id) override;
  aura::Window* GetActiveWindow() const override;
  aura::Window* GetFocusedWindow() const override;
  aura::Window* GetRootWindowForNewWindows() const override;
  aura::client::CursorClient* GetCursorClient() override;
  aura::client::DragDropClient* GetDragDropClient() override;
  void AddPreTargetHandler(ui::EventHandler* handler) override;
  void PrependPreTargetHandler(ui::EventHandler* handler) override;
  void RemovePreTargetHandler(ui::EventHandler* handler) override;
  void AddPostTargetHandler(ui::EventHandler* handler) override;
  void RemovePostTargetHandler(ui::EventHandler* handler) override;
  bool InTabletMode() const override;
  double GetDefaultDeviceScaleFactor() const override;
  double GetDeviceScaleFactorForWindow(aura::Window* window) const override;
  void SetDefaultScaleCancellation(bool default_scale_cancellation) override;

  LifetimeManager* GetLifetimeManager() override;
  aura::client::CaptureClient* GetCaptureClient() override;

  // Overridden from aura::client::DragDropDelegate:
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  aura::client::DragUpdateInfo OnDragUpdated(
      const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  aura::client::DragDropDelegate::DropCallback GetDropCallback(
      const ui::DropTargetEvent& event) override;

  // Overridden from chromeos::PowerManagerClient::Observer:
  void SuspendDone(base::TimeDelta sleep_duration) override;
  void ScreenBrightnessChanged(
      const power_manager::BacklightBrightnessChange& change) override;
  void LidEventReceived(chromeos::PowerManagerClient::LidState state,
                        base::TimeTicks timestamp) override;

  // Overridden from VSyncTimingManager::Delegate:
  void AddVSyncParameterObserver(
      mojo::PendingRemote<viz::mojom::VSyncParameterObserver> observer)
      override;

 private:
  void PerformDrop(
      std::vector<WMHelper::DragDropObserver::DropCallback> drop_callbacks,
      std::unique_ptr<ui::OSExchangeData> data,
      ui::mojom::DragOperation& output_drag_op);

  base::ObserverList<DragDropObserver>::Unchecked drag_drop_observers_;
  base::ObserverList<PowerObserver> power_observers_;
  LifetimeManager lifetime_manager_;
  VSyncTimingManager vsync_timing_manager_;
  bool default_scale_cancellation_ = true;
  base::WeakPtrFactory<WMHelperChromeOS> weak_ptr_factory_{this};
};

// Returnsn the default device scale factor used for
// ClientControlledShellSurface (ARC).
float GetDefaultDeviceScaleFactor();

}  // namespace exo

#endif  // COMPONENTS_EXO_WM_HELPER_CHROMEOS_H_
