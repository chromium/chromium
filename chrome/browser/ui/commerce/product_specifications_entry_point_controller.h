// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COMMERCE_PRODUCT_SPECIFICATIONS_ENTRY_POINT_CONTROLLER_H_
#define CHROME_BROWSER_UI_COMMERCE_PRODUCT_SPECIFICATIONS_ENTRY_POINT_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/commerce/core/compare/cluster_manager.h"

class BrowserWindowInterface;

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
    virtual void ShowEntryPointWithTitle(const std::u16string& title) {}

    // Called when entry points should hide.
    virtual void HideEntryPoint() {}
  };

  // Possible source actions that could trigger compare entry points. These must
  // be kept in sync with the values in enums.xml.
  enum class CompareEntryPointTrigger {
    FROM_SELECTION = 0,
    FROM_NAVIGATION = 1,
    kMaxValue = FROM_NAVIGATION,
  };

  explicit ProductSpecificationsEntryPointController(
      BrowserWindowInterface* browser);
  ~ProductSpecificationsEntryPointController() override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

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

  // The moment when (1) the entry point being triggered to show and (2) the
  // entry point becoming eligible to show on the UI-side could be different.
  // This method allows the entry point to check if it should still show when it
  // becomes eligible to show on the UI side.
  virtual bool ShouldExecuteEntryPointShow();

  // ClusterManager::Observer
  void OnClusterFinishedForNavigation(const GURL& url) override;

  // Gets called by CommerceUiTabHelper to be notified about any navigation
  // events in this window that happens in `contents`.
  void DidFinishNavigation(content::WebContents* contents);

  std::optional<EntryPointInfo> entry_point_info_for_testing() {
    return current_entry_point_info_;
  }

 private:
  void MaybeHideEntryPoint();

  // Check entry point info for tab selection. This will first check if the
  // `entry_point_info` is valid based on info of current browser window. Then
  // it might call server-side clustering, and ultimately trigger an observer
  // event to show the UI.
  void CheckEntryPointInfoForSelection(
      const GURL old_url,
      const GURL new_url,
      std::optional<EntryPointInfo> entry_point_info);

  // Check entry point info for navigation. This will first check if the
  // `entry_point_info` is valid based on info of current browser window. Then
  // it might call server-side clustering, and ultimately trigger an observer
  // event to show the UI.
  void CheckEntryPointInfoForNavigation(
      std::optional<EntryPointInfo> entry_point_info);

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
  raw_ptr<BrowserWindowInterface, DanglingUntriaged> browser_;
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
