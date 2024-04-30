// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_BROWSER_APP_SHELF_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_SHELF_BROWSER_APP_SHELF_CONTROLLER_H_

#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_model_observer.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance_observer.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance_registry.h"

namespace apps {
struct BrowserAppInstance;
class BrowserAppInstanceRegistry;
}  // namespace apps

namespace ash {
class ShelfModel;
struct ShelfItem;
}  // namespace ash

namespace aura {
class Window;
}

class ShelfSpinnerController;
class ChromeShelfItemFactory;

// Updates the shelf model in response to |BrowserAppInstanceRegistry| events.
//
// Observes the |BrowserAppInstanceRegistry| for lifecycle events of
// - browser-based apps in tabs and windows (web apps, system web apps) in Ash
//   and Lacros,
// - browser windows in Ash and Lacros.
//
// Updates to the shelf model:
// - sets shelf item status when an app is running or stopped,
// - creates or removes shelf items when necessary.
class BrowserAppShelfController : public apps::BrowserAppInstanceObserver,
                                  public ash::ShelfModelObserver {
 public:
  BrowserAppShelfController(
      Profile* profile,
      apps::BrowserAppInstanceRegistry& browser_app_instance_registry,
      ash::ShelfModel& model,
      ChromeShelfItemFactory& shelf_item_factory,
      ShelfSpinnerController& shelf_spinner_controller);

  BrowserAppShelfController(const BrowserAppShelfController&) = delete;
  BrowserAppShelfController& operator=(const BrowserAppShelfController&) =
      delete;

  ~BrowserAppShelfController() override;

  // apps::BrowserAppInstanceObserver overrides:
  void OnBrowserWindowAdded(
      const apps::BrowserWindowInstance& instance) override;
  void OnBrowserWindowRemoved(
      const apps::BrowserWindowInstance& instance) override;
  void OnBrowserAppAdded(const apps::BrowserAppInstance& instance) override;
  void OnBrowserAppUpdated(const apps::BrowserAppInstance& instance) override;
  void OnBrowserAppRemoved(const apps::BrowserAppInstance& instance) override;

  // ash::ShelfModelObserver overrides:
  void ShelfItemAdded(int index) override;

 private:
  // Creates a shelf item if it doesn't exist and sets its status.
  void CreateOrUpdateShelfItem(const ash::ShelfID& id,
                               ash::ShelfItemStatus status);
  // Sets shelf item status to closed and removes it if it's not pinned.
  void SetShelfItemClosed(const ash::ShelfID& id);
  void UpdateShelfItemStatus(const ash::ShelfItem& item,
                             ash::ShelfItemStatus status);
  // Updates Aura window's app-related keys when the active app associated with
  // this window changes.
  void MaybeUpdateWindowProperties(aura::Window* window);
  // Updates app-related properties of all the windows containing this app.
  void MaybeUpdateWindowPropertiesForApp(const std::string& app_id);

  raw_ptr<Profile> profile_;
  const raw_ref<ash::ShelfModel> model_;
  const raw_ref<ChromeShelfItemFactory> shelf_item_factory_;
  const raw_ref<ShelfSpinnerController, DanglingUntriaged>
      shelf_spinner_controller_;

  const raw_ref<apps::BrowserAppInstanceRegistry>
      browser_app_instance_registry_;

  base::ScopedObservation<apps::BrowserAppInstanceRegistry,
                          apps::BrowserAppInstanceObserver>
      registry_observation_{this};
  base::ScopedObservation<ash::ShelfModel, ash::ShelfModelObserver>
      shelf_model_observation_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_BROWSER_APP_SHELF_CONTROLLER_H_
