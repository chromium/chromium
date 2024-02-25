// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/sync_wifi/pending_network_configuration_tracker_impl.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "chromeos/ash/components/sync_wifi/test_data_generator.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash::sync_wifi {

namespace {

const char kFredSsid[] = "Fred";
const char kMangoSsid[] = "Mango";

const char kPendingNetworkConfigurationsPref[] =
    "sync_wifi.pending_network_configuration_updates";
const char kChangeGuidKey[] = "ChangeGuid";

}  // namespace

class PendingNetworkConfigurationTrackerImplTest : public testing::Test {
 public:
  PendingNetworkConfigurationTrackerImplTest() = default;

  PendingNetworkConfigurationTrackerImplTest(
      const PendingNetworkConfigurationTrackerImplTest&) = delete;
  PendingNetworkConfigurationTrackerImplTest& operator=(
      const PendingNetworkConfigurationTrackerImplTest&) = delete;

  void SetUp() override {
    test_pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();

    PendingNetworkConfigurationTrackerImpl::RegisterProfilePrefs(
        test_pref_service_->registry());
    tracker_ = std::make_unique<PendingNetworkConfigurationTrackerImpl>(
        test_pref_service_.get());
  }

  PendingNetworkConfigurationTrackerImpl* tracker() { return tracker_.get(); }

  const base::Value::Dict* GetPref() const {
    return &test_pref_service_.get()
                ->GetUserPref(kPendingNetworkConfigurationsPref)
                ->GetDict();
  }

  bool DoesPrefContainPendingUpdate(const NetworkIdentifier& id,
                                    const std::string& update_guid) const {
    const base::Value::Dict* dict = GetPref()->FindDict(id.SerializeToString());
    if (!dict) {
      return false;
    }

    const std::string* found_guid = dict->FindString(kChangeGuidKey);
    return found_guid && *found_guid == update_guid;
  }

  void AssertTrackerHasMatchingUpdate(
      const std::string& update_guid,
      const NetworkIdentifier& id,
      int completed_attempts = 0,
      const std::optional<sync_pb::WifiConfigurationSpecifics> specifics =
          std::nullopt) {
    std::optional<PendingNetworkConfigurationUpdate> update =
        tracker()->GetPendingUpdate(update_guid, id);
    ASSERT_TRUE(update);
    ASSERT_EQ(id, update->id());
    ASSERT_EQ(completed_attempts, update->completed_attempts());
    std::string serialized_specifics_wants;
    std::string serialized_specifics_has;
    if (specifics)
      specifics->SerializeToString(&serialized_specifics_wants);
    if (update->specifics())
      update->specifics()->SerializeToString(&serialized_specifics_has);
    ASSERT_EQ(serialized_specifics_wants, serialized_specifics_has);
  }

  const NetworkIdentifier& fred_network_id() const { return fred_network_id_; }

  const NetworkIdentifier& mango_network_id() const {
    return mango_network_id_;
  }

 private:
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  std::unique_ptr<PendingNetworkConfigurationTrackerImpl> tracker_;

  const NetworkIdentifier fred_network_id_ = GeneratePskNetworkId(kFredSsid);
  const NetworkIdentifier mango_network_id_ = GeneratePskNetworkId(kMangoSsid);
};

TEST_F(PendingNetworkConfigurationTrackerImplTest, TestMarkComplete) {
  std::string change_guid = tracker()->TrackPendingUpdate(
      fred_network_id(), /*specifics=*/std::nullopt);
  AssertTrackerHasMatchingUpdate(change_guid, fred_network_id());
  EXPECT_EQ(1u, GetPref()->size());
  EXPECT_TRUE(DoesPrefContainPendingUpdate(fred_network_id(), change_guid));
  tracker()->MarkComplete(change_guid, fred_network_id());
  EXPECT_FALSE(tracker()->GetPendingUpdate(change_guid, fred_network_id()));
  EXPECT_EQ(0u, GetPref()->size());
}

TEST_F(PendingNetworkConfigurationTrackerImplTest, TestTwoChangesSameNetwork) {
  std::string change_guid =
      tracker()->TrackPendingUpdate(fred_network_id(),
                                    /*specifics=*/std::nullopt);
  tracker()->IncrementCompletedAttempts(change_guid, fred_network_id());
  AssertTrackerHasMatchingUpdate(change_guid, fred_network_id(),
                                 /*completed_attempts=*/1);
  EXPECT_EQ(1u, GetPref()->size());
  EXPECT_EQ(1, tracker()
                   ->GetPendingUpdate(change_guid, fred_network_id())
                   ->completed_attempts());

  std::string second_change_guid =
      tracker()->TrackPendingUpdate(fred_network_id(),
                                    /*specifics=*/std::nullopt);
  EXPECT_FALSE(tracker()->GetPendingUpdate(change_guid, fred_network_id()));
  AssertTrackerHasMatchingUpdate(second_change_guid, fred_network_id());
  EXPECT_EQ(0, tracker()
                   ->GetPendingUpdate(second_change_guid, fred_network_id())
                   ->completed_attempts());
  EXPECT_EQ(1u, GetPref()->size());
}

TEST_F(PendingNetworkConfigurationTrackerImplTest,
       TestTwoChangesDifferentNetworks) {
  std::string change_guid =
      tracker()->TrackPendingUpdate(fred_network_id(),
                                    /*specifics=*/std::nullopt);
  AssertTrackerHasMatchingUpdate(change_guid, fred_network_id());
  EXPECT_TRUE(DoesPrefContainPendingUpdate(fred_network_id(), change_guid));
  EXPECT_EQ(1u, GetPref()->size());
  std::string second_change_guid =
      tracker()->TrackPendingUpdate(mango_network_id(),
                                    /*specifics=*/std::nullopt);
  AssertTrackerHasMatchingUpdate(change_guid, fred_network_id());
  AssertTrackerHasMatchingUpdate(second_change_guid, mango_network_id());
  EXPECT_TRUE(DoesPrefContainPendingUpdate(fred_network_id(), change_guid));
  EXPECT_TRUE(
      DoesPrefContainPendingUpdate(mango_network_id(), second_change_guid));
  EXPECT_EQ(2u, GetPref()->size());
}

TEST_F(PendingNetworkConfigurationTrackerImplTest, TestGetPendingUpdates) {
  std::string change_guid =
      tracker()->TrackPendingUpdate(fred_network_id(),
                                    /*specifics=*/std::nullopt);
  std::string second_change_guid =
      tracker()->TrackPendingUpdate(mango_network_id(),
                                    /*specifics=*/std::nullopt);
  std::vector<PendingNetworkConfigurationUpdate> list =
      tracker()->GetPendingUpdates();
  EXPECT_EQ(2u, list.size());
  EXPECT_EQ(change_guid, list[0].change_guid());
  EXPECT_EQ(fred_network_id(), list[0].id());
  EXPECT_EQ(second_change_guid, list[1].change_guid());
  EXPECT_EQ(mango_network_id(), list[1].id());

  tracker()->MarkComplete(change_guid, fred_network_id());
  list = tracker()->GetPendingUpdates();
  EXPECT_EQ(1u, list.size());
  EXPECT_EQ(second_change_guid, list[0].change_guid());
  EXPECT_EQ(mango_network_id(), list[0].id());
}

TEST_F(PendingNetworkConfigurationTrackerImplTest, TestGetPendingUpdate) {
  sync_pb::WifiConfigurationSpecifics specifics =
      GenerateTestWifiSpecifics(fred_network_id());
  std::string change_guid =
      tracker()->TrackPendingUpdate(fred_network_id(), specifics);

  AssertTrackerHasMatchingUpdate(change_guid, fred_network_id(),
                                 /*completed_attempts=*/0, specifics);
}

TEST_F(PendingNetworkConfigurationTrackerImplTest, TestRetryCounting) {
  std::string change_guid =
      tracker()->TrackPendingUpdate(fred_network_id(),
                                    /*specifics=*/std::nullopt);
  AssertTrackerHasMatchingUpdate(change_guid, fred_network_id());
  EXPECT_EQ(1u, GetPref()->size());
  EXPECT_EQ(0, tracker()
                   ->GetPendingUpdate(change_guid, fred_network_id())
                   ->completed_attempts());
  tracker()->IncrementCompletedAttempts(change_guid, fred_network_id());
  tracker()->IncrementCompletedAttempts(change_guid, fred_network_id());
  tracker()->IncrementCompletedAttempts(change_guid, fred_network_id());
  EXPECT_EQ(3, tracker()
                   ->GetPendingUpdate(change_guid, fred_network_id())
                   ->completed_attempts());
  tracker()->IncrementCompletedAttempts(change_guid, fred_network_id());
  tracker()->IncrementCompletedAttempts(change_guid, fred_network_id());
  EXPECT_EQ(5, tracker()
                   ->GetPendingUpdate(change_guid, fred_network_id())
                   ->completed_attempts());
}

}  // namespace ash::sync_wifi
