// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_SHARING_DEVICE_REGISTRATION_H_
#define COMPONENTS_SHARING_MESSAGE_SHARING_DEVICE_REGISTRATION_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/sync/protocol/device_info_specifics.pb.h"
#include "components/sync_device_info/device_info.h"

enum class SharingDeviceRegistrationResult;

// Responsible for registering and unregistering device with
// SharingSyncPreference.
class SharingDeviceRegistration {
 public:
  using RegistrationCallback =
      base::OnceCallback<void(SharingDeviceRegistrationResult)>;

  virtual ~SharingDeviceRegistration() = default;

  // Registers device with sharing sync preferences. Takes a |callback| function
  // which receives the result of FCM registration for device.
  virtual void RegisterDevice(RegistrationCallback callback) = 0;

  // Un-registers device with sharing sync preferences.
  virtual void UnregisterDevice(RegistrationCallback callback) = 0;

  // Returns if device can handle receiving phone numbers for calling.
  virtual bool IsClickToCallSupported() const = 0;

  // Returns if device can handle receiving of shared clipboard contents.
  virtual bool IsSharedClipboardSupported() const = 0;

  // Returns if device can handle receiving of sms fetcher requests.
  virtual bool IsSmsFetcherSupported() const = 0;

  // Returns if device can handle receiving of remote copy contents.
  virtual bool IsRemoteCopySupported() const = 0;

  // Returns if device can handle receiving of optimization guide push
  // notification.
  virtual bool IsOptimizationGuidePushNotificationSupported() const = 0;

  // For testing
  virtual void SetEnabledFeaturesForTesting(
      std::set<sync_pb::SharingSpecificFields_EnabledFeatures>
          enabled_features) = 0;
};

#endif  // COMPONENTS_SHARING_MESSAGE_SHARING_DEVICE_REGISTRATION_H_
