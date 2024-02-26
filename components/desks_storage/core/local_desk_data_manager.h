// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKS_STORAGE_CORE_LOCAL_DESK_DATA_MANAGER_H_
#define COMPONENTS_DESKS_STORAGE_CORE_LOCAL_DESK_DATA_MANAGER_H_

#include <stddef.h>

#include <memory>

#include "ash/public/cpp/desk_template.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "base/uuid.h"
#include "components/account_id/account_id.h"
#include "components/desks_storage/core/admin_template_model.h"
#include "components/desks_storage/core/desk_model.h"

namespace ash {
class OverviewTestBase;
}  // namespace ash

namespace desks_storage {
class LocalDeskDataManagerTest;

// The LocalDeskDataManager is the local storage implementation of
// the DeskModel interface and handles storage operations for local
// desk templates and save and recall desks.
//
// TODO(crbug.com/1227215): add calls to DeskModelObserver
class LocalDeskDataManager : public DeskModel, public AdminTemplateModel {
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

  // Local Desk Data Manager is agnostic towards a specific directory to save
  // information in, below are the possible locations the cache could be read
  // an wrote.  KSavedDeskDir is the default parameter for instantiating a
  // local desk data manager.
  enum class StorageLocation {
    // This data manager reads and writes to the standard saved desks template
    // directory.
    kSavedDeskDir,

    // This data manager reads and writes to the admin templates directory, also
    // known as the app launch automation directory.
    kAppLaunchAutomationDir,
  };

  LocalDeskDataManager(
      const base::FilePath& user_data_dir_path,
      const AccountId& account_id,
      StorageLocation storage_location = StorageLocation::kSavedDeskDir);

  LocalDeskDataManager(const LocalDeskDataManager&) = delete;
  LocalDeskDataManager& operator=(const LocalDeskDataManager&) = delete;
  ~LocalDeskDataManager() override;

  struct LoadCacheResult {
    LoadCacheResult(CacheStatus status,
                    std::vector<std::unique_ptr<ash::DeskTemplate>> entries);
    LoadCacheResult(LoadCacheResult&& other);
    ~LoadCacheResult();
    CacheStatus status;
    std::vector<std::unique_ptr<ash::DeskTemplate>> entries;
  };

  struct DeleteTaskResult {
    DeleteTaskResult(DeleteEntryStatus status,
                     std::vector<std::unique_ptr<ash::DeskTemplate>> entries);
    DeleteTaskResult(DeleteTaskResult&& other);
    ~DeleteTaskResult();
    DeleteEntryStatus status;
    std::vector<std::unique_ptr<ash::DeskTemplate>> entries;
  };

  // DeskModel:
  DeskModel::GetAllEntriesResult GetAllEntries() override;
  DeskModel::GetEntryByUuidResult GetEntryByUUID(
      const base::Uuid& uuid) override;
  void AddOrUpdateEntry(std::unique_ptr<ash::DeskTemplate> new_entry,
                        AddOrUpdateEntryCallback callback) override;
  void DeleteEntry(const base::Uuid& uuid,
                   DeleteEntryCallback callback) override;
  void DeleteAllEntries(DeleteEntryCallback callback) override;
  size_t GetEntryCount() const override;
  size_t GetSaveAndRecallDeskEntryCount() const override;
  size_t GetDeskTemplateEntryCount() const override;
  size_t GetMaxSaveAndRecallDeskEntryCount() const override;
  size_t GetMaxDeskTemplateEntryCount() const override;
  std::set<base::Uuid> GetAllEntryUuids() const override;
  bool IsReady() const override;
  bool IsSyncing() const override;
  ash::DeskTemplate* FindOtherEntryWithName(
      const std::u16string& name,
      ash::DeskTemplateType type,
      const base::Uuid& uuid) const override;
  std::string GetCacheGuid() override;

  // AdminTemplateModel:
  void UpdateEntry(std::unique_ptr<ash::DeskTemplate> entry) override;

  // This must be called after the class has been initialized and ready in the
  // test.
  void SetupFloatingWorkspaceForTest();
  static void SetDisableMaxTemplateLimitForTesting(bool disabled);

 private:
  friend class ash::OverviewTestBase;
  friend class LocalDeskDataManagerTest;

  // Update entry status used in testing to verify
  // behavior.
  enum class UpdateEntryStatus {
    kOk,
    kNotFound,
    kDuplicate,
    kOutdatedPolicy,
  };

  // Loads templates from `user_data_dir_path` and `sub_directory_name` into the
  // `saved_desks_list_`, based on the template's desk type, if the cache is not
  // loaded yet.
  static LoadCacheResult LoadCacheOnBackgroundSequence(
      const base::FilePath& user_data_dir_path,
      const std::string sub_directory_name);

  // Add or update an entry by `new_entry`'s UUID.
  static AddOrUpdateEntryStatus AddOrUpdateEntryTask(
      const base::FilePath& local_saved_desk_path,
      const base::Uuid uuid,
      base::Value entry_base_value,
      ash::DeskTemplateType desk_type);

  // Wrapper method to call AddOrUpdateEntryCallback.
  void OnAddOrUpdateEntry(AddOrUpdateEntryCallback callback,
                          bool is_update,
                          ash::DeskTemplateType desk_type,
                          const base::Uuid uuid,
                          std::unique_ptr<ash::DeskTemplate> old_entry,
                          std::unique_ptr<ash::DeskTemplate> new_entry,
                          AddOrUpdateEntryStatus status);

  using SavedDesks =
      base::flat_map<base::Uuid, std::unique_ptr<ash::DeskTemplate>>;

  // Remove entry with `uuid`. If the entry with `uuid` does not
  // exist, then the deletion is considered a success.
  static DeleteTaskResult DeleteEntryTask(
      const base::FilePath& local_saved_desk_path,
      const base::Uuid& uuid,
      std::vector<std::unique_ptr<ash::DeskTemplate>> roll_back_entry);

  // Delete all entries.
  static DeleteTaskResult DeleteAllEntriesTask(
      const base::FilePath& local_saved_desk_path,
      std::vector<std::unique_ptr<ash::DeskTemplate>> entries);

  // Wrapper method to call DeleteEntryCallback.
  void OnDeleteEntry(DeleteEntryCallback callback,
                     DeleteTaskResult delete_return);

  // Returns the desk type of the `uuid`.
  ash::DeskTemplateType GetDeskTypeOfUuid(const base::Uuid uuid) const;

  // Returns the max entry count of desk type `desk_type`.
  size_t GetMaxEntryCountByDeskType(ash::DeskTemplateType desk_type) const;

  // Wrapper method to load the read files into the `saved_desks_list_`
  // cache.
  void MoveEntriesIntoCache(LoadCacheResult cache_result);

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

  // Used in testing to verify update behavior.
  UpdateEntryStatus last_update_status_;

  // In memory cache of saved desks based on their type.
  base::flat_map<ash::DeskTemplateType, SavedDesks> saved_desks_list_;

  // Weak pointer factory for posting tasks to task runner.
  base::WeakPtrFactory<LocalDeskDataManager> weak_ptr_factory_{this};
};

}  // namespace desks_storage

#endif  // COMPONENTS_DESKS_STORAGE_CORE_LOCAL_DESK_DATA_MANAGER_H_
