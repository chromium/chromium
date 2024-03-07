// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/text_message_provider.h"

#include <memory>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/scoped_observation.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/network/fake_network_metadata_store.h"
#include "chromeos/ash/components/network/metrics/cellular_network_metrics_logger.h"
#include "chromeos/ash/components/network/mock_managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_sms_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/text_message_suppression_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {
constexpr char kNumber[] = "000-000-0000";
constexpr char kText1[] = "Fake Sms Message 1";
constexpr char kText2[] = "Fake Sms Message 2";
constexpr char kTimestamp[] = "Thu Aug  3 13:26:04 EDT 2023";
constexpr char kTestGUID1[] = "1";
constexpr char kTestGUID2[] = "2";
constexpr char kEmptyGUID[] = "";

class TestObserver : public TextMessageProvider::Observer {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  void MessageReceived(const std::string& guid,
                       const TextMessageData& message_data) override {
    text_messages_.emplace_back(message_data.number, message_data.text,
                                message_data.timestamp);
  }

  const TextMessageData& GetLastTextMessage() {
    CHECK(!text_messages_.empty());
    return text_messages_.back();
  }

  size_t MessageCount() { return text_messages_.size(); }

 private:
  std::vector<TextMessageData> text_messages_;
};

}  // namespace

class TextMessageProviderTest : public testing::Test {
 public:
  void SetUp() override {
    // Initialize shill_client fakes as |network_sms_handler_| depends on them
    // during initialization and destruction.
    shill_clients::InitializeFakes();
    network_state_handler_ = NetworkStateHandler::InitializeForTest();
    // Used new as constructor is private.
    network_sms_handler_.reset(new NetworkSmsHandler());
    network_sms_handler_->Init(network_state_handler_.get());
    provider_ = std::make_unique<TextMessageProvider>();
    provider_->Init(network_sms_handler_.get(),
                    &mock_managed_network_configuration_handler_);
    provider_->SetNetworkMetadataStore(&fake_network_metadata_store_);
    observation.Observe(provider_.get());

    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    // These need to be destroyed in reverse order as we need to shutdown the
    // fakes from shill_client.
    observation.Reset();
    provider_.reset();
    network_sms_handler_.reset();
    network_state_handler_.reset();
    shill_clients::Shutdown();
  }

  void SimulateMessageReceived(const std::string& guid,
                               const TextMessageData& data) {
    provider_->MessageReceivedFromNetwork(guid, data);
  }

  void SimulatePolicyChanged() { provider_->PoliciesChanged(/*userhash=*/""); }

  void AssertTestObserverValue(
      size_t expected_count,
      std::optional<TextMessageData> expected_message) {
    ASSERT_EQ(expected_count, test_observer_.MessageCount());

    if (!expected_message.has_value()) {
      return;
    }
    EXPECT_EQ(expected_message->number.value(),
              test_observer_.GetLastTextMessage().number.value());
    EXPECT_EQ(expected_message->text.value(),
              test_observer_.GetLastTextMessage().text.value());
    EXPECT_EQ(expected_message->timestamp.value(),
              test_observer_.GetLastTextMessage().timestamp.value());
  }

  void AssertPolicyBuckets(size_t allow_count,
                           size_t suppress_count,
                           size_t unset_count) {
    histogram_tester_->ExpectBucketCount(
        CellularNetworkMetricsLogger::
            kPolicyAllowTextMessagesSuppressionStateHistogram,
        CellularNetworkMetricsLogger::PolicyTextMessageSuppressionState::kUnset,
        unset_count);
    histogram_tester_->ExpectBucketCount(
        CellularNetworkMetricsLogger::
            kPolicyAllowTextMessagesSuppressionStateHistogram,
        CellularNetworkMetricsLogger::PolicyTextMessageSuppressionState::
            kTextMessagesAllow,
        allow_count);
    histogram_tester_->ExpectBucketCount(
        CellularNetworkMetricsLogger::
            kPolicyAllowTextMessagesSuppressionStateHistogram,
        CellularNetworkMetricsLogger::PolicyTextMessageSuppressionState::
            kTextMessagesSuppress,
        suppress_count);
  }

  MockManagedNetworkConfigurationHandler*
  mock_managed_network_configuration_handler() {
    return &mock_managed_network_configuration_handler_;
  }

  FakeNetworkMetadataStore* fake_network_metadata_store() {
    return &fake_network_metadata_store_;
  }

 private:
  TestObserver test_observer_;
  testing::NiceMock<MockManagedNetworkConfigurationHandler>
      mock_managed_network_configuration_handler_;

  FakeNetworkMetadataStore fake_network_metadata_store_;
  std::unique_ptr<NetworkSmsHandler> network_sms_handler_;
  std::unique_ptr<TextMessageProvider> provider_;
  std::unique_ptr<NetworkStateHandler> network_state_handler_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  base::test::SingleThreadTaskEnvironment task_environment_;

  base::ScopedObservation<TextMessageProvider, TextMessageProvider::Observer>
      observation{&test_observer_};
};

TEST_F(TextMessageProviderTest, MessageReceivedPolicyAllowUserSuppressTest) {
  EXPECT_CALL(*mock_managed_network_configuration_handler(),
              GetAllowTextMessages)
      .WillOnce(::testing::Return(PolicyTextMessageSuppressionState::kAllow));

  fake_network_metadata_store()->SetUserTextMessageSuppressionState(
      kTestGUID1, UserTextMessageSuppressionState::kSuppress);
  fake_network_metadata_store()->SetUserTextMessageSuppressionState(
      kTestGUID2, UserTextMessageSuppressionState::kSuppress);

  TextMessageData message_data{kNumber, kText1, kTimestamp};
  TextMessageData message_data_2{kNumber, kText2, kTimestamp};

  SimulatePolicyChanged();
  SimulateMessageReceived(kTestGUID1, message_data);
  AssertTestObserverValue(
      /*expected_count=*/1, std::move(message_data));
  SimulateMessageReceived(kTestGUID2, message_data_2);
  AssertTestObserverValue(
      /*expected_count=*/2, std::move(message_data_2));
  AssertPolicyBuckets(/*allow_count=*/1u, /*suppress_count=*/0u,
                      /*unset_count=*/0u);
}

TEST_F(TextMessageProviderTest, MessageReceivedPolicySuppressUserAllowTest) {
  EXPECT_CALL(*mock_managed_network_configuration_handler(),
              GetAllowTextMessages)
      .WillOnce(
          ::testing::Return(PolicyTextMessageSuppressionState::kSuppress));

  fake_network_metadata_store()->SetUserTextMessageSuppressionState(
      kTestGUID1, UserTextMessageSuppressionState::kAllow);
  fake_network_metadata_store()->SetUserTextMessageSuppressionState(
      kTestGUID2, UserTextMessageSuppressionState::kAllow);

  SimulatePolicyChanged();
  SimulateMessageReceived(kTestGUID1, {kNumber, kText1, kTimestamp});
  AssertTestObserverValue(/*expected_count=*/0, std::nullopt);
  SimulateMessageReceived(kTestGUID2, {kNumber, kText2, kTimestamp});
  AssertTestObserverValue(/*expected_count=*/0, std::nullopt);
  AssertPolicyBuckets(/*allow_count=*/0u, /*suppress_count=*/1u,
                      /*unset_count=*/0u);
}

TEST_F(TextMessageProviderTest, MessageReceivedPolicyAllowUserAllowTest) {
  EXPECT_CALL(*mock_managed_network_configuration_handler(),
              GetAllowTextMessages)
      .WillOnce(::testing::Return(PolicyTextMessageSuppressionState::kAllow));

  fake_network_metadata_store()->SetUserTextMessageSuppressionState(
      kTestGUID1, UserTextMessageSuppressionState::kAllow);

  TextMessageData message_data{kNumber, kText1, kTimestamp};

  SimulatePolicyChanged();
  SimulateMessageReceived(kTestGUID1, message_data);
  AssertTestObserverValue(
      /*expected_count=*/1, std::move(message_data));
  AssertPolicyBuckets(/*allow_count=*/1u, /*suppress_count=*/0u,
                      /*unset_count=*/0u);
}

TEST_F(TextMessageProviderTest, MessageReceivedPolicySuppressUserSuppressTest) {
  EXPECT_CALL(*mock_managed_network_configuration_handler(),
              GetAllowTextMessages)
      .WillOnce(
          ::testing::Return(PolicyTextMessageSuppressionState::kSuppress));

  fake_network_metadata_store()->SetUserTextMessageSuppressionState(
      kTestGUID1, UserTextMessageSuppressionState::kSuppress);

  SimulatePolicyChanged();
  SimulateMessageReceived(kTestGUID1, {kNumber, kText1, kTimestamp});
  AssertTestObserverValue(
      /*expected_count=*/0, std::nullopt);
  AssertPolicyBuckets(/*allow_count=*/0u, /*suppress_count=*/1u,
                      /*unset_count=*/0u);
}

TEST_F(TextMessageProviderTest,
       MessageReceivedPolicyUnsetOneNetworkAllowOneNetworkSuppressTest) {
  EXPECT_CALL(*mock_managed_network_configuration_handler(),
              GetAllowTextMessages)
      .WillOnce(::testing::Return(PolicyTextMessageSuppressionState::kUnset));
  SimulatePolicyChanged();

  // User suppression state is Allow for |kTestGUID1| and Suppress for
  // |kTestGUID2|.
  fake_network_metadata_store()->SetUserTextMessageSuppressionState(
      kTestGUID1, UserTextMessageSuppressionState::kAllow);
  fake_network_metadata_store()->SetUserTextMessageSuppressionState(
      kTestGUID2, UserTextMessageSuppressionState::kSuppress);

  TextMessageData message_data{kNumber, kText1, kTimestamp};
  SimulateMessageReceived(kTestGUID2, message_data);
  AssertTestObserverValue(/*expected_count=*/0, std::nullopt);
  SimulateMessageReceived(kTestGUID1, message_data);
  AssertTestObserverValue(/*expected_count=*/1, std::move(message_data));

  // User suppression state is Suppress for |kTestGUID1| and Allow for
  // |kTestGUID2|.
  fake_network_metadata_store()->SetUserTextMessageSuppressionState(
      kTestGUID1, UserTextMessageSuppressionState::kSuppress);
  fake_network_metadata_store()->SetUserTextMessageSuppressionState(
      kTestGUID2, UserTextMessageSuppressionState::kAllow);

  TextMessageData message_data_2{kNumber, kText2, kTimestamp};
  SimulateMessageReceived(kTestGUID1, message_data_2);
  AssertTestObserverValue(/*expected_count=*/1, std::nullopt);
  SimulateMessageReceived(kTestGUID2, message_data_2);
  AssertTestObserverValue(/*expected_count=*/2, std::move(message_data_2));

  AssertPolicyBuckets(/*allow_count=*/0u, /*suppress_count=*/0u,
                      /*unset_count=*/1u);
}

TEST_F(TextMessageProviderTest, PolicyChangedMetricsTest) {
  EXPECT_CALL(*mock_managed_network_configuration_handler(),
              GetAllowTextMessages)
      .WillOnce(::testing::Return(PolicyTextMessageSuppressionState::kUnset));
  SimulatePolicyChanged();
  AssertPolicyBuckets(/*allow_count=*/0u, /*suppress_count=*/0u,
                      /*unset_count=*/1u);

  EXPECT_CALL(*mock_managed_network_configuration_handler(),
              GetAllowTextMessages)
      .WillOnce(::testing::Return(PolicyTextMessageSuppressionState::kAllow));
  SimulatePolicyChanged();
  AssertPolicyBuckets(/*allow_count=*/1u, /*suppress_count=*/0u,
                      /*unset_count=*/1u);

  EXPECT_CALL(*mock_managed_network_configuration_handler(),
              GetAllowTextMessages)
      .WillOnce(
          ::testing::Return(PolicyTextMessageSuppressionState::kSuppress));
  SimulatePolicyChanged();
  AssertPolicyBuckets(/*allow_count=*/1u, /*suppress_count=*/1u,
                      /*unset_count=*/1u);

  // Confirm that a second update with the same value isn't logged
  EXPECT_CALL(*mock_managed_network_configuration_handler(),
              GetAllowTextMessages)
      .WillOnce(
          ::testing::Return(PolicyTextMessageSuppressionState::kSuppress));
  SimulatePolicyChanged();
  AssertPolicyBuckets(/*allow_count=*/1u, /*suppress_count=*/1u,
                      /*unset_count=*/1u);
}

TEST_F(TextMessageProviderTest, EmptyGuidTest) {
  EXPECT_CALL(*mock_managed_network_configuration_handler(),
              GetAllowTextMessages)
      .WillOnce(
          ::testing::Return(PolicyTextMessageSuppressionState::kSuppress));

  // Policy suppression doesn't rely on GUID and should block the text message.
  SimulatePolicyChanged();
  SimulateMessageReceived(kEmptyGUID, {kNumber, kText1, kTimestamp});
  AssertTestObserverValue(/*expected_count=*/0,
                          /*expected_message=*/std::nullopt);

  EXPECT_CALL(*mock_managed_network_configuration_handler(),
              GetAllowTextMessages)
      .WillOnce(::testing::Return(PolicyTextMessageSuppressionState::kAllow));

  // When allowed by policy the message is received.
  SimulatePolicyChanged();
  SimulateMessageReceived(kEmptyGUID, {kNumber, kText1, kTimestamp});
  AssertTestObserverValue(/*expected_count=*/1,
                          TextMessageData{kNumber, kText1, kTimestamp});

  EXPECT_CALL(*mock_managed_network_configuration_handler(),
              GetAllowTextMessages)
      .WillOnce(::testing::Return(PolicyTextMessageSuppressionState::kUnset));

  // When the policy is unset, regardless of the user suppression state, the
  // message will be received.
  SimulatePolicyChanged();
  fake_network_metadata_store()->SetUserTextMessageSuppressionState(
      kEmptyGUID, UserTextMessageSuppressionState::kSuppress);
  SimulateMessageReceived(kEmptyGUID, {kNumber, kText1, kTimestamp});
  AssertTestObserverValue(/*expected_count=*/2,
                          TextMessageData{kNumber, kText1, kTimestamp});
  fake_network_metadata_store()->SetUserTextMessageSuppressionState(
      kEmptyGUID, UserTextMessageSuppressionState::kAllow);

  SimulateMessageReceived(kEmptyGUID, {kNumber, kText1, kTimestamp});
  AssertTestObserverValue(/*expected_count=*/3,
                          TextMessageData{kNumber, kText1, kTimestamp});
}

}  // namespace ash
