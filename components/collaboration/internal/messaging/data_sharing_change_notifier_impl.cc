// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/messaging/data_sharing_change_notifier_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "components/collaboration/internal/messaging/data_sharing_change_notifier.h"
#include "components/data_sharing/public/data_sharing_service.h"

namespace collaboration::messaging {

DataSharingChangeNotifierImpl::DataSharingChangeNotifierImpl(
    data_sharing::DataSharingService* data_sharing_service)
    : data_sharing_service_(data_sharing_service) {}

DataSharingChangeNotifierImpl::~DataSharingChangeNotifierImpl() = default;

void DataSharingChangeNotifierImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
  if (is_initialized_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&DataSharingChangeNotifierImpl::
                           NotifyDataSharingChangeNotifierInitialized,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void DataSharingChangeNotifierImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

DataSharingChangeNotifier::FlushCallback
DataSharingChangeNotifierImpl::Initialize() {
  if (data_sharing_service_->IsGroupDataModelLoaded()) {
    is_initialized_ = true;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&DataSharingChangeNotifierImpl::
                           NotifyDataSharingChangeNotifierInitialized,
                       weak_ptr_factory_.GetWeakPtr()));
  }
  // Regardless of whether the service was already initialized or not, we now
  // need to start observing it to get all future events. If it was not
  // already initialized, we also need OnGroupDataModelLoaded() call.
  data_sharing_service_observer_.Observe(data_sharing_service_);

  return base::BindOnce(
      &DataSharingChangeNotifierImpl::FlushGroupEventsSinceStartup,
      weak_ptr_factory_.GetWeakPtr());
}

void DataSharingChangeNotifierImpl::OnGroupDataModelLoaded() {
  if (is_initialized_) {
    // The DataSharingService was ready at startup, so we do not need to do
    // anything now.
    return;
  }

  is_initialized_ = true;

  // This is the first time we know about initialization, so inform our
  // observers. Since we are reacting to a callback, we do not need to post
  // this.
  NotifyDataSharingChangeNotifierInitialized();
}

bool DataSharingChangeNotifierImpl::IsInitialized() {
  return is_initialized_;
}

void DataSharingChangeNotifierImpl::OnGroupAdded(
    const data_sharing::GroupData& group_data,
    const base::Time& event_time) {
  if (!has_flushed_ || sync_bridge_update_type_ !=
                           data_sharing::SyncBridgeUpdateType::kDefaultState) {
    return;
  }
  OnGroupAddedInternal(group_data.group_token.group_id, group_data, event_time);
}

void DataSharingChangeNotifierImpl::OnGroupRemoved(
    const data_sharing::GroupId& group_id,
    const base::Time& event_time) {
  if (!has_flushed_ || sync_bridge_update_type_ !=
                           data_sharing::SyncBridgeUpdateType::kDefaultState) {
    return;
  }
  OnGroupRemovedInternal(group_id, event_time);
}

void DataSharingChangeNotifierImpl::OnGroupMemberAdded(
    const data_sharing::GroupId& group_id,
    const GaiaId& member_gaia_id,
    const base::Time& event_time) {
  if (!has_flushed_ || sync_bridge_update_type_ !=
                           data_sharing::SyncBridgeUpdateType::kDefaultState) {
    return;
  }
  std::optional<data_sharing::GroupData> group_data =
      GetGroupDataIfAvailable(group_id);
  if (!group_data) {
    // This can happen during startup while we are processing multiple events
    // in-order where a group can have first added a member and later have been
    // deleted. In those cases, observers will be informed of the group removal
    // when we get to that event.
    return;
  }
  for (Observer& observer : observers_) {
    observer.OnGroupMemberAdded(*group_data, member_gaia_id, event_time);
  }
}

void DataSharingChangeNotifierImpl::OnGroupMemberRemoved(
    const data_sharing::GroupId& group_id,
    const GaiaId& member_gaia_id,
    const base::Time& event_time) {
  if (!has_flushed_ || sync_bridge_update_type_ !=
                           data_sharing::SyncBridgeUpdateType::kDefaultState) {
    return;
  }
  std::optional<data_sharing::GroupData> group_data =
      GetGroupDataIfAvailable(group_id);
  if (!group_data) {
    // This can happen during startup while we are processing multiple events
    // in-order where a group can have first lost a member and later have been
    // deleted. In those cases, observers will be informed of the group removal
    // when we get to that event.
    return;
  }
  for (Observer& observer : observers_) {
    observer.OnGroupMemberRemoved(*group_data, member_gaia_id, event_time);
  }
}

void DataSharingChangeNotifierImpl::OnSyncBridgeUpdateTypeChanged(
    data_sharing::SyncBridgeUpdateType sync_bridge_update_type) {
  sync_bridge_update_type_ = sync_bridge_update_type;
}

void DataSharingChangeNotifierImpl::NotifyDataSharingChangeNotifierInitialized()
    const {
  for (auto& observer : observers_) {
    observer.OnDataSharingChangeNotifierInitialized();
  }
}

std::optional<data_sharing::GroupData>
DataSharingChangeNotifierImpl::GetGroupDataIfAvailable(
    const data_sharing::GroupId& group_id) {
  std::optional<data_sharing::GroupData> group_data =
      data_sharing_service_->ReadGroup(group_id);
  if (group_data) {
    return group_data;
  }
  return data_sharing_service_->GetPossiblyRemovedGroup(group_id);
}

void DataSharingChangeNotifierImpl::OnGroupAddedInternal(
    const data_sharing::GroupId& group_id,
    const std::optional<data_sharing::GroupData>& group_data,
    const base::Time& event_time) {
  std::optional<data_sharing::GroupData> group_data_for_publish = group_data;
  if (!group_data_for_publish) {
    group_data_for_publish = GetGroupDataIfAvailable(group_id);
  }
  for (Observer& observer : observers_) {
    observer.OnGroupAdded(group_id, group_data_for_publish, event_time);
  }
}

void DataSharingChangeNotifierImpl::OnGroupRemovedInternal(
    const data_sharing::GroupId& group_id,
    const base::Time& event_time) {
  std::optional<data_sharing::GroupData> group_data_for_publish =
      GetGroupDataIfAvailable(group_id);
  for (Observer& observer : observers_) {
    observer.OnGroupRemoved(group_id, group_data_for_publish, event_time);
  }
}

void DataSharingChangeNotifierImpl::FlushGroupEventsSinceStartup() {
  CHECK(is_initialized_);
  has_flushed_ = true;

  for (const data_sharing::GroupEvent& event :
       data_sharing_service_->GetGroupEventsSinceStartup()) {
    switch (event.event_type) {
      case data_sharing::GroupEvent::EventType::kGroupAdded:
        OnGroupAddedInternal(event.group_id, std::nullopt, event.event_time);
        break;
      case data_sharing::GroupEvent::EventType::kGroupRemoved:
        OnGroupRemovedInternal(event.group_id, event.event_time);
        break;
      case data_sharing::GroupEvent::EventType::kMemberAdded:
        // Membership changes should always have an affected GAIA ID.
        CHECK(event.affected_member_gaia_id);
        OnGroupMemberAdded(event.group_id, *event.affected_member_gaia_id,
                           event.event_time);
        break;
      case data_sharing::GroupEvent::EventType::kMemberRemoved:
        // Membership changes should always have an affected GAIA ID.
        CHECK(event.affected_member_gaia_id);
        OnGroupMemberRemoved(event.group_id, *event.affected_member_gaia_id,
                             event.event_time);
        break;
    }
  }
}

}  // namespace collaboration::messaging
