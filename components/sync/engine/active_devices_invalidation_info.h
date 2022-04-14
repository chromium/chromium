// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_ACTIVE_DEVICES_INVALIDATION_INFO_H_
#define COMPONENTS_SYNC_ENGINE_ACTIVE_DEVICES_INVALIDATION_INFO_H_

#include <string>
#include <vector>

#include "components/sync/base/model_type.h"

namespace syncer {

// This class keeps information about known active devices' invalidation fields.
// It's used for invalidations-related fields in a commit messages.
class ActiveDevicesInvalidationInfo {
 public:
  // Uninitialized object represents the case when sync engine is not
  // initialized and there is no information about other devices.
  static ActiveDevicesInvalidationInfo CreateUninitialized();

  // Creates and initializes the object with all collected
  // |fcm_registration_tokens| and |interested_data_types| from all other
  // devices (and the local device if the client waits for self-invalidations).
  static ActiveDevicesInvalidationInfo Create(
      std::vector<std::string> fcm_registration_tokens,
      ModelTypeSet interested_data_types);

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
  bool IsSingleClientForTypes(const ModelTypeSet& types) const;

  // Returns a list with all remote FCM registration tokens known to the current
  // device. The list may contain the local device's token if a reflection
  // should be sent from the server.
  const std::vector<std::string>& fcm_registration_tokens() const {
    return fcm_registration_tokens_;
  }

 private:
  explicit ActiveDevicesInvalidationInfo(bool initialized);

  bool initialized_ = false;
  std::vector<std::string> fcm_registration_tokens_;
  ModelTypeSet interested_data_types_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_ACTIVE_DEVICES_INVALIDATION_INFO_H_
