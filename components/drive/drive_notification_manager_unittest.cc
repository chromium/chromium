// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/strings/strcat.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/drive/drive_notification_manager.h"
#include "components/drive/drive_notification_observer.h"
#include "components/invalidation/impl/fake_invalidation_service.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "google/cacheinvalidation/types.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {

namespace {

const invalidation::ObjectId kDefaultCorpusObjectId(
    ipc::invalidation::ObjectSource::COSMO_CHANGELOG,
    "CHANGELOG");

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
  ~FakeDriveNotificationObserver() override {}

  // DriveNotificationObserver overrides
  void OnNotificationReceived(
      const std::map<std::string, int64_t>& ids) override {
    notification_ids_ = ids;
  }
  void OnNotificationTimerFired() override {}
  void OnPushNotificationEnabled(bool enabled) override {}

  const std::map<std::string, int64_t> GetNotificationIds() const {
    return notification_ids_;
  }

  void ClearNotificationIds() { notification_ids_.clear(); }

 private:
  std::map<std::string, int64_t> notification_ids_;
};

invalidation::ObjectId CreateTeamDriveInvalidationObjectId(
    const std::string& team_drive_id) {
  return invalidation::ObjectId(
      ipc::invalidation::ObjectSource::COSMO_CHANGELOG,
      base::StrCat({"TD:", team_drive_id}));
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
  auto registered_ids =
      fake_invalidation_service_->invalidator_registrar().GetAllRegisteredIds();

  syncer::ObjectIdSet expected_object_ids = {kDefaultCorpusObjectId};
  EXPECT_EQ(expected_object_ids, registered_ids);

  const std::string team_drive_id_1 = "td_id_1";
  const auto team_drive_1_object_id =
      CreateTeamDriveInvalidationObjectId(team_drive_id_1);

  // Add the team drive
  drive_notification_manager_->UpdateTeamDriveIds({team_drive_id_1}, {});
  registered_ids =
      fake_invalidation_service_->invalidator_registrar().GetAllRegisteredIds();

  expected_object_ids = {kDefaultCorpusObjectId, team_drive_1_object_id};
  EXPECT_EQ(expected_object_ids, registered_ids);

  // Remove the team drive.
  drive_notification_manager_->UpdateTeamDriveIds({}, {team_drive_id_1});
  registered_ids =
      fake_invalidation_service_->invalidator_registrar().GetAllRegisteredIds();

  expected_object_ids = {kDefaultCorpusObjectId};
  EXPECT_EQ(expected_object_ids, registered_ids);

  const std::string team_drive_id_2 = "td_id_2";
  const auto team_drive_2_object_id =
      CreateTeamDriveInvalidationObjectId(team_drive_id_2);

  // Add two team drives
  drive_notification_manager_->UpdateTeamDriveIds(
      {team_drive_id_1, team_drive_id_2}, {});
  registered_ids =
      fake_invalidation_service_->invalidator_registrar().GetAllRegisteredIds();

  expected_object_ids = {kDefaultCorpusObjectId, team_drive_1_object_id,
                         team_drive_2_object_id};
  EXPECT_EQ(expected_object_ids, registered_ids);

  drive_notification_manager_->UpdateTeamDriveIds({}, {team_drive_id_1});
  registered_ids =
      fake_invalidation_service_->invalidator_registrar().GetAllRegisteredIds();

  expected_object_ids = {kDefaultCorpusObjectId, team_drive_2_object_id};
  EXPECT_EQ(expected_object_ids, registered_ids);

  // Remove a team drive that doesn't exists with no changes.
  const std::string team_drive_id_3 = "td_id_3";
  drive_notification_manager_->UpdateTeamDriveIds({}, {team_drive_id_3});
  registered_ids =
      fake_invalidation_service_->invalidator_registrar().GetAllRegisteredIds();

  expected_object_ids = {kDefaultCorpusObjectId, team_drive_2_object_id};
  EXPECT_EQ(expected_object_ids, registered_ids);
}

TEST_F(DriveNotificationManagerTest, TestBatchInvalidation) {
  // By default we'll be registered for the default change notification.

  // Emitting an invalidation should not call our observer until the timer
  // expires.
  fake_invalidation_service_->EmitInvalidationForTest(
      syncer::Invalidation::InitUnknownVersion(kDefaultCorpusObjectId));
  EXPECT_TRUE(drive_notification_observer_->GetNotificationIds().empty());

  task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(5));

  // Default corpus is has the id "" when sent to observers.
  std::map<std::string, int64_t> expected_ids = {{"", -1}};
  EXPECT_EQ(expected_ids, drive_notification_observer_->GetNotificationIds());
  drive_notification_observer_->ClearNotificationIds();

  // Register a team drive for notifications
  const std::string team_drive_id_1 = "td_id_1";
  const auto team_drive_1_object_id =
      CreateTeamDriveInvalidationObjectId(team_drive_id_1);
  drive_notification_manager_->UpdateTeamDriveIds({team_drive_id_1}, {});

  // Emit invalidation for default corpus, should not emit a team drive
  // invalidation.
  fake_invalidation_service_->EmitInvalidationForTest(
      syncer::Invalidation::Init(kDefaultCorpusObjectId, 1, ""));
  EXPECT_TRUE(drive_notification_observer_->GetNotificationIds().empty());

  task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(5));

  // Default corpus is has the id "" when sent to observers.
  expected_ids = {{"", 1}};
  EXPECT_EQ(expected_ids, drive_notification_observer_->GetNotificationIds());
  drive_notification_observer_->ClearNotificationIds();

  // Emit team drive invalidation
  fake_invalidation_service_->EmitInvalidationForTest(
      syncer::Invalidation::Init(team_drive_1_object_id, 2, ""));
  EXPECT_TRUE(drive_notification_observer_->GetNotificationIds().empty());

  task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(5));
  expected_ids = {{team_drive_id_1, 2}};
  EXPECT_EQ(expected_ids, drive_notification_observer_->GetNotificationIds());
  drive_notification_observer_->ClearNotificationIds();

  // Emit both default corpus and team drive.
  fake_invalidation_service_->EmitInvalidationForTest(
      syncer::Invalidation::Init(kDefaultCorpusObjectId, 1, ""));
  fake_invalidation_service_->EmitInvalidationForTest(
      syncer::Invalidation::Init(team_drive_1_object_id, 2, ""));
  EXPECT_TRUE(drive_notification_observer_->GetNotificationIds().empty());

  task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(5));
  expected_ids = {{"", 1}, {team_drive_id_1, 2}};
  EXPECT_EQ(expected_ids, drive_notification_observer_->GetNotificationIds());
}

}  // namespace drive
