// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_PUBLIC_PERSONAL_COLLABORATION_DATA_PERSONAL_COLLABORATION_DATA_SERVICE_H_
#define COMPONENTS_DATA_SHARING_PUBLIC_PERSONAL_COLLABORATION_DATA_PERSONAL_COLLABORATION_DATA_SERVICE_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation_traits.h"
#include "base/supports_user_data.h"
#include "base/uuid.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/base/collaboration_id.h"
#include "components/sync/protocol/shared_tab_group_account_data_specifics.pb.h"

namespace data_sharing::personal_collaboration_data {

// The core class for managing personal data associated with a collaboration.
class PersonalCollaborationDataService : public KeyedService,
                                         public base::SupportsUserData {
 public:
  using SuccessCallback = base::OnceCallback<void(bool success)>;

  enum class SpecificsType {
    kUnknown = 0,
    kSharedTabSpecifics = 1,
    kSharedTabGroupSpecifics = 2,
    kMaxValue = kSharedTabGroupSpecifics,
  };
  // Observers observing updates to the personal collaboration data which can be
  // originate by either the local or remote clients.
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when the bridge(database) has been loaded. Will be called
    // immediately if the bridge has already initialized.
    virtual void OnInitialized() {}

    // Called when specifics have been added or changed.
    virtual void OnSpecificsUpdated(
        SpecificsType specifics_type,
        const std::string& storage_key,
        const sync_pb::SharedTabGroupAccountDataSpecifics& specifics) {}

    // Invoked when the PersonalCollaborationDataService is being destroyed.
    virtual void OnPersonalCollaborationDataServiceDestroyed() {}
  };

  PersonalCollaborationDataService() = default;
  ~PersonalCollaborationDataService() override = default;

  // Disallow copy/assign.
  PersonalCollaborationDataService(const PersonalCollaborationDataService&) =
      delete;
  PersonalCollaborationDataService& operator=(
      const PersonalCollaborationDataService&) = delete;

  // Add / remove observers.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Retrieves details for a specifics given the storage key. This possibly
  // trims the unknown field.
  virtual std::optional<sync_pb::SharedTabGroupAccountDataSpecifics>
  GetSpecifics(SpecificsType specifics_type,
               const std::string& storage_key) = 0;

  // Retrieves all local specifics. The returned pointers are owned by the
  // service and should not be stored. They are valid until the next
  // modification of the data.
  virtual std::vector<const sync_pb::SharedTabGroupAccountDataSpecifics*>
  GetAllSpecifics() const = 0;

  // Called to create or update the specifics associated with the given storage
  // key.
  virtual void CreateOrUpdateSpecifics(
      SpecificsType specifics_type,
      const std::string& storage_key,
      const sync_pb::SharedTabGroupAccountDataSpecifics& specifics) = 0;

  // Deletes a specifics associated with the given storage key.
  virtual void DeleteSpecifics(SpecificsType specifics_type,
                               const std::string& storage_key) = 0;

  // Returns whether the service has fully initialized.
  virtual bool IsInitialized() const = 0;
};

}  // namespace data_sharing::personal_collaboration_data

namespace base {
template <>
struct ScopedObservationTraits<
    data_sharing::personal_collaboration_data::PersonalCollaborationDataService,
    data_sharing::personal_collaboration_data::
        PersonalCollaborationDataService::Observer> {
  static void AddObserver(
      data_sharing::personal_collaboration_data::
          PersonalCollaborationDataService* source,
      data_sharing::personal_collaboration_data::
          PersonalCollaborationDataService::Observer* observer) {
    source->AddObserver(observer);
  }
  static void RemoveObserver(
      data_sharing::personal_collaboration_data::
          PersonalCollaborationDataService* source,
      data_sharing::personal_collaboration_data::
          PersonalCollaborationDataService::Observer* observer) {
    source->RemoveObserver(observer);
  }
};
}  // namespace base

#endif  // COMPONENTS_DATA_SHARING_PUBLIC_PERSONAL_COLLABORATION_DATA_PERSONAL_COLLABORATION_DATA_SERVICE_H_
