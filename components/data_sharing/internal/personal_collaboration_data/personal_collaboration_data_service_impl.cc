// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/personal_collaboration_data/personal_collaboration_data_service_impl.h"

#include "base/notreached.h"

namespace data_sharing::personal_collaboration_data {

PersonalCollaborationDataServiceImpl::PersonalCollaborationDataServiceImpl() =
    default;

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
  // TODO(haileywang): Implement actual logic to check if the service has
  // finished loading initial data or is ready for use.
  return is_initialized_;
}

}  // namespace data_sharing::personal_collaboration_data
