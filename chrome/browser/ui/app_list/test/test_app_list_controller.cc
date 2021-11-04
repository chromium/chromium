// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/test/test_app_list_controller.h"

#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"

namespace test {

TestAppListController::TestAppListController() = default;
TestAppListController::~TestAppListController() = default;

ash::AppListClient* TestAppListController::GetClient() {
  return AppListClientImpl::GetInstance();
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
