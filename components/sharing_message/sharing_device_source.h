// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_SHARING_DEVICE_SOURCE_H_
#define COMPONENTS_SHARING_MESSAGE_SHARING_DEVICE_SOURCE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "components/sharing_message/sharing_target_device_info.h"
#include "components/sync/protocol/device_info_specifics.pb.h"

class SharingDeviceSource {
 public:
  SharingDeviceSource();

  SharingDeviceSource(const SharingDeviceSource&) = delete;
  SharingDeviceSource& operator=(const SharingDeviceSource&) = delete;

  virtual ~SharingDeviceSource();

  // Returns if the source is ready. Calling GetAllDevices before this is true
  // returns an empty list.
  virtual bool IsReady() = 0;

  // Returns the device matching |guid| or nullopt if no match was found.
  virtual std::optional<SharingTargetDeviceInfo> GetDeviceByGuid(
      const std::string& guid) = 0;

  // Returns all device candidates for |required_feature|. Internally filters
  // out older devices and returns them in (not strictly) decreasing order of
  // last updated timestamp.
  virtual std::vector<SharingTargetDeviceInfo> GetDeviceCandidates(
      sync_pb::SharingSpecificFields::EnabledFeatures required_feature) = 0;

  // Adds a callback to be run when the SharingDeviceSource is ready. If a
  // callback is added when it is already ready, it will be run immediately.
  void AddReadyCallback(base::OnceClosure callback);

 protected:
  void MaybeRunReadyCallbacks();

 private:
  std::vector<base::OnceClosure> ready_callbacks_;
};

#endif  // COMPONENTS_SHARING_MESSAGE_SHARING_DEVICE_SOURCE_H_
