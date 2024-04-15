// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/android/tab_group_sync_service_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "components/saved_tab_groups/android/tab_group_sync_conversions_bridge.h"
#include "components/saved_tab_groups/android/tab_group_sync_conversions_utils.h"
#include "components/saved_tab_groups/native_j_unittests_jni_headers/TabGroupSyncServiceAndroidUnitTest_jni.h"
#include "components/saved_tab_groups/saved_tab_group_test_utils.h"
#include "components/sync/test/test_matchers.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::android::AttachCurrentThread;
using testing::_;
using testing::Eq;
using testing::Return;
using testing::SaveArg;

namespace tab_groups {
namespace {

class MockTabGroupSyncService : public TabGroupSyncService {
 public:
  MockTabGroupSyncService() = default;
  ~MockTabGroupSyncService() override = default;

  MOCK_METHOD(void, AddGroup, (const SavedTabGroup&));
  MOCK_METHOD(void, RemoveGroup, (const LocalTabGroupID&));
  MOCK_METHOD(void,
              UpdateVisualData,
              (const LocalTabGroupID, const tab_groups::TabGroupVisualData*));
  MOCK_METHOD(void,
              AddTab,
              (const LocalTabGroupID&,
               const LocalTabID&,
               const std::u16string&,
               GURL,
               std::optional<size_t>));
  MOCK_METHOD(void,
              UpdateTab,
              (const LocalTabGroupID&,
               const LocalTabID&,
               const std::u16string&,
               GURL,
               std::optional<size_t>));
  MOCK_METHOD(void, RemoveTab, (const LocalTabGroupID&, const LocalTabID&));

  MOCK_METHOD(std::vector<SavedTabGroup>, GetAllGroups, ());
  MOCK_METHOD(std::optional<SavedTabGroup>, GetGroup, (const base::Uuid&));
  MOCK_METHOD(std::optional<SavedTabGroup>, GetGroup, (LocalTabGroupID&));

  MOCK_METHOD(void,
              UpdateLocalTabGroupId,
              (const base::Uuid&, const LocalTabGroupID&));
  MOCK_METHOD(void,
              UpdateLocalTabId,
              (const LocalTabGroupID&, const base::Uuid&, const LocalTabID&));

  MOCK_METHOD(syncer::ModelTypeSyncBridge*, bridge, ());

  MOCK_METHOD(void, AddObserver, (Observer*));
  MOCK_METHOD(void, RemoveObserver, (Observer*));
};

MATCHER_P(UuidEq, uuid, "") {
  return arg.saved_guid() == uuid;
}

}  // namespace

class TabGroupSyncServiceAndroidTest : public testing::Test {
 public:
  TabGroupSyncServiceAndroidTest() = default;
  ~TabGroupSyncServiceAndroidTest() override = default;

  void SetUp() override {
    j_test_ = Java_TabGroupSyncServiceAndroidUnitTest_Constructor(
        AttachCurrentThread());
    CreateBridge();
    SetUpJavaTestObserver();
  }

  void CreateBridge() {
    EXPECT_CALL(tab_group_sync_service_, AddObserver(_));
    bridge_ =
        std::make_unique<TabGroupSyncServiceAndroid>(&tab_group_sync_service_);
    j_service_ = bridge_->GetJavaObject();
  }

  void SetUpJavaTestObserver() {
    auto* env = AttachCurrentThread();
    Java_TabGroupSyncServiceAndroidUnitTest_setUpTestObserver(env, j_test_,
                                                              j_service_);
  }

  void TearDown() override {
    EXPECT_CALL(tab_group_sync_service_, RemoveObserver(_));
  }

  MockTabGroupSyncService tab_group_sync_service_;
  std::unique_ptr<TabGroupSyncServiceAndroid> bridge_;
  base::android::ScopedJavaLocalRef<jobject> j_service_;
  base::android::ScopedJavaGlobalRef<jobject> j_test_;
};

TEST_F(TabGroupSyncServiceAndroidTest, OnInitialized) {
  bridge_->OnInitialized();
  Java_TabGroupSyncServiceAndroidUnitTest_testOnInitialized(
      AttachCurrentThread(), j_test_);
}

TEST_F(TabGroupSyncServiceAndroidTest, UuidConversion) {
  auto* env = AttachCurrentThread();
  base::Uuid uuid = base::Uuid::ParseCaseInsensitive("abcdefghKL");
  auto j_uuid = UuidToJavaString(env, uuid);
  auto uuid2 = JavaStringToUuid(
      env, base::android::JavaParamRef<jstring>(env, j_uuid.Release()));
  EXPECT_EQ(uuid, uuid2);
}

TEST_F(TabGroupSyncServiceAndroidTest, TabGroupIdConversion) {
  LocalTabGroupID tab_group_id = 5;
  EXPECT_EQ(FromJavaTabGroupId(ToJavaTabGroupId(
                std::make_optional<LocalTabGroupID>(tab_group_id))),
            tab_group_id);
}

TEST_F(TabGroupSyncServiceAndroidTest, TabIdConversion) {
  LocalTabID tab_id = 5;
  EXPECT_EQ(FromJavaTabId(ToJavaTabId(std::make_optional<LocalTabID>(tab_id))),
            tab_id);
}

TEST_F(TabGroupSyncServiceAndroidTest, SaveTabGroupConversion) {
  auto* env = AttachCurrentThread();
  SavedTabGroup group = test::CreateTestSavedTabGroup();
  group.SetTitle(u"Some Title");
  group.SetColor(tab_groups::TabGroupColorId::kRed);

  SavedTabGroupTab tab3(GURL(), u"Tab title", group.saved_guid(),
                        /*position=*/std::nullopt,
                        /*saved_tab_guid=*/std::nullopt, /*local_tab_id=*/9);
  group.AddTabLocally(tab3);
  auto j_group = TabGroupSyncConversionsBridge::CreateGroup(env, group);
  Java_TabGroupSyncServiceAndroidUnitTest_testSavedTabGroupJavaConversion(
      AttachCurrentThread(), j_test_, j_group);
}

TEST_F(TabGroupSyncServiceAndroidTest, OnTabGroupAdded) {
  auto* env = AttachCurrentThread();
  SavedTabGroup group = test::CreateTestSavedTabGroup();
  group.SetTitle(u"Test Group");
  group.SetColor(tab_groups::TabGroupColorId::kBlue);
  bridge_->OnTabGroupAdded(group, TriggerSource::REMOTE);
  Java_TabGroupSyncServiceAndroidUnitTest_testOnTabGroupAdded(env, j_test_);
}

TEST_F(TabGroupSyncServiceAndroidTest, OnTabGroupUpdated) {
  auto* env = AttachCurrentThread();
  SavedTabGroup group = test::CreateTestSavedTabGroup();
  group.SetTitle(u"Test Group");
  group.SetColor(tab_groups::TabGroupColorId::kBlue);
  bridge_->OnTabGroupAdded(group, TriggerSource::REMOTE);
  Java_TabGroupSyncServiceAndroidUnitTest_testOnTabGroupUpdated(env, j_test_);
}

TEST_F(TabGroupSyncServiceAndroidTest, OnTabGroupRemoved) {
  auto* env = AttachCurrentThread();
  bridge_->OnTabGroupRemoved(4);
  Java_TabGroupSyncServiceAndroidUnitTest_testOnTabGroupRemoved(env, j_test_);
}

TEST_F(TabGroupSyncServiceAndroidTest, CreateGroup) {
  auto* env = AttachCurrentThread();
  SavedTabGroup captured_group = test::CreateTestSavedTabGroup();
  EXPECT_CALL(tab_group_sync_service_, AddGroup(_))
      .WillOnce(SaveArg<0>(&captured_group));
  Java_TabGroupSyncServiceAndroidUnitTest_testCreateGroup(env, j_test_);

  EXPECT_TRUE(captured_group.local_group_id().has_value());
  EXPECT_EQ(4, captured_group.local_group_id().value());
}

TEST_F(TabGroupSyncServiceAndroidTest, RemoveGroup) {
  EXPECT_CALL(tab_group_sync_service_, RemoveGroup(Eq(4)));
  Java_TabGroupSyncServiceAndroidUnitTest_testRemoveGroup(AttachCurrentThread(),
                                                          j_test_);
}

TEST_F(TabGroupSyncServiceAndroidTest, UpdateVisualData) {
  EXPECT_CALL(tab_group_sync_service_, UpdateVisualData(Eq(4), _));
  Java_TabGroupSyncServiceAndroidUnitTest_testUpdateVisualData(
      AttachCurrentThread(), j_test_);
}

TEST_F(TabGroupSyncServiceAndroidTest, GetAllGroups) {
  auto group = test::CreateTestSavedTabGroup();
  std::vector<SavedTabGroup> expectedGroups = {group};
  EXPECT_CALL(tab_group_sync_service_, GetAllGroups())
      .WillOnce(Return(expectedGroups));
  Java_TabGroupSyncServiceAndroidUnitTest_testGetAllGroups(
      AttachCurrentThread(), j_test_);
}

TEST_F(TabGroupSyncServiceAndroidTest, GetGroupBySyncId) {
  auto* env = AttachCurrentThread();
  auto group1 = test::CreateTestSavedTabGroup();
  base::Uuid uuid2 = base::Uuid::ParseCaseInsensitive("abcdefghKL");

  EXPECT_CALL(tab_group_sync_service_, GetGroup(group1.saved_guid()))
      .WillOnce(Return(std::make_optional<SavedTabGroup>(group1)));
  EXPECT_CALL(tab_group_sync_service_, GetGroup(uuid2))
      .WillOnce(Return(std::nullopt));

  auto j_uuid1 = UuidToJavaString(env, group1.saved_guid());
  auto j_uuid2 = UuidToJavaString(env, uuid2);
  Java_TabGroupSyncServiceAndroidUnitTest_testGetGroupBySyncId(
      env, j_test_, j_uuid1, j_uuid2);
}

TEST_F(TabGroupSyncServiceAndroidTest, UpdateLocalTabGroupId) {
  auto* env = AttachCurrentThread();
  base::Uuid group_id = base::Uuid::GenerateRandomV4();
  auto j_group_id = UuidToJavaString(env, group_id);

  EXPECT_CALL(tab_group_sync_service_,
              UpdateLocalTabGroupId(Eq(group_id), Eq(4)));
  Java_TabGroupSyncServiceAndroidUnitTest_testUpdateLocalTabGroupId(
      AttachCurrentThread(), j_test_, j_group_id, 4);
}

TEST_F(TabGroupSyncServiceAndroidTest, UpdateLocalTabId) {
  auto* env = AttachCurrentThread();
  base::Uuid tab_id = base::Uuid::GenerateRandomV4();
  auto j_tab_id = UuidToJavaString(env, tab_id);

  EXPECT_CALL(tab_group_sync_service_,
              UpdateLocalTabId(Eq(2), Eq(tab_id), Eq(4)));
  Java_TabGroupSyncServiceAndroidUnitTest_testUpdateLocalTabId(
      AttachCurrentThread(), j_test_, 2, j_tab_id, 4);
}

}  // namespace tab_groups
