// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"

namespace tab_groups {
namespace {

using testing::NotNull;

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

  void OnTabGroupAdded(const SavedTabGroup& group,
                       TriggerSource source) override {
    callback_received_ = true;
    if (quit_) {
      std::move(quit_).Run();
    }
  }

  void OnTabGroupUpdated(const SavedTabGroup& group,
                         TriggerSource source) override {
    callback_received_ = true;
    if (quit_) {
      std::move(quit_).Run();
    }
  }

  void WaitUntilCallbackReceived() {
    if (callback_received_) {
      return;
    }
    base::RunLoop run_loop;
    quit_ = run_loop.QuitClosure();
    run_loop.Run();

    // Reset status.
    callback_received_ = false;
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
  base::OnceClosure quit_;
  bool callback_received_ = false;
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

IN_PROC_BROWSER_TEST_F(TabGroupSyncDelegateBrowserTest,
                       OpenNewTabAndNavigateExistingOnConnectNewSavedGroup) {
  TabGroupSyncService* service =
      TabGroupSyncServiceFactory::GetForProfile(browser()->profile());
  service->AddObserver(this);

  // Create a new tab group with one tab. The tab group should be saved.
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), 0, false);
  LocalTabGroupID local_id = browser()->tab_strip_model()->AddToNewGroup({0});
  ASSERT_TRUE(
      browser()->tab_strip_model()->group_model()->ContainsTabGroup(local_id));
  ASSERT_TRUE(service->GetGroup(local_id));

  // Unsave the local group.
  service->UnsaveGroup(local_id);
  ASSERT_TRUE(
      browser()->tab_strip_model()->group_model()->ContainsTabGroup(local_id));
  ASSERT_FALSE(service->GetGroup(local_id));

  // Connect the local tab group with a new saved group.
  SavedTabGroup new_saved_group(u"Title", tab_groups::TabGroupColorId::kBlue,
                                {}, 0);
  new_saved_group.AddTabLocally(SavedTabGroupTab(
      GURL("http://www.google.com/1"), u"title 1", new_saved_group.saved_guid(),
      /*position=*/0));
  new_saved_group.AddTabLocally(SavedTabGroupTab(
      GURL("http://www.google.com/2"), u"title 2", new_saved_group.saved_guid(),
      /*position=*/1));
  service->AddGroup(new_saved_group);

  service->ConnectLocalTabGroup(new_saved_group.saved_guid(), local_id,
                                OpeningSource::kUnknown);

  // Local group model should be updated to match the new saved group.
  ASSERT_TRUE(
      browser()->tab_strip_model()->group_model()->ContainsTabGroup(local_id));
  const TabGroup* local_tab_group =
      browser()->tab_strip_model()->group_model()->GetTabGroup(local_id);

  EXPECT_EQ(local_tab_group->visual_data()->title(), u"Title");
  EXPECT_EQ(local_tab_group->visual_data()->color(),
            tab_groups::TabGroupColorId::kBlue);

  // Verify that a new tab was added, and the existing one navigated to the
  // correct URL.
  const gfx::Range tab_range = local_tab_group->ListTabs();
  ASSERT_EQ(tab_range.length(), 2u);
  EXPECT_EQ(browser()
                ->tab_strip_model()
                ->GetWebContentsAt(tab_range.start())
                ->GetURL(),
            GURL("http://www.google.com/1"));
  EXPECT_EQ(browser()
                ->tab_strip_model()
                ->GetWebContentsAt(tab_range.start() + 1)
                ->GetURL(),
            GURL("http://www.google.com/2"));
}

IN_PROC_BROWSER_TEST_F(TabGroupSyncDelegateBrowserTest,
                       CloseTabAndNavigateRemainingOnConnectNewSavedGroup) {
  TabGroupSyncService* service =
      TabGroupSyncServiceFactory::GetForProfile(browser()->profile());
  service->AddObserver(this);

  // Create a new tab group with one tab. The tab group should be saved.
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), 0, false);
  chrome::AddTabAt(browser(), GURL("about:blank"), 1, false);
  LocalTabGroupID local_id =
      browser()->tab_strip_model()->AddToNewGroup({0, 1});
  ASSERT_TRUE(
      browser()->tab_strip_model()->group_model()->ContainsTabGroup(local_id));
  ASSERT_TRUE(service->GetGroup(local_id));

  // Unsave the local group.
  service->UnsaveGroup(local_id);
  ASSERT_TRUE(
      browser()->tab_strip_model()->group_model()->ContainsTabGroup(local_id));
  ASSERT_FALSE(service->GetGroup(local_id));

  // Connect the local tab group with a new saved group.
  SavedTabGroup new_saved_group(u"Title", tab_groups::TabGroupColorId::kBlue,
                                {}, 0);
  new_saved_group.AddTabLocally(SavedTabGroupTab(
      GURL("http://www.google.com/1"), u"title 1", new_saved_group.saved_guid(),
      /*position=*/0));
  service->AddGroup(new_saved_group);

  service->ConnectLocalTabGroup(new_saved_group.saved_guid(), local_id,
                                OpeningSource::kUnknown);

  // Local group model should be updated to match the new saved group.
  ASSERT_TRUE(
      browser()->tab_strip_model()->group_model()->ContainsTabGroup(local_id));
  const TabGroup* local_tab_group =
      browser()->tab_strip_model()->group_model()->GetTabGroup(local_id);

  EXPECT_EQ(local_tab_group->visual_data()->title(), u"Title");
  EXPECT_EQ(local_tab_group->visual_data()->color(),
            tab_groups::TabGroupColorId::kBlue);

  // Verify that only one tab remains and it's navigated to the correct URL.
  const gfx::Range tab_range = local_tab_group->ListTabs();
  ASSERT_EQ(tab_range.length(), 1u);
  EXPECT_EQ(browser()
                ->tab_strip_model()
                ->GetWebContentsAt(tab_range.start())
                ->GetURL(),
            GURL("http://www.google.com/1"));
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
  WaitUntilCallbackReceived();
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

IN_PROC_BROWSER_TEST_F(TabGroupSyncDelegateBrowserTest,
                       PreserveCollapsedStateOnRemoteUpdate) {
  TabGroupSyncService* service =
      TabGroupSyncServiceFactory::GetForProfile(browser()->profile());
  service->AddObserver(this);

  chrome::AddTabAt(browser(), GURL("chrome://newtab"), 0, false);
  const LocalTabGroupID local_id =
      browser()->tab_strip_model()->AddToNewGroup({0});

  TabGroup* local_group =
      browser()->tab_strip_model()->group_model()->GetTabGroup(local_id);
  ASSERT_THAT(local_group, NotNull());
  local_group->SetVisualData(
      TabGroupVisualData(u"Title", tab_groups::TabGroupColorId::kBlue,
                         /*is_collapsed=*/true),
      /*is_customized=*/true);

  // Verify that the group is saved.
  std::optional<SavedTabGroup> saved_group = service->GetGroup(local_id);
  ASSERT_TRUE(saved_group.has_value());

  // Simulate a remote update to the group.
  model_->MergeRemoteGroupMetadata(
      saved_group->saved_guid(), u"title", TabGroupColorId::kRed,
      /*position=*/std::nullopt, /*creator_cache_guid=*/std::nullopt,
      /*last_updater_cache_guid=*/std::nullopt,
      /*update_time=*/base::Time::Now());

  // Wait for the tab group update to complete because sync updates are
  // asynchronous.
  ASSERT_TRUE(base::test::RunUntil([local_group]() {
    return local_group->visual_data()->color() ==
           tab_groups::TabGroupColorId::kRed;
  }));

  // The local group should still be collapsed.
  EXPECT_TRUE(local_group->visual_data()->is_collapsed());
}

}  // namespace
}  // namespace tab_groups
