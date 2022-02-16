// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKS_STORAGE_CORE_LOCAL_DESK_DATA_MANAGER_H_
#define COMPONENTS_DESKS_STORAGE_CORE_LOCAL_DESK_DATA_MANAGER_H_

#include <map>
#include <memory>

#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "components/desks_storage/core/desk_model.h"

namespace ash {
class DeskTemplate;
class OverviewTestBase;
}  // namespace ash

namespace desks_storage {
// The LocalDeskDataManager is the local storage implementation of
// the DeskModel interface and handles storage operations for local
// desk templates.
//
// TODO(crbug.com/1227215): add calls to DeskModelObserver
class LocalDeskDataManager : public DeskModel {
 public:
  // This enumerates the possible statuses of the cache and is
  // used by the implementation in order to change the outcomes
  // of operations given certain states as well as to instantiate
  // the cache if it hasn't been instantiated.
  enum class CacheStatus {
    // Cache is ready for operations.
    kOk,

    // Cache needs to be initialized before operations can be performed.
    kNotInitialized,

    // The Path the DataManager was constructed with is invalid.  All DeskModel
    // statuses returned from this object will return failures.
    kInvalidPath,
  };

  explicit LocalDeskDataManager(const base::FilePath& path);

  LocalDeskDataManager(const LocalDeskDataManager&) = delete;
  LocalDeskDataManager& operator=(const LocalDeskDataManager&) = delete;
  ~LocalDeskDataManager() override;

  // DeskModel:
  void GetAllEntries(GetAllEntriesCallback callback) override;
  void GetEntryByUUID(const std::string& uuid,
                      GetEntryByUuidCallback callback) override;
  void AddOrUpdateEntry(std::unique_ptr<ash::DeskTemplate> new_entry,
                        AddOrUpdateEntryCallback callback) override;
  void DeleteEntry(const std::string& uuid,
                   DeleteEntryCallback callback) override;
  void DeleteAllEntries(DeleteEntryCallback callback) override;
  std::size_t GetEntryCount() const override;
  std::size_t GetMaxEntryCount() const override;
  std::vector<base::GUID> GetAllEntryUuids() const override;
  bool IsReady() const override;
  bool IsSyncing() const override;

  static void SetDisableMaxTemplateLimitForTesting(bool disabled);

 private:
  friend class ash::OverviewTestBase;

  // Loads desk templates from |local_path_| into cache if the cache is not
  // loaded yet.
  void EnsureCacheIsLoaded();

  // Gets all desk templates from user's local template directory.
  void GetAllEntriesTask(DeskModel::GetAllEntriesStatus* status_ptr,
                         std::vector<ash::DeskTemplate*>* entries_ptr);

  // Wrapper method to call GetAllEntriesCallback.
  void OnGetAllEntries(
      std::unique_ptr<DeskModel::GetAllEntriesStatus> status_ptr,
      std::unique_ptr<std::vector<ash::DeskTemplate*>> entries_ptr,
      DeskModel::GetAllEntriesCallback callback);

  // Get a specific desk template by |uuid_str|.
  void GetEntryByUuidTask(const std::string& uuid_str,
                          DeskModel::GetEntryByUuidStatus* status_ptr,
                          ash::DeskTemplate** entry_ptr_ptr);

  // Wrapper method to call GetEntryByUuidCallback.
  void OnGetEntryByUuid(
      const std::string& uuid_str,
      std::unique_ptr<DeskModel::GetEntryByUuidStatus> status_ptr,
      std::unique_ptr<ash::DeskTemplate*> entry_ptr_ptr,
      DeskModel::GetEntryByUuidCallback callback);

  // Add or update a desk template by |new_entry|'s UUID.
  void AddOrUpdateEntryTask(std::unique_ptr<ash::DeskTemplate> new_entry,
                            DeskModel::AddOrUpdateEntryStatus* status_ptr);

  // Wrapper method to call AddOrUpdateEntryCallback.
  void OnAddOrUpdateEntry(
      std::unique_ptr<DeskModel::AddOrUpdateEntryStatus> status_ptr,
      DeskModel::AddOrUpdateEntryCallback callback);

  // Remove entry with |uuid_str|. If the entry with |uuid_str| does not
  // exist, then the deletion is considered a success.
  void DeleteEntryTask(const std::string& uuid_str,
                       DeskModel::DeleteEntryStatus* status_ptr);

  // Delete all entries.
  void DeleteAllEntriesTask(DeskModel::DeleteEntryStatus* status_ptr);

  // Wrapper method to call DeleteEntryCallback.
  void OnDeleteEntry(std::unique_ptr<DeskModel::DeleteEntryStatus> status_ptr,
                     DeskModel::DeleteEntryCallback callback);

  // Returns true if |templates_| contains a desk template with |name|.
  bool HasTemplateWithName(const std::u16string& name);

  // Task runner used to schedule tasks on the IO thread.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // File path to the template subdirection in user data directory's:
  // e.g. "/path/to/user/data/dir/templates".
  const base::FilePath local_path_;

  // In-memory desk template cache that owns desk_templates so that these desk
  // templates can be retrieved by GetAllEntries.
  // |templates_| is keyed by UUIDs.
  std::map<base::GUID, std::unique_ptr<ash::DeskTemplate>> templates_;

  // Flag indicating the status of this in memory cache.
  CacheStatus cache_status_;

  // Weak pointer factory for posting tasks to task runner.
  base::WeakPtrFactory<LocalDeskDataManager> weak_ptr_factory_{this};
};

}  // namespace desks_storage

#endif  // COMPONENTS_DESKS_STORAGE_CORE_LOCAL_DESK_DATA_MANAGER_H_
