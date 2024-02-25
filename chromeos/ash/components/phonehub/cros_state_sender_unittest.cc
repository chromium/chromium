// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/cros_state_sender.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/mock_timer.h"
#include "chromeos/ash/components/phonehub/fake_attestation_certificate_generator.h"
#include "chromeos/ash/components/phonehub/fake_message_sender.h"
#include "chromeos/ash/components/phonehub/mutable_phone_model.h"
#include "chromeos/ash/components/phonehub/phone_model_test_util.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_connection_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace phonehub {

using multidevice_setup::mojom::Feature;
using multidevice_setup::mojom::FeatureState;

class CrosStateSenderTest : public testing::Test {
 protected:
  CrosStateSenderTest() = default;
  ~CrosStateSenderTest() override = default;

  CrosStateSenderTest(const CrosStateSender&) = delete;
  CrosStateSenderTest& operator=(const CrosStateSender&) = delete;

  // testing::Test:
  void SetUp() override {
    auto timer = std::make_unique<base::MockOneShotTimer>();
    mock_timer_ = timer.get();

    fake_message_sender_ = std::make_unique<FakeMessageSender>();
    fake_connection_manager_ =
        std::make_unique<secure_channel::FakeConnectionManager>();
    fake_multidevice_setup_client_ =
        std::make_unique<multidevice_setup::FakeMultiDeviceSetupClient>();
    phone_model_ = std::make_unique<MutablePhoneModel>();
    auto fake_attestation_certificate_generator =
        std::make_unique<FakeAttestationCertificateGenerator>();
    fake_attestation_certificate_generator_ =
        fake_attestation_certificate_generator.get();
    cros_state_sender_ = base::WrapUnique(new CrosStateSender(
        fake_message_sender_.get(), fake_connection_manager_.get(),
        fake_multidevice_setup_client_.get(), phone_model_.get(),
        std::move(timer), std::move(fake_attestation_certificate_generator)));
  }

  base::TimeDelta GetRetryDelay() { return cros_state_sender_->retry_delay_; }
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<FakeMessageSender> fake_message_sender_;
  std::unique_ptr<secure_channel::FakeConnectionManager>
      fake_connection_manager_;
  std::unique_ptr<multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;
  std::unique_ptr<MutablePhoneModel> phone_model_;
  raw_ptr<FakeAttestationCertificateGenerator, DanglingUntriaged>
      fake_attestation_certificate_generator_;
  raw_ptr<base::MockOneShotTimer, DanglingUntriaged> mock_timer_;

 private:
  std::unique_ptr<CrosStateSender> cros_state_sender_;
};

TEST_F(CrosStateSenderTest, PerformUpdateCrosStateRetrySequence) {
  fake_connection_manager_->SetStatus(
      secure_channel::ConnectionManager::Status::kConnected);
  EXPECT_EQ(1u, fake_message_sender_->GetCrosStateCallCount());

  // The retry time follows a doubling sequence.
  EXPECT_EQ(base::Seconds(15u), GetRetryDelay());
  mock_timer_->Fire();
  EXPECT_TRUE(mock_timer_->IsRunning());
  EXPECT_EQ(2u, fake_message_sender_->GetCrosStateCallCount());

  EXPECT_EQ(base::Seconds(30u), GetRetryDelay());
  mock_timer_->Fire();
  EXPECT_TRUE(mock_timer_->IsRunning());
  EXPECT_EQ(3u, fake_message_sender_->GetCrosStateCallCount());

  EXPECT_EQ(base::Seconds(60u), GetRetryDelay());
  mock_timer_->Fire();
  EXPECT_TRUE(mock_timer_->IsRunning());
  EXPECT_EQ(4u, fake_message_sender_->GetCrosStateCallCount());

  // The phone model becomes populated, stops retrying.
  EXPECT_EQ(base::Seconds(120u), GetRetryDelay());
  phone_model_->SetPhoneStatusModel(CreateFakePhoneStatusModel());
  mock_timer_->Fire();
  EXPECT_FALSE(mock_timer_->IsRunning());
  EXPECT_EQ(4u, fake_message_sender_->GetCrosStateCallCount());
  EXPECT_EQ(base::Seconds(120u), GetRetryDelay());

  fake_connection_manager_->SetStatus(
      secure_channel::ConnectionManager::Status::kConnecting);
  EXPECT_FALSE(mock_timer_->IsRunning());

  // Cancellation of the retry timer occurs properly when an attempt is
  // reinitiated but the status is not connecting.
  fake_connection_manager_->SetStatus(
      secure_channel::ConnectionManager::Status::kConnected);
  EXPECT_TRUE(mock_timer_->IsRunning());
  fake_connection_manager_->SetStatus(
      secure_channel::ConnectionManager::Status::kConnecting);
  EXPECT_FALSE(mock_timer_->IsRunning());
}

TEST_F(CrosStateSenderTest, UpdatesOnConnected) {
  feature_list_.InitWithFeatures(/* enabled_features= */ {},
                                 /* disabled_features= */ {features::kEcheSWA});
  // Set notification feature to be enabled.
  fake_multidevice_setup_client_->SetFeatureState(
      Feature::kPhoneHubNotifications, FeatureState::kEnabledByUser);
  // Set camera roll feature to be enabled.
  fake_multidevice_setup_client_->SetFeatureState(Feature::kPhoneHubCameraRoll,
                                                  FeatureState::kEnabledByUser);
  // Expect no new messages since connection has not been established.
  EXPECT_EQ(0u, fake_message_sender_->GetCrosStateCallCount());
  EXPECT_FALSE(mock_timer_->IsRunning());

  // Update connection state to connecting.
  fake_connection_manager_->SetStatus(
      secure_channel::ConnectionManager::Status::kConnecting);
  // Connecting state does not trigger a request message.
  EXPECT_EQ(0u, fake_message_sender_->GetCrosStateCallCount());
  EXPECT_FALSE(mock_timer_->IsRunning());

  // Simulate connected state. Expect a new message to be sent.
  fake_connection_manager_->SetStatus(
      secure_channel::ConnectionManager::Status::kConnected);
  EXPECT_TRUE(std::get<0>(fake_message_sender_->GetRecentCrosState()));
  EXPECT_TRUE(std::get<1>(fake_message_sender_->GetRecentCrosState()));
  EXPECT_EQ(nullptr, std::get<2>(fake_message_sender_->GetRecentCrosState()));
  EXPECT_EQ(1u, fake_message_sender_->GetCrosStateCallCount());

  // Phone model is populated.
  phone_model_->SetPhoneStatusModel(CreateFakePhoneStatusModel());
  mock_timer_->Fire();
  EXPECT_EQ(1u, fake_message_sender_->GetCrosStateCallCount());

  // Simulate disconnected state, this should not trigger a new request.
  fake_connection_manager_->SetStatus(
      secure_channel::ConnectionManager::Status::kDisconnected);
  EXPECT_TRUE(std::get<0>(fake_message_sender_->GetRecentCrosState()));
  EXPECT_TRUE(std::get<1>(fake_message_sender_->GetRecentCrosState()));
  EXPECT_EQ(1u, fake_message_sender_->GetCrosStateCallCount());
  EXPECT_FALSE(mock_timer_->IsRunning());
}

TEST_F(CrosStateSenderTest, CrosStateMessageIncludesAttestationIfEcheEnabled) {
  feature_list_.InitWithFeatures(/* enabled_features= */ {features::kEcheSWA},
                                 /* disabled_features= */ {});
  // Set notification feature to be enabled.
  fake_multidevice_setup_client_->SetFeatureState(
      Feature::kPhoneHubNotifications, FeatureState::kEnabledByUser);
  // Set camera roll feature to be enabled.
  fake_multidevice_setup_client_->SetFeatureState(Feature::kPhoneHubCameraRoll,
                                                  FeatureState::kEnabledByUser);
  fake_connection_manager_->SetStatus(
      secure_channel::ConnectionManager::Status::kConnected);
  EXPECT_TRUE(std::get<0>(fake_message_sender_->GetRecentCrosState()));
  EXPECT_TRUE(std::get<1>(fake_message_sender_->GetRecentCrosState()));
  EXPECT_EQ(&(fake_attestation_certificate_generator_->CERTS),
            std::get<2>(fake_message_sender_->GetRecentCrosState()));
  EXPECT_EQ(1u, fake_message_sender_->GetCrosStateCallCount());
}

TEST_F(CrosStateSenderTest, ResendOnNewAttestationCertificate) {
  feature_list_.InitWithFeatures(/* enabled_features= */ {features::kEcheSWA},
                                 /* disabled_features= */ {});
  // Set notification feature to be enabled.
  fake_multidevice_setup_client_->SetFeatureState(
      Feature::kPhoneHubNotifications, FeatureState::kEnabledByUser);
  // Set camera roll feature to be enabled.
  fake_multidevice_setup_client_->SetFeatureState(Feature::kPhoneHubCameraRoll,
                                                  FeatureState::kEnabledByUser);
  fake_connection_manager_->SetStatus(
      secure_channel::ConnectionManager::Status::kConnected);
  EXPECT_EQ(1u, fake_message_sender_->GetCrosStateCallCount());

  fake_attestation_certificate_generator_->RetrieveCertificate();
  EXPECT_EQ(2u, fake_message_sender_->GetCrosStateCallCount());
}

TEST_F(CrosStateSenderTest, NotificationFeatureStateChanged) {
  // Set connection state to be connected.
  fake_connection_manager_->SetStatus(
      secure_channel::ConnectionManager::Status::kConnected);

  // Phone model is populated.
  phone_model_->SetPhoneStatusModel(CreateFakePhoneStatusModel());
  EXPECT_TRUE(mock_timer_->IsRunning());

  // Expect new messages to be sent when connection state is connected.
  EXPECT_FALSE(std::get<0>(fake_message_sender_->GetRecentCrosState()));
  EXPECT_FALSE(std::get<1>(fake_message_sender_->GetRecentCrosState()));
  EXPECT_EQ(1u, fake_message_sender_->GetCrosStateCallCount());
  mock_timer_->Fire();

  // Simulate enabling notification feature state and expect cros state to be
  // enabled.
  fake_multidevice_setup_client_->SetFeatureState(
      Feature::kPhoneHubNotifications, FeatureState::kEnabledByUser);
  EXPECT_TRUE(std::get<0>(fake_message_sender_->GetRecentCrosState()));
  EXPECT_EQ(2u, fake_message_sender_->GetCrosStateCallCount());
  mock_timer_->Fire();

  // Update a different feature state and expect that it did not affect the
  // cros state.
  fake_multidevice_setup_client_->SetFeatureState(
      Feature::kSmartLock, FeatureState::kDisabledByUser);
  EXPECT_TRUE(std::get<0>(fake_message_sender_->GetRecentCrosState()));
  EXPECT_EQ(3u, fake_message_sender_->GetCrosStateCallCount());
  mock_timer_->Fire();

  // Simulate disabling notification feature state and expect cros state to be
  // disabled.
  fake_multidevice_setup_client_->SetFeatureState(
      Feature::kPhoneHubNotifications, FeatureState::kDisabledByUser);
  EXPECT_FALSE(std::get<0>(fake_message_sender_->GetRecentCrosState()));
  EXPECT_EQ(4u, fake_message_sender_->GetCrosStateCallCount());

  // Simulate enabling camera roll feature state and expect cros state to be
  // updated.
  fake_multidevice_setup_client_->SetFeatureState(Feature::kPhoneHubCameraRoll,
                                                  FeatureState::kEnabledByUser);
  EXPECT_TRUE(std::get<1>(fake_message_sender_->GetRecentCrosState()));
  EXPECT_EQ(5u, fake_message_sender_->GetCrosStateCallCount());

  // Firing the timer does not cause the cros state to be sent again.
  mock_timer_->Fire();
  EXPECT_EQ(5u, fake_message_sender_->GetCrosStateCallCount());
}

}  // namespace phonehub
}  // namespace ash
