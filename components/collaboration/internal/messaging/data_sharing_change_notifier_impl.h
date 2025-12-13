// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_DATA_SHARING_CHANGE_NOTIFIER_IMPL_H_
#define COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_DATA_SHARING_CHANGE_NOTIFIER_IMPL_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "components/collaboration/internal/messaging/data_sharing_change_notifier.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "google_apis/gaia/gaia_id.h"

namespace collaboration::messaging {

// The concrete implementation of the `DataSharingChangeNotifier`.
class DataSharingChangeNotifierImpl : public DataSharingChangeNotifier {
 public:
  explicit DataSharingChangeNotifierImpl(
      data_sharing::DataSharingService* data_sharing_service);
  ~DataSharingChangeNotifierImpl() override;

  // DataSharingChangeNotifier.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  FlushCallback Initialize() override;
  bool IsInitialized() override;

  // DataSharingService::Observer.
  void OnGroupDataModelLoaded() override;
  void OnGroupAdded(const data_sharing::GroupData& group_data,
                    const base::Time& event_time) override;
  void OnGroupRemoved(const data_sharing::GroupId& group_id,
                      const base::Time& event_time) override;
  void OnGroupMemberAdded(const data_sharing::GroupId& group_id,
                          const GaiaId& member_gaia_id,
                          const base::Time& event_time) override;
  void OnGroupMemberRemoved(const data_sharing::GroupId& group_id,
                            const GaiaId& member_gaia_id,
                            const base::Time& event_time) override;
  void OnSyncBridgeUpdateTypeChanged(
      data_sharing::SyncBridgeUpdateType sync_bridge_update_type) override;

 private:
  // Informs observers that this has been initialized. This is in a separate
  // method to ensure that we can post this if the DataSharingService is already
  // initialized when we try to initialize this class.
  void NotifyDataSharingChangeNotifierInitialized() const;

  // Fetches all group events since startup and publishes them to all observers.
  // This is used as the return value to Initialize(), and is intended to be a
  // way for the owner of this object to ensure they receive every update from
  // DataSharingService at a time they are able to handle it.
  void FlushGroupEventsSinceStartup();

  // Returns the GroupData for a group if it is still available, using all
  // methods available for fetching GroupData.
  std::optional<data_sharing::GroupData> GetGroupDataIfAvailable(
      const data_sharing::GroupId& group_id);

  // Publishes information about added groups. If group_data is already
  // available, it can be provided. This is the case at runtime, but not during
  // startup.
  void OnGroupAddedInternal(
      const data_sharing::GroupId& group_id,
      const std::optional<data_sharing::GroupData>& group_data,
      const base::Time& event_time);
  // Publishes information about removed groups.
  void OnGroupRemovedInternal(const data_sharing::GroupId& group_id,
                              const base::Time& event_time);

  // Whether this has already been initialized.
  bool is_initialized_ = false;

  // Whether we have performed the initial flush of messages or not. We can only
  // publish changes if this is true.
  bool has_flushed_ = false;

  // Our scoped observer of the DataSharingService. Using ScopedObservation
  // simplifies our destruction logic.
  base::ScopedObservation<data_sharing::DataSharingService,
                          data_sharing::DataSharingService::Observer>
      data_sharing_service_observer_{this};

  // The list of observers observing this particular class.
  base::ObserverList<Observer> observers_;

  // The DataSharingService that is the source of the updates.
  raw_ptr<data_sharing::DataSharingService> data_sharing_service_;

  // Whether the collaboration sync bridge is undergoing initial merge or
  // disable sync (which mostly happens during sign-in / sign-out). During this
  // period, the incoming sync changes should be ignored which would otherwise
  // create an avalanche of false notifications.
  data_sharing::SyncBridgeUpdateType sync_bridge_update_type_ =
      data_sharing::SyncBridgeUpdateType::kDefaultState;

  base::WeakPtrFactory<DataSharingChangeNotifierImpl> weak_ptr_factory_{this};
};

}  // namespace collaboration::messaging

#endif  // COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_DATA_SHARING_CHANGE_NOTIFIER_IMPL_H_
