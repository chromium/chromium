// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "base/test/gtest_util.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/android_tab_strip_api_injector.h"
#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/android_tab_strip_model_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/testing/utils.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_api/testing/event_matchers.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/pinned_tab_collection.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_strip_collection.h"
#include "components/tabs/public/unpinned_tab_collection.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "url/gurl.h"

namespace tabs_api {

class AndroidTabStripApiEventsBrowserTest : public AndroidBrowserTest {
 public:
  AndroidTabStripApiEventsBrowserTest() = default;
  ~AndroidTabStripApiEventsBrowserTest() override = default;

  void SetUpOnMainThread() override {
    AndroidBrowserTest::SetUpOnMainThread();

    model_ = &testing::GetTabModel(GetProfile());
    auto android_injector =
        std::make_unique<AndroidTabStripApiInjector>(model_);
    service_ =
        std::make_unique<TabStripServiceImpl>(std::move(android_injector));
  }

  void TearDownOnMainThread() override {
    // Necessary to prevent out of order dtor.
    service_.reset();
    AndroidBrowserTest::TearDownOnMainThread();
  }

 protected:
  static base::PassKey<AndroidTabStripModelAdapter> GetPassKey() {
    return AndroidTabStripModelAdapter::GetPassKey();
  }

  raw_ptr<TabModel> model_;
  std::unique_ptr<TabStripService> service_;
};

class TestTabStripClient : public observation::TabStripApiBatchedObserver {
 public:
  void OnTabEvents(const std::vector<mojom::TabsEventPtr>& events) override {
    for (auto& event : events) {
      received.push_back(event.Clone());
    }
  }

  std::vector<mojom::TabsEventPtr> received;
};

IN_PROC_BROWSER_TEST_F(AndroidTabStripApiEventsBrowserTest,
                       TabModelObservation) {
  TestTabStripClient client;
  service_->AddObserver(&client);
  absl::Cleanup cleanup = [&] { service_->RemoveObserver(&client); };

  ASSERT_EQ(1, model_->GetTabCount());
  auto handle0 = model_->GetTab(0)->GetHandle();
  auto* tab_strip_collection = model_->GetTabStripCollection(GetPassKey());

  // Operation 1: Duplicate Tab 0.
  model_->DuplicateTab(handle0);
  ASSERT_EQ(2, model_->GetTabCount());
  auto handle1 = model_->GetTab(1)->GetHandle();

  // We expect 3 events from DuplicateTab:
  // 0. Tab created
  // 1. Tab deselected (handle0)
  // 2. Tab selected (handle1)
  ASSERT_EQ(3u, client.received.size());
  ASSERT_TRUE(client.received[0]->is_tabs_created_event());
  ASSERT_TRUE(client.received[1]->is_data_changed_event());
  ASSERT_TRUE(client.received[2]->is_data_changed_event());

  client.received.clear();

  // Operation 2: Move Tab 1 to position 0.

  Path path(
      {NodeId::FromWindowId(base::NumberToString(model_->GetSessionId().id())),
       NodeId::FromTabCollectionHandle(tab_strip_collection->GetHandle()),
       NodeId::FromTabCollectionHandle(
           tab_strip_collection->unpinned_collection()->GetHandle())});
  Position pos(0, path);

  auto result = service_->MoveNode(NodeId::FromTabHandle(handle1), pos);
  ASSERT_TRUE(result.has_value());

  // We expect 1 event: Tab moved.
  ASSERT_EQ(1u, client.received.size());
  ASSERT_TRUE(client.received[0]->is_node_moved_event());

  auto& moved_event = client.received[0]->get_node_moved_event();
  // Tab 1 was at index 1 and moved to index 0.
  ASSERT_EQ(1u, moved_event->from.index());
  ASSERT_EQ(0u, moved_event->to.index());

  client.received.clear();

  // Operation 3: Create a Tab Group with Tab 0 and Tab 1.
  std::vector<tabs::TabHandle> tabs_to_group = {handle0, handle1};
  auto group_id_opt = model_->CreateTabGroup(tabs_to_group);
  ASSERT_TRUE(group_id_opt.has_value());
  auto group_id = group_id_opt.value();

  // We expect 1 event: Tab Group Created.
  ASSERT_EQ(3u, client.received.size());
  EXPECT_THAT(client.received,
              ::testing::Contains(CollectionCreated(
                  [](const mojom::OnCollectionCreatedEventPtr& event) {
                    return event->collection->data->is_tab_group();
                  })));
  EXPECT_THAT(client.received,
              ::testing::Contains(
                  NodeMoved([&](const mojom::OnNodeMovedEventPtr& event) {
                    return event->id == NodeId::FromTabHandle(handle0) &&
                           event->to.index() == 0u;
                  })));
  EXPECT_THAT(client.received,
              ::testing::Contains(
                  NodeMoved([&](const mojom::OnNodeMovedEventPtr& event) {
                    return event->id == NodeId::FromTabHandle(handle1) &&
                           event->to.index() == 1u;
                  })));

  client.received.clear();

  // Operation 4: Change Tab Group Visuals.
  tab_groups::TabGroupVisualData visual_data(
      u"My New Group", tab_groups::TabGroupColorId::kCyan);
  model_->SetTabGroupVisualData(group_id, visual_data);

  // We expect 1 event: Tab Group Visuals Changed.
  ASSERT_EQ(1u, client.received.size());
  EXPECT_THAT(client.received[0],
              TabGroupChanged([&](const mojom::TabGroupChangePtr& change) {
                return change->data->data.title() == u"My New Group" &&
                       change->data->data.color() ==
                           tab_groups::TabGroupColorId::kCyan;
              }));

  client.received.clear();

  // Operation 5: Ungroup Tab 1.
  model_->Ungroup({handle1});

  // We expect 1 event: Tab 1 moved out of group.
  ASSERT_EQ(1u, client.received.size());

  Path unpinned_path(
      {NodeId::FromWindowId(base::NumberToString(model_->GetSessionId().id())),
       NodeId::FromTabCollectionHandle(tab_strip_collection->GetHandle()),
       NodeId::FromTabCollectionHandle(
           tab_strip_collection->unpinned_collection()->GetHandle())});

  auto* group_collection =
      tab_strip_collection->GetTabGroupCollection(group_id);
  ASSERT_NE(nullptr, group_collection);
  auto group_collection_handle = group_collection->GetHandle();
  Path group_path(
      {NodeId::FromWindowId(base::NumberToString(model_->GetSessionId().id())),
       NodeId::FromTabCollectionHandle(tab_strip_collection->GetHandle()),
       NodeId::FromTabCollectionHandle(
           tab_strip_collection->unpinned_collection()->GetHandle()),
       NodeId::FromTabCollectionHandle(group_collection_handle)});

  EXPECT_THAT(client.received[0],
              NodeMoved([&](const mojom::OnNodeMovedEventPtr& event) {
                return event->id == NodeId::FromTabHandle(handle1) &&
                       event->from.path() == group_path &&
                       event->to.path() == unpinned_path;
              }));

  client.received.clear();

  // Operation 6: Close Tab 1.
  model_->CloseTab(handle1);

  // We expect 2 events: Tab 1 closed, and Tab 0 activated (since Tab 1 was
  // active). On Android, the JNI notification split between selection and
  // activation can cause an extra redundant TabChanged event, leading to 2 or 3
  // events:
  //   - Event 0: NodesClosed (for handle1)
  //   - Event 1: TabChanged (for handle0 -> active=true, selected=true)
  //   - Event 2: TabChanged (for handle0 -> active=true, selected=true)
  //   [Redundant]
  // The final state is always correct and verified by the matchers below.
  ASSERT_GE(client.received.size(), 2u);
  ASSERT_LE(client.received.size(), 3u);
  EXPECT_THAT(client.received,
              ::testing::Contains(
                  NodesClosed([&](const mojom::OnNodesClosedEventPtr& event) {
                    return event->node_ids.size() == 1 &&
                           event->node_ids[0] == NodeId::FromTabHandle(handle1);
                  })));
  EXPECT_THAT(
      client.received,
      ::testing::Contains(TabChanged([&](const mojom::TabChangePtr& change) {
        return change->data->id == NodeId::FromTabHandle(handle0) &&
               change->data->is_active;
      })));
}

// Test event handling when triggered from the API.
IN_PROC_BROWSER_TEST_F(AndroidTabStripApiEventsBrowserTest, EndToEndEvents) {
  TestTabStripClient client;
  service_->AddObserver(&client);
  absl::Cleanup cleanup = [&] { service_->RemoveObserver(&client); };

  ASSERT_EQ(1, model_->GetTabCount());
  auto handle0 = model_->GetTab(0)->GetHandle();
  auto id0 = NodeId::FromTabHandle(handle0);
  auto* tab_strip_collection = model_->GetTabStripCollection(GetPassKey());

  // Operation 1: Create a new tab at the end.
  auto result = service_->CreateTabAt(std::nullopt, std::nullopt);
  ASSERT_TRUE(result.has_value());
  auto tab1 = std::move(result.value());
  auto id1 = tab1->id;

  // We expect 3 events: tab created, deselect handle0, select tab1.
  // On Android, the JNI notification split between selection and activation
  // can cause an extra redundant TabChanged event, leading to 3 or 4 events:
  //   - Event 0: TabsCreated (for tab1)
  //   - Event 1: TabChanged (for handle0 -> active=false, selected=false)
  //   - Event 2: TabChanged (for tab1 -> active=true, selected=true)
  //   - Event 3: TabChanged (for tab1 -> active=true, selected=true)
  //   [Redundant]
  // The final state is always correct and verified by the matchers below.
  ASSERT_GE(client.received.size(), 3u);
  ASSERT_LE(client.received.size(), 4u);
  EXPECT_THAT(client.received,
              ::testing::Contains(
                  TabsCreated([&](const mojom::OnTabsCreatedEventPtr& event) {
                    return event->tabs.size() == 1u &&
                           event->tabs[0]->tab->id == id1 &&
                           event->tabs[0]->position.index() == 1u;
                  })));
  EXPECT_THAT(
      client.received,
      ::testing::Contains(TabChanged([&](const mojom::TabChangePtr& change) {
        return change->data->id == id0 && !change->data->is_active &&
               !change->data->is_selected;
      })));
  EXPECT_THAT(
      client.received,
      ::testing::Contains(TabChanged([&](const mojom::TabChangePtr& change) {
        return change->data->id == id1 && change->data->is_active &&
               change->data->is_selected;
      })));

  client.received.clear();

  // Operation 2: Activate the first tab (handle0) again.
  auto act_result = service_->ActivateTab(id0);
  ASSERT_TRUE(act_result.has_value());

  // We expect 2 events: deselect tab1, select tab0.
  // On Android, the JNI notification split between selection and activation
  // can cause extra redundant TabChanged events, leading to 2 to 4 events:
  //   - Event 0: TabChanged (for tab1 -> active=false, selected=false)
  //   - Event 1: TabChanged (for tab0 -> active=true, selected=true)
  //   - Event 2/3: Redundant TabChanged events for tab0/tab1
  // The final state is always correct and verified by the matchers below.
  ASSERT_GE(client.received.size(), 2u);
  ASSERT_LE(client.received.size(), 4u);
  EXPECT_THAT(
      client.received,
      ::testing::Contains(TabChanged([&](const mojom::TabChangePtr& change) {
        return change->data->id == id0 && change->data->is_active &&
               change->data->is_selected;
      })));
  EXPECT_THAT(
      client.received,
      ::testing::Contains(TabChanged([&](const mojom::TabChangePtr& change) {
        return change->data->id == id1 && !change->data->is_active &&
               !change->data->is_selected;
      })));

  client.received.clear();

  // Operation 3: Move tab1 (which is at index 1) to index 0.

  Path path(
      {NodeId::FromWindowId(base::NumberToString(model_->GetSessionId().id())),
       NodeId::FromTabCollectionHandle(tab_strip_collection->GetHandle()),
       NodeId::FromTabCollectionHandle(
           tab_strip_collection->unpinned_collection()->GetHandle())});
  Position pos(0, path);

  auto move_result = service_->MoveNode(id1, pos);
  ASSERT_TRUE(move_result.has_value());

  // We expect:
  // 1. OnNodeMovedEvent
  ASSERT_EQ(1u, client.received.size());
  EXPECT_THAT(client.received[0],
              NodeMoved([&](const mojom::OnNodeMovedEventPtr& event) {
                return event->id == id1 && event->from.index() == 1 &&
                       event->to.index() == 0;
              }));

  client.received.clear();

  // Operation 4: Close tab1.
  auto close_result = service_->CloseNodes({id1});
  ASSERT_TRUE(close_result.has_value());

  // We expect:
  // 1. OnNodesClosedEvent for id1.
  ASSERT_EQ(1u, client.received.size());
  EXPECT_THAT(client.received[0],
              NodesClosed([&](const mojom::OnNodesClosedEventPtr& event) {
                return event->node_ids.size() == 1 && event->node_ids[0] == id1;
              }));
}

}  // namespace tabs_api
