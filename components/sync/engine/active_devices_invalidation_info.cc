// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/active_devices_invalidation_info.h"

#include <utility>

namespace syncer {

namespace {

// The maximum number of FCM registration tokens to be returned for a commit
// message.
constexpr size_t kMaxFcmRegistrationTokens = 5;

}  // namespace

// static
ActiveDevicesInvalidationInfo
ActiveDevicesInvalidationInfo::CreateUninitialized() {
  return ActiveDevicesInvalidationInfo(/*initialized=*/false);
}

// static
ActiveDevicesInvalidationInfo ActiveDevicesInvalidationInfo::Create(
    std::vector<std::string> all_fcm_registration_tokens,
    DataTypeSet all_interested_data_types,
    std::map<std::string, DataTypeSet> fcm_token_and_interested_data_types,
    DataTypeSet old_invalidations_interested_data_types) {
  ActiveDevicesInvalidationInfo result(/*initialized=*/true);
  result.all_fcm_registration_tokens_ = std::move(all_fcm_registration_tokens);
  result.all_interested_data_types_ = all_interested_data_types;
  result.fcm_token_and_interested_data_types_ =
      std::move(fcm_token_and_interested_data_types);
  result.old_invalidations_interested_data_types_ =
      old_invalidations_interested_data_types;
  return result;
}

ActiveDevicesInvalidationInfo::ActiveDevicesInvalidationInfo(bool initialized)
    : initialized_(initialized) {}

ActiveDevicesInvalidationInfo::~ActiveDevicesInvalidationInfo() = default;

ActiveDevicesInvalidationInfo::ActiveDevicesInvalidationInfo(
    const ActiveDevicesInvalidationInfo&) = default;
ActiveDevicesInvalidationInfo& ActiveDevicesInvalidationInfo::operator=(
    const ActiveDevicesInvalidationInfo&) = default;
ActiveDevicesInvalidationInfo::ActiveDevicesInvalidationInfo(
    ActiveDevicesInvalidationInfo&&) = default;
ActiveDevicesInvalidationInfo& ActiveDevicesInvalidationInfo::operator=(
    ActiveDevicesInvalidationInfo&&) = default;

bool ActiveDevicesInvalidationInfo::IsSingleClientForTypes(
    const DataTypeSet& types) const {
  if (!initialized_) {
    return false;
  }

  return Intersection(types, all_interested_data_types_).empty();
}

bool ActiveDevicesInvalidationInfo::
    IsSingleClientWithStandaloneInvalidationsForTypes(
        const DataTypeSet& types) const {
  if (!initialized_) {
    return false;
  }

  return Intersection(types,
                      GetAllInterestedDataTypesForStandaloneInvalidations())
      .empty();
}

bool ActiveDevicesInvalidationInfo::IsSingleClientWithOldInvalidationsForTypes(
    const DataTypeSet& types) const {
  if (!initialized_) {
    return false;
  }

  return Intersection(types, old_invalidations_interested_data_types_).empty();
}

std::vector<std::string>
ActiveDevicesInvalidationInfo::GetFcmRegistrationTokensForInterestedClients(
    DataTypeSet types) const {
  std::vector<std::string> fcm_tokens;
  for (const auto& fcm_token_with_data_types :
       fcm_token_and_interested_data_types_) {
    if (Intersection(types, fcm_token_with_data_types.second).empty()) {
      continue;
    }
    if (fcm_tokens.size() >= kMaxFcmRegistrationTokens) {
      fcm_tokens.clear();
      // Single client is used to determine if there are devices with standalone
      // invalidations enabled.
      DCHECK(!IsSingleClientWithStandaloneInvalidationsForTypes(types));
      break;
    }
    fcm_tokens.push_back(fcm_token_with_data_types.first);
  }
  return fcm_tokens;
}

DataTypeSet ActiveDevicesInvalidationInfo::
    GetAllInterestedDataTypesForStandaloneInvalidations() const {
  DataTypeSet result;
  for (const auto& fcm_token_with_data_types :
       fcm_token_and_interested_data_types_) {
    result.PutAll(fcm_token_with_data_types.second);
  }
  return result;
}

}  // namespace syncer
