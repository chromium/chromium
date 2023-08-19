// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/text_message_provider.h"

#include <memory>

#include "base/scoped_observation.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_sms_handler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {
constexpr char kNumber[] = "000-000-0000";
constexpr char kText[] = "Fake Sms Message";
constexpr char kTimestamp[] = "Thu Aug  3 13:26:04 EDT 2023";
constexpr char kTestGUID1[] = "1";

class TestObserver : public TextMessageProvider::Observer {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  void MessageReceived(const TextMessageData& message_data) override {
    last_text_message_.text = message_data.text;
    last_text_message_.number = message_data.number;
    last_text_message_.timestamp = message_data.timestamp;
  }

  const TextMessageData& GetTextMessage() { return last_text_message_; }

 private:
  TextMessageData last_text_message_{absl::nullopt, absl::nullopt,
                                     absl::nullopt};
};

}  // namespace

class TextMessageProviderTest : public testing::Test {
 public:
  void SetUp() override {
    // Initialize shill_client fakes as |network_sms_handler_| depends on them
    // during initialization and destruction.
    shill_clients::InitializeFakes();
    // Used new as constructor is private.
    network_sms_handler_.reset(new NetworkSmsHandler());
    network_sms_handler_->Init();
    provider_ = std::make_unique<TextMessageProvider>();
    provider_->Init(network_sms_handler_.get());
  }

  void TearDown() override {
    // These need to be destroyed in reverse order as we need to shutdown the
    // fakes from shill_client.
    provider_.reset();
    network_sms_handler_.reset();
    shill_clients::Shutdown();
  }

  TestObserver* test_observer() { return &test_observer_; }

  void ObserveProvider(
      base::ScopedObservation<TextMessageProvider,
                              TextMessageProvider::Observer>& observation) {
    observation.Observe(provider_.get());
  }

  void SimulateMessageReceived(const TextMessageData& data) {
    provider_->MessageReceivedFromNetwork(kTestGUID1, data);
  }

 private:
  TestObserver test_observer_;
  std::unique_ptr<NetworkSmsHandler> network_sms_handler_;
  std::unique_ptr<TextMessageProvider> provider_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(TextMessageProviderTest, ObserverTest) {
  base::ScopedObservation<TextMessageProvider, TextMessageProvider::Observer>
      observation{test_observer()};
  ObserveProvider(observation);
  const TextMessageData message_data{kNumber, kText, kTimestamp};
  SimulateMessageReceived(message_data);

  EXPECT_EQ(test_observer()->GetTextMessage().number.value(), kNumber);
  EXPECT_EQ(test_observer()->GetTextMessage().text.value(), kText);
  EXPECT_EQ(test_observer()->GetTextMessage().timestamp.value(), kTimestamp);
}

}  // namespace ash
