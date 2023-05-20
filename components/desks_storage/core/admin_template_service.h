// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKS_STORAGE_CORE_ADMIN_TEMPLATE_SERVICE_H_
#define COMPONENTS_DESKS_STORAGE_CORE_ADMIN_TEMPLATE_SERVICE_H_

#include "components/desks_storage/core/local_desk_data_manager.h"
#include "components/keyed_service/core/keyed_service.h"

namespace desks_storage {
class DeskModel;
class AdminTemplateModel;

// Service that provides AdminTemplateModelInstances
class AdminTemplateService : public KeyedService {
 public:
  // Standard constructor used in instances where we dont want to introduce
  // creates the sub-directory "app_launch_automation/" in the users' data
  // directory.
  AdminTemplateService(const base::FilePath& user_data_dir_path,
                       const AccountId& account_id);
  AdminTemplateService(const AdminTemplateService&) = delete;
  AdminTemplateService& operator=(const AdminTemplateService&) = delete;
  ~AdminTemplateService() override;

  // Returns the intended admin model. This method can return nullptr.
  virtual AdminTemplateModel* GetAdminModel();

  // Returns a full desk model, this should only be used by tests and the
  // storage backend when receiving a policy.  This method can return nullptr.
  virtual DeskModel* GetFullDeskModel();

  // Lets the caller know if the underlying storage backend is ready to be used.
  bool IsReady();

 private:
  std::unique_ptr<LocalDeskDataManager> data_manager_;
};

}  // namespace desks_storage

#endif  // COMPONENTS_DESKS_STORAGE_CORE_ADMIN_TEMPLATE_SERVICE_H_
