// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_settings_service.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_store.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/managed_user_setting_specifics.pb.h"
#include "components/sync/test/fake_sync_change_processor.h"
#include "components/sync/test/sync_change_processor_wrapper_for_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {

const char kAtomicItemName[] = "X-Wombat";
const char kSettingsName[] = "TestingSetting";
const char kSettingsValue[] = "SettingsValue";
const char kSplitItemName[] = "X-SuperMoosePowers";

class SupervisedUserSettingsServiceTest : public ::testing::Test {
 protected:
  SupervisedUserSettingsServiceTest() = default;
  ~SupervisedUserSettingsServiceTest() override = default;

  std::unique_ptr<syncer::SyncChangeProcessor> CreateSyncProcessor() {
    sync_processor_ = std::make_unique<syncer::FakeSyncChangeProcessor>();
    return std::unique_ptr<syncer::SyncChangeProcessor>(
        new syncer::SyncChangeProcessorWrapperForTest(sync_processor_.get()));
  }

  void StartSyncing(const syncer::SyncDataList& initial_sync_data) {
    std::optional<syncer::ModelError> error =
        settings_service_.MergeDataAndStartSyncing(
            syncer::SUPERVISED_USER_SETTINGS, initial_sync_data,
            CreateSyncProcessor());
    EXPECT_FALSE(error.has_value());
  }

  void UploadSplitItem(const std::string& key, const std::string& value) {
    split_items_.Set(key, value);
    settings_service_.SaveItem(
        SupervisedUserSettingsService::MakeSplitSettingKey(kSplitItemName, key),
        base::Value(value));
  }

  void UploadAtomicItem(const std::string& value) {
    atomic_setting_value_ = base::Value(value);
    settings_service_.SaveItem(kAtomicItemName,
                               base::Value(value));
  }

  void VerifySyncDataItem(syncer::SyncData sync_data) {
    const sync_pb::ManagedUserSettingSpecifics& supervised_user_setting =
        sync_data.GetSpecifics().managed_user_setting();
    base::Value* expected_value = nullptr;
    if (supervised_user_setting.name() == kAtomicItemName) {
      expected_value = &atomic_setting_value_.value();
    } else {
      EXPECT_TRUE(base::StartsWith(supervised_user_setting.name(),
                                   std::string(kSplitItemName) + ':',
                                   base::CompareCase::SENSITIVE));
      std::string key =
          supervised_user_setting.name().substr(strlen(kSplitItemName) + 1);
      expected_value = split_items_.Find(key);
      EXPECT_TRUE(expected_value);
    }
    ASSERT_TRUE(expected_value);
    EXPECT_EQ(*expected_value,
              base::JSONReader::Read(supervised_user_setting.value()));
  }

  void OnNewSettingsAvailable(const base::Value::Dict& settings) {
    if (settings.empty()) {
      settings_.reset();
    } else {
      settings_ = settings.Clone();
    }
  }

  // Check that a single website approval has been added correctly.
  void CheckWebsiteApproval(
      syncer::SyncChange::SyncChangeType expected_sync_change_type,
      const std::string& expected_key) {
    // Check that we are uploading sync data.
    ASSERT_EQ(1u, sync_processor_->changes().size());
    syncer::SyncChange sync_change = sync_processor_->changes()[0];
    EXPECT_EQ(expected_sync_change_type, sync_change.change_type());
    EXPECT_EQ(
        sync_change.sync_data().GetSpecifics().managed_user_setting().name(),
        expected_key);
    EXPECT_EQ(std::optional<base::Value>(true),
              base::JSONReader::Read(sync_change.sync_data()
                                         .GetSpecifics()
                                         .managed_user_setting()
                                         .value()));

    // It should also show up in local Sync data.
    syncer::SyncDataList sync_data = settings_service_.GetAllSyncDataForTesting(
        syncer::SUPERVISED_USER_SETTINGS);
    for (const syncer::SyncData& sync_data_item : sync_data) {
      if (sync_data_item.GetSpecifics().managed_user_setting().name().compare(
              expected_key) == 0) {
        EXPECT_EQ(
            std::optional<base::Value>(true),
            base::JSONReader::Read(
                sync_data_item.GetSpecifics().managed_user_setting().value()));
        return;
      }
    }
    FAIL() << "Expected key not found in local sync data";
  }

  // testing::Test overrides:
  void SetUp() override {
    TestingPrefStore* pref_store = new TestingPrefStore;
    settings_service_.Init(pref_store);
    user_settings_subscription_ =
        settings_service_.SubscribeForSettingsChange(base::BindRepeating(
            &SupervisedUserSettingsServiceTest::OnNewSettingsAvailable,
            base::Unretained(this)));
    pref_store->SetInitializationCompleted();
    ASSERT_FALSE(settings_);
    settings_service_.SetActive(true);
    ASSERT_TRUE(settings_);
  }

  void TearDown() override { settings_service_.Shutdown(); }

  base::test::TaskEnvironment task_environment_;
  base::Value::Dict split_items_;
  std::optional<base::Value> atomic_setting_value_;
  SupervisedUserSettingsService settings_service_;
  std::optional<base::Value::Dict> settings_;
  base::CallbackListSubscription user_settings_subscription_;

  std::unique_ptr<syncer::FakeSyncChangeProcessor> sync_processor_;
};

TEST_F(SupervisedUserSettingsServiceTest, ProcessAtomicSetting) {
  StartSyncing(syncer::SyncDataList());
  ASSERT_TRUE(settings_);
  const base::Value* value = settings_->Find(kSettingsName);
  EXPECT_FALSE(value);

  settings_.reset();
  syncer::SyncData data =
      SupervisedUserSettingsService::CreateSyncDataForSetting(
          kSettingsName, base::Value(kSettingsValue));
  syncer::SyncChangeList change_list;
  change_list.push_back(
      syncer::SyncChange(FROM_HERE, syncer::SyncChange::ACTION_ADD, data));
  std::optional<syncer::ModelError> error =
      settings_service_.ProcessSyncChanges(FROM_HERE, change_list);
  EXPECT_FALSE(error.has_value()) << error.value().ToString();
  ASSERT_TRUE(settings_);

  value = settings_->Find(kSettingsName);
  ASSERT_TRUE(value);
  std::string string_value;
  EXPECT_TRUE(value->is_string());
  if (value->is_string()) {
    string_value = value->GetString();
  }
  EXPECT_EQ(kSettingsValue, string_value);
}

TEST_F(SupervisedUserSettingsServiceTest, ProcessSplitSetting) {
  StartSyncing(syncer::SyncDataList());
  ASSERT_TRUE(settings_);
  const base::Value* value = nullptr;
  value = settings_->Find(kSettingsName);
  EXPECT_FALSE(value);

  base::Value::Dict dict;
  dict.Set("foo", "bar");
  dict.Set("awesomesauce", true);
  dict.Set("eaudecologne", 4711);

  settings_.reset();
  syncer::SyncChangeList change_list;
  for (const auto item : dict) {
    syncer::SyncData data =
        SupervisedUserSettingsService::CreateSyncDataForSetting(
            SupervisedUserSettingsService::MakeSplitSettingKey(kSettingsName,
                                                               item.first),
            item.second);
    change_list.push_back(
        syncer::SyncChange(FROM_HERE, syncer::SyncChange::ACTION_ADD, data));
  }
  std::optional<syncer::ModelError> error =
      settings_service_.ProcessSyncChanges(FROM_HERE, change_list);
  EXPECT_FALSE(error.has_value()) << error.value().ToString();
  ASSERT_TRUE(settings_);
  const base::Value::Dict* settings_dict = settings_->FindDict(kSettingsName);
  ASSERT_TRUE(settings_dict);
  EXPECT_EQ(*settings_dict, dict);
}

TEST_F(SupervisedUserSettingsServiceTest, NotifyForWebsiteApprovals) {
  base::MockCallback<SupervisedUserSettingsService::WebsiteApprovalCallback>
      mock_callback;
  auto subscription =
      settings_service_.SubscribeForNewWebsiteApproval(mock_callback.Get());

  StartSyncing(syncer::SyncDataList());
  ASSERT_TRUE(settings_);
  settings_.reset();

  syncer::SyncData dataForAllowedHost =
      SupervisedUserSettingsService::CreateSyncDataForSetting(
          SupervisedUserSettingsService::MakeSplitSettingKey(
              supervised_user::kContentPackManualBehaviorHosts, "allowedhost"),
          base::Value(true));
  syncer::SyncData dataForBlockedHost =
      SupervisedUserSettingsService::CreateSyncDataForSetting(
          SupervisedUserSettingsService::MakeSplitSettingKey(
              supervised_user::kContentPackManualBehaviorHosts, "blockedhost"),
          base::Value(false));

  syncer::SyncChangeList change_list;
  change_list.push_back(syncer::SyncChange(
      FROM_HERE, syncer::SyncChange::ACTION_ADD, dataForAllowedHost));
  change_list.push_back(syncer::SyncChange(
      FROM_HERE, syncer::SyncChange::ACTION_ADD, dataForBlockedHost));
  // Expect subscribers to be notified for the newly allowed host and NOT the
  // newly blocked host.
  EXPECT_CALL(mock_callback, Run("allowedhost")).Times(1);
  EXPECT_CALL(mock_callback, Run("blockedhost")).Times(0);
  settings_service_.ProcessSyncChanges(FROM_HERE, change_list);

  change_list.clear();
  change_list.push_back(syncer::SyncChange(
      FROM_HERE, syncer::SyncChange::ACTION_DELETE, dataForAllowedHost));
  change_list.push_back(syncer::SyncChange(
      FROM_HERE, syncer::SyncChange::ACTION_DELETE, dataForBlockedHost));
  // Expect subscribers to be notified for the previously blocked host and NOT
  // the previously allowed host.
  EXPECT_CALL(mock_callback, Run("allowedhost")).Times(0);
  EXPECT_CALL(mock_callback, Run("blockedhost")).Times(1);
  settings_service_.ProcessSyncChanges(FROM_HERE, change_list);
}

TEST_F(SupervisedUserSettingsServiceTest, Merge) {
  StartSyncing(syncer::SyncDataList());
  EXPECT_TRUE(settings_service_
                  .GetAllSyncDataForTesting(syncer::SUPERVISED_USER_SETTINGS)
                  .empty());

  ASSERT_TRUE(settings_);
  EXPECT_FALSE(settings_->Find(kSettingsName));

  settings_.reset();

  {
    syncer::SyncDataList sync_data;
    // Adding 1 Atomic entry.
    sync_data.push_back(SupervisedUserSettingsService::CreateSyncDataForSetting(
        kSettingsName, base::Value(kSettingsValue)));
    // Adding 2 SplitSettings from dictionary.
    base::Value::Dict dict;
    dict.Set("foo", "bar");
    dict.Set("eaudecologne", 4711);
    for (const auto item : dict) {
      sync_data.push_back(
          SupervisedUserSettingsService::CreateSyncDataForSetting(
              SupervisedUserSettingsService::MakeSplitSettingKey(kSplitItemName,
                                                                 item.first),
              item.second));
    }
    StartSyncing(sync_data);
    EXPECT_EQ(3u,
              settings_service_
                  .GetAllSyncDataForTesting(syncer::SUPERVISED_USER_SETTINGS)
                  .size());
    settings_service_.StopSyncing(syncer::SUPERVISED_USER_SETTINGS);
  }

  {
    // Here we are carry over the preference state that was set earlier.
    syncer::SyncDataList sync_data;
    // Adding 1 atomic Item in the queue.
    UploadAtomicItem("hurdle");
    // Adding 2 split Item in the queue.
    UploadSplitItem("burp", "baz");
    UploadSplitItem("item", "second");

    base::Value::Dict dict;
    dict.Set("foo", "burp");
    dict.Set("item", "first");
    // Adding 2 SplitSettings from dictionary.
    for (const auto item : dict) {
      sync_data.push_back(
          SupervisedUserSettingsService::CreateSyncDataForSetting(
              SupervisedUserSettingsService::MakeSplitSettingKey(kSplitItemName,
                                                                 item.first),
              item.second));
    }
    StartSyncing(sync_data);
    EXPECT_EQ(4u,
              settings_service_
                  .GetAllSyncDataForTesting(syncer::SUPERVISED_USER_SETTINGS)
                  .size());
  }
}

TEST_F(SupervisedUserSettingsServiceTest, SetLocalSetting) {
  const base::Value* value = nullptr;
  value = settings_->Find(kSettingsName);
  EXPECT_FALSE(value);

  settings_.reset();
  settings_service_.SetLocalSetting(kSettingsName, base::Value(kSettingsValue));
  ASSERT_TRUE(settings_);
  value = settings_->Find(kSettingsName);
  ASSERT_TRUE(value);
  std::string string_value;
  EXPECT_TRUE(value->is_string());
  if (value->is_string()) {
    string_value = value->GetString();
  }
  EXPECT_EQ(kSettingsValue, string_value);
}

TEST_F(SupervisedUserSettingsServiceTest, UploadItem) {
  UploadSplitItem("foo", "bar");
  UploadSplitItem("blurp", "baz");
  UploadAtomicItem("hurdle");

  // Uploading should produce changes when we start syncing.
  StartSyncing(syncer::SyncDataList());
  ASSERT_EQ(3u, sync_processor_->changes().size());
  for (const syncer::SyncChange& sync_change : sync_processor_->changes()) {
    EXPECT_EQ(syncer::SyncChange::ACTION_ADD, sync_change.change_type());
    VerifySyncDataItem(sync_change.sync_data());
  }

  // It should also show up in local Sync data.
  syncer::SyncDataList sync_data = settings_service_.GetAllSyncDataForTesting(
      syncer::SUPERVISED_USER_SETTINGS);
  EXPECT_EQ(3u, sync_data.size());
  for (const syncer::SyncData& sync_data_item : sync_data) {
    VerifySyncDataItem(sync_data_item);
  }

  // Uploading after we have started syncing should work too.
  sync_processor_->changes().clear();
  UploadSplitItem("froodle", "narf");
  ASSERT_EQ(1u, sync_processor_->changes().size());
  syncer::SyncChange change = sync_processor_->changes()[0];
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD, change.change_type());
  VerifySyncDataItem(change.sync_data());

  sync_data = settings_service_.GetAllSyncDataForTesting(
      syncer::SUPERVISED_USER_SETTINGS);
  EXPECT_EQ(4u, sync_data.size());
  for (const syncer::SyncData& sync_data_item : sync_data) {
    VerifySyncDataItem(sync_data_item);
  }

  // Uploading an item with a previously seen key should create an UPDATE
  // action.
  sync_processor_->changes().clear();
  UploadSplitItem("blurp", "snarl");
  ASSERT_EQ(1u, sync_processor_->changes().size());
  change = sync_processor_->changes()[0];
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, change.change_type());
  VerifySyncDataItem(change.sync_data());

  sync_data = settings_service_.GetAllSyncDataForTesting(
      syncer::SUPERVISED_USER_SETTINGS);
  EXPECT_EQ(4u, sync_data.size());
  for (const syncer::SyncData& sync_data_item : sync_data) {
    VerifySyncDataItem(sync_data_item);
  }

  sync_processor_->changes().clear();
  UploadAtomicItem("fjord");
  ASSERT_EQ(1u, sync_processor_->changes().size());
  change = sync_processor_->changes()[0];
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, change.change_type());
  VerifySyncDataItem(change.sync_data());

  sync_data = settings_service_.GetAllSyncDataForTesting(
      syncer::SUPERVISED_USER_SETTINGS);
  EXPECT_EQ(4u, sync_data.size());
  for (const syncer::SyncData& sync_data_item : sync_data) {
    VerifySyncDataItem(sync_data_item);
  }

  // The uploaded items should not show up as settings.
  EXPECT_FALSE(settings_->Find(kAtomicItemName));
  EXPECT_FALSE(settings_->Find(kSplitItemName));

  // Restarting sync should not create any new changes.
  settings_service_.StopSyncing(syncer::SUPERVISED_USER_SETTINGS);
  StartSyncing(sync_data);
  ASSERT_EQ(0u, sync_processor_->changes().size());
}

TEST_F(SupervisedUserSettingsServiceTest, RecordLocalWebsiteApproval) {
  // Record a website approval before sync is enabled.
  settings_service_.RecordLocalWebsiteApproval("youtube.com");

  // Uploading should produce changes when we start syncing.
  StartSyncing(syncer::SyncDataList());
  CheckWebsiteApproval(syncer::SyncChange::ACTION_ADD,
                       "ContentPackManualBehaviorHosts:youtube.com");

  // Uploading after we have started syncing should work too.
  sync_processor_->changes().clear();
  settings_service_.RecordLocalWebsiteApproval("photos.google.com");
  CheckWebsiteApproval(syncer::SyncChange::ACTION_ADD,
                       "ContentPackManualBehaviorHosts:photos.google.com");

  // Uploading an item with a previously seen key should create an UPDATE
  // action.
  sync_processor_->changes().clear();
  settings_service_.RecordLocalWebsiteApproval("youtube.com");
  CheckWebsiteApproval(syncer::SyncChange::ACTION_UPDATE,
                       "ContentPackManualBehaviorHosts:youtube.com");
}

}  // namespace supervised_user
