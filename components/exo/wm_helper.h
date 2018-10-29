// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WM_HELPER_H_
#define COMPONENTS_EXO_WM_HELPER_H_

#include <vector>

#include "ash/display/window_tree_host_manager.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/base/cursor/cursor.h"
#include "ui/compositor/compositor_vsync_manager.h"

namespace ash {
class TabletModeObserver;
}

namespace aura {
class env;
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

// A helper class for accessing WindowManager related features.
class WMHelper : public aura::client::DragDropDelegate {
 public:
  class DragDropObserver {
   public:
    virtual void OnDragEntered(const ui::DropTargetEvent& event) = 0;
    virtual int OnDragUpdated(const ui::DropTargetEvent& event) = 0;
    virtual void OnDragExited() = 0;
    virtual int OnPerformDrop(const ui::DropTargetEvent& event) = 0;

   protected:
    virtual ~DragDropObserver() {}
  };

  explicit WMHelper(aura::Env* env);
  ~WMHelper() override;

  static void SetInstance(WMHelper* helper);
  static WMHelper* GetInstance();
  static bool HasInstance();

  aura::Env* env() { return env_; }

  void AddActivationObserver(wm::ActivationChangeObserver* observer);
  void RemoveActivationObserver(wm::ActivationChangeObserver* observer);
  void AddFocusObserver(aura::client::FocusChangeObserver* observer);
  void RemoveFocusObserver(aura::client::FocusChangeObserver* observer);
  void AddTabletModeObserver(ash::TabletModeObserver* observer);
  void RemoveTabletModeObserver(ash::TabletModeObserver* observer);

  void AddDisplayConfigurationObserver(
      ash::WindowTreeHostManager::Observer* observer);
  void RemoveDisplayConfigurationObserver(
      ash::WindowTreeHostManager::Observer* observer);
  void AddDragDropObserver(DragDropObserver* observer);
  void RemoveDragDropObserver(DragDropObserver* observer);
  void SetDragDropDelegate(aura::Window*);
  void ResetDragDropDelegate(aura::Window*);
  void AddVSyncObserver(ui::CompositorVSyncManager::Observer* observer);
  void RemoveVSyncObserver(ui::CompositorVSyncManager::Observer* observer);

  const display::ManagedDisplayInfo& GetDisplayInfo(int64_t display_id) const;
  const std::vector<uint8_t>& GetDisplayIdentificationData(
      int64_t display_id) const;

  aura::Window* GetPrimaryDisplayContainer(int container_id);
  aura::Window* GetActiveWindow() const;
  aura::Window* GetFocusedWindow() const;
  aura::client::CursorClient* GetCursorClient();
  void AddPreTargetHandler(ui::EventHandler* handler);
  void PrependPreTargetHandler(ui::EventHandler* handler);
  void RemovePreTargetHandler(ui::EventHandler* handler);
  void AddPostTargetHandler(ui::EventHandler* handler);
  void RemovePostTargetHandler(ui::EventHandler* handler);
  bool IsTabletModeWindowManagerEnabled() const;
  double GetDefaultDeviceScaleFactor() const;

  // Overridden from aura::client::DragDropDelegate:
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  int OnPerformDrop(const ui::DropTargetEvent& event) override;

 private:
  base::ObserverList<DragDropObserver>::Unchecked drag_drop_observers_;

  // The most recently cached VSync parameters, sent to observers on addition.
  base::TimeTicks vsync_timebase_;
  base::TimeDelta vsync_interval_;
  scoped_refptr<ui::CompositorVSyncManager> vsync_manager_;
  aura::Env* const env_;

  DISALLOW_COPY_AND_ASSIGN(WMHelper);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_WM_HELPER_H_
