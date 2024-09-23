// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_LOCAL_DEVICE_INFO_PROVIDER_H_
#define COMPONENTS_SYNC_DEVICE_INFO_LOCAL_DEVICE_INFO_PROVIDER_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/time/time.h"
#include "components/sync/base/data_type.h"
#include "components/version_info/channel.h"

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
  [[nodiscard]] virtual base::CallbackListSubscription
  RegisterOnInitializedCallback(const base::RepeatingClosure& callback) = 0;
};

class MutableLocalDeviceInfoProvider : public LocalDeviceInfoProvider {
 public:
  // Initialize initializes the LocalDeviceInfoProvider using the given values.
  // The |device_info_restored_from_store| argument contains a previous
  // DeviceInfo loaded from the store and may be nullptr if unavailable. If
  // provided it is only used as a fallback and the provided arguments, and data
  // from the DeviceInfoSyncClient, take precedence.
  virtual void Initialize(
      const std::string& cache_guid,
      const std::string& client_name,
      const std::string& manufacturer_name,
      const std::string& model_name,
      const std::string& full_hardware_class,
      const DeviceInfo* device_info_restored_from_store) = 0;
  virtual void Clear() = 0;

  // Updates the local device's client name. Initialize() must be called before
  // calling this function.
  virtual void UpdateClientName(const std::string& client_name) = 0;

  virtual void UpdateRecentSignInTime(base::Time time) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_LOCAL_DEVICE_INFO_PROVIDER_H_
