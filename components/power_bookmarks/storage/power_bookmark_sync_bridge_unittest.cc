// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_bookmarks/storage/power_bookmark_sync_bridge.h"

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "components/power_bookmarks/common/power.h"
#include "components/power_bookmarks/common/power_test_util.h"
#include "components/power_bookmarks/storage/power_bookmark_backend.h"
#include "components/power_bookmarks/storage/power_bookmark_sync_metadata_database.h"
#include "components/sync/model/sync_metadata_store.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;
using testing::SizeIs;

namespace power_bookmarks {

MATCHER_P(MatchesPowerByGUID, expected_guid, "") {
  return arg.guid() == expected_guid;
}

class MockTransaction : public Transaction {
  bool Commit() override { return true; }
};

class MockDelegate : public PowerBookmarkSyncBridge::Delegate {
 public:
  void CreatePower(std::unique_ptr<Power> power) {
    power_map_[power->guid_string()] = std::move(power);
  }

  std::vector<std::unique_ptr<Power>> GetAllPowers() override {
    std::vector<std::unique_ptr<Power>> powers;
    for (auto const& pair : power_map_) {
      powers.push_back(pair.second->Clone());
    }
    return powers;
  }

  std::vector<std::unique_ptr<Power>> GetPowersForGUIDs(
      const std::vector<std::string>& guids) override {
    std::vector<std::unique_ptr<Power>> powers;
    return powers;
  }

  std::unique_ptr<Power> GetPowerForGUID(const std::string& guid) override {
    if (power_map_.count(guid)) {
      return power_map_[guid]->Clone();
    }
    return nullptr;
  }

  std::unique_ptr<Transaction> BeginTransaction() override {
    return std::make_unique<MockTransaction>();
  }

  MOCK_METHOD1(CreateOrMergePowerFromSync, bool(const Power& power));
  MOCK_METHOD1(DeletePowerFromSync, bool(const std::string&));
  MOCK_METHOD0(GetSyncMetadataDatabase, PowerBookmarkSyncMetadataDatabase*());
  MOCK_METHOD0(NotifyPowersChanged, void());

 private:
  std::map<std::string, std::unique_ptr<Power>> power_map_;
};

class PowerBookmarkSyncBridgeTest : public ::testing::Test {
 public:
  PowerBookmarkSyncBridgeTest() = default;
  ~PowerBookmarkSyncBridgeTest() override = default;

 protected:
  void SetUp() override {
    EXPECT_TRUE(temp_directory_.CreateUniqueTempDir());
    ON_CALL(processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
    bridge_ = std::make_unique<PowerBookmarkSyncBridge>(
        delegate_.GetSyncMetadataDatabase(), &delegate_,
        processor_.CreateForwardingProcessor());
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> processor_;
  testing::NiceMock<MockDelegate> delegate_;

  base::ScopedTempDir temp_directory_;

  std::unique_ptr<PowerBookmarkSyncBridge> bridge_;
};

TEST_F(PowerBookmarkSyncBridgeTest, MergeFullSyncDataAdd) {
  auto power = MakePower(GURL("https://google.com"),
                         sync_pb::PowerBookmarkSpecifics::PowerType::
                             PowerBookmarkSpecifics_PowerType_POWER_TYPE_MOCK);
  auto guid = power->guid_string();
  delegate_.CreatePower(std::move(power));

  auto power2 = MakePower(GURL("https://google.com"),
                          sync_pb::PowerBookmarkSpecifics::PowerType::
                              PowerBookmarkSpecifics_PowerType_POWER_TYPE_MOCK);
  syncer::EntityChangeList entity_changes;
  syncer::EntityData data;
  power2->ToPowerBookmarkSpecifics(data.specifics.mutable_power_bookmark());
  entity_changes.push_back(
      syncer::EntityChange::CreateAdd(power2->guid_string(), std::move(data)));

  EXPECT_CALL(delegate_,
              CreateOrMergePowerFromSync(MatchesPowerByGUID(power2->guid())))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(processor_, Put(guid, _, _));
  EXPECT_CALL(delegate_, NotifyPowersChanged());

  ASSERT_FALSE(bridge_
                   ->MergeFullSyncData(bridge_->CreateMetadataChangeList(),
                                       std::move(entity_changes))
                   .has_value());
}

TEST_F(PowerBookmarkSyncBridgeTest, ApplyIncrementalSyncChangesAdd) {
  delegate_.CreatePower(
      MakePower(GURL("https://google.com"),
                sync_pb::PowerBookmarkSpecifics::PowerType::
                    PowerBookmarkSpecifics_PowerType_POWER_TYPE_MOCK));

  auto power2 = MakePower(GURL("https://google.com"),
                          sync_pb::PowerBookmarkSpecifics::PowerType::
                              PowerBookmarkSpecifics_PowerType_POWER_TYPE_MOCK);
  syncer::EntityChangeList entity_changes;
  syncer::EntityData data;
  power2->ToPowerBookmarkSpecifics(data.specifics.mutable_power_bookmark());
  entity_changes.push_back(
      syncer::EntityChange::CreateAdd(power2->guid_string(), std::move(data)));

  EXPECT_CALL(delegate_,
              CreateOrMergePowerFromSync(MatchesPowerByGUID(power2->guid())))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(delegate_, NotifyPowersChanged());

  ASSERT_FALSE(
      bridge_
          ->ApplyIncrementalSyncChanges(bridge_->CreateMetadataChangeList(),
                                        std::move(entity_changes))
          .has_value());
}

TEST_F(PowerBookmarkSyncBridgeTest, ApplyIncrementalSyncChangesUpdate) {
  auto power1 = MakePower(GURL("https://google.com"),
                          sync_pb::PowerBookmarkSpecifics::PowerType::
                              PowerBookmarkSpecifics_PowerType_POWER_TYPE_MOCK);
  auto guid = power1->guid();
  auto power2 = power1->Clone();
  delegate_.CreatePower(std::move(power1));

  syncer::EntityChangeList entity_changes;
  syncer::EntityData data;
  power2->ToPowerBookmarkSpecifics(data.specifics.mutable_power_bookmark());
  entity_changes.push_back(
      syncer::EntityChange::CreateAdd(power2->guid_string(), std::move(data)));

  EXPECT_CALL(delegate_, CreateOrMergePowerFromSync(MatchesPowerByGUID(guid)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(delegate_, NotifyPowersChanged());

  ASSERT_FALSE(
      bridge_
          ->ApplyIncrementalSyncChanges(bridge_->CreateMetadataChangeList(),
                                        std::move(entity_changes))
          .has_value());
}

TEST_F(PowerBookmarkSyncBridgeTest, ApplyIncrementalSyncChangesDelete) {
  auto power1 = MakePower(GURL("https://google.com"),
                          sync_pb::PowerBookmarkSpecifics::PowerType::
                              PowerBookmarkSpecifics_PowerType_POWER_TYPE_MOCK);
  auto guid = power1->guid_string();
  delegate_.CreatePower(std::move(power1));

  EXPECT_CALL(delegate_, DeletePowerFromSync(guid))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(delegate_, NotifyPowersChanged());

  syncer::EntityChangeList entity_changes;
  syncer::EntityData data;
  entity_changes.push_back(syncer::EntityChange::CreateDelete(guid));

  ASSERT_FALSE(
      bridge_
          ->ApplyIncrementalSyncChanges(bridge_->CreateMetadataChangeList(),
                                        std::move(entity_changes))
          .has_value());
}

TEST_F(PowerBookmarkSyncBridgeTest, ApplyIncrementalSyncChangesFail) {
  auto power = MakePower(GURL("https://google.com"),
                         sync_pb::PowerBookmarkSpecifics::PowerType::
                             PowerBookmarkSpecifics_PowerType_POWER_TYPE_MOCK);
  syncer::EntityChangeList entity_changes;
  syncer::EntityData data;
  power->ToPowerBookmarkSpecifics(data.specifics.mutable_power_bookmark());
  entity_changes.push_back(
      syncer::EntityChange::CreateAdd(power->guid_string(), std::move(data)));

  // When `CreateOrMergePowerFromSync` call fails, `ApplyIncrementalSyncChanges`
  // will return an error.
  EXPECT_CALL(delegate_,
              CreateOrMergePowerFromSync(MatchesPowerByGUID(power->guid())))
      .WillRepeatedly(Return(false));

  ASSERT_TRUE(
      bridge_
          ->ApplyIncrementalSyncChanges(bridge_->CreateMetadataChangeList(),
                                        std::move(entity_changes))
          .has_value());
}

}  // namespace power_bookmarks
