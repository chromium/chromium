// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_TEST_APP_LIST_SYNCABLE_SERVICE_TEST_BASE_H_
#define CHROME_BROWSER_UI_APP_LIST_TEST_APP_LIST_SYNCABLE_SERVICE_TEST_BASE_H_

#include <string>

#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"

namespace test {

class AppListSyncableServiceTestBase : public AppListTestBase {
 public:
  AppListSyncableServiceTestBase();
  AppListSyncableServiceTestBase(const AppListSyncableServiceTestBase&) =
      delete;
  AppListSyncableServiceTestBase& operator=(
      const AppListSyncableServiceTestBase&) = delete;
  ~AppListSyncableServiceTestBase() override;

  // AppListTestBase:
  void SetUp() override;

 protected:
  // Used by subclasses to set up the fake model updater. If this method is not
  // overridden, `ChromeAppListModelUpdater` is used.
  virtual void SetUpFakeModelUpdaterFactoryIfNecessary() {}

  // Remove all existing sync items.
  void RemoveAllExistingItems();

  void InstallExtension(extensions::Extension* extension);

  // Gets the ids of the items in model updater ordered by item's ordinal
  // position.
  std::vector<std::string> GetOrderedItemIdsFromModelUpdater();

  // Similar to `GetOrderedItemIdsFromModelUpdater()`. But items are from
  // `AppListSyncableService` and they are ordered by positions in sync data.
  // Note that an item's position in model updater could be different from that
  // in sync data when app list is under temporary sort.
  std::vector<std::string> GetOrderedItemIdsFromSyncableService();

  // Gets the names of the items ordered by the positions stored in sync data.
  std::vector<std::string> GetNamesOfSortedItemsFromSyncableService();

  AppListModelUpdater* GetModelUpdater();

  const app_list::AppListSyncableService::SyncItem* GetSyncItem(
      const std::string& id) const;

  app_list::AppListSyncableService* app_list_syncable_service() {
    return app_list_syncable_service_.get();
  }

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<app_list::AppListSyncableService> app_list_syncable_service_;
};

}  // namespace test

#endif  // CHROME_BROWSER_UI_APP_LIST_TEST_APP_LIST_SYNCABLE_SERVICE_TEST_BASE_H_
