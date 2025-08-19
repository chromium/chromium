// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/personal_collaboration_data/personal_collaboration_data_service_impl.h"

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"

namespace data_sharing::personal_collaboration_data {

namespace {

std::string CreateStorageKeyWithType(
    PersonalCollaborationDataService::SpecificsType specifics_type,
    const std::string& storage_key) {
  // Special case tab and tab group since previous data were stored without a
  // namespace.
  if (specifics_type == PersonalCollaborationDataService::SpecificsType::
                            kSharedTabSpecifics ||
      specifics_type == PersonalCollaborationDataService::SpecificsType::
                            kSharedTabGroupSpecifics) {
    return storage_key;
  }

  int type_as_int = static_cast<int>(specifics_type);
  return base::NumberToString(type_as_int) + "|" + storage_key;
}

}  // namespace

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
  return bridge_->GetSpecificsForStorageKey(
      CreateStorageKeyWithType(specifics_type, storage_key));
}

std::vector<const sync_pb::SharedTabGroupAccountDataSpecifics*>
PersonalCollaborationDataServiceImpl::GetAllSpecifics() const {
  std::vector<const sync_pb::SharedTabGroupAccountDataSpecifics*> result;
  const auto& specifics_map = bridge_->GetAllSpecifics();
  result.reserve(specifics_map.size());
  for (const auto& pair : specifics_map) {
    result.push_back(&pair.second);
  }
  return result;
}

void PersonalCollaborationDataServiceImpl::CreateOrUpdateSpecifics(
    SpecificsType specifics_type,
    const std::string& storage_key,
    const sync_pb::SharedTabGroupAccountDataSpecifics& specifics) {
  switch (specifics_type) {
    case SpecificsType::kSharedTabSpecifics:
      CHECK(specifics.has_shared_tab_details());
      break;
    case SpecificsType::kSharedTabGroupSpecifics:
      CHECK(specifics.has_shared_tab_group_details());
      break;
    default:
      NOTREACHED();
  }

  bridge_->CreateOrUpdateSpecifics(
      CreateStorageKeyWithType(specifics_type, storage_key), specifics);
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
