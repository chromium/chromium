// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_LOCAL_DEVICE_INFO_PROVIDER_H_
#define COMPONENTS_SYNC_DEVICE_INFO_LOCAL_DEVICE_INFO_PROVIDER_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "components/sync/base/model_type.h"
#include "components/version_info/version_info.h"

namespace syncer {

class DeviceInfo;

// Interface for providing sync specific information about the
// local device.
class LocalDeviceInfoProvider {
 public:
  virtual ~LocalDeviceInfoProvider() = default;

  virtual version_info::Channel GetChannel() const = 0;

  // Returns sync's representation of the local device info, or nullptr if the
  // device info is unavailable (e.g. Initialize() hasn't been called). The
  // returned object is fully owned by LocalDeviceInfoProvider; it must not be
  // freed by the caller and should not be stored.
  virtual const DeviceInfo* GetLocalDeviceInfo() const = 0;

  // Registers a callback to be called when local device info becomes available.
  // The callback will remain registered until the returned subscription is
  // destroyed, which must occur before the CallbackList is destroyed.
  virtual base::CallbackListSubscription RegisterOnInitializedCallback(
      const base::RepeatingClosure& callback) WARN_UNUSED_RESULT = 0;
};

class MutableLocalDeviceInfoProvider : public LocalDeviceInfoProvider {
 public:
  virtual void Initialize(const std::string& cache_guid,
                          const std::string& client_name,
                          const std::string& manufacturer_name,
                          const std::string& model_name,
                          const std::string& last_fcm_registration_token,
                          const ModelTypeSet& last_interested_data_types) = 0;
  virtual void Clear() = 0;

  // Updates the local device's client name. Initialize() must be called before
  // calling this function.
  virtual void UpdateClientName(const std::string& client_name) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_LOCAL_DEVICE_INFO_PROVIDER_H_
