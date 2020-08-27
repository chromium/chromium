// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/phone_hub_manager_impl.h"

#include "chromeos/components/phonehub/do_not_disturb_controller_impl.h"
#include "chromeos/components/phonehub/feature_status_provider_impl.h"
#include "chromeos/components/phonehub/mutable_phone_model.h"
#include "chromeos/components/phonehub/notification_access_manager_impl.h"
#include "chromeos/components/phonehub/notification_manager_impl.h"
#include "chromeos/components/phonehub/tether_controller_impl.h"

namespace chromeos {
namespace phonehub {

PhoneHubManagerImpl::PhoneHubManagerImpl(
    PrefService* pref_service,
    device_sync::DeviceSyncClient* device_sync_client,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client)
    : do_not_disturb_controller_(
          std::make_unique<DoNotDisturbControllerImpl>()),
      feature_status_provider_(std::make_unique<FeatureStatusProviderImpl>(
          device_sync_client,
          multidevice_setup_client)),
      notification_access_manager_(
          std::make_unique<NotificationAccessManagerImpl>(pref_service)),
      notification_manager_(std::make_unique<NotificationManagerImpl>()),
      phone_model_(std::make_unique<MutablePhoneModel>()),
      tether_controller_(
          std::make_unique<TetherControllerImpl>(multidevice_setup_client)) {}

PhoneHubManagerImpl::~PhoneHubManagerImpl() = default;

DoNotDisturbController* PhoneHubManagerImpl::GetDoNotDisturbController() {
  return do_not_disturb_controller_.get();
}

FeatureStatusProvider* PhoneHubManagerImpl::GetFeatureStatusProvider() {
  return feature_status_provider_.get();
}

NotificationAccessManager* PhoneHubManagerImpl::GetNotificationAccessManager() {
  return notification_access_manager_.get();
}

NotificationManager* PhoneHubManagerImpl::GetNotificationManager() {
  return notification_manager_.get();
}

PhoneModel* PhoneHubManagerImpl::GetPhoneModel() {
  return phone_model_.get();
}

TetherController* PhoneHubManagerImpl::GetTetherController() {
  return tether_controller_.get();
}

void PhoneHubManagerImpl::Shutdown() {
  tether_controller_.reset();
  phone_model_.reset();
  notification_access_manager_.reset();
  feature_status_provider_.reset();
  do_not_disturb_controller_.reset();
}

}  // namespace phonehub
}  // namespace chromeos
