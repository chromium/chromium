// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/push_notification_manager.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::SaveArg;

namespace optimization_guide {

class TestDelegate : public PushNotificationManager::Delegate {
 public:
  using RemoveMultiplePair =
      std::pair<proto::KeyRepresentation, base::flat_set<std::string>>;

  TestDelegate() = default;
  ~TestDelegate() = default;

  const std::vector<RemoveMultiplePair>& removed_entries() const {
    return removed_entries_;
  }

  void RemoveFetchedEntriesByHintKeys(
      base::OnceClosure on_success,
      proto::KeyRepresentation key_representation,
      const base::flat_set<std::string>& hint_keys) override {
    removed_entries_.emplace_back(key_representation, hint_keys);
    std::move(on_success).Run();
  }

 private:
  std::vector<RemoveMultiplePair> removed_entries_;
};

class MockObserver : public PushNotificationManager::Observer {
 public:
  MockObserver() = default;
  ~MockObserver() override = default;
  MOCK_METHOD(void,
              OnNotificationPayload,
              (proto::OptimizationType, const optimization_guide::proto::Any&),
              (override));
};

class AnotherMockObserver : public PushNotificationManager::Observer {
 public:
  AnotherMockObserver() = default;
  ~AnotherMockObserver() override = default;

  MOCK_METHOD(void,
              OnNotificationPayload,
              (proto::OptimizationType, const optimization_guide::proto::Any&),
              (override));
};

class PushNotificationManagerUnitTest : public testing::Test {
 public:
  PushNotificationManagerUnitTest() = default;
  ~PushNotificationManagerUnitTest() override = default;

  void SetUp() override {
    manager_.SetDelegate(&delegate_);
    manager_.AddObserver(&observer_);
  }

  MockObserver* observer() { return &observer_; }

  PushNotificationManager* manager() { return &manager_; }

  TestDelegate* delegate() { return &delegate_; }

  bool manager_has_observer(PushNotificationManager::Observer* observer) {
    return manager()->observers_.HasObserver(observer);
  }

 private:
  MockObserver observer_;
  PushNotificationManager manager_;
  TestDelegate delegate_;
};

TEST_F(PushNotificationManagerUnitTest, TestNewNotificationReceived) {
  base::HistogramTester histogram_tester;

  proto::HintNotificationPayload notification;
  notification.set_hint_key("hintkey");
  notification.set_key_representation(proto::KeyRepresentation::HOST);
  notification.set_optimization_type(
      proto::OptimizationType::PERFORMANCE_HINTS);
  optimization_guide::proto::Any* payload = notification.mutable_payload();
  payload->set_type_url("type_url");
  payload->set_value("value");

  optimization_guide::proto::Any payload_to_observer;
  EXPECT_CALL(*observer(), OnNotificationPayload(
                               proto::OptimizationType::PERFORMANCE_HINTS, _))
      .WillOnce(SaveArg<1>(&payload_to_observer));

  manager()->OnNewPushNotification(notification);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PushNotifications.ReceivedNotificationType",
      proto::OptimizationType::PERFORMANCE_HINTS, 1);
  EXPECT_EQ(payload_to_observer.type_url(), "type_url");
  EXPECT_EQ(payload_to_observer.value(), "value");
  std::vector<TestDelegate::RemoveMultiplePair> removed_entries =
      delegate()->removed_entries();
  EXPECT_EQ(1U, removed_entries.size());
  EXPECT_EQ(proto::KeyRepresentation::HOST, removed_entries[0].first);
  EXPECT_EQ(base::flat_set<std::string>{"hintkey"}, removed_entries[0].second);
}

TEST_F(PushNotificationManagerUnitTest, TestNewNotificationReceivedNoPayload) {
  base::HistogramTester histogram_tester;

  proto::HintNotificationPayload notification;
  notification.set_hint_key("hintkey");
  notification.set_key_representation(proto::KeyRepresentation::HOST);
  notification.set_optimization_type(
      proto::OptimizationType::PERFORMANCE_HINTS);
  EXPECT_CALL(*observer(), OnNotificationPayload(_, _)).Times(0);

  manager()->OnNewPushNotification(notification);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PushNotifications.ReceivedNotificationType",
      proto::OptimizationType::PERFORMANCE_HINTS, 0);
  EXPECT_EQ(0U, delegate()->removed_entries().size());
}

TEST_F(PushNotificationManagerUnitTest, TestAddRemoveObserver) {
  AnotherMockObserver another_mock_observer;
  manager()->AddObserver(&another_mock_observer);
  EXPECT_TRUE(manager_has_observer(&another_mock_observer));
  manager()->RemoveObserver(&another_mock_observer);
  EXPECT_FALSE(manager_has_observer(&another_mock_observer));
}

}  // namespace optimization_guide
