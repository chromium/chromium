// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_ACTIVE_DEVICES_INVALIDATION_INFO_H_
#define COMPONENTS_SYNC_ENGINE_ACTIVE_DEVICES_INVALIDATION_INFO_H_

#include <map>
#include <string>
#include <vector>

#include "components/sync/base/data_type.h"

namespace syncer {

// This class keeps information about known active devices' invalidation fields.
// It's used for invalidations-related fields in a commit messages.
class ActiveDevicesInvalidationInfo {
 public:
  // Uninitialized object represents the case when sync engine is not
  // initialized and there is no information about other devices.
  static ActiveDevicesInvalidationInfo CreateUninitialized();

  // Creates and initializes the object with all collected
  // |all_fcm_registration_tokens| and |interested_data_types| from all other
  // devices (and the local device if the client waits for self-invalidations).
  // |fcm_token_and_interested_data_types| contains FCM registration tokens and
  // a corresponding interested data type list.
  static ActiveDevicesInvalidationInfo Create(
      std::vector<std::string> all_fcm_registration_tokens,
      DataTypeSet all_interested_data_types,
      std::map<std::string, DataTypeSet> fcm_token_and_interested_data_types,
      DataTypeSet old_invalidations_interested_data_types);

  ~ActiveDevicesInvalidationInfo();

  ActiveDevicesInvalidationInfo(const ActiveDevicesInvalidationInfo&);
  ActiveDevicesInvalidationInfo& operator=(
      const ActiveDevicesInvalidationInfo&);
  ActiveDevicesInvalidationInfo(ActiveDevicesInvalidationInfo&&);
  ActiveDevicesInvalidationInfo& operator=(ActiveDevicesInvalidationInfo&&);

  // Returns true if there are no other devices interested in invalidations for
  // the given |types|. Otherwise returns false (and in case when it's unknown,
  // e.g. while DeviceInfo is not initialized). When reflections are enabled,
  // returns false even if current client is the only one.
  bool IsSingleClientForTypes(const DataTypeSet& types) const;

  // Returns true if there are no other active DeviceInfos with enabled sync
  // standalone invalidations interested in the given |types|. Returns false in
  // all other cases:
  // * When it's unknown if there are other clients (e.g. when DeviceInfo data
  //   type is not initialized yet.
  // * There is at least one client with enabled sync standalone invalidations
  //   subscribed for the given |types| (including current client if reflections
  //   are enabled).
  bool IsSingleClientWithStandaloneInvalidationsForTypes(
      const DataTypeSet& types) const;

  // Returns true if there are no other active DeviceInfos with *disabled* synce
  // standalone invalidations interested in the given |types|.
  bool IsSingleClientWithOldInvalidationsForTypes(
      const DataTypeSet& types) const;

  // Returns a list with all remote FCM registration tokens known to the current
  // device. The list may contain the local device's token if a reflection
  // should be sent from the server.
  const std::vector<std::string>& all_fcm_registration_tokens() const {
    return all_fcm_registration_tokens_;
  }

  // Returns a list of all remote FCM registration tokens known to current
  // device which are interested in at least one of the given |types|. This is a
  // subset of the list returned by all_fcm_registration_tokens().
  std::vector<std::string> GetFcmRegistrationTokensForInterestedClients(
      DataTypeSet types) const;

 private:
  explicit ActiveDevicesInvalidationInfo(bool initialized);

  DataTypeSet GetAllInterestedDataTypesForStandaloneInvalidations() const;

  bool initialized_ = false;
  std::vector<std::string> all_fcm_registration_tokens_;
  DataTypeSet all_interested_data_types_;
  std::map<std::string, DataTypeSet> fcm_token_and_interested_data_types_;
  DataTypeSet old_invalidations_interested_data_types_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_ACTIVE_DEVICES_INVALIDATION_INFO_H_
