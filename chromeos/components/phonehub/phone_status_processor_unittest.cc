// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/phone_status_processor.h"

#include <google/protobuf/repeated_field.h>
#include <memory>
#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/components/phonehub/fake_do_not_disturb_controller.h"
#include "chromeos/components/phonehub/fake_feature_status_provider.h"
#include "chromeos/components/phonehub/fake_find_my_device_controller.h"
#include "chromeos/components/phonehub/fake_message_receiver.h"
#include "chromeos/components/phonehub/fake_notification_access_manager.h"
#include "chromeos/components/phonehub/fake_notification_manager.h"
#include "chromeos/components/phonehub/mutable_phone_model.h"
#include "chromeos/components/phonehub/notification_manager.h"
#include "chromeos/components/phonehub/phone_status_model.h"
#include "chromeos/components/phonehub/proto/phonehub_api.pb.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"

namespace chromeos {
namespace phonehub {
using multidevice_setup::mojom::HostStatus;

class PhoneStatusProcessorTest : public testing::Test {
 protected:
  PhoneStatusProcessorTest()
      : test_remote_device_(
            chromeos::multidevice::CreateRemoteDeviceRefForTest()) {}
  PhoneStatusProcessorTest(const PhoneStatusProcessorTest&) = delete;
  PhoneStatusProcessorTest& operator=(const PhoneStatusProcessorTest&) = delete;
  ~PhoneStatusProcessorTest() override = default;

  void SetUp() override {
    fake_do_not_disturb_controller_ =
        std::make_unique<FakeDoNotDisturbController>();
    fake_feature_status_provider_ =
        std::make_unique<FakeFeatureStatusProvider>(FeatureStatus::kDisabled);
    fake_message_receiver_ = std::make_unique<FakeMessageReceiver>();
    fake_find_my_device_controller_ =
        std::make_unique<FakeFindMyDeviceController>();
    fake_notification_access_manager_ =
        std::make_unique<FakeNotificationAccessManager>();
    fake_notification_manager_ = std::make_unique<FakeNotificationManager>();
    mutable_phone_model_ = std::make_unique<MutablePhoneModel>();
    fake_multidevice_setup_client_ =
        std::make_unique<multidevice_setup::FakeMultiDeviceSetupClient>();
  }

  void CreatePhoneStatusProcessor() {
    phone_status_processor_ = std::make_unique<PhoneStatusProcessor>(
        fake_do_not_disturb_controller_.get(),
        fake_feature_status_provider_.get(), fake_message_receiver_.get(),
        fake_find_my_device_controller_.get(),
        fake_notification_access_manager_.get(),
        fake_notification_manager_.get(), fake_multidevice_setup_client_.get(),
        mutable_phone_model_.get());
  }

  void InitializeNotificationProto(proto::Notification* notification) {
    auto origin_app = std::make_unique<proto::App>();
    origin_app->set_package_name("package");
    origin_app->set_visible_name("visible");
    origin_app->set_icon("321");

    notification->add_actions();
    proto::Action* mutable_action = notification->mutable_actions(0);
    mutable_action->set_id(0u);
    mutable_action->set_title("action title");
    mutable_action->set_type(proto::Action_InputType::Action_InputType_TEXT);

    notification->set_id(0u);
    notification->set_epoch_time_millis(1u);
    notification->set_allocated_origin_app(origin_app.release());
    notification->set_title("title");
    notification->set_importance(proto::NotificationImportance::HIGH);
    notification->set_text_content("content");
    notification->set_contact_image("123");
    notification->set_shared_image("123");
  }

  chromeos::multidevice::RemoteDeviceRef test_remote_device_;
  std::unique_ptr<FakeDoNotDisturbController> fake_do_not_disturb_controller_;
  std::unique_ptr<FakeFeatureStatusProvider> fake_feature_status_provider_;
  std::unique_ptr<FakeMessageReceiver> fake_message_receiver_;
  std::unique_ptr<FakeFindMyDeviceController> fake_find_my_device_controller_;
  std::unique_ptr<FakeNotificationAccessManager>
      fake_notification_access_manager_;
  std::unique_ptr<FakeNotificationManager> fake_notification_manager_;
  std::unique_ptr<MutablePhoneModel> mutable_phone_model_;
  std::unique_ptr<multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;
  std::unique_ptr<PhoneStatusProcessor> phone_status_processor_;
};

TEST_F(PhoneStatusProcessorTest, PhoneStatusSnapshotUpdate) {
  fake_multidevice_setup_client_->SetHostStatusWithDevice(
      std::make_pair(HostStatus::kHostVerified, test_remote_device_));
  CreatePhoneStatusProcessor();

  auto expected_phone_properties = std::make_unique<proto::PhoneProperties>();
  expected_phone_properties->set_notification_mode(
      proto::NotificationMode::DO_NOT_DISTURB_ON);
  expected_phone_properties->set_notification_access_state(
      proto::NotificationAccessState::ACCESS_GRANTED);
  expected_phone_properties->set_ring_status(
      proto::FindMyDeviceRingStatus::RINGING);
  expected_phone_properties->set_battery_percentage(24u);
  expected_phone_properties->set_charging_state(
      proto::ChargingState::CHARGING_AC);
  expected_phone_properties->set_signal_strength(
      proto::SignalStrength::FOUR_BARS);
  expected_phone_properties->set_mobile_provider("google");
  expected_phone_properties->set_connection_state(
      proto::MobileConnectionState::SIM_WITH_RECEPTION);

  proto::PhoneStatusSnapshot expected_snapshot;
  expected_snapshot.set_allocated_properties(
      expected_phone_properties.release());
  expected_snapshot.add_notifications();
  InitializeNotificationProto(expected_snapshot.mutable_notifications(0));

  // Simulate feature set to enabled and connected.
  fake_feature_status_provider_->SetStatus(FeatureStatus::kEnabledAndConnected);

  // Simulate receiving a proto message.
  fake_message_receiver_->NotifyPhoneStatusSnapshotReceived(expected_snapshot);

  EXPECT_EQ(1u, fake_notification_manager_->num_notifications());
  EXPECT_EQ(base::UTF8ToUTF16(test_remote_device_.name()),
            *mutable_phone_model_->phone_name());
  EXPECT_TRUE(fake_do_not_disturb_controller_->IsDndEnabled());
  EXPECT_TRUE(fake_find_my_device_controller_->IsPhoneRinging());
  EXPECT_TRUE(fake_notification_access_manager_->HasAccessBeenGranted());

  base::Optional<PhoneStatusModel> phone_status_model =
      mutable_phone_model_->phone_status_model();
  EXPECT_EQ(PhoneStatusModel::ChargingState::kChargingAc,
            phone_status_model->charging_state());
  EXPECT_EQ(24u, phone_status_model->battery_percentage());
  EXPECT_EQ(base::UTF8ToUTF16("google"),
            phone_status_model->mobile_connection_metadata()->mobile_provider);
  EXPECT_EQ(PhoneStatusModel::SignalStrength::kFourBars,
            phone_status_model->mobile_connection_metadata()->signal_strength);
  EXPECT_EQ(PhoneStatusModel::MobileStatus::kSimWithReception,
            phone_status_model->mobile_status());

  // Change feature status to disconnected.
  fake_feature_status_provider_->SetStatus(
      FeatureStatus::kEnabledButDisconnected);

  EXPECT_EQ(0u, fake_notification_manager_->num_notifications());
  EXPECT_EQ(base::UTF8ToUTF16(test_remote_device_.name()),
            *mutable_phone_model_->phone_name());
  EXPECT_FALSE(mutable_phone_model_->phone_status_model().has_value());
}

TEST_F(PhoneStatusProcessorTest, PhoneStatusUpdate) {
  fake_multidevice_setup_client_->SetHostStatusWithDevice(
      std::make_pair(HostStatus::kHostVerified, test_remote_device_));
  CreatePhoneStatusProcessor();

  auto expected_phone_properties = std::make_unique<proto::PhoneProperties>();
  expected_phone_properties->set_notification_mode(
      proto::NotificationMode::DO_NOT_DISTURB_ON);
  expected_phone_properties->set_notification_access_state(
      proto::NotificationAccessState::ACCESS_GRANTED);
  expected_phone_properties->set_ring_status(
      proto::FindMyDeviceRingStatus::RINGING);
  expected_phone_properties->set_battery_percentage(24u);
  expected_phone_properties->set_charging_state(
      proto::ChargingState::CHARGING_AC);
  expected_phone_properties->set_signal_strength(
      proto::SignalStrength::FOUR_BARS);
  expected_phone_properties->set_mobile_provider("google");
  expected_phone_properties->set_connection_state(
      proto::MobileConnectionState::SIM_WITH_RECEPTION);

  proto::PhoneStatusUpdate expected_update;
  expected_update.set_allocated_properties(expected_phone_properties.release());
  expected_update.add_updated_notifications();
  InitializeNotificationProto(expected_update.mutable_updated_notifications(0));

  // Simulate feature set to enabled and connected.
  fake_feature_status_provider_->SetStatus(FeatureStatus::kEnabledAndConnected);

  // Simulate receiving a proto message.
  fake_message_receiver_->NotifyPhoneStatusUpdateReceived(expected_update);

  EXPECT_EQ(1u, fake_notification_manager_->num_notifications());
  EXPECT_EQ(base::UTF8ToUTF16(test_remote_device_.name()),
            *mutable_phone_model_->phone_name());
  EXPECT_TRUE(fake_do_not_disturb_controller_->IsDndEnabled());
  EXPECT_TRUE(fake_find_my_device_controller_->IsPhoneRinging());
  EXPECT_TRUE(fake_notification_access_manager_->HasAccessBeenGranted());

  base::Optional<PhoneStatusModel> phone_status_model =
      mutable_phone_model_->phone_status_model();
  EXPECT_EQ(PhoneStatusModel::ChargingState::kChargingAc,
            phone_status_model->charging_state());
  EXPECT_EQ(24u, phone_status_model->battery_percentage());
  EXPECT_EQ(base::UTF8ToUTF16("google"),
            phone_status_model->mobile_connection_metadata()->mobile_provider);
  EXPECT_EQ(PhoneStatusModel::SignalStrength::kFourBars,
            phone_status_model->mobile_connection_metadata()->signal_strength);
  EXPECT_EQ(PhoneStatusModel::MobileStatus::kSimWithReception,
            phone_status_model->mobile_status());

  // Update with one removed notification.
  expected_update.add_removed_notification_ids(0u);
  fake_message_receiver_->NotifyPhoneStatusUpdateReceived(expected_update);

  EXPECT_EQ(0u, fake_notification_manager_->num_notifications());
  EXPECT_EQ(base::UTF8ToUTF16(test_remote_device_.name()),
            *mutable_phone_model_->phone_name());
  EXPECT_TRUE(fake_do_not_disturb_controller_->IsDndEnabled());
  EXPECT_TRUE(fake_find_my_device_controller_->IsPhoneRinging());
  EXPECT_TRUE(fake_notification_access_manager_->HasAccessBeenGranted());

  phone_status_model = mutable_phone_model_->phone_status_model();
  EXPECT_EQ(PhoneStatusModel::ChargingState::kChargingAc,
            phone_status_model->charging_state());
  EXPECT_EQ(24u, phone_status_model->battery_percentage());
  EXPECT_EQ(base::UTF8ToUTF16("google"),
            phone_status_model->mobile_connection_metadata()->mobile_provider);
  EXPECT_EQ(PhoneStatusModel::SignalStrength::kFourBars,
            phone_status_model->mobile_connection_metadata()->signal_strength);
  EXPECT_EQ(PhoneStatusModel::MobileStatus::kSimWithReception,
            phone_status_model->mobile_status());

  // Change feature status to disconnected.
  fake_feature_status_provider_->SetStatus(
      FeatureStatus::kEnabledButDisconnected);

  EXPECT_EQ(0u, fake_notification_manager_->num_notifications());
  EXPECT_EQ(base::UTF8ToUTF16(test_remote_device_.name()),
            *mutable_phone_model_->phone_name());
  EXPECT_FALSE(mutable_phone_model_->phone_status_model().has_value());
}

TEST_F(PhoneStatusProcessorTest, NoHostPhoneName) {
  fake_multidevice_setup_client_->SetHostStatusWithDevice(
      std::make_pair(HostStatus::kHostVerified, base::nullopt));
  CreatePhoneStatusProcessor();

  auto expected_phone_properties = std::make_unique<proto::PhoneProperties>();
  proto::PhoneStatusUpdate expected_update;
  expected_update.set_allocated_properties(expected_phone_properties.release());

  // Simulate feature set to enabled and connected.
  fake_feature_status_provider_->SetStatus(FeatureStatus::kEnabledAndConnected);

  // Simulate receiving a proto message.
  fake_message_receiver_->NotifyPhoneStatusUpdateReceived(expected_update);

  EXPECT_EQ(0u, fake_notification_manager_->num_notifications());
  EXPECT_EQ(base::nullopt, mutable_phone_model_->phone_name());
}

}  // namespace phonehub
}  // namespace chromeos
