// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKS_STORAGE_CORE_LOCAL_DESK_DATA_MANAGER_H_
#define COMPONENTS_DESKS_STORAGE_CORE_LOCAL_DESK_DATA_MANAGER_H_

#include <map>
#include <memory>

#include "ash/public/cpp/desk_template.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "base/task/task_runner_util.h"
#include "components/account_id/account_id.h"
#include "components/desks_storage/core/desk_model.h"

namespace ash {
class OverviewTestBase;
}  // namespace ash

namespace desks_storage {
// The LocalDeskDataManager is the local storage implementation of
// the DeskModel interface and handles storage operations for local
// desk templates and save and recall desks.
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

  LocalDeskDataManager(const base::FilePath& user_data_dir_path,
                       const AccountId& account_id);

  LocalDeskDataManager(const LocalDeskDataManager&) = delete;
  LocalDeskDataManager& operator=(const LocalDeskDataManager&) = delete;
  ~LocalDeskDataManager() override;

  // DeskModel:
  DeskModel::GetAllEntriesResult GetAllEntries() override;
  void GetEntryByUUID(const std::string& uuid_str,
                      GetEntryByUuidCallback callback) override;
  void AddOrUpdateEntry(std::unique_ptr<ash::DeskTemplate> new_entry,
                        AddOrUpdateEntryCallback callback) override;
  void DeleteEntry(const std::string& uuid,
                   DeleteEntryCallback callback) override;
  void DeleteAllEntries(DeleteEntryCallback callback) override;
  std::size_t GetEntryCount() const override;
  std::size_t GetMaxEntryCount() const override;
  std::size_t GetSaveAndRecallDeskEntryCount() const override;
  std::size_t GetDeskTemplateEntryCount() const override;
  std::size_t GetMaxSaveAndRecallDeskEntryCount() const override;
  std::size_t GetMaxDeskTemplateEntryCount() const override;
  std::vector<base::GUID> GetAllEntryUuids() const override;
  bool IsReady() const override;
  bool IsSyncing() const override;
  ash::DeskTemplate* FindOtherEntryWithName(
      const std::u16string& name,
      ash::DeskTemplateType type,
      const base::GUID& uuid) const override;

  static void SetDisableMaxTemplateLimitForTesting(bool disabled);
  static void SetExcludeSaveAndRecallDeskInMaxEntryCountForTesting(
      bool disabled);

 private:
  friend class ash::OverviewTestBase;

  // Loads templates from `user_data_dir_path` into the
  // `saved_desks_list_`, based on the template's desk type, if the cache is not
  // loaded yet.
  void EnsureCacheIsLoaded(
      const base::FilePath& user_data_dir_path,
      CacheStatus* cache_status_ptr,
      std::map<base::GUID, std::unique_ptr<ash::DeskTemplate>>* entries_ptr);

  // Get a specific entry by `uuid_str`.
  void GetEntryByUuidTask(const std::string& uuid_str,
                          DeskModel::GetEntryByUuidStatus* status_ptr);

  // Wrapper method to call GetEntryByUuidCallback.
  void OnGetEntryByUuid(
      const std::string& uuid_str,
      std::unique_ptr<DeskModel::GetEntryByUuidStatus> status_ptr,
      std::unique_ptr<ash::DeskTemplate*> entry_ptr_ptr,
      DeskModel::GetEntryByUuidCallback callback);

  // Add or update an entry by `new_entry`'s UUID.
  void AddOrUpdateEntryTask(const base::GUID uuid,
                            DeskModel::AddOrUpdateEntryStatus* status_ptr,
                            base::Value entry_base_value);

  // Wrapper method to call AddOrUpdateEntryCallback.
  void OnAddOrUpdateEntry(
      std::unique_ptr<DeskModel::AddOrUpdateEntryStatus> status_ptr,
      DeskModel::AddOrUpdateEntryCallback callback,
      bool is_update,
      ash::DeskTemplateType desk_type,
      const base::GUID uuid,
      std::unique_ptr<ash::DeskTemplate> entry);

  // Remove entry with `uuid_str`. If the entry with `uuid_str` does not
  // exist, then the deletion is considered a success.
  void DeleteEntryTask(const std::string& uuid_str,
                       DeskModel::DeleteEntryStatus* status_ptr);

  // Delete all entries.
  void DeleteAllEntriesTask(
      DeskModel::DeleteEntryStatus* status_ptr,
      std::map<base::GUID, std::unique_ptr<ash::DeskTemplate>>* entries_ptr);

  // Wrapper method to call DeleteEntryCallback.
  void OnDeleteEntry(
      std::unique_ptr<DeskModel::DeleteEntryStatus> status_ptr,
      std::unique_ptr<std::map<base::GUID, std::unique_ptr<ash::DeskTemplate>>>
          entries_ptr,
      DeskModel::DeleteEntryCallback callback);

  // Returns the desk type of the `uuid`.
  ash::DeskTemplateType GetDeskTypeOfUuid(const base::GUID uuid) const;

  // Wrapper method to load the read files into the `saved_desks_list_` cache.
  void MoveEntriesIntoCache(
      std::unique_ptr<CacheStatus> cache_status_ptr,
      std::unique_ptr<std::map<base::GUID, std::unique_ptr<ash::DeskTemplate>>>
          entries_ptr);

  // Task runner used to schedule tasks on the IO thread.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // File path to the user data directory's: e.g.
  // "/path/to/user/data/dir/".
  const base::FilePath user_data_dir_path_;

  // File path to the saveddesks template subdirectory in user data directory's:
  // e.g. "/path/to/user/data/dir/saveddesk".
  const base::FilePath local_saved_desk_path_;

  // Account ID of the user this class will cache app data for.
  const AccountId account_id_;

  // Cache status of the templates cache for both desk types.
  CacheStatus cache_status_;

  using SavedDesks = std::map<base::GUID, std::unique_ptr<ash::DeskTemplate>>;

  // In memory cache of saved desks based on their type.
  std::map<ash::DeskTemplateType, SavedDesks> saved_desks_list_;

  // Weak pointer factory for posting tasks to task runner.
  base::WeakPtrFactory<LocalDeskDataManager> weak_ptr_factory_{this};
};

}  // namespace desks_storage

#endif  // COMPONENTS_DESKS_STORAGE_CORE_LOCAL_DESK_DATA_MANAGER_H_
