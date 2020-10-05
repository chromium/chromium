// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/cros_state_sender.h"

#include <memory>

#include "chromeos/components/phonehub/fake_connection_manager.h"
#include "chromeos/components/phonehub/fake_message_sender.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
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
    fake_message_sender_ = std::make_unique<FakeMessageSender>();
    fake_connection_manager_ = std::make_unique<FakeConnectionManager>();
    fake_multidevice_setup_client_ =
        std::make_unique<multidevice_setup::FakeMultiDeviceSetupClient>();
    cros_state_sender_ = std::make_unique<CrosStateSender>(
        fake_message_sender_.get(), fake_connection_manager_.get(),
        fake_multidevice_setup_client_.get());
  }

  std::unique_ptr<FakeMessageSender> fake_message_sender_;
  std::unique_ptr<FakeConnectionManager> fake_connection_manager_;
  std::unique_ptr<multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;

 private:
  std::unique_ptr<CrosStateSender> cros_state_sender_;
};

TEST_F(CrosStateSenderTest, UpdatesOnConnected) {
  // Set notification feature to be enabled.
  fake_multidevice_setup_client_->SetFeatureState(
      Feature::kPhoneHubNotifications, FeatureState::kEnabledByUser);
  // Expect no new messages since connection has not been established.
  EXPECT_EQ(0u, fake_message_sender_->GetCrosStateCallCount());

  // Update connection state to connecting.
  fake_connection_manager_->SetStatus(ConnectionManager::Status::kConnecting);
  // Connecting state does not trigger a request message.
  EXPECT_EQ(0u, fake_message_sender_->GetCrosStateCallCount());

  // Simulate connected state. Expect a new message to be sent.
  fake_connection_manager_->SetStatus(ConnectionManager::Status::kConnected);
  EXPECT_TRUE(fake_message_sender_->GetRecentCrosState());
  EXPECT_EQ(1u, fake_message_sender_->GetCrosStateCallCount());

  // Simulate disconnected state, this should not trigger a new request.
  fake_connection_manager_->SetStatus(ConnectionManager::Status::kDisconnected);
  EXPECT_TRUE(fake_message_sender_->GetRecentCrosState());
  EXPECT_EQ(1u, fake_message_sender_->GetCrosStateCallCount());
}

TEST_F(CrosStateSenderTest, NotificationFeatureStateChanged) {
  // Set connection state to be connected.
  fake_connection_manager_->SetStatus(ConnectionManager::Status::kConnected);
  // Expect new messages to be sent when connection state is connected.
  EXPECT_FALSE(fake_message_sender_->GetRecentCrosState());
  EXPECT_EQ(1u, fake_message_sender_->GetCrosStateCallCount());

  // Simulate enabling notification feature state and expect cros state to be
  // enabled.
  fake_multidevice_setup_client_->SetFeatureState(
      Feature::kPhoneHubNotifications, FeatureState::kEnabledByUser);
  EXPECT_TRUE(fake_message_sender_->GetRecentCrosState());
  EXPECT_EQ(2u, fake_message_sender_->GetCrosStateCallCount());

  // Update a different feature state and expect that it did not affect the
  // cros state.
  fake_multidevice_setup_client_->SetFeatureState(
      Feature::kSmartLock, FeatureState::kDisabledByUser);
  EXPECT_TRUE(fake_message_sender_->GetRecentCrosState());
  EXPECT_EQ(3u, fake_message_sender_->GetCrosStateCallCount());

  // Simulate disabling notification feature state and expect cros state to be
  // disabled.
  fake_multidevice_setup_client_->SetFeatureState(
      Feature::kPhoneHubNotifications, FeatureState::kDisabledByUser);
  EXPECT_FALSE(fake_message_sender_->GetRecentCrosState());
  EXPECT_EQ(4u, fake_message_sender_->GetCrosStateCallCount());
}

}  // namespace phonehub
}  // namespace chromeos
