// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/admin_template_service.h"
#include "base/files/file_util.h"

namespace desks_storage {

AdminTemplateService::AdminTemplateService(
    const base::FilePath& user_data_dir_path,
    const AccountId& account_id) {
  data_manager_ = std::make_unique<LocalDeskDataManager>(
      user_data_dir_path, account_id,
      LocalDeskDataManager::StorageLocation::kAppLaunchAutomationDir);
}

AdminTemplateService::~AdminTemplateService() = default;

AdminTemplateModel* AdminTemplateService::GetAdminModel() {
  return data_manager_.get();
}

DeskModel* AdminTemplateService::GetFullDeskModel() {
  return data_manager_.get();
}

bool AdminTemplateService::IsReady() {
  return data_manager_->IsReady();
}

}  // namespace desks_storage
