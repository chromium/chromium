// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_INTERNAL_PERSONAL_COLLABORATION_DATA_PERSONAL_COLLABORATION_DATA_SERVICE_IMPL_H_
#define COMPONENTS_DATA_SHARING_INTERNAL_PERSONAL_COLLABORATION_DATA_PERSONAL_COLLABORATION_DATA_SERVICE_IMPL_H_

#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/data_sharing/internal/personal_collaboration_data/personal_collaboration_data_sync_bridge.h"
#include "components/data_sharing/public/personal_collaboration_data/personal_collaboration_data_service.h"
#include "components/sync/base/collaboration_id.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_store.h"

namespace data_sharing::personal_collaboration_data {

// The core class for managing personal account linked collaboration data.
class PersonalCollaborationDataServiceImpl
    : public PersonalCollaborationDataService,
      public PersonalCollaborationDataSyncBridge::Observer {
 public:
  using Observer = PersonalCollaborationDataService::Observer;

  PersonalCollaborationDataServiceImpl(
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      syncer::OnceDataTypeStoreFactory data_type_store_factory);
  ~PersonalCollaborationDataServiceImpl() override;

  // Disallow copy/assign.
  PersonalCollaborationDataServiceImpl(
      const PersonalCollaborationDataServiceImpl&) = delete;
  PersonalCollaborationDataServiceImpl& operator=(
      const PersonalCollaborationDataServiceImpl&) = delete;

  // PersonalCollaborationDataService implementation:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  std::optional<sync_pb::SharedTabGroupAccountDataSpecifics> GetSpecifics(
      SpecificsType specifics_type,
      const std::string& storage_key) override;
  std::vector<const sync_pb::SharedTabGroupAccountDataSpecifics*>
  GetAllSpecifics() const override;
  void CreateOrUpdateSpecifics(
      SpecificsType specifics_type,
      const std::string& storage_key,
      const sync_pb::SharedTabGroupAccountDataSpecifics& specifics) override;
  void DeleteSpecifics(SpecificsType specifics_type,
                       const std::string& storage_key) override;
  bool IsInitialized() const override;

  // PersonalCollaborationDataSyncBridge::Observer implementation.
  void OnEntityAddedOrUpdatedFromSync(
      const sync_pb::SharedTabGroupAccountDataSpecifics& data) override;

 private:
  base::ObserverList<PersonalCollaborationDataService::Observer> observers_;
  std::unique_ptr<PersonalCollaborationDataSyncBridge> bridge_;
  base::ScopedObservation<PersonalCollaborationDataSyncBridge,
                          PersonalCollaborationDataSyncBridge::Observer>
      bridge_observer_{this};
  base::WeakPtrFactory<PersonalCollaborationDataServiceImpl> weak_ptr_factory_{
      this};
};

}  // namespace data_sharing::personal_collaboration_data

#endif  // COMPONENTS_DATA_SHARING_INTERNAL_PERSONAL_COLLABORATION_DATA_PERSONAL_COLLABORATION_DATA_SERVICE_IMPL_H_
