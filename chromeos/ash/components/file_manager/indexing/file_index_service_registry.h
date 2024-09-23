// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_FILE_INDEX_SERVICE_REGISTRY_H_
#define CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_FILE_INDEX_SERVICE_REGISTRY_H_

#include <map>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/file_manager/indexing/file_index_service.h"
#include "components/user_manager/user_manager.h"

namespace ash::file_manager {
// FileIndexService is per user. This manages the mapping from each user
// identified by AccountId to FileIndexService.
// This is effectively a singleton in production.
class COMPONENT_EXPORT(FILE_MANAGER) FileIndexServiceRegistry
    : public user_manager::UserManager::Observer {
 public:
  // Returns the global FileIndexServiceRegistry instance.
  static FileIndexServiceRegistry* Get();

  // Given user_manager's lifetime needs to outlive this instance.
  explicit FileIndexServiceRegistry(user_manager::UserManager* user_manager);
  FileIndexServiceRegistry(const FileIndexServiceRegistry&) = delete;
  FileIndexServiceRegistry operator=(FileIndexServiceRegistry&) = delete;
  ~FileIndexServiceRegistry() override;

  // Returns the FileIndexService for the given user.
  // The pointer is owned and destroyed by FileIndexServiceRegistry, it's
  // guaranteed to remain alive during the user session.
  FileIndexService* GetFileIndexService(const AccountId& account_id);

  // Shuts down all FileIndexService this instance holds.
  void Shutdown();

  // user_manager::UserManager::Observer:
  void OnUserProfileCreated(const user_manager::User& user) override;

 private:
  const raw_ptr<user_manager::UserManager> user_manager_;
  std::map<AccountId, FileIndexService> map_;

  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::Observer>
      observation_{this};
};

}  // namespace ash::file_manager

#endif  // CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_FILE_INDEX_SERVICE_REGISTRY_H_
