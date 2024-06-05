// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COMMERCE_PRODUCT_SPECIFICATIONS_ENTRY_POINT_CONTROLLER_H_
#define CHROME_BROWSER_UI_COMMERCE_PRODUCT_SPECIFICATIONS_ENTRY_POINT_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/compare/cluster_manager.h"
#include "content/public/browser/web_contents.h"

class Browser;

namespace commerce {

class ShoppingService;
class ProductSpecificationsService;

class ProductSpecificationsEntryPointController
    : public TabStripModelObserver,
      public ClusterManager::Observer {
 public:
  // Observer that will listen to ProductSpecificationsEntryPointController for
  // updates regarding visibility and content of the entry point.
  class Observer : public base::CheckedObserver {
   public:
    // Called when entry points should show with `title`.
    virtual void ShowEntryPointWithTitle(const std::string title) {}

    // Called when entry points should hide.
    virtual void HideEntryPoint() {}
  };

  explicit ProductSpecificationsEntryPointController(Browser* browser);
  ~ProductSpecificationsEntryPointController() override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;

  // Registers an observer.
  void AddObserver(Observer* observer);

  // Removes an observer.
  void RemoveObserver(Observer* observer);

  // The entry point controlled by this controller has been clicked.
  virtual void OnEntryPointExecuted();

  // The entry point controlled by this controller has been explicitly dismissed
  // by the user.
  virtual void OnEntryPointDismissed();

  // The entry point controlled by this controller has hidden. It could due to
  // that the entry point (1) has been dismissed (2) has timed out (3) has
  // been clicked (4) is no longer valid.
  virtual void OnEntryPointHidden();

  // ClusterManager::Observer
  void OnClusterFinishedForNavigation(const GURL& url) override;

  std::optional<EntryPointInfo> entry_point_info_for_testing() {
    return current_entry_point_info_;
  }

 private:
  void MaybeHideEntryPoint();

  // Show the tab strip entry point for tab selection.
  void ShowEntryPointWithTitleForSelection(
      const GURL old_url,
      const GURL new_url,
      std::optional<EntryPointInfo> entry_point_info);

  // Show the tab strip entry point for navigation.
  void ShowEntryPointWithTitleForNavigation(
      std::optional<EntryPointInfo> entry_point_info);

  // Helper method to show the entry point with title.
  void ShowEntryPointWithTitle(std::optional<EntryPointInfo> entry_point_info);

  // Info of the entry point that is currently showing, when available.
  std::optional<EntryPointInfo> current_entry_point_info_;
  raw_ptr<Browser, DanglingUntriaged> browser_;
  raw_ptr<ShoppingService, DanglingUntriaged> shopping_service_;
  raw_ptr<ClusterManager, DanglingUntriaged> cluster_manager_;
  raw_ptr<ProductSpecificationsService> product_specifications_service_;
  base::ObserverList<Observer> observers_;
  base::ScopedObservation<ClusterManager, ClusterManager::Observer>
      cluster_manager_observations_{this};
  base::WeakPtrFactory<ProductSpecificationsEntryPointController>
      weak_ptr_factory_{this};
};
}  // namespace commerce

#endif  // CHROME_BROWSER_UI_COMMERCE_PRODUCT_SPECIFICATIONS_ENTRY_POINT_CONTROLLER_H_
