// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_TEST_TEST_APP_LIST_CONTROLLER_H_
#define CHROME_BROWSER_UI_APP_LIST_TEST_TEST_APP_LIST_CONTROLLER_H_

#include "ash/public/cpp/app_list/app_list_controller.h"

class AppListModelUpdater;

namespace test {

// A fake app list controller used by browser side unit tests to emulate the
// interaction between browser and ash in tests.
class TestAppListController : public ash::AppListController {
 public:
  explicit TestAppListController(AppListModelUpdater* model_updater);
  TestAppListController(const TestAppListController&) = delete;
  TestAppListController& operator=(const TestAppListController&) = delete;
  ~TestAppListController() override;

  // ash::AppListController:
  void SetClient(ash::AppListClient* client) override {}
  ash::AppListClient* GetClient() override;
  void AddObserver(ash::AppListControllerObserver* observer) override {}
  void RemoveObserver(ash::AppListControllerObserver* obsever) override {}
  void SetActiveModel(ash::AppListModel* model,
                      ash::SearchModel* search_model) override {}
  void AddItem(std::unique_ptr<ash::AppListItemMetadata> app_item) override;
  void AddItemToFolder(std::unique_ptr<ash::AppListItemMetadata> app_item,
                       const std::string& folder_id) override;
  void RemoveItem(const std::string& id) override {}
  void RemoveUninstalledItem(const std::string& id) override {}
  void SetStatus(ash::AppListModelStatus status) override {}
  void SetSearchEngineIsGoogle(bool is_google) override {}
  void UpdateSearchBox(const std::u16string& text,
                       bool initiated_by_user) override {}
  void PublishSearchResults(
      std::vector<std::unique_ptr<ash::SearchResultMetadata>> results,
      const std::vector<ash::AppListSearchResultCategory>& categories)
      override {}
  void SetItemMetadata(const std::string& id,
                       std::unique_ptr<ash::AppListItemMetadata> data) override;
  void SetItemIconVersion(const std::string& id, int icon_version) override {}
  void SetItemIcon(const std::string& id, const gfx::ImageSkia& icon) override {
  }
  void SetItemNotificationBadgeColor(const std::string& id,
                                     const SkColor color) override {}
  void SetModelData(int profile_id,
                    std::vector<std::unique_ptr<ash::AppListItemMetadata>> apps,
                    bool is_search_engine_google) override {}
  void SetSearchResultMetadata(
      std::unique_ptr<ash::SearchResultMetadata> metadata) override {}
  void GetIdToAppListIndexMap(
      GetIdToAppListIndexMapCallback callback) override {}
  void NotifyProcessSyncChangesFinished() override {}
  void DismissAppList() override {}
  void GetAppInfoDialogBounds(
      GetAppInfoDialogBoundsCallback callback) override {}
  void ShowAppList() override {}
  aura::Window* GetWindow() override;
  bool IsVisible(const absl::optional<int64_t>& display_id) override;
  bool IsVisible() override;

 private:
  AppListModelUpdater* const model_updater_;
};

}  // namespace test

#endif  // CHROME_BROWSER_UI_APP_LIST_TEST_TEST_APP_LIST_CONTROLLER_H_
