// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WM_HELPER_H_
#define COMPONENTS_EXO_WM_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"

namespace aura {
class Window;
namespace client {
class CaptureClient;
class CursorClient;
class DragDropClient;
class FocusChangeObserver;
}  // namespace client
}  // namespace aura

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
class DropTargetEvent;
class PropertyHandler;
}  // namespace ui

namespace wm {
class ActivationChangeObserver;
}

namespace exo {
class VSyncTimingManager;

// Helper interface for accessing WindowManager related features.
class WMHelper : public aura::client::DragDropDelegate {
 public:

  class DragDropObserver {
   public:
    using DropCallback =
        base::OnceCallback<void(ui::mojom::DragOperation& output_drag_op)>;

    virtual void OnDragEntered(const ui::DropTargetEvent& event) = 0;
    virtual aura::client::DragUpdateInfo OnDragUpdated(
        const ui::DropTargetEvent& event) = 0;
    virtual void OnDragExited() = 0;
    virtual DropCallback GetDropCallback() = 0;

   protected:
    virtual ~DragDropObserver() {}
  };

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

  virtual void AddActivationObserver(
      wm::ActivationChangeObserver* observer) = 0;
  virtual void RemoveActivationObserver(
      wm::ActivationChangeObserver* observer) = 0;
  virtual void AddTooltipObserver(wm::TooltipObserver* observer) = 0;
  virtual void RemoveTooltipObserver(wm::TooltipObserver* observer) = 0;
  virtual void AddFocusObserver(
      aura::client::FocusChangeObserver* observer) = 0;
  virtual void RemoveFocusObserver(
      aura::client::FocusChangeObserver* observer) = 0;
  void AddExoWindowObserver(ExoWindowObserver* observer);
  void RemoveExoWindowObserver(ExoWindowObserver* observer);

  virtual void AddPowerObserver(PowerObserver* observer);
  virtual void RemovePowerObserver(PowerObserver* observer);

  virtual void AddDragDropObserver(DragDropObserver* observer) = 0;
  virtual void RemoveDragDropObserver(DragDropObserver* observer) = 0;
  virtual void SetDragDropDelegate(aura::Window*) = 0;
  virtual void ResetDragDropDelegate(aura::Window*) = 0;
  virtual VSyncTimingManager& GetVSyncTimingManager() = 0;

  virtual const display::ManagedDisplayInfo& GetDisplayInfo(
      int64_t display_id) const = 0;
  virtual const std::vector<uint8_t>& GetDisplayIdentificationData(
      int64_t display_id) const = 0;
  virtual bool GetActiveModeForDisplayId(
      int64_t display_id,
      display::ManagedDisplayMode* mode) const = 0;

  virtual aura::Window* GetPrimaryDisplayContainer(int container_id) = 0;
  virtual aura::Window* GetActiveWindow() const = 0;
  virtual aura::Window* GetFocusedWindow() const = 0;
  virtual aura::Window* GetRootWindowForNewWindows() const = 0;
  virtual aura::client::CursorClient* GetCursorClient() = 0;
  virtual aura::client::DragDropClient* GetDragDropClient() = 0;
  virtual void AddPreTargetHandler(ui::EventHandler* handler) = 0;
  virtual void PrependPreTargetHandler(ui::EventHandler* handler) = 0;
  virtual void RemovePreTargetHandler(ui::EventHandler* handler) = 0;
  virtual void AddPostTargetHandler(ui::EventHandler* handler) = 0;
  virtual void RemovePostTargetHandler(ui::EventHandler* handler) = 0;
  virtual bool InTabletMode() const = 0;
  virtual double GetDefaultDeviceScaleFactor() const = 0;
  virtual double GetDeviceScaleFactorForWindow(aura::Window* window) const = 0;
  virtual void SetDefaultScaleCancellation(bool default_scale_cancellation) = 0;

  virtual LifetimeManager* GetLifetimeManager() = 0;
  virtual aura::client::CaptureClient* GetCaptureClient() = 0;

  // Overridden from aura::client::DragDropDelegate:
  void OnDragEntered(const ui::DropTargetEvent& event) override = 0;
  aura::client::DragUpdateInfo OnDragUpdated(
      const ui::DropTargetEvent& event) override = 0;
  void OnDragExited() override = 0;
  aura::client::DragDropDelegate::DropCallback GetDropCallback(
      const ui::DropTargetEvent& event) override = 0;

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

  // Notifies observers that |window| has been created by exo and is ready for
  // to receive content.
  void NotifyExoWindowCreated(aura::Window* window);

 protected:
  base::ObserverList<ExoWindowObserver> exo_window_observers_;

  std::vector<std::unique_ptr<AppPropertyResolver>> resolver_list_;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_WM_HELPER_H_
