// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKS_STORAGE_CORE_DESK_MODEL_H_
#define COMPONENTS_DESKS_STORAGE_CORE_DESK_MODEL_H_

#include <stddef.h>

#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "base/values.h"

namespace ash {
class DeskTemplate;
enum class DeskTemplateType;
}

namespace apps {
class AppRegistryCache;
}

namespace desks_storage {

class DeskModelObserver;
// The DeskModel is an interface for accessing desk templates.
// Actual desk template storage backend (e.g. local file system backend and Sync
// backend) classes should implement this interface. Desk template accessor
// methods return results using callbacks to allow backend implementation
// to use asynchronous I/O.
class DeskModel {
 public:
  // Status codes for listing all desk template entries.
  // kPartialFailure indicates that one or more entries failed to load.
  enum class GetAllEntriesStatus {
    kOk,
    kPartialFailure,
    kFailure,
  };

  // Status codes for getting desk template by UUID.
  enum class GetEntryByUuidStatus {
    kOk,
    kFailure,
    kNotFound,
    kInvalidUuid,
  };

  // Status codes for adding or updating a desk template.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class AddOrUpdateEntryStatus {
    kOk = 0,
    kFailure = 1,
    kInvalidArgument = 2,
    kHitMaximumLimit = 3,
    kEntryTooLarge = 4,
    kMaxValue = kEntryTooLarge,
  };

  // Status codes for deleting desk templates.
  enum class DeleteEntryStatus {
    kOk,
    kFailure,
  };

  // status codes for getting template Json representations.
  enum class GetTemplateJsonStatus {
    kOk,
    kNotFound,
    kInvalidUuid,
    kFailure,
  };

  // Stores GetAllEntries result.
  struct GetAllEntriesResult {
    GetAllEntriesResult(
        GetAllEntriesStatus status,
        std::vector<raw_ptr<const ash::DeskTemplate, VectorExperimental>>
            entries);
    GetAllEntriesResult(GetAllEntriesResult& other);
    ~GetAllEntriesResult();

    GetAllEntriesStatus status;
    std::vector<raw_ptr<const ash::DeskTemplate, VectorExperimental>> entries;
  };

  // Stores GetEntryByUuid result.
  struct GetEntryByUuidResult {
    GetEntryByUuidResult(GetEntryByUuidStatus status,
                         std::unique_ptr<ash::DeskTemplate> entry);
    ~GetEntryByUuidResult();

    GetEntryByUuidStatus status;
    std::unique_ptr<ash::DeskTemplate> entry;
  };

  DeskModel();
  DeskModel(const DeskModel&) = delete;
  DeskModel& operator=(const DeskModel&) = delete;
  virtual ~DeskModel();

  // TODO(crbug.com/1320805): Once DeskSyncBridge is set to support saved desk,
  // add methods to support operations on both types of templates.
  // Returns all entries in the model.
  virtual GetAllEntriesResult GetAllEntries() = 0;

  // Get a specific desk template by `uuid`. Actual storage backend does not
  // need to keep desk templates in memory. The storage backend could load the
  // specified desk template into memory and then call the `callback` with a
  // unique_ptr to the loaded desk template.
  // If the specified desk template does not exist, `callback` will be called
  // with `kNotFound` and a `nullptr`. If the specified desk template exists,
  // but could not be loaded/parsed, `callback` will be called with `kFailure`
  // and a nullptr. An asynchronous `callback` is used here to accommodate
  // storage backend that need to perform asynchronous I/O.
  virtual GetEntryByUuidResult GetEntryByUUID(const base::Uuid& uuid) = 0;

  using AddOrUpdateEntryCallback =
      base::OnceCallback<void(AddOrUpdateEntryStatus status,
                              std::unique_ptr<ash::DeskTemplate> new_entry)>;
  // Add or update a desk template by `new_entry`'s UUID.
  // The given template's name could be cleaned (e.g. removing trailing
  // whitespace) and truncated to a reasonable length before saving. This method
  // will also validate the given `new_entry`. If the `new_entry` is missing
  // critical information, such as `uuid`, `callback` will be called with
  // `kInvalidArgument`. If the given desk template could not be persisted due
  // to any backend error, `callback` will be called with `kFailure`.
  virtual void AddOrUpdateEntry(std::unique_ptr<ash::DeskTemplate> new_entry,
                                AddOrUpdateEntryCallback callback) = 0;

  using GetTemplateJsonCallback =
      base::OnceCallback<void(GetTemplateJsonStatus status,
                              const base::Value& json_representation)>;
  // Retrieves a template based on its `uuid`, if found returns a std::string
  // containing the json representation of the template queried.
  virtual void GetTemplateJson(const base::Uuid& uuid,
                               apps::AppRegistryCache* app_cache,
                               GetTemplateJsonCallback callback);

  using DeleteEntryCallback =
      base::OnceCallback<void(DeleteEntryStatus status)>;
  // Remove entry with `uuid` from entries. If the entry with `uuid` does not
  // exist, then the deletion is considered a success.
  virtual void DeleteEntry(const base::Uuid& uuid,
                           DeleteEntryCallback callback) = 0;

  // Delete all entries.
  virtual void DeleteAllEntries(DeleteEntryCallback callback) = 0;

  // Gets the number of templates currently saved.
  // This method assumes each implementation has a cache and can return the
  // count synchronously.
  virtual size_t GetEntryCount() const = 0;

  // Gets the number of save and recall desks currently saved.
  virtual size_t GetSaveAndRecallDeskEntryCount() const = 0;

  // Gets the number of desk templates currently saved.
  virtual size_t GetDeskTemplateEntryCount() const = 0;

  // Gets the maximum number of save and recall desks entry this storage backend
  // could hold.
  virtual size_t GetMaxSaveAndRecallDeskEntryCount() const = 0;

  // Gets the maximum number of desk template entry this storage backend
  // could hold.
  virtual size_t GetMaxDeskTemplateEntryCount() const = 0;

  // Returns a vector of desk template UUIDs.
  // This method assumes each implementation has a cache and can return the
  // UUIDs synchronously.
  virtual std::set<base::Uuid> GetAllEntryUuids() const = 0;

  // Whether this model is ready for saving and reading desk templates.
  virtual bool IsReady() const = 0;

  // Whether this model is syncing desk templates to server.
  virtual bool IsSyncing() const = 0;

  // Returns another template that shares the same `name` as the template with
  // the uuid `uuid`. The `uuid` is used to make sure we are not returning the
  // current entry itself.
  virtual ash::DeskTemplate* FindOtherEntryWithName(
      const std::u16string& name,
      ash::DeskTemplateType type,
      const base::Uuid& uuid) const = 0;

  virtual std::string GetCacheGuid() = 0;

  // Observer registration methods. The model will remove all observers upon
  // destruction automatically.
  void AddObserver(DeskModelObserver* observer);
  void RemoveObserver(DeskModelObserver* observer);

  // Updates the preconfigured desk templates from policy.
  void SetPolicyDeskTemplates(const std::string& policy_json);

  // Removes the preconfigured desk templates from policy.
  void RemovePolicyDeskTemplates();

 protected:
  // Finds the admin desk template with the given `uuid`. Returns `nullptr`
  // if none is found.
  std::unique_ptr<ash::DeskTemplate> GetAdminDeskTemplateByUUID(
      const base::Uuid& uuid) const;

  // The observers.
  base::ObserverList<DeskModelObserver>::Unchecked observers_;

  // The preconfigured desk templates from policy (as opposed to user-defined)
  std::vector<std::unique_ptr<ash::DeskTemplate>> policy_entries_;

 private:
  // Handles conversion of DeskTemplate to policy JSON after the queried
  // DeskTemplate has been retrieved from the implemented class.
  void HandleTemplateConversionToPolicyJson(
      GetTemplateJsonCallback callback,
      apps::AppRegistryCache* app_cache,
      GetEntryByUuidStatus status,
      std::unique_ptr<ash::DeskTemplate> entry);
};

}  // namespace desks_storage

#endif  // COMPONENTS_DESKS_STORAGE_CORE_DESK_MODEL_H_
