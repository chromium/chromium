// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_APP_WINDOW_LAUNCHER_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_APP_WINDOW_LAUNCHER_ITEM_CONTROLLER_H_

#include <list>
#include <memory>
#include <string>

#include "ash/public/cpp/shelf_item_delegate.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace ui {
class BaseWindow;
}

class LauncherContextMenu;

// This is a ShelfItemDelegate for abstract app windows (extension or ARC).
// There is one instance per app, per launcher id. For apps with multiple
// windows, each item controller keeps track of all windows associated with the
// app and their activation order. Instances are owned by ash::ShelfModel.
//
// Tests are in chrome_launcher_controller_browsertest.cc
class AppWindowLauncherItemController : public ash::ShelfItemDelegate,
                                        public aura::WindowObserver {
 public:
  using WindowList = std::list<ui::BaseWindow*>;

  explicit AppWindowLauncherItemController(const ash::ShelfID& shelf_id);
  ~AppWindowLauncherItemController() override;

  void AddWindow(ui::BaseWindow* window);
  void RemoveWindow(ui::BaseWindow* window);

  void SetActiveWindow(aura::Window* window);
  ui::BaseWindow* GetAppWindow(aura::Window* window);

  // ash::ShelfItemDelegate overrides:
  AppWindowLauncherItemController* AsAppWindowLauncherItemController() override;
  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ash::ShelfLaunchSource source,
                    ItemSelectedCallback callback) override;
  AppMenuItems GetAppMenuItems(int event_flags) override;
  void GetContextMenu(int64_t display_id,
                      GetContextMenuCallback callback) override;
  void ExecuteCommand(bool from_context_menu,
                      int64_t command_id,
                      int32_t event_flags,
                      int64_t display_id) override;
  void Close() override;

  // aura::WindowObserver overrides:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;

  // Activates the window at position |index|.
  void ActivateIndexedApp(size_t index);

  // Get the number of running applications/incarnations of this.
  size_t window_count() const { return windows_.size(); }

  const WindowList& windows() const { return windows_; }

 protected:
  // Returns last active window in the controller or first window.
  ui::BaseWindow* GetLastActiveWindow();

 private:
  friend class ChromeLauncherControllerTest;

  // Returns the action performed. Should be one of SHELF_ACTION_NONE,
  // SHELF_ACTION_WINDOW_ACTIVATED, or SHELF_ACTION_WINDOW_MINIMIZED.
  ash::ShelfAction ShowAndActivateOrMinimize(ui::BaseWindow* window);

  // Activate the given |window_to_show|, or - if already selected - advance to
  // the next window of similar type.
  // Returns the action performed. Should be one of SHELF_ACTION_NONE,
  // SHELF_ACTION_WINDOW_ACTIVATED, or SHELF_ACTION_WINDOW_MINIMIZED.
  ash::ShelfAction ActivateOrAdvanceToNextAppWindow(
      ui::BaseWindow* window_to_show);

  WindowList::iterator GetFromNativeWindow(aura::Window* window);

  // Handles the case when the app window in this controller has been changed,
  // and sets the new controller icon based on the currently active window.
  void UpdateShelfItemIcon();

  // List of associated app windows
  WindowList windows_;

  // Pointer to the most recently active app window
  // TODO(khmel): Get rid of |last_active_window_| and provide more reliable
  // way to determine active window.
  ui::BaseWindow* last_active_window_ = nullptr;

  // Scoped list of observed windows (for removal on destruction)
  ScopedObserver<aura::Window, aura::WindowObserver> observed_windows_{this};

  std::unique_ptr<LauncherContextMenu> context_menu_;

  DISALLOW_COPY_AND_ASSIGN(AppWindowLauncherItemController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_APP_WINDOW_LAUNCHER_ITEM_CONTROLLER_H_
