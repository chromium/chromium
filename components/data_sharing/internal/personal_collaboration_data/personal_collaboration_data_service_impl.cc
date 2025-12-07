// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/personal_collaboration_data/personal_collaboration_data_service_impl.h"

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"

namespace data_sharing::personal_collaboration_data {

namespace {

const char kSeparator[] = "|";

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
  return base::NumberToString(type_as_int) + kSeparator + storage_key;
}

std::string GetStorageKeyWithoutType(
    PersonalCollaborationDataService::SpecificsType specifics_type,
    const std::string& storage_key) {
  size_t separator_pos = storage_key.find(kSeparator);
  if (separator_pos == std::string::npos) {
    return storage_key;
  }

  // Extract the string representations of the type and the storage key.
  std::string type_str = storage_key.substr(0, separator_pos);
  std::string new_storage_key = storage_key.substr(separator_pos + 1);

  // Convert the type string back to an integer.
  int type_as_int;
  if (!base::StringToInt(type_str, &type_as_int)) {
    return storage_key;
  }

  if (type_as_int < 0 ||
      type_as_int >
          static_cast<int>(
              PersonalCollaborationDataService::SpecificsType::kMaxValue)) {
    return storage_key;
  }

  // Cast the integer back to the enum type.
  PersonalCollaborationDataService::SpecificsType storage_specifics_type =
      static_cast<PersonalCollaborationDataService::SpecificsType>(type_as_int);

  if (storage_specifics_type != specifics_type) {
    return storage_key;
  }

  // Return the parsed values as a pair.
  return new_storage_key;
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
    base::OnceCallback<
        void(sync_pb::SharedTabGroupAccountDataSpecifics* specifics)> mutator) {
  if (!bridge_->IsInitialized()) {
    pending_actions_.emplace_back(base::BindOnce(
        &PersonalCollaborationDataServiceImpl::CreateOrUpdateSpecifics,
        weak_ptr_factory_.GetWeakPtr(), specifics_type, storage_key,
        std::move(mutator)));
    return;
  }

  const std::string storage_key_with_type =
      CreateStorageKeyWithType(specifics_type, storage_key);
  std::optional<sync_pb::SharedTabGroupAccountDataSpecifics> specifics =
      bridge_->GetTrimmedRemoteSpecifics(storage_key_with_type);
  if (!specifics.has_value()) {
    specifics = sync_pb::SharedTabGroupAccountDataSpecifics();
  }

  // The callers should fill in the specifics data in the callback function.
  std::move(mutator).Run(&specifics.value());
  switch (specifics_type) {
    case SpecificsType::kSharedTabSpecifics:
      CHECK(specifics->has_shared_tab_details());
      break;
    case SpecificsType::kSharedTabGroupSpecifics:
      CHECK(specifics->has_shared_tab_group_details());
      break;
    default:
      NOTREACHED();
  }

  bridge_->CreateOrUpdateSpecifics(storage_key_with_type, specifics.value());
}

void PersonalCollaborationDataServiceImpl::DeleteSpecifics(
    SpecificsType specifics_type,
    const std::string& storage_key) {
  if (!bridge_->IsInitialized()) {
    pending_actions_.emplace_back(base::BindOnce(
        &PersonalCollaborationDataServiceImpl::DeleteSpecifics,
        weak_ptr_factory_.GetWeakPtr(), specifics_type, storage_key));
    return;
  }
  bridge_->RemoveSpecifics(
      CreateStorageKeyWithType(specifics_type, storage_key));
}

bool PersonalCollaborationDataServiceImpl::IsInitialized() const {
  return bridge_->IsInitialized();
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
PersonalCollaborationDataServiceImpl::GetControllerDelegate() {
  return bridge_->change_processor()->GetControllerDelegate();
}

void PersonalCollaborationDataServiceImpl::OnInitialized() {
  ProcessPendingActions();
  for (Observer& observer : observers_) {
    observer.OnInitialized();
  }
}

void PersonalCollaborationDataServiceImpl::OnEntityAddedOrUpdatedFromSync(
    const std::string& storage_key,
    const sync_pb::SharedTabGroupAccountDataSpecifics& data) {
  SpecificsType type = SpecificsType::kUnknown;
  if (data.has_shared_tab_details()) {
    type = SpecificsType::kSharedTabSpecifics;
  } else if (data.has_shared_tab_group_details()) {
    type = SpecificsType::kSharedTabGroupSpecifics;
  } else {
    NOTREACHED();
  }

  for (Observer& observer : observers_) {
    observer.OnSpecificsUpdated(
        type, GetStorageKeyWithoutType(type, storage_key), data);
  }
}

void PersonalCollaborationDataServiceImpl::ProcessPendingActions() {
  CHECK(bridge_->IsInitialized());
  while (!pending_actions_.empty()) {
    base::OnceClosure callback = std::move(pending_actions_.front());
    pending_actions_.pop_front();
    std::move(callback).Run();
  }
}

}  // namespace data_sharing::personal_collaboration_data
