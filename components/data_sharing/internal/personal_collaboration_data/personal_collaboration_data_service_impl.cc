// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/personal_collaboration_data/personal_collaboration_data_service_impl.h"

#include "base/notreached.h"

namespace data_sharing::personal_collaboration_data {

PersonalCollaborationDataServiceImpl::PersonalCollaborationDataServiceImpl(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    syncer::OnceDataTypeStoreFactory data_type_store_factory)
    : bridge_(std::make_unique<PersonalCollaborationDataSyncBridge>(
          std::move(change_processor),
          std::move(data_type_store_factory))) {
  bridge_observer_.Observe(bridge_.get());
}

PersonalCollaborationDataServiceImpl::~PersonalCollaborationDataServiceImpl() =
    default;

void PersonalCollaborationDataServiceImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PersonalCollaborationDataServiceImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::optional<sync_pb::SharedTabGroupAccountDataSpecifics>
PersonalCollaborationDataServiceImpl::GetSpecifics(
    SpecificsType specifics_type,
    const std::string& storage_key) {
  // TODO(haileywang): Implement actual logic.
  return sync_pb::SharedTabGroupAccountDataSpecifics();
}

void PersonalCollaborationDataServiceImpl::CreateOrUpdateSpecifics(
    SpecificsType specifics_type,
    const std::string& storage_key,
    const sync_pb::SharedTabGroupAccountDataSpecifics& specifics) {
  // TODO(haileywang): Implement actual logic.
  NOTREACHED();
}

void PersonalCollaborationDataServiceImpl::DeleteSpecifics(
    SpecificsType specifics_type,
    const std::string& storage_key) {
  // TODO(haileywang): Implement actual logic.
  NOTREACHED();
}

bool PersonalCollaborationDataServiceImpl::IsInitialized() const {
  return bridge_->IsInitialized();
}

void PersonalCollaborationDataServiceImpl::OnEntityAddedOrUpdatedFromSync(
    const sync_pb::SharedTabGroupAccountDataSpecifics& data) {
  // TODO(haileywang): Implement actual logic to update tab group details.
  NOTREACHED();
}

}  // namespace data_sharing::personal_collaboration_data
