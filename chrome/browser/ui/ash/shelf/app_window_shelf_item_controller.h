// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_APP_WINDOW_SHELF_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_SHELF_APP_WINDOW_SHELF_ITEM_CONTROLLER_H_

#include <list>
#include <memory>

#include "ash/public/cpp/shelf_item_delegate.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

class AppWindowBase;
class ShelfContextMenu;

// This is a ShelfItemDelegate for abstract app windows (extension or ARC).
// There is one instance per app, per launcher id. For apps with multiple
// windows, each item controller keeps track of all windows associated with the
// app and their activation order. Instances are owned by ash::ShelfModel.
//
// Tests are in chrome_shelf_controller_browsertest.cc
class AppWindowShelfItemController : public ash::ShelfItemDelegate,
                                     public aura::WindowObserver {
 public:
  using WindowList = std::list<raw_ptr<AppWindowBase, CtnExperimental>>;

  explicit AppWindowShelfItemController(const ash::ShelfID& shelf_id);

  AppWindowShelfItemController(const AppWindowShelfItemController&) = delete;
  AppWindowShelfItemController& operator=(const AppWindowShelfItemController&) =
      delete;

  ~AppWindowShelfItemController() override;

  void AddWindow(AppWindowBase* window);
  void RemoveWindow(AppWindowBase* window);

  void SetActiveWindow(aura::Window* window);
  AppWindowBase* GetAppWindow(aura::Window* window, bool include_hidden);

  // ash::ShelfItemDelegate overrides:
  AppWindowShelfItemController* AsAppWindowShelfItemController() override;
  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ash::ShelfLaunchSource source,
                    ItemSelectedCallback callback,
                    const ItemFilterPredicate& filter_predicate) override;
  AppMenuItems GetAppMenuItems(
      int event_flags,
      const ItemFilterPredicate& filter_predicate) override;
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
  AppWindowBase* GetLastActiveWindow();

 private:
  friend class ChromeShelfControllerTestBase;

  WindowList::iterator GetFromNativeWindow(aura::Window* window,
                                           WindowList& list);

  // Handles the case when the app window in this controller has been changed,
  // and sets the new controller icon based on the currently active window.
  void UpdateShelfItemIcon();

  // Move a window between windows_ and hidden_windows_ list, depending on
  // changes in the ash::kHideInShelfKey property.
  void UpdateWindowInLists(aura::Window* window);

  // List of visible associated app windows
  WindowList windows_;

  // List of hidden associated app windows. These windows will not appear in
  // the UI.
  WindowList hidden_windows_;

  // Pointer to the most recently active app window
  // TODO(khmel): Get rid of |last_active_window_| and provide more reliable
  // way to determine active window.
  raw_ptr<AppWindowBase> last_active_window_ = nullptr;

  // Scoped list of observed windows (for removal on destruction)
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      observed_windows_{this};

  std::unique_ptr<ShelfContextMenu> context_menu_;
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_APP_WINDOW_SHELF_ITEM_CONTROLLER_H_
