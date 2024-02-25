// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_STANDALONE_BROWSER_EXTENSION_APP_SHELF_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_SHELF_STANDALONE_BROWSER_EXTENSION_APP_SHELF_ITEM_CONTROLLER_H_

#include <memory>
#include <optional>
#include <vector>

#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_model_observer.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "components/services/app_service/public/cpp/icon_loader.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"

class StandaloneBrowserExtensionAppContextMenu;

// This class controls both the appearance and behavior of shelf items for
// lacros-based extension apps -- aka packaged v2 apps / chrome apps.
//
// This class is considered the source of truth for information about running
// chrome apps. It's responsible for updating the InstanceRegistry.
class StandaloneBrowserExtensionAppShelfItemController
    : public ash::ShelfItemDelegate,
      public ash::ShelfModelObserver,
      public aura::WindowObserver,
      public wm::ActivationChangeObserver {
 public:
  // This constructor is used for a shelf item controller for a pinned item.
  explicit StandaloneBrowserExtensionAppShelfItemController(
      const ash::ShelfID& shelf_id);

  // This constructor is used for a shelf item controller for a running
  // instance.
  StandaloneBrowserExtensionAppShelfItemController(const ash::ShelfID& shelf_id,
                                                   aura::Window* window);

  StandaloneBrowserExtensionAppShelfItemController(
      const StandaloneBrowserExtensionAppShelfItemController&) = delete;
  StandaloneBrowserExtensionAppShelfItemController& operator=(
      const StandaloneBrowserExtensionAppShelfItemController&) = delete;
  ~StandaloneBrowserExtensionAppShelfItemController() override;

  // ash::ShelfItemDelegate overrides:
  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ash::ShelfLaunchSource source,
                    ItemSelectedCallback callback,
                    const ItemFilterPredicate& filter_predicate) override;
  void GetContextMenu(int64_t display_id,
                      GetContextMenuCallback callback) override;
  void ExecuteCommand(bool from_context_menu,
                      int64_t command_id,
                      int32_t event_flags,
                      int64_t display_id) override;
  void Close() override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(wm::ActivationChangeObserver::ActivationReason reason,
                         aura::Window* new_active,
                         aura::Window* old_active) override;

  // ash::ShelfModelObserver overrides:
  // This instance is guaranteed to be constructed before the corresponding
  // shelf item is added to the ShelfModel. That's because all shelf items must
  // be added atomically with a delegate to the shelf model. That means that the
  // delegate must be constructed before the insertion happens.
  // This class listens for the addition so that it can set the correct initial
  // state on the shelf item.
  void ShelfItemAdded(int index) override;

  // This method is called by ChromeAppWindowTrackerAsh to inform this member of
  // a newly running instance associated with this app. This class is
  // responsible for tracking destruction of that instance.
  void StartTrackingInstance(aura::Window* window);

  // Returns the number of open windows associated with the extension app.
  size_t WindowCount();

 private:
  using ShelfItem = ash::ShelfItem;

  // Called by AppServiceProxy once an icon has been loaded.
  void OnLoadIcon(apps::IconValuePtr icon_value);

  // aura::WindowObserver overrides:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
  void OnWindowDestroying(aura::Window* window) override;

  // Updates AppService's instance to set the activated status for `window`.
  void SetWindowActivated(aura::Window* window, bool is_active);

  // Sets AppService's instance state when `window` is added. Based on `window`
  // visibility and activated status, set instance status as visible or
  // activated.
  void InitWindowStatus(aura::Window* window);

  // Updates AppService's instance state as `instance_state` for `window`.
  void UpdateInstance(aura::Window* window, apps::InstanceState instance_state);

  // Returns the shelf index of the corresponding shelf item. Guaranteed to be a
  // valid index since this instance exists if and only if a shelf item exists.
  int GetShelfIndex();

  // Returns whether the corresponding shelf item has been added to the shelf.
  // There is a brief period of time after construction of this instance when
  // this has not occurred yet.
  bool ItemAddedToShelf();

  // The vector of windows associated with the shelf item. This can be the empty
  // vector if the item is pinned and there are no running instances. Otherwise,
  // there should be exactly one window per instance.
  // In order to preserve order that menu items show up, we use a vector instead
  // of a set. The order of the windows in the vector is simply the order in
  // which they were tracked by this file.
  std::vector<raw_ptr<aura::Window, VectorExperimental>> windows_;

  // The list of windows that was last shown via a context menu. Windows as they
  // are destroyed are removed from this list. The existing API only returns a
  // command_id as metadata, so there is no way to accurately record the item
  // that was selected, in case windows are destroyed in between.
  std::vector<raw_ptr<aura::Window, VectorExperimental>> context_menu_windows_;

  // This member lets the IconLoader know that we still need the icon.
  std::unique_ptr<apps::IconLoader::Releaser> icon_loader_releaser_;

  // Stores the icon.
  std::optional<gfx::ImageSkia> icon_;

  // This class is responsible for displaying a single instance of a context
  // menu. If multiple context menus are requested, only the latest instance
  // will be retained.
  std::unique_ptr<StandaloneBrowserExtensionAppContextMenu> context_menu_;

  // The map to save the instance state for each window to update AppService
  // Instance.
  base::flat_map<aura::Window*, apps::InstanceState> window_status_;

  // Observes the shelf model for item additions in order to set initial state
  // on the corresponding ShelfItem.
  base::ScopedObservation<ash::ShelfModel, ash::ShelfModelObserver>
      shelf_model_observation_{this};

  // Observes windows in |pending_windows_| for destruction.
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      window_observations_{this};

  base::ScopedObservation<wm::ActivationClient, wm::ActivationChangeObserver>
      activation_client_observation_{this};

  base::WeakPtrFactory<StandaloneBrowserExtensionAppShelfItemController>
      weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_STANDALONE_BROWSER_EXTENSION_APP_SHELF_ITEM_CONTROLLER_H_
