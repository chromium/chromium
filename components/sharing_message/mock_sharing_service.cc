// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/mock_sharing_service.h"

#include "components/sharing_message/sharing_device_source.h"
#include "components/sharing_message/sharing_fcm_handler.h"
#include "components/sharing_message/sharing_fcm_sender.h"
#include "components/sharing_message/sharing_handler_registry.h"
#include "components/sharing_message/sharing_sync_preference.h"
#include "components/sharing_message/vapid_key_manager.h"
#include "components/sync/protocol/device_info_specifics.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

class FakeSharingDeviceRegistration : public SharingDeviceRegistration {
 public:
  FakeSharingDeviceRegistration() = default;
  ~FakeSharingDeviceRegistration() override = default;

  void RegisterDevice(
      SharingDeviceRegistration::RegistrationCallback callback) override {}

  void UnregisterDevice(
      SharingDeviceRegistration::RegistrationCallback callback) override {}

  bool IsClickToCallSupported() const override { return false; }

  bool IsSharedClipboardSupported() const override { return false; }

  bool IsSmsFetcherSupported() const override { return false; }

  bool IsRemoteCopySupported() const override { return false; }

  bool IsOptimizationGuidePushNotificationSupported() const override {
    return false;
  }

  void SetEnabledFeaturesForTesting(
      std::set<sync_pb::SharingSpecificFields_EnabledFeatures> enabled_features)
      override {}
};

MockSharingService::MockSharingService()
    : SharingService(
          /*sync_prefs=*/nullptr,
          /*vapid_key_manager=*/nullptr,
          std::make_unique<FakeSharingDeviceRegistration>(),
          /*message_sender=*/nullptr,
          /*device_source=*/nullptr,
          /*handler_registry=*/nullptr,
          std::make_unique<SharingFCMHandler>(/*gcm_driver=*/nullptr,
                                              /*sharing_fcm_sender=*/nullptr,
                                              /*sync_preference=*/nullptr,
                                              /*handler_registry=*/nullptr),
          /*sync_service=*/nullptr,
          /*favicon_service=*/nullptr,
          /*send_tab_model=*/nullptr,
          /*task_runner=*/nullptr) {}

MockSharingService::~MockSharingService() = default;
