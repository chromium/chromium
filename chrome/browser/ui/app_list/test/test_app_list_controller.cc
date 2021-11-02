// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/test/test_app_list_controller.h"

#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"

namespace test {

TestAppListController::TestAppListController(AppListModelUpdater* model_updater)
    : model_updater_(model_updater) {}

TestAppListController::~TestAppListController() = default;

ash::AppListClient* TestAppListController::GetClient() {
  return AppListClientImpl::GetInstance();
}

void TestAppListController::AddItem(
    std::unique_ptr<ash::AppListItemMetadata> app_item) {
  model_updater_->OnItemAdded(std::move(app_item));
}

void TestAppListController::AddItemToFolder(
    std::unique_ptr<ash::AppListItemMetadata> app_item,
    const std::string& folder_id) {
  app_item->folder_id = folder_id;
  model_updater_->OnItemUpdated(std::move(app_item));
}

void TestAppListController::SetItemMetadata(
    const std::string& id,
    std::unique_ptr<ash::AppListItemMetadata> data) {
  model_updater_->OnItemUpdated(std::move(data));
}

aura::Window* TestAppListController::GetWindow() {
  NOTIMPLEMENTED();
  return nullptr;
}

bool TestAppListController::IsVisible(
    const absl::optional<int64_t>& display_id) {
  NOTIMPLEMENTED();
  return false;
}

bool TestAppListController::IsVisible() {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace test
