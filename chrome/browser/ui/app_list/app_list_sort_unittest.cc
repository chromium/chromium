// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/chrome_app_list_model_updater.h"
#include "chrome/browser/ui/app_list/test/app_list_syncable_service_test_base.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller.h"
#include "chrome/common/pref_names.h"
#include "components/crx_file/id_util.h"

using crx_file::id_util::GenerateId;

class TemporaryAppListSortTest : public test::AppListSyncableServiceTestBase {
 public:
  TemporaryAppListSortTest() {
    feature_list_.InitWithFeatures(
        {ash::features::kLauncherAppSort, ash::features::kProductivityLauncher},
        {});
  }
  ~TemporaryAppListSortTest() override = default;

  void SetUp() override {
    AppListSyncableServiceTestBase::SetUp();
    app_list_controller_ = std::make_unique<test::TestAppListController>();
    GetModelUpdater()->SetActive(true);
    content::RunAllTasksUntilIdle();
  }

  ChromeAppListModelUpdater* GetChromeModelUpdater() {
    return static_cast<ChromeAppListModelUpdater*>(GetModelUpdater());
  }

  ash::AppListSortOrder GetTemporarySortOrder() {
    return GetChromeModelUpdater()->GetTemporarySortOrderForTest();
  }

  // Returns the app list order stored as preference.
  ash::AppListSortOrder GetSortOrderFromPrefs() {
    return static_cast<ash::AppListSortOrder>(
        app_list_syncable_service()->profile()->GetPrefs()->GetInteger(
            prefs::kAppListPreferredOrder));
  }

  void Commit() {
    static_cast<ChromeAppListModelUpdater*>(GetModelUpdater())
        ->EndTemporarySortAndTakeAction(
            ChromeAppListModelUpdater::EndAction::kCommit);
  }

  bool IsUnderTemporarySort() {
    return static_cast<ChromeAppListModelUpdater*>(GetModelUpdater())
        ->is_under_temporary_sort();
  }

 private:
  base::test::ScopedFeatureList feature_lists_;
  std::unique_ptr<test::TestAppListController> app_list_controller_;
};

// Verifies sorting works as expected when the app list is under temporary sort.
TEST_F(TemporaryAppListSortTest, SortUponTemporaryOrder) {
  RemoveAllExistingItems();

  // Add three apps.
  const std::string kItemId1 = CreateNextAppId(GenerateId("app_id1"));
  scoped_refptr<extensions::Extension> app1 =
      MakeApp("A", kItemId1, extensions::Extension::NO_FLAGS);
  InstallExtension(app1.get());

  const std::string kItemId2 = CreateNextAppId(GenerateId("app_id2"));
  scoped_refptr<extensions::Extension> app2 =
      MakeApp("B", kItemId2, extensions::Extension::NO_FLAGS);
  InstallExtension(app2.get());

  const std::string kItemId3 = CreateNextAppId(GenerateId("app_id3"));
  scoped_refptr<extensions::Extension> app3 =
      MakeApp("C", kItemId3, extensions::Extension::NO_FLAGS);
  InstallExtension(app3.get());

  // Verify the default status. Note that when the order is kCustom, a new app
  // should be placed at the front.
  EXPECT_EQ(ash::AppListSortOrder::kCustom, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedItemIdsFromModelUpdater(),
            std::vector<std::string>({kItemId3, kItemId2, kItemId1}));

  // Sort apps with name alphabetical order.
  GetModelUpdater()->OnSortRequested(ash::AppListSortOrder::kNameAlphabetical);

  // The permanent sort order does not change while the temporary order updates.
  EXPECT_EQ(ash::AppListSortOrder::kCustom, GetSortOrderFromPrefs());
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetTemporarySortOrder());

  // The app positions stored in the model updater change.
  EXPECT_EQ(GetOrderedItemIdsFromModelUpdater(),
            std::vector<std::string>({kItemId1, kItemId2, kItemId3}));

  // Permanent positions (i.e. the positions stored in sync data) do not change.
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>({kItemId3, kItemId2, kItemId1}));

  // Sort again without exiting temporary sort.
  GetModelUpdater()->OnSortRequested(ash::AppListSortOrder::kNameAlphabetical);

  // Verify sort orders and app positions.
  EXPECT_EQ(ash::AppListSortOrder::kCustom, GetSortOrderFromPrefs());
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetTemporarySortOrder());
  EXPECT_EQ(GetOrderedItemIdsFromModelUpdater(),
            std::vector<std::string>({kItemId1, kItemId2, kItemId3}));
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>({kItemId3, kItemId2, kItemId1}));

  // Sort again without exiting temporary sort.
  GetModelUpdater()->OnSortRequested(ash::AppListSortOrder::kNameAlphabetical);

  // Verify sort orders and app positions.
  EXPECT_EQ(ash::AppListSortOrder::kCustom, GetSortOrderFromPrefs());
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetTemporarySortOrder());
  EXPECT_EQ(GetOrderedItemIdsFromModelUpdater(),
            std::vector<std::string>({kItemId1, kItemId2, kItemId3}));
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>({kItemId3, kItemId2, kItemId1}));
  EXPECT_TRUE(IsUnderTemporarySort());
}

// Verifies that committing name sort order works as expected.
TEST_F(TemporaryAppListSortTest, CommitNameOrder) {
  RemoveAllExistingItems();

  // Add three apps.
  const std::string kItemId1 = CreateNextAppId(GenerateId("app_id1"));
  scoped_refptr<extensions::Extension> app1 =
      MakeApp("A", kItemId1, extensions::Extension::NO_FLAGS);
  InstallExtension(app1.get());

  const std::string kItemId2 = CreateNextAppId(GenerateId("app_id2"));
  scoped_refptr<extensions::Extension> app2 =
      MakeApp("B", kItemId2, extensions::Extension::NO_FLAGS);
  InstallExtension(app2.get());

  const std::string kItemId3 = CreateNextAppId(GenerateId("app_id3"));
  scoped_refptr<extensions::Extension> app3 =
      MakeApp("C", kItemId3, extensions::Extension::NO_FLAGS);
  InstallExtension(app3.get());

  // Sort with name alphabetical order.
  GetModelUpdater()->OnSortRequested(ash::AppListSortOrder::kNameAlphabetical);

  // Verify that the permanent sort order and the permanent app positions do
  // not change.
  EXPECT_EQ(ash::AppListSortOrder::kCustom, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>({kItemId3, kItemId2, kItemId1}));

  // Commit the temporary sort order. Verify that permanent data update.
  Commit();
  EXPECT_FALSE(IsUnderTemporarySort());
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>({kItemId1, kItemId2, kItemId3}));
}

// Verifies that moving an item on the app list that is under temporary sort
// works as expected.
TEST_F(TemporaryAppListSortTest, HandleMoveItem) {
  RemoveAllExistingItems();

  // Install four apps.
  const std::string kItemId1 = CreateNextAppId(GenerateId("app_id1"));
  scoped_refptr<extensions::Extension> app1 =
      MakeApp("A", kItemId1, extensions::Extension::NO_FLAGS);
  InstallExtension(app1.get());

  const std::string kItemId2 = CreateNextAppId(GenerateId("app_id2"));
  scoped_refptr<extensions::Extension> app2 =
      MakeApp("B", kItemId2, extensions::Extension::NO_FLAGS);
  InstallExtension(app2.get());

  const std::string kItemId3 = CreateNextAppId(GenerateId("app_id3"));
  scoped_refptr<extensions::Extension> app3 =
      MakeApp("C", kItemId3, extensions::Extension::NO_FLAGS);
  InstallExtension(app3.get());

  const std::string kItemId4 = CreateNextAppId(GenerateId("app_id4"));
  scoped_refptr<extensions::Extension> app4 =
      MakeApp("D", kItemId4, extensions::Extension::NO_FLAGS);
  InstallExtension(app4.get());

  // Sort apps with name alphabetical order, commit the temporary order then
  // verify the state.
  ChromeAppListModelUpdater* model_updater = GetChromeModelUpdater();
  model_updater->OnSortRequested(ash::AppListSortOrder::kNameAlphabetical);
  Commit();
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>({kItemId1, kItemId2, kItemId3, kItemId4}));

  // Sort with name reverse alphabetical order without committing.
  model_updater->OnSortRequested(
      ash::AppListSortOrder::kNameReverseAlphabetical);
  EXPECT_EQ(GetOrderedItemIdsFromModelUpdater(),
            std::vector<std::string>({kItemId4, kItemId3, kItemId2, kItemId1}));
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>({kItemId1, kItemId2, kItemId3, kItemId4}));

  // Move `app4` to the position between `app2` and `app3`.
  const syncer::StringOrdinal target_position =
      model_updater->FindItem(kItemId2)->position().CreateBetween(
          model_updater->FindItem(kItemId3)->position());
  model_updater->RequestPositionUpdate(
      kItemId4, target_position, ash::RequestPositionUpdateReason::kMoveItem);

  // Verify the following things:
  // (1) The app list is not under temporary sort.
  // (2) The permanent sort order is cleared.
  // (3) The positions in both model updater and sync data are expected.
  EXPECT_FALSE(IsUnderTemporarySort());
  EXPECT_EQ(ash::AppListSortOrder::kCustom, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedItemIdsFromModelUpdater(),
            std::vector<std::string>({kItemId3, kItemId4, kItemId2, kItemId1}));
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>({kItemId3, kItemId4, kItemId2, kItemId1}));
}

// Verifies that reverting the temporary name sort order works as expected.
TEST_F(TemporaryAppListSortTest, RevertNameOrder) {
  RemoveAllExistingItems();

  // Install Three apps.
  const std::string kItemId1 = CreateNextAppId(GenerateId("app_id1"));
  scoped_refptr<extensions::Extension> app1 =
      MakeApp("A", kItemId1, extensions::Extension::NO_FLAGS);
  InstallExtension(app1.get());

  const std::string kItemId2 = CreateNextAppId(GenerateId("app_id2"));
  scoped_refptr<extensions::Extension> app2 =
      MakeApp("B", kItemId2, extensions::Extension::NO_FLAGS);
  InstallExtension(app2.get());

  const std::string kItemId3 = CreateNextAppId(GenerateId("app_id3"));
  scoped_refptr<extensions::Extension> app3 =
      MakeApp("C", kItemId3, extensions::Extension::NO_FLAGS);
  InstallExtension(app3.get());

  // Sort apps with name alphabetical order, commit the temporary order then
  // verify the state.
  AppListModelUpdater* model_updater = GetModelUpdater();
  model_updater->OnSortRequested(ash::AppListSortOrder::kNameAlphabetical);
  Commit();
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>({kItemId1, kItemId2, kItemId3}));

  // Sort with name reverse alphabetical order without committing.
  model_updater->OnSortRequested(
      ash::AppListSortOrder::kNameReverseAlphabetical);
  EXPECT_EQ(GetOrderedItemIdsFromModelUpdater(),
            std::vector<std::string>({kItemId3, kItemId2, kItemId1}));
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>({kItemId1, kItemId2, kItemId3}));

  // Revert the temporary sort order.
  model_updater->OnSortRevertRequested();

  // Verify the following things:
  // (1) The app list is not under temporary sort.
  // (2) The permanent order does not change.
  // (3) The permanent item positions do not change.
  // (4) The positions in the model updater are reset with the permanent
  // positions.
  EXPECT_FALSE(IsUnderTemporarySort());
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedItemIdsFromModelUpdater(),
            std::vector<std::string>({kItemId1, kItemId2, kItemId3}));
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>({kItemId1, kItemId2, kItemId3}));
}
