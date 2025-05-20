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
#include "chrome/browser/collaboration/messaging/messaging_backend_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_web_contents_listener.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_action_context_desktop.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_delegate_desktop.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_bar.h"
#include "chrome/common/channel_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/collaboration/public/messaging/empty_messaging_backend_service.h"
#include "components/collaboration/public/messaging/messaging_backend_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/internal/tab_group_sync_service_test_utils.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_store_service.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"

namespace tab_groups {
namespace {

using testing::NotNull;

// Tests desktop behaviors for the SavedTabGroups feature such as performing
// local changes and simulating sync events.
class TabGroupSyncDelegateBrowserTest : public InProcessBrowserTest,
                                        public TabGroupSyncService::Observer {
 public:
  TabGroupSyncDelegateBrowserTest() {
    features_.InitWithFeatures({kTabGroupSyncServiceDesktopMigration}, {});

    dependency_manager_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &TabGroupSyncDelegateBrowserTest::RegisterFakeServices,
                base::Unretained(this)));
  }

  void RegisterFakeServices(content::BrowserContext* context) {
    collaboration::messaging::MessagingBackendServiceFactory::GetInstance()
        ->SetTestingFactory(
            context, base::BindRepeating([](content::BrowserContext* context)
                                             -> std::unique_ptr<KeyedService> {
              return std::make_unique<
                  collaboration::messaging::EmptyMessagingBackendService>();
            }));
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
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // `service_` is instantiated the first time GetForProfile() is called.
    TabGroupSyncService* service =
        TabGroupSyncServiceFactory::GetForProfile(browser()->profile());
    service->AddObserver(this);
  }

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
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

    service_ = service.get();
    return std::move(service);
  }

  base::test::ScopedFeatureList features_;
  base::CallbackListSubscription subscription_;
  raw_ptr<SavedTabGroupModel> model_;
  raw_ptr<TabGroupSyncService> service_;
  base::OnceClosure quit_;
  bool callback_received_ = false;
  base::CallbackListSubscription dependency_manager_subscription_;
};

IN_PROC_BROWSER_TEST_F(TabGroupSyncDelegateBrowserTest,
                       GetBrowserWithTabGroupId) {
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);

  LocalTabGroupID group_id = browser()->tab_strip_model()->AddToNewGroup({0});
  EXPECT_EQ(browser(), SavedTabGroupUtils::GetBrowserWithTabGroupId(group_id));
}

IN_PROC_BROWSER_TEST_F(TabGroupSyncDelegateBrowserTest,
                       UngroupingTabIsRemovedFromSavedGroup) {
  chrome::AddTabAt(browser(), GURL("https://google.com"), 1, false);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);

  LocalTabGroupID group_id =
      browser()->tab_strip_model()->AddToNewGroup({0, 1});

  SavedTabGroupWebContentsListener* listener =
      browser()
          ->tab_strip_model()
          ->GetTabAtIndex(0)
          ->GetTabFeatures()
          ->saved_tab_group_web_contents_listener();

  ASSERT_TRUE(listener->saved_group());
  EXPECT_EQ(listener->saved_group()->local_group_id(), group_id);
  ASSERT_TRUE(model_->Contains(group_id));
  EXPECT_EQ(model_->Get(group_id)->saved_tabs().size(), 2u);

  // Move the first tab outside the group.
  browser()->tab_strip_model()->MoveWebContentsAt(0, 0, true, std::nullopt);

  EXPECT_FALSE(listener->saved_group());
  ASSERT_TRUE(model_->Contains(group_id));
  EXPECT_EQ(model_->Get(group_id)->saved_tabs().size(), 1u);
}

IN_PROC_BROWSER_TEST_F(TabGroupSyncDelegateBrowserTest,
                       AddingTabToGroupAddsItToSavedGroup) {
  chrome::AddTabAt(browser(), GURL("https://google.com"), 1, false);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);
  LocalTabGroupID group_id = browser()->tab_strip_model()->AddToNewGroup({1});

  SavedTabGroupWebContentsListener* listener =
      browser()
          ->tab_strip_model()
          ->GetTabAtIndex(0)
          ->GetTabFeatures()
          ->saved_tab_group_web_contents_listener();

  EXPECT_FALSE(listener->saved_group());
  ASSERT_TRUE(model_->Contains(group_id));
  EXPECT_EQ(model_->Get(group_id)->saved_tabs().size(), 1u);

  // Move the first tab inside the group.
  browser()->tab_strip_model()->MoveWebContentsAt(0, 0, true, group_id);

  ASSERT_TRUE(listener->saved_group());
  ASSERT_TRUE(model_->Contains(group_id));
  EXPECT_EQ(model_->Get(group_id)->saved_tabs().size(), 2u);
}

IN_PROC_BROWSER_TEST_F(TabGroupSyncDelegateBrowserTest,
                       AlreadyOpenedGroupIsFocused) {
  // Create 2 tabs; Add 1 to a tab group.
  chrome::AddTabAt(browser(), GURL("https://google.com"), 1, true);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);

  LocalTabGroupID group_id = browser()->tab_strip_model()->AddToNewGroup({0});
  ASSERT_TRUE(model_->Contains(group_id));

  std::optional<SavedTabGroup> saved_group = service_->GetGroup(group_id);
  ASSERT_TRUE(saved_group);

  base::Uuid sync_id = saved_group->saved_guid();

  // Attempt to open the group again.
  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 1);
  std::optional<LocalTabGroupID> opened_group_id = service_->OpenTabGroup(
      sync_id, std::make_unique<TabGroupActionContextDesktop>(
                   browser(), OpeningSource::kOpenedFromRevisitUi));

  EXPECT_TRUE(opened_group_id);
  EXPECT_EQ(opened_group_id, group_id);
  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 0);
}

IN_PROC_BROWSER_TEST_F(TabGroupSyncDelegateBrowserTest,
                       AlreadyOpenedGroupsActiveTabDoesNotChange) {
  // Create 3 tabs; Add 2 to a tab group.
  chrome::AddTabAt(browser(), GURL("https://google.com"), 1, true);
  chrome::AddTabAt(browser(), GURL("https://google.com"), 2, true);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 3);

  LocalTabGroupID group_id =
      browser()->tab_strip_model()->AddToNewGroup({1, 2});
  ASSERT_TRUE(model_->Contains(group_id));

  std::optional<SavedTabGroup> saved_group = service_->GetGroup(group_id);
  ASSERT_TRUE(saved_group);

  base::Uuid sync_id = saved_group->saved_guid();

  // Attempt to open the group again and ensure the 2nd tab in the group is
  // still the active tab.
  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 2);
  std::optional<LocalTabGroupID> opened_group_id = service_->OpenTabGroup(
      sync_id, std::make_unique<TabGroupActionContextDesktop>(
                   browser(), OpeningSource::kOpenedFromRevisitUi));
  EXPECT_TRUE(opened_group_id);
  EXPECT_EQ(opened_group_id, group_id);
  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 2);

  // Activate a tab not in the group and attempt to open the group again. Expect
  // that the active tab becomes the first tab in the group.
  browser()->tab_strip_model()->ActivateTabAt(0);
  opened_group_id = service_->OpenTabGroup(
      sync_id, std::make_unique<TabGroupActionContextDesktop>(
                   browser(), OpeningSource::kOpenedFromRevisitUi));
  EXPECT_TRUE(opened_group_id);
  EXPECT_EQ(opened_group_id, group_id);
  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 1);
}

IN_PROC_BROWSER_TEST_F(TabGroupSyncDelegateBrowserTest,
                       RemovedGroupFromSyncClosedLocallyIfOpen) {
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), 0, false);

  LocalTabGroupID local_id = browser()->tab_strip_model()->AddToNewGroup({0});

  EXPECT_TRUE(
      browser()->tab_strip_model()->group_model()->ContainsTabGroup(local_id));
  EXPECT_TRUE(service_->GetGroup(local_id));

  // FromSync calls are asynchronous, so wait for the task to complete.
  model_->RemovedFromSync(local_id);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return !browser()->tab_strip_model()->group_model()->ContainsTabGroup(
        local_id);
  }));

  EXPECT_FALSE(service_->GetGroup(local_id));
}

IN_PROC_BROWSER_TEST_F(TabGroupSyncDelegateBrowserTest,
                       AddedGroupFromSyncNotOpenedAutomatically) {
  SavedTabGroup group(u"Title", TabGroupColorId::kBlue, {}, 0);
  SavedTabGroupTab tab1(GURL("about:blank"), u"about:blank", group.saved_guid(),
                        /*position=*/0);
  group.AddTabLocally(tab1);
  const base::Uuid sync_id = group.saved_guid();
  EXPECT_FALSE(service_->GetGroup(sync_id));

  // FromSync calls are asynchronous, so wait for the task to complete.
  model_->AddedFromSync(std::move(group));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return service_->GetGroup(sync_id).has_value(); }));

  EXPECT_EQ(
      0u, browser()->tab_strip_model()->group_model()->ListTabGroups().size());
  EXPECT_FALSE(service_->GetGroup(sync_id)->local_group_id().has_value());
}

IN_PROC_BROWSER_TEST_F(TabGroupSyncDelegateBrowserTest,
                       OpeningNewTabFromSyncOpensInLocalGroup) {
  // Create a new tab group with one tab. The tab group should be saved.
  LocalTabGroupID local_id = browser()->tab_strip_model()->AddToNewGroup({0});
  ASSERT_TRUE(
      browser()->tab_strip_model()->group_model()->ContainsTabGroup(local_id));

  std::optional<SavedTabGroup> saved_group = service_->GetGroup(local_id);
  ASSERT_TRUE(saved_group);

  SavedTabGroupTab tab_1(GURL("http://www.google.com/1"), u"title 1",
                         saved_group->saved_guid(),
                         /*position=*/0);

  SavedTabGroupTab tab_2(GURL("http://www.google.com/2"), u"title 2",
                         saved_group->saved_guid(),
                         /*position=*/2);

  model_->AddTabToGroupFromSync(saved_group->saved_guid(), std::move(tab_1));
  WaitUntilCallbackReceived();

  model_->AddTabToGroupFromSync(saved_group->saved_guid(), std::move(tab_2));
  WaitUntilCallbackReceived();

  // Verify that the new tabs were added to the group in the correct order.
  const gfx::Range tab_range = browser()
                                   ->tab_strip_model()
                                   ->group_model()
                                   ->GetTabGroup(local_id)
                                   ->ListTabs();
  ASSERT_EQ(tab_range.length(), 3u);
  EXPECT_EQ(browser()->tab_strip_model()->GetWebContentsAt(0)->GetURL(),
            GURL("http://www.google.com/1"));
  EXPECT_EQ(browser()->tab_strip_model()->GetWebContentsAt(1)->GetURL(),
            GURL("about:blank"));
  EXPECT_EQ(browser()->tab_strip_model()->GetWebContentsAt(2)->GetURL(),
            GURL("http://www.google.com/2"));
}

IN_PROC_BROWSER_TEST_F(TabGroupSyncDelegateBrowserTest,
                       NavigatedTabFromSyncNavigatesLocalTab) {
  // Create a new tab group with one tab.
  LocalTabGroupID local_id = browser()->tab_strip_model()->AddToNewGroup({0});
  ASSERT_TRUE(
      browser()->tab_strip_model()->group_model()->ContainsTabGroup(local_id));
  ASSERT_TRUE(model_->Contains(local_id));

  // Simulate a sync navigation.
  GURL url_to_navigate_to = GURL("https://www.google.com/1");
  browser()
      ->tab_strip_model()
      ->GetTabAtIndex(0)
      ->GetTabFeatures()
      ->saved_tab_group_web_contents_listener()
      ->NavigateToUrlForTest(url_to_navigate_to);

  EXPECT_EQ(browser()->tab_strip_model()->GetWebContentsAt(0)->GetURL(),
            url_to_navigate_to);
}

IN_PROC_BROWSER_TEST_F(TabGroupSyncDelegateBrowserTest,
                       SimulateLocalThenSyncTabNavigations) {
  // Create a new tab group with one tab. The tab group should be saved.
  LocalTabGroupID local_id = browser()->tab_strip_model()->AddToNewGroup({0});
  ASSERT_TRUE(
      browser()->tab_strip_model()->group_model()->ContainsTabGroup(local_id));
  ASSERT_TRUE(model_->Contains(local_id));

  // Perform a local navigation.
  GURL first_url_to_navigate_to = GURL("https://www.google.com/1");
  browser()
      ->tab_strip_model()
      ->GetWebContentsAt(0)
      ->GetController()
      .LoadURLWithParams(content::NavigationController::LoadURLParams(
          first_url_to_navigate_to));
  EXPECT_EQ(browser()->tab_strip_model()->GetWebContentsAt(0)->GetURL(),
            first_url_to_navigate_to);

  // Perform a sync navigation.
  GURL second_url_to_navigate_to = GURL("https://www.google.com/2");
  browser()
      ->tab_strip_model()
      ->GetTabAtIndex(0)
      ->GetTabFeatures()
      ->saved_tab_group_web_contents_listener()
      ->NavigateToUrlForTest(second_url_to_navigate_to);
  EXPECT_EQ(browser()->tab_strip_model()->GetWebContentsAt(0)->GetURL(),
            second_url_to_navigate_to);
}

IN_PROC_BROWSER_TEST_F(TabGroupSyncDelegateBrowserTest,
                       SimulateSyncThenLocalTabNavigations) {
  // Create a new tab group with one tab. The tab group should be saved.
  LocalTabGroupID local_id = browser()->tab_strip_model()->AddToNewGroup({0});
  ASSERT_TRUE(
      browser()->tab_strip_model()->group_model()->ContainsTabGroup(local_id));
  ASSERT_TRUE(model_->Contains(local_id));

  // Perform a local navigation.
  GURL first_url_to_navigate_to = GURL("https://www.google.com/1");
  browser()
      ->tab_strip_model()
      ->GetTabAtIndex(0)
      ->GetTabFeatures()
      ->saved_tab_group_web_contents_listener()
      ->NavigateToUrlForTest(first_url_to_navigate_to);
  EXPECT_EQ(browser()->tab_strip_model()->GetWebContentsAt(0)->GetURL(),
            first_url_to_navigate_to);

  // Perform a sync navigation.
  GURL second_url_to_navigate_to = GURL("https://www.google.com/2");
  browser()
      ->tab_strip_model()
      ->GetWebContentsAt(0)
      ->GetController()
      .LoadURLWithParams(content::NavigationController::LoadURLParams(
          second_url_to_navigate_to));
  EXPECT_EQ(browser()->tab_strip_model()->GetWebContentsAt(0)->GetURL(),
            second_url_to_navigate_to);
}

IN_PROC_BROWSER_TEST_F(TabGroupSyncDelegateBrowserTest,
                       RemoveTabFromSyncRemovesLocalTab) {
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), 1, false);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);

  // Create a new tab group with two tabs.
  LocalTabGroupID local_id =
      browser()->tab_strip_model()->AddToNewGroup({0, 1});
  ASSERT_TRUE(
      browser()->tab_strip_model()->group_model()->ContainsTabGroup(local_id));

  std::optional<SavedTabGroup> saved_group = service_->GetGroup(local_id);
  ASSERT_TRUE(saved_group);
  WaitUntilCallbackReceived();

  model_->RemoveTabFromGroupFromSync(
      saved_group->saved_guid(),
      saved_group->saved_tabs().at(0).saved_tab_guid());
  WaitUntilCallbackReceived();

  // Verify that the first tab was removed from the group.
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);
  const gfx::Range tab_range = browser()
                                   ->tab_strip_model()
                                   ->group_model()
                                   ->GetTabGroup(local_id)
                                   ->ListTabs();
  ASSERT_EQ(tab_range.length(), 1u);

  EXPECT_EQ(browser()->tab_strip_model()->GetWebContentsAt(0)->GetURL(),
            GURL("chrome://newtab"));
}

IN_PROC_BROWSER_TEST_F(TabGroupSyncDelegateBrowserTest,
                       RemoveLastTabFromSyncKeepsGroupAndAddsPendingNTP) {
  chrome::AddTabAt(browser(), GURL("chrome://history"), 1, false);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);

  // Create a new tab group with a tab.
  LocalTabGroupID local_id = browser()->tab_strip_model()->AddToNewGroup({1});
  ASSERT_TRUE(
      browser()->tab_strip_model()->group_model()->ContainsTabGroup(local_id));

  const SavedTabGroup* saved_group = model_->Get(local_id);
  ASSERT_TRUE(saved_group);
  WaitUntilCallbackReceived();

  model_->RemoveTabFromGroupFromSync(
      saved_group->saved_guid(),
      saved_group->saved_tabs().at(0).saved_tab_guid());
  WaitUntilCallbackReceived();

  // Verify the last tab is still open in the tab group.
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);
  const gfx::Range tab_range = browser()
                                   ->tab_strip_model()
                                   ->group_model()
                                   ->GetTabGroup(local_id)
                                   ->ListTabs();
  ASSERT_EQ(tab_range.length(), 1u);

  // Verify the chrome://history tab has a pending ntp entry in the saved group.
  EXPECT_EQ(saved_group->saved_tabs().size(), 1u);
  EXPECT_TRUE(saved_group->saved_tabs()[0].is_pending_ntp());
}

IN_PROC_BROWSER_TEST_F(TabGroupSyncDelegateBrowserTest,
                       RemoveGroupFromSyncRemovesLocalTabGroup) {
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), 0, false);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);

  // Create a new tab group with 1 tab.
  LocalTabGroupID local_id = browser()->tab_strip_model()->AddToNewGroup({1});
  ASSERT_TRUE(
      browser()->tab_strip_model()->group_model()->ContainsTabGroup(local_id));

  std::optional<SavedTabGroup> saved_group = service_->GetGroup(local_id);
  ASSERT_TRUE(saved_group);

  model_->RemovedFromSync(saved_group->saved_guid());
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return !browser()->tab_strip_model()->group_model()->ContainsTabGroup(
        local_id);
  }));

  // Verify that the tab_group was removed from the browser..
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);
  EXPECT_FALSE(
      browser()->tab_strip_model()->group_model()->ContainsTabGroup(local_id));
  EXPECT_FALSE(model_->Contains(local_id));
}

IN_PROC_BROWSER_TEST_F(TabGroupSyncDelegateBrowserTest,
                       TabReorderedLocallyUpdateSavedTabGroupTabOrder) {
  chrome::AddTabAt(browser(), GURL("https://google.com"), 1, false);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);
  LocalTabGroupID group_id =
      browser()->tab_strip_model()->AddToNewGroup({0, 1});

  const SavedTabGroup* saved_group = model_->Get(group_id);
  ASSERT_TRUE(saved_group);

  LocalTabID first_tab_id =
      browser()->tab_strip_model()->GetTabAtIndex(0)->GetHandle().raw_value();
  LocalTabID second_tab_id =
      browser()->tab_strip_model()->GetTabAtIndex(1)->GetHandle().raw_value();

  ASSERT_EQ(2u, saved_group->saved_tabs().size());
  EXPECT_EQ(saved_group->saved_tabs()[0].local_tab_id().value(), first_tab_id);
  EXPECT_EQ(saved_group->saved_tabs()[1].local_tab_id().value(), second_tab_id);

  // Move the first tab to the right of the second tab.
  browser()->tab_strip_model()->MoveWebContentsAt(0, 1, true, group_id);

  EXPECT_EQ(saved_group->saved_tabs()[0].local_tab_id().value(), second_tab_id);
  EXPECT_EQ(saved_group->saved_tabs()[1].local_tab_id().value(), first_tab_id);

  chrome::AddTabAt(browser(), GURL("https://google.com"), 2, false);
  browser()->tab_strip_model()->MoveGroupTo(group_id, 1);

  EXPECT_EQ(saved_group->saved_tabs()[0].local_tab_id().value(), second_tab_id);
  EXPECT_EQ(saved_group->saved_tabs()[1].local_tab_id().value(), first_tab_id);
}

IN_PROC_BROWSER_TEST_F(TabGroupSyncDelegateBrowserTest, ReorderDiscardedTab) {
  chrome::AddTabAt(browser(), GURL("https://google.com"), 1, false);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);
  LocalTabGroupID group_id =
      browser()->tab_strip_model()->AddToNewGroup({0, 1});

  const SavedTabGroup* saved_group = model_->Get(group_id);
  ASSERT_TRUE(saved_group);

  LocalTabID first_tab_id =
      browser()->tab_strip_model()->GetTabAtIndex(0)->GetHandle().raw_value();
  LocalTabID second_tab_id =
      browser()->tab_strip_model()->GetTabAtIndex(1)->GetHandle().raw_value();

  ASSERT_EQ(2u, saved_group->saved_tabs().size());
  EXPECT_EQ(saved_group->saved_tabs()[0].local_tab_id().value(), first_tab_id);
  EXPECT_EQ(saved_group->saved_tabs()[1].local_tab_id().value(), second_tab_id);

  // Discard the first tab and move it to the right of the second tab.
  std::unique_ptr<content::WebContents> replacement_web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile()));
  browser()->tab_strip_model()->DiscardWebContentsAt(
      0, std::move(replacement_web_contents));
  browser()->tab_strip_model()->MoveWebContentsAt(0, 1, true, group_id);

  EXPECT_EQ(saved_group->saved_tabs()[0].local_tab_id().value(), second_tab_id);
  EXPECT_EQ(saved_group->saved_tabs()[1].local_tab_id().value(), first_tab_id);
}

IN_PROC_BROWSER_TEST_F(TabGroupSyncDelegateBrowserTest,
                       TabGroupsPinnedByDefaultOnCreation) {
  chrome::AddTabAt(browser(), GURL("https://google.com"), 1, false);
  chrome::AddTabAt(browser(), GURL("https://google.com"), 2, false);

  ASSERT_EQ(browser()->tab_strip_model()->count(), 3);
  LocalTabGroupID group_id_1 = browser()->tab_strip_model()->AddToNewGroup({0});
  LocalTabGroupID group_id_2 = browser()->tab_strip_model()->AddToNewGroup({1});
  LocalTabGroupID group_id_3 = browser()->tab_strip_model()->AddToNewGroup({2});

  std::vector<SavedTabGroup> saved_groups = model_->saved_tab_groups();

  // Verify all groups are pinned by default.
  EXPECT_TRUE(saved_groups[0].is_pinned());
  EXPECT_TRUE(saved_groups[1].is_pinned());
  EXPECT_TRUE(saved_groups[2].is_pinned());

  // Pinning reverses the saving order.
  EXPECT_EQ(saved_groups[0].local_group_id(), group_id_3);
  EXPECT_EQ(saved_groups[1].local_group_id(), group_id_2);
  EXPECT_EQ(saved_groups[2].local_group_id(), group_id_1);
}

IN_PROC_BROWSER_TEST_F(TabGroupSyncDelegateBrowserTest,
                       ReorderTabFromSyncReordersLocalTab) {
  chrome::AddTabAt(browser(), GURL("https://google.com"), 1, false);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);
  LocalTabGroupID group_id =
      browser()->tab_strip_model()->AddToNewGroup({0, 1});

  const SavedTabGroup* saved_group = model_->Get(group_id);
  ASSERT_TRUE(saved_group);

  LocalTabID first_tab_id =
      browser()->tab_strip_model()->GetTabAtIndex(0)->GetHandle().raw_value();
  LocalTabID second_tab_id =
      browser()->tab_strip_model()->GetTabAtIndex(1)->GetHandle().raw_value();

  ASSERT_EQ(2u, saved_group->saved_tabs().size());
  EXPECT_EQ(saved_group->saved_tabs()[0].local_tab_id().value(), first_tab_id);
  EXPECT_EQ(saved_group->saved_tabs()[1].local_tab_id().value(), second_tab_id);

  // Move the first tab after the second. And the second before the first.
  SavedTabGroupTab tab_1 = *saved_group->GetTab(first_tab_id);
  SavedTabGroupTab tab_2 = *saved_group->GetTab(second_tab_id);
  tab_1.SetPosition(1);
  tab_2.SetPosition(0);
  model_->MergeRemoteTab(std::move(tab_1));
  model_->MergeRemoteTab(std::move(tab_2));
  WaitUntilCallbackReceived();
  WaitUntilCallbackReceived();

  // Verify the positions are correct.
  ASSERT_EQ(2u, saved_group->saved_tabs().size());
  EXPECT_EQ(saved_group->saved_tabs()[0].local_tab_id().value(), second_tab_id);
  EXPECT_EQ(saved_group->saved_tabs()[1].local_tab_id().value(), first_tab_id);
}

IN_PROC_BROWSER_TEST_F(TabGroupSyncDelegateBrowserTest,
                       OpenNewTabAndNavigateExistingOnConnectNewSavedGroup) {
  // Create a new tab group with one tab. The tab group should be saved.
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), 0, false);
  LocalTabGroupID local_id = browser()->tab_strip_model()->AddToNewGroup({0});
  ASSERT_TRUE(
      browser()->tab_strip_model()->group_model()->ContainsTabGroup(local_id));
  ASSERT_TRUE(service_->GetGroup(local_id));

  // Unsave the local group.
  service_->UnsaveGroup(local_id);
  ASSERT_TRUE(
      browser()->tab_strip_model()->group_model()->ContainsTabGroup(local_id));
  ASSERT_FALSE(service_->GetGroup(local_id));

  // Connect the local tab group with a new saved group.
  SavedTabGroup new_saved_group(u"Title", TabGroupColorId::kBlue, {}, 0);
  new_saved_group.AddTabLocally(SavedTabGroupTab(
      GURL("http://www.google.com/1"), u"title 1", new_saved_group.saved_guid(),
      /*position=*/0));
  new_saved_group.AddTabLocally(SavedTabGroupTab(
      GURL("http://www.google.com/2"), u"title 2", new_saved_group.saved_guid(),
      /*position=*/1));
  service_->AddGroup(new_saved_group);

  service_->ConnectLocalTabGroup(new_saved_group.saved_guid(), local_id,
                                 OpeningSource::kUnknown);

  // Local group model should be updated to match the new saved group.
  ASSERT_TRUE(
      browser()->tab_strip_model()->group_model()->ContainsTabGroup(local_id));
  const TabGroup* local_tab_group =
      browser()->tab_strip_model()->group_model()->GetTabGroup(local_id);

  EXPECT_EQ(local_tab_group->visual_data()->title(), u"Title");
  EXPECT_EQ(local_tab_group->visual_data()->color(), TabGroupColorId::kBlue);
  EXPECT_EQ(local_tab_group->ListTabs().length(), 2u);

  // iterate through all of the tabs activating them so that deferred
  // navigations are resolved.
  const gfx::Range tab_range = local_tab_group->ListTabs();
  for (size_t i = tab_range.start(); i < tab_range.end(); ++i) {
    browser()->tab_strip_model()->ActivateTabAt(i);
  }

  // Verify that a new tab was added, and the existing one navigated to the
  // correct URL.
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
  // Create a new tab group with one tab. The tab group should be saved.
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), 0, false);
  chrome::AddTabAt(browser(), GURL("about:blank"), 1, false);
  LocalTabGroupID local_id =
      browser()->tab_strip_model()->AddToNewGroup({0, 1});
  ASSERT_TRUE(
      browser()->tab_strip_model()->group_model()->ContainsTabGroup(local_id));
  ASSERT_TRUE(service_->GetGroup(local_id));

  // Unsave the local group.
  service_->UnsaveGroup(local_id);
  ASSERT_TRUE(
      browser()->tab_strip_model()->group_model()->ContainsTabGroup(local_id));
  ASSERT_FALSE(service_->GetGroup(local_id));

  // Connect the local tab group with a new saved group.
  SavedTabGroup new_saved_group(u"Title", TabGroupColorId::kBlue, {}, 0);
  new_saved_group.AddTabLocally(SavedTabGroupTab(
      GURL("http://www.google.com/1"), u"title 1", new_saved_group.saved_guid(),
      /*position=*/0));
  service_->AddGroup(new_saved_group);

  service_->ConnectLocalTabGroup(new_saved_group.saved_guid(), local_id,
                                 OpeningSource::kUnknown);

  // Local group model should be updated to match the new saved group.
  ASSERT_TRUE(
      browser()->tab_strip_model()->group_model()->ContainsTabGroup(local_id));
  const TabGroup* local_tab_group =
      browser()->tab_strip_model()->group_model()->GetTabGroup(local_id);

  EXPECT_EQ(local_tab_group->visual_data()->title(), u"Title");
  EXPECT_EQ(local_tab_group->visual_data()->color(), TabGroupColorId::kBlue);

  // iterate through all of the tabs activating them so that deferred
  // navigations are resolved.
  const gfx::Range tab_range = local_tab_group->ListTabs();
  for (size_t i = tab_range.start(); i < tab_range.end(); ++i) {
    browser()->tab_strip_model()->ActivateTabAt(i);
  }

  // Verify that only one tab remains and it's navigated to the correct URL.
  ASSERT_EQ(tab_range.length(), 1u);
  EXPECT_EQ(browser()
                ->tab_strip_model()
                ->GetWebContentsAt(tab_range.start())
                ->GetURL(),
            GURL("http://www.google.com/1"));
}

// SaveTabGroup with position set is always placed before the one without
// position set. If both have position set, the one with lower position number
// should place before. If both positions are the same or both are not set, the
// one with more recent update time should place before.
// Regression test. See crbug.com/370013915.
IN_PROC_BROWSER_TEST_F(
    TabGroupSyncDelegateBrowserTest,
    GroupsWithIndicesOutsideLocalIndexRangeInsertedAtTheEnd) {
  auto saved_tab_group_bar =
      std::make_unique<SavedTabGroupBar>(browser(), service_, false);
  EXPECT_EQ(1u, saved_tab_group_bar->children().size());

  chrome::AddTabAt(browser(), GURL("chrome://newtab"), 0, false);
  LocalTabGroupID local_id = browser()->tab_strip_model()->AddToNewGroup({0});
  EXPECT_TRUE(
      browser()->tab_strip_model()->group_model()->ContainsTabGroup(local_id));
  WaitUntilCallbackReceived();

  // The first group is automatically set to 0. it should be first in the list.
  std::optional<SavedTabGroup> locally_created_group_at_0 =
      service_->GetGroup(local_id);
  ASSERT_TRUE(locally_created_group_at_0);
  ASSERT_EQ(0, locally_created_group_at_0->position());
  EXPECT_EQ(2u, saved_tab_group_bar->children().size());

  SavedTabGroup group_with_position_set_2(/*title=*/u"Group 2",
                                          /*color=*/TabGroupColorId::kPink,
                                          /*urls=*/{},
                                          /*position=*/2);
  SavedTabGroupTab tab2(GURL("about:blank"), u"about:blank",
                        group_with_position_set_2.saved_guid(),
                        /*position=*/0);
  group_with_position_set_2.AddTabLocally(tab2);

  SavedTabGroup group_with_position_set_10(u"Group 3", TabGroupColorId::kGreen,
                                           {}, 10);
  SavedTabGroupTab tab3(GURL("about:blank"), u"about:blank",
                        group_with_position_set_10.saved_guid(),
                        /*position=*/0);
  group_with_position_set_10.AddTabLocally(tab3);

  const base::Uuid sync_id_1 = locally_created_group_at_0->saved_guid();
  const base::Uuid sync_id_2 = group_with_position_set_2.saved_guid();
  const base::Uuid sync_id_3 = group_with_position_set_10.saved_guid();

  // FromSync calls are asynchronous, so wait for the task to complete.
  model_->AddedFromSync(std::move(group_with_position_set_10));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return service_->GetGroup(sync_id_3).has_value(); }));
  EXPECT_EQ(3u, saved_tab_group_bar->children().size());

  model_->AddedFromSync(std::move(group_with_position_set_2));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return service_->GetGroup(sync_id_2).has_value(); }));
  EXPECT_EQ(4u, saved_tab_group_bar->children().size());

  // Make sure positions werent updated.
  ASSERT_EQ(service_->GetGroup(sync_id_1).value().position(), 0);
  ASSERT_EQ(service_->GetGroup(sync_id_2).value().position(), 2);
  ASSERT_EQ(service_->GetGroup(sync_id_3).value().position(), 10);

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
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), 0, false);
  const LocalTabGroupID local_id =
      browser()->tab_strip_model()->AddToNewGroup({0});

  TabGroup* local_group =
      browser()->tab_strip_model()->group_model()->GetTabGroup(local_id);
  ASSERT_THAT(local_group, NotNull());
  browser()->tab_strip_model()->ChangeTabGroupVisuals(
      local_id,
      TabGroupVisualData(u"Title", TabGroupColorId::kBlue,
                         /*is_collapsed=*/true),
      /*is_customized=*/true);

  // Verify that the group is saved.
  std::optional<SavedTabGroup> saved_group = service_->GetGroup(local_id);
  ASSERT_TRUE(saved_group.has_value());

  // Simulate a remote update to the group.
  model_->MergeRemoteGroupMetadata(
      saved_group->saved_guid(), u"title", TabGroupColorId::kRed,
      /*position=*/std::nullopt, /*creator_cache_guid=*/std::nullopt,
      /*last_updater_cache_guid=*/std::nullopt,
      /*update_time=*/base::Time::Now(),
      /*updated_by=*/GaiaId());

  // Wait for the tab group update to complete because sync updates are
  // asynchronous.
  ASSERT_TRUE(base::test::RunUntil([local_group]() {
    return local_group->visual_data()->color() == TabGroupColorId::kRed;
  }));

  // The local group should still be collapsed.
  EXPECT_TRUE(local_group->visual_data()->is_collapsed());
}

IN_PROC_BROWSER_TEST_F(TabGroupSyncDelegateBrowserTest,
                       TabRemovalsFromSyncDontCauseZeroTabStateInLocal) {
  // Starts with one tab.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Add a tab and create a group.
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), 0, false);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  const LocalTabGroupID local_id =
      browser()->tab_strip_model()->AddToNewGroup({0});

  TabGroup* local_group =
      browser()->tab_strip_model()->group_model()->GetTabGroup(local_id);
  ASSERT_THAT(local_group, NotNull());
  ASSERT_EQ(1, local_group->tab_count());
  browser()->tab_strip_model()->ChangeTabGroupVisuals(
      local_id,
      TabGroupVisualData(u"Title", TabGroupColorId::kBlue,
                         /*is_collapsed=*/true),
      /*is_customized=*/true);

  // Verify that the group is saved and has exactly one tab.
  WaitUntilCallbackReceived();
  std::optional<SavedTabGroup> saved_group = service_->GetGroup(local_id);
  ASSERT_TRUE(saved_group.has_value());
  ASSERT_EQ(1u, saved_group->saved_tabs().size());
  base::Uuid saved_group_id = saved_group->saved_guid();
  base::Uuid saved_tab_id = saved_group->saved_tabs()[0].saved_tab_guid();

  // Simulate three updates received by sync: one tab removal, two tab
  // additions. This could generate transient zero tab state but shouldn't close
  // the group locally. We send the addition first and removal next because this
  // is the order merges are sent from bridge. If removal is sent first, model
  // will delete the group instead for last tab closure.
  const SavedTabGroupTab added_tab1(GURL(chrome::kChromeUINewTabURL),
                                    u"New Tab 1", saved_group_id,
                                    /*position=*/0);
  const SavedTabGroupTab added_tab2(GURL(chrome::kChromeUINewTabURL),
                                    u"New Tab 2", saved_group_id,
                                    /*position=*/1);
  model_->AddTabToGroupFromSync(saved_group_id, added_tab1);
  model_->AddTabToGroupFromSync(saved_group_id, added_tab2);
  model_->RemoveTabFromGroupFromSync(saved_group_id, saved_tab_id);
  WaitUntilCallbackReceived();
  WaitUntilCallbackReceived();

  saved_group = service_->GetGroup(local_id);
  ASSERT_EQ(2u, saved_group->saved_tabs().size());

  // Verify that the group still exists in the tab strip and has 2 tabs in
  // total.
  EXPECT_EQ(2u, SavedTabGroupUtils::GetTabsInGroup(local_id).size());
  EXPECT_EQ(3, browser()->tab_strip_model()->count());
}

}  // namespace
}  // namespace tab_groups
