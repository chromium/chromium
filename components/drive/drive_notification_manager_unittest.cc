// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/drive_notification_manager.h"

#include <memory>

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/drive/drive_notification_observer.h"
#include "components/drive/features.h"
#include "components/invalidation/impl/fake_invalidation_service.h"
#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/invalidation_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {

namespace {

const invalidation::Topic kDefaultCorpusTopic = "Drive";

enum NotificationSource {
  kNotificationXMPP = 0,
  kNotificationPolling = 1,
  kMaxValue = kNotificationPolling
};

struct ShutdownHelper {
  template <typename T>
  void operator()(T* object) const {
    if (object) {
      object->Shutdown();
      delete object;
    }
  }
};

class FakeDriveNotificationObserver : public DriveNotificationObserver {
 public:
  ~FakeDriveNotificationObserver() override = default;

  // DriveNotificationObserver overrides
  void OnNotificationReceived(
      const std::map<std::string, int64_t>& ids) override {
    notification_ids_ = ids;
  }
  void OnNotificationTimerFired() override { timer_fired_count_++; }
  void OnPushNotificationEnabled(bool enabled) override {}

  const std::map<std::string, int64_t> GetNotificationIds() const {
    return notification_ids_;
  }
  int GetNotificationTimerCount() const { return timer_fired_count_; }

  void ClearNotificationIds() { notification_ids_.clear(); }
  void ClearNotificationTimerCount() { timer_fired_count_ = 0; }

 private:
  std::map<std::string, int64_t> notification_ids_;
  int timer_fired_count_ = 0;
};

invalidation::Topic CreateTeamDriveInvalidationTopic(
    const std::string& team_drive_id) {
  return base::StrCat({"team-drive-", team_drive_id});
}

}  // namespace

class DriveNotificationManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
        base::TestMockTimeTaskRunner::Type::kBoundToThread);
    fake_invalidation_service_ =
        std::make_unique<invalidation::FakeInvalidationService>();
    drive_notification_observer_ =
        std::make_unique<FakeDriveNotificationObserver>();

    // Can't use make_unique with a custom deleter.
    drive_notification_manager_.reset(
        new DriveNotificationManager(fake_invalidation_service_.get()));

    drive_notification_manager_->AddObserver(
        drive_notification_observer_.get());
  }

  void TearDown() override {
    drive_notification_manager_->RemoveObserver(
        drive_notification_observer_.get());
  }

  bool IsInvalidatorRegistered(const invalidation::Topic& topic) {
    return fake_invalidation_service_->invalidator_registrar()
        .GetRegisteredTopics(drive_notification_manager_.get())
        .contains(topic);
  }

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  std::unique_ptr<invalidation::FakeInvalidationService>
      fake_invalidation_service_;
  std::unique_ptr<FakeDriveNotificationObserver> drive_notification_observer_;
  std::unique_ptr<DriveNotificationManager, ShutdownHelper>
      drive_notification_manager_;
};

TEST_F(DriveNotificationManagerTest, RegisterTeamDrives) {
  // By default, we should have registered for default corpus notifications on
  // initialization.
  auto subscribed_topics = fake_invalidation_service_->invalidator_registrar()
                               .GetAllSubscribedTopics();

  // TODO(crbug.com/1029698): replace invalidation::TopicMap with
  // invalidation::TopicSet once |is_public| become the part of dedicated
  // invalidation::Topic struct. This should simplify this test.
  invalidation::TopicMap expected_topics;
  expected_topics.emplace(kDefaultCorpusTopic,
                          invalidation::TopicMetadata{/*is_public=*/false});
  EXPECT_EQ(expected_topics, subscribed_topics);

  const std::string team_drive_id_1 = "td_id_1";
  const auto team_drive_1_topic =
      CreateTeamDriveInvalidationTopic(team_drive_id_1);

  // Add the team drive
  drive_notification_manager_->UpdateTeamDriveIds({team_drive_id_1}, {});
  subscribed_topics = fake_invalidation_service_->invalidator_registrar()
                          .GetAllSubscribedTopics();

  expected_topics.emplace(team_drive_1_topic,
                          invalidation::TopicMetadata{/*is_public=*/true});
  EXPECT_EQ(expected_topics, subscribed_topics);

  // Remove the team drive.
  drive_notification_manager_->UpdateTeamDriveIds({}, {team_drive_id_1});
  subscribed_topics = fake_invalidation_service_->invalidator_registrar()
                          .GetAllSubscribedTopics();

  expected_topics.erase(team_drive_1_topic);
  EXPECT_EQ(expected_topics, subscribed_topics);

  const std::string team_drive_id_2 = "td_id_2";
  const auto team_drive_2_topic =
      CreateTeamDriveInvalidationTopic(team_drive_id_2);

  // Add two team drives.
  drive_notification_manager_->UpdateTeamDriveIds(
      {team_drive_id_1, team_drive_id_2}, {});
  subscribed_topics = fake_invalidation_service_->invalidator_registrar()
                          .GetAllSubscribedTopics();

  expected_topics.emplace(team_drive_1_topic,
                          invalidation::TopicMetadata{/*is_public=*/true});
  expected_topics.emplace(team_drive_2_topic,
                          invalidation::TopicMetadata{/*is_public=*/true});
  EXPECT_EQ(expected_topics, subscribed_topics);

  // Remove the first team drive.
  drive_notification_manager_->UpdateTeamDriveIds({}, {team_drive_id_1});
  subscribed_topics = fake_invalidation_service_->invalidator_registrar()
                          .GetAllSubscribedTopics();

  expected_topics.erase(team_drive_1_topic);
  EXPECT_EQ(expected_topics, subscribed_topics);

  // Remove a team drive that doesn't exists with no changes.
  const std::string team_drive_id_3 = "td_id_3";
  drive_notification_manager_->UpdateTeamDriveIds({}, {team_drive_id_3});
  subscribed_topics = fake_invalidation_service_->invalidator_registrar()
                          .GetAllSubscribedTopics();

  EXPECT_EQ(expected_topics, subscribed_topics);
}

TEST_F(DriveNotificationManagerTest, TestBatchInvalidation) {
  base::HistogramTester histogram_tester;

  // By default we'll be registered for the default change notification.
  EXPECT_TRUE(IsInvalidatorRegistered(kDefaultCorpusTopic));

  // Emitting an invalidation should not call our observer until the timer
  // expires.
  fake_invalidation_service_->EmitInvalidationForTest(
      invalidation::Invalidation(kDefaultCorpusTopic, 1, ""));
  EXPECT_TRUE(drive_notification_observer_->GetNotificationIds().empty());

  task_runner_->FastForwardBy(base::Seconds(30));

  // Default corpus is has the id "" when sent to observers.
  // DriveNotificationManager::NotifyObserversToUpdate increases the
  // invalidation version by 1 before sending it to observers.
  std::map<std::string, int64_t> expected_ids = {{"", 2}};
  EXPECT_EQ(expected_ids, drive_notification_observer_->GetNotificationIds());
  histogram_tester.ExpectUniqueSample(
      "Storage.SyncFileSystem.NotificationSource",
      NotificationSource::kNotificationXMPP, 1);
  drive_notification_observer_->ClearNotificationIds();

  // Register a team drive for notifications.
  const std::string team_drive_id_1 = "td_id_1";
  const auto team_drive_1_topic =
      CreateTeamDriveInvalidationTopic(team_drive_id_1);
  drive_notification_manager_->UpdateTeamDriveIds({team_drive_id_1}, {});
  EXPECT_TRUE(IsInvalidatorRegistered(team_drive_1_topic));

  // Emit invalidation for default corpus, should not emit a team drive
  // invalidation.
  fake_invalidation_service_->EmitInvalidationForTest(
      invalidation::Invalidation(kDefaultCorpusTopic, 1, ""));
  EXPECT_TRUE(drive_notification_observer_->GetNotificationIds().empty());

  task_runner_->FastForwardBy(base::Seconds(30));

  // Default corpus is has the id "" when sent to observers.
  expected_ids = {{"", 2}};
  EXPECT_EQ(expected_ids, drive_notification_observer_->GetNotificationIds());
  histogram_tester.ExpectUniqueSample(
      "Storage.SyncFileSystem.NotificationSource",
      NotificationSource::kNotificationXMPP, 2);
  drive_notification_observer_->ClearNotificationIds();

  // Emit team drive invalidation
  fake_invalidation_service_->EmitInvalidationForTest(
      invalidation::Invalidation(team_drive_1_topic, 2, ""));
  EXPECT_TRUE(drive_notification_observer_->GetNotificationIds().empty());

  task_runner_->FastForwardBy(base::Seconds(30));
  expected_ids = {{team_drive_id_1, 2}};
  EXPECT_EQ(expected_ids, drive_notification_observer_->GetNotificationIds());
  histogram_tester.ExpectUniqueSample(
      "Storage.SyncFileSystem.NotificationSource",
      NotificationSource::kNotificationXMPP, 3);
  drive_notification_observer_->ClearNotificationIds();

  // Emit both default corpus and team drive.
  fake_invalidation_service_->EmitInvalidationForTest(
      invalidation::Invalidation(kDefaultCorpusTopic, 1, ""));
  fake_invalidation_service_->EmitInvalidationForTest(
      invalidation::Invalidation(team_drive_1_topic, 2, ""));

  // Emit with an earlier version. This should be ignored.
  fake_invalidation_service_->EmitInvalidationForTest(
      invalidation::Invalidation(kDefaultCorpusTopic, 0, ""));

  EXPECT_TRUE(drive_notification_observer_->GetNotificationIds().empty());

  task_runner_->FastForwardBy(base::Seconds(30));
  expected_ids = {{"", 2}, {team_drive_id_1, 2}};
  EXPECT_EQ(expected_ids, drive_notification_observer_->GetNotificationIds());
  histogram_tester.ExpectUniqueSample(
      "Storage.SyncFileSystem.NotificationSource",
      NotificationSource::kNotificationXMPP, 4);
}

TEST_F(DriveNotificationManagerTest, UnregisterOnNoObservers) {
  // By default, we should have registered for default corpus notifications on
  // initialization.
  auto subscribed_topics = fake_invalidation_service_->invalidator_registrar()
                               .GetAllSubscribedTopics();

  // TODO(crbug.com/1029698): replace invalidation::TopicMap with
  // invalidation::TopicSet once |is_public| become the part of dedicated
  // invalidation::Topic struct.
  invalidation::TopicMap expected_topics;
  expected_topics.emplace(kDefaultCorpusTopic,
                          invalidation::TopicMetadata{/*is_public=*/false});
  EXPECT_EQ(expected_topics, subscribed_topics);

  // Stop observing drive notification manager.
  drive_notification_manager_->RemoveObserver(
      drive_notification_observer_.get());

  subscribed_topics = fake_invalidation_service_->invalidator_registrar()
                          .GetAllSubscribedTopics();
  EXPECT_EQ(invalidation::TopicMap(), subscribed_topics);

  // Start observing drive notification manager again. It should subscribe to
  // the previously subscried topics.
  drive_notification_manager_->AddObserver(drive_notification_observer_.get());
  subscribed_topics = fake_invalidation_service_->invalidator_registrar()
                          .GetAllSubscribedTopics();
  EXPECT_EQ(expected_topics, subscribed_topics);
}

class DriveNotificationManagerPollingTest
    : public DriveNotificationManagerTest {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kEnablePollingInterval, {{"PollingIntervalInSecs", "5"}});
    DriveNotificationManagerTest::SetUp();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DriveNotificationManagerPollingTest, OverridePollingInterval) {
  // By default we'll be registered for the default change notification.
  EXPECT_TRUE(IsInvalidatorRegistered(kDefaultCorpusTopic));
  EXPECT_EQ(drive_notification_observer_->GetNotificationTimerCount(), 0);

  // Should poll by timer with interval set by feature param + randomized noise.
  task_runner_->FastForwardBy(base::Seconds(20));
  EXPECT_GE(drive_notification_observer_->GetNotificationTimerCount(), 2);
  drive_notification_observer_->ClearNotificationTimerCount();

  // Should continuously poll by timer when there are no invalidations received.
  task_runner_->FastForwardBy(base::Seconds(70));
  EXPECT_GE(drive_notification_observer_->GetNotificationTimerCount(), 7);
}

}  // namespace drive
