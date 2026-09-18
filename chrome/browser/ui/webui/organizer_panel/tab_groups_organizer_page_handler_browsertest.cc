// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/organizer_panel/tab_groups_organizer_page_handler.h"

#include <vector>

#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/organizer_panel/tab_groups.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/tab_groups/tab_group_color.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class TabGroupsOrganizerPageHandlerBrowserTest : public InProcessBrowserTest {
 public:
  tab_groups::TabGroupSyncService* sync_service() {
    return tab_groups::TabGroupSyncServiceFactory::GetForProfile(
        browser()->GetProfile());
  }
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
      /*position=*/std::nullopt, id1, /*local_group_id=*/std::nullopt,
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

  mojo::Remote<organizer_panel::mojom::TabGroupsOrganizerPageHandler>
      handler_remote;
  TabGroupsOrganizerPageHandler handler(
      handler_remote.BindNewPipeAndPassReceiver(), browser()->GetProfile());

  base::test::TestFuture<std::vector<organizer_panel::mojom::TabGroupPtr>>
      future;
  handler_remote->GetTabGroups(future.GetCallback());

  const std::vector<organizer_panel::mojom::TabGroupPtr>& returned_groups =
      future.Get();

  ASSERT_EQ(3u, returned_groups.size());

  // Groups are returned sorted by most recently used descending.
  EXPECT_EQ(returned_groups[0]->id, id1);
  EXPECT_EQ(returned_groups[0]->title, "Group 1");
  EXPECT_EQ(returned_groups[0]->color, tab_groups::TabGroupColorId::kBlue);

  EXPECT_EQ(returned_groups[1]->id, id2);
  EXPECT_EQ(returned_groups[1]->title, "Group 2");
  EXPECT_EQ(returned_groups[1]->color, tab_groups::TabGroupColorId::kRed);

  EXPECT_EQ(returned_groups[2]->id, id3);
  EXPECT_EQ(returned_groups[2]->title, "Group 3");
  EXPECT_EQ(returned_groups[2]->color, tab_groups::TabGroupColorId::kGreen);
}

}  // namespace
