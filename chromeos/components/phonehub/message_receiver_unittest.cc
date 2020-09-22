// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/message_receiver_impl.h"

#include "base/strings/strcat.h"
#include "chromeos/components/phonehub/fake_connection_manager.h"
#include "chromeos/components/phonehub/proto/phonehub_api.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <memory>

namespace chromeos {
namespace phonehub {
namespace {

class FakeObserver : public MessageReceiver::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t snapshot_num_calls() const {
    return phone_status_snapshot_updated_num_calls_;
  }

  size_t status_updated_num_calls() const {
    return phone_status_updated_num_calls_;
  }

  proto::PhoneStatusSnapshot last_snapshot() const { return last_snapshot_; }

  proto::PhoneStatusUpdate last_status_update() const {
    return last_status_update_;
  }

  // MessageReceiver::Observer:
  void OnPhoneStatusSnapshotReceived(
      proto::PhoneStatusSnapshot phone_status_snapshot) override {
    last_snapshot_ = phone_status_snapshot;
    ++phone_status_snapshot_updated_num_calls_;
  }

  void OnPhoneStatusUpdateReceived(
      proto::PhoneStatusUpdate phone_status_update) override {
    last_status_update_ = phone_status_update;
    ++phone_status_updated_num_calls_;
  }

 private:
  size_t phone_status_snapshot_updated_num_calls_ = 0;
  size_t phone_status_updated_num_calls_ = 0;
  proto::PhoneStatusSnapshot last_snapshot_;
  proto::PhoneStatusUpdate last_status_update_;
};

std::string SerializeMessage(proto::MessageType message_type,
                             const google::protobuf::MessageLite* request) {
  // Add two space characters, followed by the serialized proto.
  std::string message = base::StrCat({"  ", request->SerializeAsString()});

  // Replace the first two characters with |message_type| as a 16-bit int.
  uint16_t* ptr =
      reinterpret_cast<uint16_t*>(const_cast<char*>(message.data()));
  *ptr = static_cast<uint16_t>(message_type);
  return message;
}

}  // namespace

class MessageReceiverImplTest : public testing::Test {
 protected:
  MessageReceiverImplTest()
      : fake_connection_manager_(std::make_unique<FakeConnectionManager>()) {}
  MessageReceiverImplTest(const MessageReceiverImplTest&) = delete;
  MessageReceiverImplTest& operator=(const MessageReceiverImplTest&) = delete;
  ~MessageReceiverImplTest() override = default;

  void SetUp() override {
    message_receiver_ =
        std::make_unique<MessageReceiverImpl>(fake_connection_manager_.get());
    message_receiver_->AddObserver(&fake_observer_);
  }

  void TearDown() override {
    message_receiver_->RemoveObserver(&fake_observer_);
  }

  size_t GetNumPhoneStatusSnapshotCalls() const {
    return fake_observer_.snapshot_num_calls();
  }

  size_t GetNumPhoneStatusUpdatedCalls() const {
    return fake_observer_.status_updated_num_calls();
  }

  proto::PhoneStatusSnapshot GetLastSnapshot() const {
    return fake_observer_.last_snapshot();
  }

  proto::PhoneStatusUpdate GetLastStatusUpdate() const {
    return fake_observer_.last_status_update();
  }

  FakeObserver fake_observer_;
  std::unique_ptr<FakeConnectionManager> fake_connection_manager_;
  std::unique_ptr<MessageReceiverImpl> message_receiver_;
};

TEST_F(MessageReceiverImplTest, OnPhoneStatusSnapshotReceieved) {
  const int32_t expected_battery_percentage = 15;
  auto expected_phone_properties = std::make_unique<proto::PhoneProperties>();
  expected_phone_properties->set_battery_percentage(
      expected_battery_percentage);

  proto::PhoneStatusSnapshot expected_snapshot;
  expected_snapshot.set_allocated_properties(
      expected_phone_properties.release());
  expected_snapshot.add_notifications();

  // Simulate receiving a message.
  const std::string expected_message =
      SerializeMessage(proto::PHONE_STATUS_SNAPSHOT, &expected_snapshot);
  fake_connection_manager_->NotifyMessageReceived(expected_message);

  proto::PhoneStatusSnapshot actual_snapshot = GetLastSnapshot();

  EXPECT_EQ(1u, GetNumPhoneStatusSnapshotCalls());
  EXPECT_EQ(0u, GetNumPhoneStatusUpdatedCalls());
  EXPECT_EQ(expected_battery_percentage,
            actual_snapshot.properties().battery_percentage());
  EXPECT_EQ(1, actual_snapshot.notifications_size());
}

TEST_F(MessageReceiverImplTest, OnPhoneStatusUpdated) {
  const int32_t expected_battery_percentage = 15u;
  auto expected_phone_properties = std::make_unique<proto::PhoneProperties>();
  expected_phone_properties->set_battery_percentage(
      expected_battery_percentage);

  proto::PhoneStatusUpdate expected_update;
  expected_update.set_allocated_properties(expected_phone_properties.release());
  expected_update.add_updated_notifications();

  const int64_t expected_removed_id = 24u;
  expected_update.add_removed_notification_ids(expected_removed_id);

  // Simulate receiving a message.
  const std::string expected_message =
      SerializeMessage(proto::PHONE_STATUS_UPDATE, &expected_update);
  fake_connection_manager_->NotifyMessageReceived(expected_message);

  proto::PhoneStatusUpdate actual_update = GetLastStatusUpdate();

  EXPECT_EQ(0u, GetNumPhoneStatusSnapshotCalls());
  EXPECT_EQ(1u, GetNumPhoneStatusUpdatedCalls());
  EXPECT_EQ(expected_battery_percentage,
            actual_update.properties().battery_percentage());
  EXPECT_EQ(1, actual_update.updated_notifications_size());
  EXPECT_EQ(expected_removed_id, actual_update.removed_notification_ids()[0]);
}

}  // namespace phonehub
}  // namespace chromeos
