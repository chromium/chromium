// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/organizer_panel/tab_groups_organizer_page_handler.h"

#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/run_loop.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chrome/browser/collaboration/messaging/messaging_backend_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/organizer_panel/tab_groups.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/collaboration/public/messaging/empty_messaging_backend_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/utils.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace {

class FakeTabGroupsOrganizerPage
    : public organizer_panel::mojom::TabGroupsOrganizerPage {
 public:
  mojo::PendingRemote<organizer_panel::mojom::TabGroupsOrganizerPage>
  BindAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void TabGroupAdded(organizer_panel::mojom::TabGroupPtr tab_group) override {
    added_groups_.push_back(std::move(tab_group));
    if (added_quit_closure_ && added_groups_.size() >= expected_added_count_) {
      std::move(added_quit_closure_).Run();
    }
  }

  void TabGroupRemoved(const std::string& id) override {
    removed_group_ids_.push_back(id);
    if (removed_quit_closure_ &&
        removed_group_ids_.size() >= expected_removed_count_) {
      std::move(removed_quit_closure_).Run();
    }
  }

  void TabGroupUpdated(organizer_panel::mojom::TabGroupPtr tab_group) override {
    updated_groups_.push_back(std::move(tab_group));
    if (updated_quit_closure_ &&
        updated_groups_.size() >= expected_updated_count_) {
      std::move(updated_quit_closure_).Run();
    }
  }

  void WaitForTabGroupAdded() { WaitForAddedCount(added_groups_.size() + 1); }

  void WaitForAddedCount(size_t count) {
    if (added_groups_.size() >= count) {
      return;
    }
    expected_added_count_ = count;
    base::RunLoop run_loop;
    added_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void WaitForTabGroupRemoved() {
    WaitForRemovedCount(removed_group_ids_.size() + 1);
  }

  void WaitForRemovedCount(size_t count) {
    if (removed_group_ids_.size() >= count) {
      return;
    }
    expected_removed_count_ = count;
    base::RunLoop run_loop;
    removed_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void WaitForTabGroupUpdated() {
    WaitForUpdatedCount(updated_groups_.size() + 1);
  }

  void WaitForUpdatedCount(size_t count) {
    if (updated_groups_.size() >= count) {
      return;
    }
    expected_updated_count_ = count;
    base::RunLoop run_loop;
    updated_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  const std::vector<organizer_panel::mojom::TabGroupPtr>& added_groups() const {
    return added_groups_;
  }
  const std::vector<std::string>& removed_group_ids() const {
    return removed_group_ids_;
  }
  const std::vector<organizer_panel::mojom::TabGroupPtr>& updated_groups()
      const {
    return updated_groups_;
  }

 private:
  mojo::Receiver<organizer_panel::mojom::TabGroupsOrganizerPage> receiver_{
      this};
  std::vector<organizer_panel::mojom::TabGroupPtr> added_groups_;
  std::vector<std::string> removed_group_ids_;
  std::vector<organizer_panel::mojom::TabGroupPtr> updated_groups_;
  base::OnceClosure added_quit_closure_;
  base::OnceClosure removed_quit_closure_;
  base::OnceClosure updated_quit_closure_;
  size_t expected_added_count_ = 0;
  size_t expected_removed_count_ = 0;
  size_t expected_updated_count_ = 0;
};

class TabGroupsOrganizerPageHandlerBrowserTest : public InProcessBrowserTest {
 public:
  TabGroupsOrganizerPageHandlerBrowserTest() {
    dependency_manager_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &TabGroupsOrganizerPageHandlerBrowserTest::RegisterFakeServices,
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

  tab_groups::TabGroupSyncService* sync_service() {
    return tab_groups::TabGroupSyncServiceFactory::GetForProfile(
        browser()->GetProfile());
  }

 private:
  base::CallbackListSubscription dependency_manager_subscription_;
};

IN_PROC_BROWSER_TEST_F(TabGroupsOrganizerPageHandlerBrowserTest,
                       GetTabGroupsSortedByMostRecentlyUsed) {
  tab_groups::TabGroupSyncService* service = sync_service();
  ASSERT_TRUE(service);

  base::Time now = base::Time::Now();

  // Group 1: Created 10 minutes ago, but interacted with most recently (1 min
  // ago).
  base::Uuid id1 = base::Uuid::GenerateRandomV4();
  tab_groups::SavedTabGroupTab tab1(GURL("https://www.google.com"), u"Google",
                                    id1, /*position=*/0);
  tab_groups::SavedTabGroup group1(
      u"Group 1", tab_groups::TabGroupColorId::kBlue, {tab1},
      /*position=*/std::nullopt, id1,
      /*local_group_id=*/tab_groups::TabGroupId::GenerateNew(),
      /*creator_cache_guid=*/std::nullopt,
      /*last_updater_cache_guid=*/std::nullopt,
      /*created_before_syncing_tab_groups=*/false,
      /*creation_time=*/now - base::Minutes(10),
      /*update_time=*/now - base::Minutes(10));
  group1.SetLastUserInteractionTime(now - base::Minutes(1));

  // Group 2: Created 5 minutes ago, no explicit user interaction time set
  // (falls back to update_time of 5 mins ago).
  base::Uuid id2 = base::Uuid::GenerateRandomV4();
  tab_groups::SavedTabGroupTab tab2(GURL("https://www.youtube.com"), u"YouTube",
                                    id2, /*position=*/0);
  tab_groups::SavedTabGroup group2(
      u"Group 2", tab_groups::TabGroupColorId::kRed, {tab2},
      /*position=*/std::nullopt, id2, /*local_group_id=*/std::nullopt,
      /*creator_cache_guid=*/std::nullopt,
      /*last_updater_cache_guid=*/std::nullopt,
      /*created_before_syncing_tab_groups=*/false,
      /*creation_time=*/now - base::Minutes(5),
      /*update_time=*/now - base::Minutes(5));

  // Group 3: Created 2 minutes ago, interacted with 8 minutes ago (least
  // recently used).
  base::Uuid id3 = base::Uuid::GenerateRandomV4();
  tab_groups::SavedTabGroupTab tab3(GURL("https://www.chromium.org"),
                                    u"Chromium", id3, /*position=*/0);
  tab_groups::SavedTabGroup group3(
      u"Group 3", tab_groups::TabGroupColorId::kGreen, {tab3},
      /*position=*/std::nullopt, id3, /*local_group_id=*/std::nullopt,
      /*creator_cache_guid=*/std::nullopt,
      /*last_updater_cache_guid=*/std::nullopt,
      /*created_before_syncing_tab_groups=*/false,
      /*creation_time=*/now - base::Minutes(2),
      /*update_time=*/now - base::Minutes(2));
  group3.SetLastUserInteractionTime(now - base::Minutes(8));

  service->AddGroup(group1);
  service->AddGroup(group2);
  service->AddGroup(group3);

  FakeTabGroupsOrganizerPage page;
  mojo::Remote<organizer_panel::mojom::TabGroupsOrganizerPageHandler>
      handler_remote;
  TabGroupsOrganizerPageHandler handler(
      handler_remote.BindNewPipeAndPassReceiver(), page.BindAndPassRemote(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  base::test::TestFuture<std::vector<organizer_panel::mojom::TabGroupPtr>>
      future;
  handler_remote->GetTabGroups(future.GetCallback());

  const std::vector<organizer_panel::mojom::TabGroupPtr>& returned_groups =
      future.Get();

  ASSERT_EQ(3u, returned_groups.size());

  // Groups are returned sorted by most recently used descending.
  EXPECT_EQ(returned_groups[0]->id, id1.AsLowercaseString());
  EXPECT_EQ(returned_groups[0]->title, "Group 1");
  EXPECT_EQ(returned_groups[0]->color, tab_groups::TabGroupColorId::kBlue);

  EXPECT_EQ(returned_groups[1]->id, id2.AsLowercaseString());
  EXPECT_EQ(returned_groups[1]->title, "Group 2");
  EXPECT_EQ(returned_groups[1]->color, tab_groups::TabGroupColorId::kRed);

  EXPECT_EQ(returned_groups[2]->id, id3.AsLowercaseString());
  EXPECT_EQ(returned_groups[2]->title, "Group 3");
  EXPECT_EQ(returned_groups[2]->color, tab_groups::TabGroupColorId::kGreen);
}

IN_PROC_BROWSER_TEST_F(TabGroupsOrganizerPageHandlerBrowserTest,
                       OpenTabGroupOpensClosedGroupInTabStrip) {
  tab_groups::TabGroupSyncService* service = sync_service();
  ASSERT_TRUE(service);

  base::Uuid id = base::Uuid::GenerateRandomV4();
  tab_groups::SavedTabGroupTab tab(GURL("https://www.google.com"), u"Google",
                                   id, /*position=*/0);
  tab_groups::SavedTabGroup group(
      u"Closed Group", tab_groups::TabGroupColorId::kBlue, {tab},
      /*position=*/std::nullopt, id, /*local_group_id=*/std::nullopt,
      /*creator_cache_guid=*/std::nullopt,
      /*last_updater_cache_guid=*/std::nullopt,
      /*created_before_syncing_tab_groups=*/false);
  service->AddGroup(group);

  ASSERT_FALSE(service->GetGroup(id)->local_group_id().has_value());

  FakeTabGroupsOrganizerPage page;
  mojo::Remote<organizer_panel::mojom::TabGroupsOrganizerPageHandler>
      handler_remote;
  TabGroupsOrganizerPageHandler handler(
      handler_remote.BindNewPipeAndPassReceiver(), page.BindAndPassRemote(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  handler_remote->OpenTabGroup(id.AsLowercaseString());
  handler_remote.FlushForTesting();

  std::optional<tab_groups::SavedTabGroup> opened_group = service->GetGroup(id);
  ASSERT_TRUE(opened_group.has_value());
  ASSERT_TRUE(opened_group->local_group_id().has_value());
  EXPECT_TRUE(browser()->GetTabStripModel()->group_model()->ContainsTabGroup(
      opened_group->local_group_id().value()));
}

IN_PROC_BROWSER_TEST_F(TabGroupsOrganizerPageHandlerBrowserTest,
                       OpenTabGroupFocusesAlreadyOpenGroup) {
  tab_groups::TabGroupSyncService* service = sync_service();
  ASSERT_TRUE(service);

  // Add a second tab so we have two tabs.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_EQ(2, browser()->GetTabStripModel()->count());

  // Put tab 0 into a tab group.
  tab_groups::TabGroupId local_group_id =
      browser()->GetTabStripModel()->AddToNewGroup({0});

  // Ensure tab 1 (outside the group) is the active tab.
  browser()->GetTabStripModel()->ActivateTabAt(1);
  ASSERT_EQ(1, browser()->GetTabStripModel()->active_index());

  std::optional<tab_groups::SavedTabGroup> saved_group =
      service->GetGroup(local_group_id);
  ASSERT_TRUE(saved_group.has_value());

  FakeTabGroupsOrganizerPage page;
  mojo::Remote<organizer_panel::mojom::TabGroupsOrganizerPageHandler>
      handler_remote;
  TabGroupsOrganizerPageHandler handler(
      handler_remote.BindNewPipeAndPassReceiver(), page.BindAndPassRemote(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  handler_remote->OpenTabGroup(saved_group->saved_guid().AsLowercaseString());
  handler_remote.FlushForTesting();

  // Clicking an open group should activate the first tab in that group (tab 0).
  EXPECT_EQ(0, browser()->GetTabStripModel()->active_index());
}

IN_PROC_BROWSER_TEST_F(TabGroupsOrganizerPageHandlerBrowserTest,
                       NotifiesPageOnTabGroupAdded) {
  tab_groups::TabGroupSyncService* service = sync_service();
  ASSERT_TRUE(service);

  FakeTabGroupsOrganizerPage page;
  mojo::Remote<organizer_panel::mojom::TabGroupsOrganizerPageHandler>
      handler_remote;
  TabGroupsOrganizerPageHandler handler(
      handler_remote.BindNewPipeAndPassReceiver(), page.BindAndPassRemote(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  base::Uuid id = base::Uuid::GenerateRandomV4();
  tab_groups::SavedTabGroupTab tab(GURL("https://www.google.com"), u"Google",
                                   id, /*position=*/0);
  tab_groups::SavedTabGroup group(u"New Group",
                                  tab_groups::TabGroupColorId::kYellow, {tab},
                                  /*position=*/std::nullopt, id);

  service->AddGroup(group);
  page.WaitForTabGroupAdded();

  ASSERT_EQ(1u, page.added_groups().size());
  EXPECT_EQ(page.added_groups()[0]->id, id.AsLowercaseString());
  EXPECT_EQ(page.added_groups()[0]->title, "New Group");
  EXPECT_EQ(page.added_groups()[0]->color,
            tab_groups::TabGroupColorId::kYellow);
}

IN_PROC_BROWSER_TEST_F(TabGroupsOrganizerPageHandlerBrowserTest,
                       NotifiesPageOnTabGroupRemoved) {
  tab_groups::TabGroupSyncService* service = sync_service();
  ASSERT_TRUE(service);

  base::Uuid id = base::Uuid::GenerateRandomV4();
  tab_groups::SavedTabGroupTab tab(GURL("https://www.google.com"), u"Google",
                                   id, /*position=*/0);
  tab_groups::SavedTabGroup group(u"Group", tab_groups::TabGroupColorId::kBlue,
                                  {tab}, /*position=*/std::nullopt, id);
  service->AddGroup(group);

  FakeTabGroupsOrganizerPage page;
  mojo::Remote<organizer_panel::mojom::TabGroupsOrganizerPageHandler>
      handler_remote;
  TabGroupsOrganizerPageHandler handler(
      handler_remote.BindNewPipeAndPassReceiver(), page.BindAndPassRemote(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  service->RemoveGroup(id);
  page.WaitForTabGroupRemoved();

  ASSERT_EQ(1u, page.removed_group_ids().size());
  EXPECT_EQ(page.removed_group_ids()[0], id.AsLowercaseString());
}

IN_PROC_BROWSER_TEST_F(TabGroupsOrganizerPageHandlerBrowserTest,
                       NotifiesPageOnTabGroupUpdated) {
  tab_groups::TabGroupSyncService* service = sync_service();
  ASSERT_TRUE(service);

  base::Uuid id = base::Uuid::GenerateRandomV4();
  tab_groups::TabGroupId local_id = tab_groups::TabGroupId::GenerateNew();
  tab_groups::SavedTabGroupTab tab(GURL("https://www.google.com"), u"Google",
                                   id, /*position=*/0);
  tab_groups::SavedTabGroup group(u"Original Title",
                                  tab_groups::TabGroupColorId::kBlue, {tab},
                                  /*position=*/std::nullopt, id, local_id);
  service->AddGroup(group);

  FakeTabGroupsOrganizerPage page;
  mojo::Remote<organizer_panel::mojom::TabGroupsOrganizerPageHandler>
      handler_remote;
  TabGroupsOrganizerPageHandler handler(
      handler_remote.BindNewPipeAndPassReceiver(), page.BindAndPassRemote(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  tab_groups::TabGroupVisualData visual_data(u"Updated Title",
                                             tab_groups::TabGroupColorId::kRed);
  service->UpdateVisualData(local_id, &visual_data);
  page.WaitForTabGroupUpdated();

  ASSERT_EQ(1u, page.updated_groups().size());
  EXPECT_EQ(page.updated_groups()[0]->id, id.AsLowercaseString());
  EXPECT_EQ(page.updated_groups()[0]->title, "Updated Title");
  EXPECT_EQ(page.updated_groups()[0]->color, tab_groups::TabGroupColorId::kRed);
}

IN_PROC_BROWSER_TEST_F(TabGroupsOrganizerPageHandlerBrowserTest,
                       ShowContextMenu) {
  tab_groups::TabGroupSyncService* service = sync_service();
  ASSERT_TRUE(service);

  base::Uuid id = base::Uuid::GenerateRandomV4();
  tab_groups::SavedTabGroupTab tab(GURL("https://www.google.com"), u"Google",
                                   id, /*position=*/0);
  tab_groups::SavedTabGroup group(u"Group 1",
                                  tab_groups::TabGroupColorId::kBlue, {tab},
                                  /*position=*/std::nullopt, id);
  service->AddGroup(group);

  FakeTabGroupsOrganizerPage page;
  mojo::Remote<organizer_panel::mojom::TabGroupsOrganizerPageHandler>
      handler_remote;
  TabGroupsOrganizerPageHandler handler(
      handler_remote.BindNewPipeAndPassReceiver(), page.BindAndPassRemote(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  handler_remote->ShowContextMenu(id.AsLowercaseString(),
                                  gfx::Rect(10, 20, 30, 40), base::DoNothing());
  handler_remote.FlushForTesting();
  EXPECT_TRUE(handler.IsContextMenuRunningForTesting());
}

IN_PROC_BROWSER_TEST_F(TabGroupsOrganizerPageHandlerBrowserTest,
                       ShowContextMenuOutOfBoundsDoesNotShowMenu) {
  tab_groups::TabGroupSyncService* service = sync_service();
  ASSERT_TRUE(service);

  base::Uuid id = base::Uuid::GenerateRandomV4();
  tab_groups::SavedTabGroupTab tab(GURL("https://www.google.com"), u"Google",
                                   id, /*position=*/0);
  tab_groups::SavedTabGroup group(u"Group 1",
                                  tab_groups::TabGroupColorId::kBlue, {tab},
                                  /*position=*/std::nullopt, id);
  service->AddGroup(group);

  FakeTabGroupsOrganizerPage page;
  mojo::Remote<organizer_panel::mojom::TabGroupsOrganizerPageHandler>
      handler_remote;
  TabGroupsOrganizerPageHandler handler(
      handler_remote.BindNewPipeAndPassReceiver(), page.BindAndPassRemote(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  handler_remote->ShowContextMenu(
      id.AsLowercaseString(), gfx::Rect(-10, -20, 30, 40), base::DoNothing());
  handler_remote.FlushForTesting();
  EXPECT_FALSE(handler.IsContextMenuRunningForTesting());
}

IN_PROC_BROWSER_TEST_F(TabGroupsOrganizerPageHandlerBrowserTest,
                       GetTabGroupsIncludesNonSavedTabGroups) {
  tab_groups::TabGroupSyncService* service = sync_service();
  ASSERT_TRUE(service);

  base::Time now = base::Time::Now();

  // Add a saved tab group interacted with 5 minutes ago.
  base::Uuid saved_id = base::Uuid::GenerateRandomV4();
  tab_groups::SavedTabGroupTab saved_tab(GURL("https://www.google.com"),
                                         u"Google", saved_id, /*position=*/0);
  tab_groups::SavedTabGroup saved_group(
      u"Saved Group", tab_groups::TabGroupColorId::kBlue, {saved_tab},
      /*position=*/std::nullopt, saved_id, /*local_group_id=*/std::nullopt,
      /*creator_cache_guid=*/std::nullopt,
      /*last_updater_cache_guid=*/std::nullopt,
      /*created_before_syncing_tab_groups=*/false,
      /*creation_time=*/now - base::Minutes(5),
      /*update_time=*/now - base::Minutes(5));
  service->AddGroup(saved_group);

  // Create a local group in the tabstrip and remove it from sync service so it
  // represents a non-saved local group.
  tab_groups::TabGroupId local_group_id =
      browser()->GetTabStripModel()->AddToNewGroup({0});
  tab_groups::TabGroupVisualData visual_data(
      u"Unsaved Local Group", tab_groups::TabGroupColorId::kGreen);
  browser()->GetTabStripModel()->ChangeTabGroupVisuals(local_group_id,
                                                       visual_data);
  service->RemoveGroup(local_group_id);
  ASSERT_FALSE(service->GetGroup(local_group_id).has_value());

  FakeTabGroupsOrganizerPage page;
  mojo::Remote<organizer_panel::mojom::TabGroupsOrganizerPageHandler>
      handler_remote;
  TabGroupsOrganizerPageHandler handler(
      handler_remote.BindNewPipeAndPassReceiver(), page.BindAndPassRemote(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  base::test::TestFuture<std::vector<organizer_panel::mojom::TabGroupPtr>>
      future;
  handler_remote->GetTabGroups(future.GetCallback());

  const std::vector<organizer_panel::mojom::TabGroupPtr>& returned_groups =
      future.Get();

  ASSERT_EQ(2u, returned_groups.size());

  // The newly created local group's tab was active more recently than 5 mins
  // ago, so it should appear first.
  EXPECT_EQ(returned_groups[0]->id,
            tab_groups::LocalTabGroupIDToString(local_group_id));
  EXPECT_EQ(returned_groups[0]->title, "Unsaved Local Group");
  EXPECT_EQ(returned_groups[0]->color, tab_groups::TabGroupColorId::kGreen);
  EXPECT_TRUE(returned_groups[0]->is_open);

  EXPECT_EQ(returned_groups[1]->id, saved_id.AsLowercaseString());
  EXPECT_EQ(returned_groups[1]->title, "Saved Group");
  EXPECT_EQ(returned_groups[1]->color, tab_groups::TabGroupColorId::kBlue);
  EXPECT_FALSE(returned_groups[1]->is_open);
}

IN_PROC_BROWSER_TEST_F(TabGroupsOrganizerPageHandlerBrowserTest,
                       OpenTabGroupFocusesNonSavedGroup) {
  tab_groups::TabGroupSyncService* service = sync_service();
  ASSERT_TRUE(service);

  // Add a second tab so we have two tabs.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_EQ(2, browser()->GetTabStripModel()->count());

  // Put tab 0 into a tab group and remove it from sync service.
  tab_groups::TabGroupId local_group_id =
      browser()->GetTabStripModel()->AddToNewGroup({0});
  service->RemoveGroup(local_group_id);
  ASSERT_FALSE(service->GetGroup(local_group_id).has_value());

  // Ensure tab 1 (outside the group) is the active tab.
  browser()->GetTabStripModel()->ActivateTabAt(1);
  ASSERT_EQ(1, browser()->GetTabStripModel()->active_index());

  FakeTabGroupsOrganizerPage page;
  mojo::Remote<organizer_panel::mojom::TabGroupsOrganizerPageHandler>
      handler_remote;
  TabGroupsOrganizerPageHandler handler(
      handler_remote.BindNewPipeAndPassReceiver(), page.BindAndPassRemote(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  base::test::TestFuture<std::vector<organizer_panel::mojom::TabGroupPtr>>
      future;
  handler_remote->GetTabGroups(future.GetCallback());
  const std::vector<organizer_panel::mojom::TabGroupPtr>& returned_groups =
      future.Get();
  ASSERT_EQ(1u, returned_groups.size());

  handler_remote->OpenTabGroup(returned_groups[0]->id);
  handler_remote.FlushForTesting();

  // Clicking the non-saved open group should activate the first tab in that
  // group (tab 0).
  EXPECT_EQ(0, browser()->GetTabStripModel()->active_index());
}

IN_PROC_BROWSER_TEST_F(TabGroupsOrganizerPageHandlerBrowserTest,
                       NotifiesPageOnNonSavedTabGroupAddedUpdatedAndRemoved) {
  tab_groups::TabGroupSyncService* service = sync_service();
  ASSERT_TRUE(service);
  // Mark sync service as uninitialized so new local tab groups are not added to
  // the sync service model.
  service->SetIsInitializedForTesting(false);

  FakeTabGroupsOrganizerPage page;
  mojo::Remote<organizer_panel::mojom::TabGroupsOrganizerPageHandler>
      handler_remote;
  TabGroupsOrganizerPageHandler handler(
      handler_remote.BindNewPipeAndPassReceiver(), page.BindAndPassRemote(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  tab_groups::TabGroupId local_id =
      browser()->GetTabStripModel()->AddToNewGroup({0});
  ASSERT_FALSE(service->GetGroup(local_id).has_value());
  page.WaitForTabGroupAdded();

  ASSERT_EQ(1u, page.added_groups().size());
  std::string unsaved_id = page.added_groups()[0]->id;
  EXPECT_EQ(unsaved_id, tab_groups::LocalTabGroupIDToString(local_id));
  EXPECT_TRUE(page.added_groups()[0]->is_open);

  tab_groups::TabGroupVisualData visual_data(
      u"Updated Unsaved Group", tab_groups::TabGroupColorId::kPurple);
  browser()->GetTabStripModel()->ChangeTabGroupVisuals(local_id, visual_data);
  page.WaitForTabGroupUpdated();

  ASSERT_EQ(1u, page.updated_groups().size());
  EXPECT_EQ(page.updated_groups()[0]->id, unsaved_id);
  EXPECT_EQ(page.updated_groups()[0]->title, "Updated Unsaved Group");
  EXPECT_EQ(page.updated_groups()[0]->color,
            tab_groups::TabGroupColorId::kPurple);

  browser()->GetTabStripModel()->RemoveFromGroup({0});
  page.WaitForTabGroupRemoved();

  ASSERT_EQ(1u, page.removed_group_ids().size());
  EXPECT_EQ(page.removed_group_ids()[0], unsaved_id);
}

IN_PROC_BROWSER_TEST_F(TabGroupsOrganizerPageHandlerBrowserTest,
                       CloseAllTabsInGroupSendsSingleTabGroupRemoved) {
  tab_groups::TabGroupSyncService* service = sync_service();
  ASSERT_TRUE(service);
  service->SetIsInitializedForTesting(false);

  // Add a second tab so closing the group does not close the browser.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_EQ(2, browser()->GetTabStripModel()->count());

  FakeTabGroupsOrganizerPage page;
  mojo::Remote<organizer_panel::mojom::TabGroupsOrganizerPageHandler>
      handler_remote;
  TabGroupsOrganizerPageHandler handler(
      handler_remote.BindNewPipeAndPassReceiver(), page.BindAndPassRemote(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  tab_groups::TabGroupId local_id =
      browser()->GetTabStripModel()->AddToNewGroup({0});
  page.WaitForTabGroupAdded();
  ASSERT_EQ(1u, page.added_groups().size());
  std::string unsaved_id = page.added_groups()[0]->id;

  browser()->GetTabStripModel()->CloseAllTabsInGroup(local_id);
  page.WaitForTabGroupRemoved();

  handler_remote.FlushForTesting();
  EXPECT_EQ(1u, page.removed_group_ids().size());
  EXPECT_EQ(page.removed_group_ids()[0], unsaved_id);
}

IN_PROC_BROWSER_TEST_F(TabGroupsOrganizerPageHandlerBrowserTest,
                       LocalTabGroupTransitionsToSavedTabGroup) {
  tab_groups::TabGroupSyncService* service = sync_service();
  ASSERT_TRUE(service);
  service->SetIsInitializedForTesting(false);

  FakeTabGroupsOrganizerPage page;
  mojo::Remote<organizer_panel::mojom::TabGroupsOrganizerPageHandler>
      handler_remote;
  TabGroupsOrganizerPageHandler handler(
      handler_remote.BindNewPipeAndPassReceiver(), page.BindAndPassRemote(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  tab_groups::TabGroupId local_id =
      browser()->GetTabStripModel()->AddToNewGroup({0});
  page.WaitForAddedCount(1);

  ASSERT_EQ(1u, page.added_groups().size());
  std::string unsaved_id = page.added_groups()[0]->id;
  EXPECT_EQ(unsaved_id, tab_groups::LocalTabGroupIDToString(local_id));

  // Now simulate the sync service initializing, which reconciles and adds the
  // group to the sync service.
  service->SetIsInitializedForTesting(true);
  page.WaitForRemovedCount(1);
  page.WaitForAddedCount(2);

  // The local ID was removed, and the saved UUID was added.
  EXPECT_EQ(page.removed_group_ids()[0], unsaved_id);
  std::optional<tab_groups::SavedTabGroup> saved_group =
      service->GetGroup(local_id);
  ASSERT_TRUE(saved_group.has_value());
  EXPECT_EQ(page.added_groups()[1]->id,
            saved_group->saved_guid().AsLowercaseString());
}

IN_PROC_BROWSER_TEST_F(
    TabGroupsOrganizerPageHandlerBrowserTest,
    GetTabGroupsIncludesNonSavedTabGroupsAcrossMultipleWindows) {
  tab_groups::TabGroupSyncService* service = sync_service();
  ASSERT_TRUE(service);

  // Group in window 1.
  tab_groups::TabGroupId group_id1 =
      browser()->GetTabStripModel()->AddToNewGroup({0});
  tab_groups::TabGroupVisualData visual_data1(
      u"Window 1 Group", tab_groups::TabGroupColorId::kGreen);
  browser()->GetTabStripModel()->ChangeTabGroupVisuals(group_id1, visual_data1);
  service->RemoveGroup(group_id1);
  ASSERT_FALSE(service->GetGroup(group_id1).has_value());

  // Create window 2 and add a group in window 2.
  BrowserWindowInterface* browser2 = CreateBrowser(browser()->GetProfile());
  ASSERT_TRUE(browser2);
  ASSERT_EQ(1, browser2->GetTabStripModel()->count());

  tab_groups::TabGroupId group_id2 =
      browser2->GetTabStripModel()->AddToNewGroup({0});
  tab_groups::TabGroupVisualData visual_data2(
      u"Window 2 Group", tab_groups::TabGroupColorId::kBlue);
  browser2->GetTabStripModel()->ChangeTabGroupVisuals(group_id2, visual_data2);
  service->RemoveGroup(group_id2);
  ASSERT_FALSE(service->GetGroup(group_id2).has_value());

  FakeTabGroupsOrganizerPage page;
  mojo::Remote<organizer_panel::mojom::TabGroupsOrganizerPageHandler>
      handler_remote;
  TabGroupsOrganizerPageHandler handler(
      handler_remote.BindNewPipeAndPassReceiver(), page.BindAndPassRemote(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  base::test::TestFuture<std::vector<organizer_panel::mojom::TabGroupPtr>>
      future;
  handler_remote->GetTabGroups(future.GetCallback());

  const std::vector<organizer_panel::mojom::TabGroupPtr>& returned_groups =
      future.Get();

  ASSERT_EQ(2u, returned_groups.size());
  std::vector<std::string> returned_titles = {returned_groups[0]->title,
                                              returned_groups[1]->title};
  EXPECT_THAT(returned_titles, testing::UnorderedElementsAre("Window 1 Group",
                                                             "Window 2 Group"));
}

IN_PROC_BROWSER_TEST_F(TabGroupsOrganizerPageHandlerBrowserTest,
                       NotifiesPageOnNonSavedTabGroupInAnotherWindow) {
  tab_groups::TabGroupSyncService* service = sync_service();
  ASSERT_TRUE(service);
  service->SetIsInitializedForTesting(false);

  FakeTabGroupsOrganizerPage page;
  mojo::Remote<organizer_panel::mojom::TabGroupsOrganizerPageHandler>
      handler_remote;
  TabGroupsOrganizerPageHandler handler(
      handler_remote.BindNewPipeAndPassReceiver(), page.BindAndPassRemote(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  BrowserWindowInterface* browser2 = CreateBrowser(browser()->GetProfile());
  ASSERT_TRUE(browser2);
  ASSERT_EQ(1, browser2->GetTabStripModel()->count());

  tab_groups::TabGroupId local_id2 =
      browser2->GetTabStripModel()->AddToNewGroup({0});
  page.WaitForTabGroupAdded();

  ASSERT_EQ(1u, page.added_groups().size());
  std::string unsaved_id2 = page.added_groups()[0]->id;
  EXPECT_EQ(unsaved_id2, tab_groups::LocalTabGroupIDToString(local_id2));

  tab_groups::TabGroupVisualData visual_data(
      u"Updated Window 2 Group", tab_groups::TabGroupColorId::kOrange);
  browser2->GetTabStripModel()->ChangeTabGroupVisuals(local_id2, visual_data);
  page.WaitForTabGroupUpdated();

  ASSERT_EQ(1u, page.updated_groups().size());
  EXPECT_EQ(page.updated_groups()[0]->id, unsaved_id2);
  EXPECT_EQ(page.updated_groups()[0]->title, "Updated Window 2 Group");
  EXPECT_EQ(page.updated_groups()[0]->color,
            tab_groups::TabGroupColorId::kOrange);

  browser2->GetTabStripModel()->RemoveFromGroup({0});
  page.WaitForTabGroupRemoved();

  ASSERT_EQ(1u, page.removed_group_ids().size());
  EXPECT_EQ(page.removed_group_ids()[0], unsaved_id2);
}

IN_PROC_BROWSER_TEST_F(TabGroupsOrganizerPageHandlerBrowserTest,
                       OpenTabGroupFocusesNonSavedGroupInAnotherWindow) {
  tab_groups::TabGroupSyncService* service = sync_service();
  ASSERT_TRUE(service);

  BrowserWindowInterface* browser2 = CreateBrowser(browser()->GetProfile());
  ASSERT_TRUE(browser2);
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser2, GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_EQ(2, browser2->GetTabStripModel()->count());

  tab_groups::TabGroupId local_id2 =
      browser2->GetTabStripModel()->AddToNewGroup({0});
  service->RemoveGroup(local_id2);
  ASSERT_FALSE(service->GetGroup(local_id2).has_value());

  browser2->GetTabStripModel()->ActivateTabAt(1);
  ASSERT_EQ(1, browser2->GetTabStripModel()->active_index());

  FakeTabGroupsOrganizerPage page;
  mojo::Remote<organizer_panel::mojom::TabGroupsOrganizerPageHandler>
      handler_remote;
  TabGroupsOrganizerPageHandler handler(
      handler_remote.BindNewPipeAndPassReceiver(), page.BindAndPassRemote(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  handler_remote->OpenTabGroup(tab_groups::LocalTabGroupIDToString(local_id2));
  handler_remote.FlushForTesting();

  EXPECT_EQ(0, browser2->GetTabStripModel()->active_index());
}

IN_PROC_BROWSER_TEST_F(TabGroupsOrganizerPageHandlerBrowserTest,
                       CreateNewTabGroup) {
  base::UserActionTester user_action_tester;
  FakeTabGroupsOrganizerPage page;
  mojo::Remote<organizer_panel::mojom::TabGroupsOrganizerPageHandler>
      handler_remote;
  TabGroupsOrganizerPageHandler handler(
      handler_remote.BindNewPipeAndPassReceiver(), page.BindAndPassRemote(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  const int initial_tab_count = browser()->GetTabStripModel()->count();
  const size_t initial_group_count =
      browser()->GetTabStripModel()->group_model()->ListTabGroups().size();

  handler_remote->CreateNewTabGroup();
  handler_remote.FlushForTesting();

  EXPECT_EQ(initial_tab_count + 1, browser()->GetTabStripModel()->count());
  EXPECT_EQ(
      initial_group_count + 1,
      browser()->GetTabStripModel()->group_model()->ListTabGroups().size());
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "TabGroups_SavedTabGroups_"
                   "CreateNewGroupTriggeredFromOrganizerPanel"));
}

}  // namespace
