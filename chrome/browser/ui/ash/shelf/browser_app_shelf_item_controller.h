// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_BROWSER_APP_SHELF_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_SHELF_BROWSER_APP_SHELF_ITEM_CONTROLLER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/shelf_item_delegate.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "base/unguessable_token.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance_observer.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance_registry.h"
#include "components/services/app_service/public/cpp/icon_loader.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "ui/gfx/image/image_skia.h"

namespace apps {
class BrowserAppInstanceRegistry;
}

class Profile;
class ShelfContextMenu;

class BrowserAppShelfItemController : public ash::ShelfItemDelegate,
                                      public apps::BrowserAppInstanceObserver {
 public:
  BrowserAppShelfItemController(const ash::ShelfID& shelf_id, Profile* profile);

  BrowserAppShelfItemController(const BrowserAppShelfItemController&) = delete;
  BrowserAppShelfItemController& operator=(
      const BrowserAppShelfItemController&) = delete;

  ~BrowserAppShelfItemController() override;

  // ash::ShelfItemDelegate overrides:
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

  // BrowserAppInstanceObserver overrides:
  void OnBrowserWindowAdded(
      const apps::BrowserWindowInstance& instance) override;
  void OnBrowserWindowRemoved(
      const apps::BrowserWindowInstance& instance) override;
  void OnBrowserAppAdded(const apps::BrowserAppInstance& instance) override;
  void OnBrowserAppRemoved(const apps::BrowserAppInstance& instance) override;

 private:
  using CommandToInstanceMap = base::flat_map<int, base::UnguessableToken>;

  // Gets a list of instances (a pair of app menu command ID and instance ID)
  // matching the predicate.
  std::vector<std::pair<int, base::UnguessableToken>> GetMatchingInstances(
      const ItemFilterPredicate& filter_predicate);
  // Gets the command ID for this item. The item must exist.
  int GetInstanceCommand(const base::UnguessableToken& id);

  void LoadIcon(int32_t size_hint_in_dip, apps::LoadIconCallback callback);
  void OnLoadMediumIcon(apps::IconValuePtr icon_value);
  void OnLoadBittyIcon(apps::IconValuePtr icon_value);

  raw_ptr<Profile> profile_;
  const raw_ref<apps::BrowserAppInstanceRegistry> registry_;

  // ShelfContextMenu instance needs to be alive for the duration of the
  // GetMenuModel call.
  std::unique_ptr<ShelfContextMenu> context_menu_;

  std::unique_ptr<apps::IconLoader::Releaser> icon_loader_releaser_;
  gfx::ImageSkia medium_icon_;
  gfx::ImageSkia bitty_icon_;

  // Map of app menu item command IDs to instance IDs, used to maintain a stable
  // association of instances to command IDs and to order the items by launch
  // time. The set of instances is usually under 10 items so a flat map is
  // sufficient.
  CommandToInstanceMap command_to_instance_map_;
  int last_command_id_{0};

  base::ScopedObservation<apps::BrowserAppInstanceRegistry,
                          apps::BrowserAppInstanceObserver>
      registry_observation_{this};

  base::WeakPtrFactory<BrowserAppShelfItemController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_BROWSER_APP_SHELF_ITEM_CONTROLLER_H_
