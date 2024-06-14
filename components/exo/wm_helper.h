// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WM_HELPER_H_
#define COMPONENTS_EXO_WM_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/exo/vsync_timing_manager.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/base/cursor/cursor.h"
#include "ui/display/manager/display_manager_observer.h"

namespace aura {
class Window;
namespace client {
class CaptureClient;
class CursorClient;
class DragDropClient;
class FocusChangeObserver;
}  // namespace client
}  // namespace aura

namespace ash {
class TabletModeObserver;
}

namespace wm {
class ActivationChangeObserver;
class TooltipObserver;
}

namespace display {
class ManagedDisplayInfo;
class ManagedDisplayMode;
}

namespace ui {
class EventHandler;
class PropertyHandler;
}  // namespace ui

namespace wm {
class ActivationChangeObserver;
}

namespace exo {
class VSyncTimingManager;

// Helper interface for accessing WindowManager related features.
class WMHelper : public chromeos::PowerManagerClient::Observer,
                 public VSyncTimingManager::Delegate {
 public:
  // Used to notify objects when WMHelper is being destroyed. This allows
  // objects that wait for various external depenencies to cleanup as part of
  // the shutdown process.
  class LifetimeManager {
   public:
    class Observer : public base::CheckedObserver {
     public:
      ~Observer() override = default;

      virtual void OnDestroyed() = 0;
    };

    LifetimeManager();

    LifetimeManager(const LifetimeManager&) = delete;
    LifetimeManager& operator=(const LifetimeManager&) = delete;

    ~LifetimeManager();

    void AddObserver(Observer* observer);
    void RemoveObserver(Observer* observer);

    void NotifyDestroyed();

   private:
    base::ObserverList<Observer> observers_;
  };

  // Used to resolve the properties to be set to the window
  // based on the |app_id| and |startup_id|.
  class AppPropertyResolver {
   public:
    struct Params {
      std::string app_id;
      std::string startup_id;
      int32_t window_session_id = -1;
      bool for_creation = false;
    };
    virtual ~AppPropertyResolver() = default;
    virtual void PopulateProperties(
        const Params& params,
        ui::PropertyHandler& out_properties_container) = 0;
  };

  class ExoWindowObserver : public base::CheckedObserver {
   public:
    // Called every time exo creates a new window but before it is shown.
    virtual void OnExoWindowCreated(aura::Window* window) {}
  };

  // Interface for Exo classes needing to listen to PowerManagerClient events.
  //
  // Only implemented for ChromeOS, otherwise a no-op.
  class PowerObserver : public base::CheckedObserver {
   public:
    virtual void SuspendDone() {}
    virtual void ScreenBrightnessChanged(double percent) {}
    virtual void LidEventReceived(bool opened) {}
  };

  WMHelper();

  WMHelper(const WMHelper&) = delete;
  WMHelper& operator=(const WMHelper&) = delete;

  ~WMHelper() override;

  static WMHelper* GetInstance();
  static bool HasInstance();

  // A virtual ulitity function to return the container window for unit test.
  virtual aura::Window* GetPrimaryDisplayContainer(int container_id);

  // Registers an AppPropertyResolver. Multiple resolver can be registered and
  // all resolvers are called in the registration order by the method below.
  void RegisterAppPropertyResolver(
      std::unique_ptr<AppPropertyResolver> resolver);

  // Populates window properties for given |app_id| and |startup_id|.
  // |for_creation| == true means this is called before a widget gets
  // created, and false means this is called when the application id is set
  // after the widget is created.
  void PopulateAppProperties(const AppPropertyResolver::Params& params,
                             ui::PropertyHandler& out_properties_container);

  void AddExoWindowObserver(ExoWindowObserver* observer);
  void RemoveExoWindowObserver(ExoWindowObserver* observer);

  // Notifies observers that |window| has been created by exo and is ready for
  // to receive content.
  void NotifyExoWindowCreated(aura::Window* window);

  void AddActivationObserver(wm::ActivationChangeObserver* observer);
  void RemoveActivationObserver(wm::ActivationChangeObserver* observer);
  void AddTooltipObserver(wm::TooltipObserver* observer);
  void RemoveTooltipObserver(wm::TooltipObserver* observer);
  void AddFocusObserver(aura::client::FocusChangeObserver* observer);
  void RemoveFocusObserver(aura::client::FocusChangeObserver* observer);
  void AddPowerObserver(WMHelper::PowerObserver* observer);
  void RemovePowerObserver(WMHelper::PowerObserver* observer);
  VSyncTimingManager& GetVSyncTimingManager();
  const display::ManagedDisplayInfo& GetDisplayInfo(int64_t display_id) const;
  const std::vector<uint8_t>& GetDisplayIdentificationData(
      int64_t display_id) const;
  bool GetActiveModeForDisplayId(int64_t display_id,
                                 display::ManagedDisplayMode* mode) const;
  aura::Window* GetActiveWindow() const;
  aura::Window* GetFocusedWindow() const;
  aura::client::CursorClient* GetCursorClient();
  aura::client::DragDropClient* GetDragDropClient();
  void AddPreTargetHandler(ui::EventHandler* handler);
  void PrependPreTargetHandler(ui::EventHandler* handler);
  void RemovePreTargetHandler(ui::EventHandler* handler);
  void AddPostTargetHandler(ui::EventHandler* handler);
  void RemovePostTargetHandler(ui::EventHandler* handler);
  double GetDeviceScaleFactorForWindow(aura::Window* window) const;
  void SetDefaultScaleCancellation(bool default_scale_cancellation);
  bool use_default_scale_cancellation() const {
    return default_scale_cancellation_;
  }
  void AddTabletModeObserver(ash::TabletModeObserver* observer);
  void RemoveTabletModeObserver(ash::TabletModeObserver* observer);
  void AddDisplayConfigurationObserver(
      display::DisplayManagerObserver* observer);
  void RemoveDisplayConfigurationObserver(
      display::DisplayManagerObserver* observer);
  void AddFrameThrottlingObserver();
  void RemoveFrameThrottlingObserver();

  LifetimeManager* GetLifetimeManager();
  aura::client::CaptureClient* GetCaptureClient();

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
  base::ObserverList<ExoWindowObserver> exo_window_observers_;

  std::vector<std::unique_ptr<AppPropertyResolver>> resolver_list_;

  base::ObserverList<PowerObserver> power_observers_;
  LifetimeManager lifetime_manager_;
  VSyncTimingManager vsync_timing_manager_;
  bool default_scale_cancellation_ = true;
  base::WeakPtrFactory<WMHelper> weak_ptr_factory_{this};
};

// Returnsn the default device scale factor used for
// ClientControlledShellSurface (ARC).
float GetDefaultDeviceScaleFactor();

}  // namespace exo

#endif  // COMPONENTS_EXO_WM_HELPER_H_
