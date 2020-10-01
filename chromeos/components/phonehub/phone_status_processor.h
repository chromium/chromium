// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_PHONE_STATUS_PROCESSOR_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_PHONE_STATUS_PROCESSOR_H_

#include "chromeos/components/phonehub/feature_status_provider.h"
#include "chromeos/components/phonehub/message_receiver.h"
#include "chromeos/components/phonehub/proto/phonehub_api.pb.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"

#include <google/protobuf/repeated_field.h>

using google::protobuf::RepeatedPtrField;

namespace chromeos {
namespace phonehub {

class DoNotDisturbController;
class FeatureStatusProvider;
class FindMyDeviceController;
class NotificationAccessManager;
class NotificationManager;
class MutablePhoneModel;

// Responsible for receiving incoming protos and calling on clients to update
// their models.
class PhoneStatusProcessor
    : public MessageReceiver::Observer,
      public FeatureStatusProvider::Observer,
      public multidevice_setup::MultiDeviceSetupClient::Observer {
 public:
  PhoneStatusProcessor(
      DoNotDisturbController* do_not_disturb_controller,
      FeatureStatusProvider* feature_status_provider,
      MessageReceiver* message_receiver,
      FindMyDeviceController* find_my_device_controller,
      NotificationAccessManager* notification_access_manager,
      NotificationManager* notification_manager,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      MutablePhoneModel* phone_model);
  ~PhoneStatusProcessor() override;

  PhoneStatusProcessor(const PhoneStatusProcessor&) = delete;
  PhoneStatusProcessor& operator=(const PhoneStatusProcessor&) = delete;

 private:
  // FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override;

  // MessageReceiver::Observer:
  void OnPhoneStatusSnapshotReceived(
      proto::PhoneStatusSnapshot phone_status_snapshot) override;
  void OnPhoneStatusUpdateReceived(
      proto::PhoneStatusUpdate phone_status_update) override;

  // MultiDeviceSetupClient::Observer:
  void OnHostStatusChanged(
      const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
          host_device_with_status) override;

  void SetReceivedNotifications(
      const RepeatedPtrField<proto::Notification>& notification_protos);

  void SetReceivedPhoneStatusModelStates(
      const proto::PhoneProperties& phone_properties);

  void MaybeSetPhoneModelName(
      const base::Optional<multidevice::RemoteDeviceRef>& remote_device);

  void SetDoNotDisturbState(proto::NotificationMode mode);

  DoNotDisturbController* do_not_disturb_controller_;
  FeatureStatusProvider* feature_status_provider_;
  MessageReceiver* message_receiver_;
  FindMyDeviceController* find_my_device_controller_;
  NotificationAccessManager* notification_access_manager_;
  NotificationManager* notification_manager_;
  multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client_;
  MutablePhoneModel* phone_model_;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_PHONE_STATUS_PROCESSOR_H_