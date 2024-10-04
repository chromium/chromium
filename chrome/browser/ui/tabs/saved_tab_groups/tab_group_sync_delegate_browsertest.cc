// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_delegate_desktop.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_bar.h"
#include "chrome/common/channel_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/internal/tab_group_sync_service_test_utils.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_store_service.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/tab_groups/tab_group_color.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"

namespace tab_groups {

class TabGroupSyncDelegateBrowserTest : public InProcessBrowserTest,
                                        public TabGroupSyncService::Observer {
 public:
  TabGroupSyncDelegateBrowserTest() {
    features_.InitWithFeatures(
        {tab_groups::kTabGroupsSaveV2,
         tab_groups::kTabGroupSyncServiceDesktopMigration},
        {tab_groups::kTabGroupsSaveUIUpdate});
  }

  void OnWillBeDestroyed() override {
    if (service_) {
      service_->RemoveObserver(this);
    }

    service_ = nullptr;
    model_ = nullptr;
  }

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&TabGroupSyncDelegateBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    TabGroupSyncServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindOnce(
            &TabGroupSyncDelegateBrowserTest::CreateMockTabGroupSyncService,
            base::Unretained(this)));
  }

  std::unique_ptr<KeyedService> CreateMockTabGroupSyncService(
      content::BrowserContext* context) {
    Profile* profile = static_cast<Profile*>(context);
    auto model = std::make_unique<SavedTabGroupModel>();
    model_ = model.get();

    syncer::DeviceInfoTracker* device_info_tracker =
        DeviceInfoSyncServiceFactory::GetForProfile(profile)
            ->GetDeviceInfoTracker();

    auto service = test::CreateTabGroupSyncService(
        std::move(model), DataTypeStoreServiceFactory::GetForProfile(profile),
        profile->GetPrefs(), device_info_tracker,
        /*optimization_guide=*/nullptr,
        /*identity_manager=*/nullptr);

    std::unique_ptr<TabGroupSyncDelegateDesktop> delegate =
        std::make_unique<TabGroupSyncDelegateDesktop>(service.get(), profile);
    service->SetTabGroupSyncDelegate(std::move(delegate));

    service->SetIsInitializedForTesting(true);
    service_ = service.get();
    return std::move(service);
  }

  base::test::ScopedFeatureList features_;
  base::CallbackListSubscription subscription_;
  raw_ptr<SavedTabGroupModel> model_;
  raw_ptr<TabGroupSyncService> service_;
};

IN_PROC_BROWSER_TEST_F(TabGroupSyncDelegateBrowserTest,
                       RemovedGroupFromSyncClosedLocallyIfOpen) {
  TabGroupSyncService* service =
      TabGroupSyncServiceFactory::GetForProfile(browser()->profile());
  service->AddObserver(this);

  chrome::AddTabAt(browser(), GURL("chrome://newtab"), 0, false);

  LocalTabGroupID local_id = browser()->tab_strip_model()->AddToNewGroup({0});

  EXPECT_TRUE(
      browser()->tab_strip_model()->group_model()->ContainsTabGroup(local_id));
  EXPECT_TRUE(service->GetGroup(local_id));

  // FromSync calls are asynchronous, so wait for the task to complete.
  model_->RemovedFromSync(local_id);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return !browser()->tab_strip_model()->group_model()->ContainsTabGroup(
        local_id);
  }));

  EXPECT_FALSE(service->GetGroup(local_id));
}

IN_PROC_BROWSER_TEST_F(TabGroupSyncDelegateBrowserTest,
                       AddedGroupFromSyncNotOpenedAutomatically) {
  TabGroupSyncService* service =
      TabGroupSyncServiceFactory::GetForProfile(browser()->profile());
  service->AddObserver(this);

  SavedTabGroup group(u"Title", tab_groups::TabGroupColorId::kBlue, {}, 0);
  SavedTabGroupTab tab1(GURL("about:blank"), u"about:blank", group.saved_guid(),
                        /*position=*/0);
  group.AddTabLocally(tab1);
  const base::Uuid sync_id = group.saved_guid();
  EXPECT_FALSE(service->GetGroup(sync_id));

  // FromSync calls are asynchronous, so wait for the task to complete.
  model_->AddedFromSync(std::move(group));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return service->GetGroup(sync_id).has_value(); }));

  EXPECT_EQ(
      0u, browser()->tab_strip_model()->group_model()->ListTabGroups().size());
  EXPECT_FALSE(service->GetGroup(sync_id)->local_group_id().has_value());
}

// Regression test. See crbug.com/370013915.
IN_PROC_BROWSER_TEST_F(
    TabGroupSyncDelegateBrowserTest,
    GroupsWithIndicesOutsideLocalIndexRangeInsertedAtTheEnd) {
  TabGroupSyncService* service =
      TabGroupSyncServiceFactory::GetForProfile(browser()->profile());
  service->AddObserver(this);

  auto saved_tab_group_bar =
      std::make_unique<SavedTabGroupBar>(browser(), service_, false);
  EXPECT_EQ(1u, saved_tab_group_bar->children().size());

  chrome::AddTabAt(browser(), GURL("chrome://newtab"), 0, false);
  LocalTabGroupID local_id = browser()->tab_strip_model()->AddToNewGroup({0});
  EXPECT_TRUE(
      browser()->tab_strip_model()->group_model()->ContainsTabGroup(local_id));
  std::optional<SavedTabGroup> group_1 = service->GetGroup(local_id);
  EXPECT_TRUE(group_1);
  EXPECT_EQ(2u, saved_tab_group_bar->children().size());

  SavedTabGroup group_2(u"Group 2", tab_groups::TabGroupColorId::kPink, {}, 2);
  SavedTabGroupTab tab2(GURL("about:blank"), u"about:blank",
                        group_2.saved_guid(),
                        /*position=*/0);
  group_2.AddTabLocally(tab2);

  SavedTabGroup group_3(u"Group 3", tab_groups::TabGroupColorId::kGreen, {},
                        10);
  SavedTabGroupTab tab3(GURL("about:blank"), u"about:blank",
                        group_3.saved_guid(),
                        /*position=*/0);
  group_3.AddTabLocally(tab3);

  const base::Uuid sync_id_1 = group_1->saved_guid();
  const base::Uuid sync_id_2 = group_2.saved_guid();
  const base::Uuid sync_id_3 = group_3.saved_guid();

  // FromSync calls are asynchronous, so wait for the task to complete.
  model_->AddedFromSync(std::move(group_3));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return service->GetGroup(sync_id_3).has_value(); }));
  EXPECT_EQ(3u, saved_tab_group_bar->children().size());

  model_->AddedFromSync(std::move(group_2));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return service->GetGroup(sync_id_2).has_value(); }));
  EXPECT_EQ(4u, saved_tab_group_bar->children().size());

  // Verify the ordering is group 1, group 2, group 3
  EXPECT_TRUE(views::IsViewClass<SavedTabGroupButton>(
      saved_tab_group_bar->children()[0]));
  EXPECT_TRUE(views::IsViewClass<SavedTabGroupButton>(
      saved_tab_group_bar->children()[1]));
  EXPECT_TRUE(views::IsViewClass<SavedTabGroupButton>(
      saved_tab_group_bar->children()[2]));

  EXPECT_EQ(sync_id_1, views::AsViewClass<SavedTabGroupButton>(
                           saved_tab_group_bar->children()[0])
                           ->guid());
  EXPECT_EQ(sync_id_2, views::AsViewClass<SavedTabGroupButton>(
                           saved_tab_group_bar->children()[1])
                           ->guid());
  EXPECT_EQ(sync_id_3, views::AsViewClass<SavedTabGroupButton>(
                           saved_tab_group_bar->children()[2])
                           ->guid());
}

}  // namespace tab_groups
