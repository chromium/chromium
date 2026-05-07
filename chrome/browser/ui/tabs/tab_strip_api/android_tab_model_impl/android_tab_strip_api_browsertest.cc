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
#include "chrome/test/base/android/android_browser_test.h"
#include "components/tabs/public/pinned_tab_collection.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_strip_collection.h"
#include "components/tabs/public/unpinned_tab_collection.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

// Can't use anonymous namespace due to friendship to get PassKey.
namespace tabs_api {

class AndroidTabStripApiBrowserTest : public AndroidBrowserTest {
 public:
  AndroidTabStripApiBrowserTest() = default;
  ~AndroidTabStripApiBrowserTest() override = default;

  void SetUpOnMainThread() override {
    AndroidBrowserTest::SetUpOnMainThread();

    model_ = &testing::GetTabModel(GetProfile());
    auto android_injector =
        std::make_unique<tabs_api::AndroidTabStripApiInjector>(model_);
    service_ = std::make_unique<tabs_api::TabStripServiceImpl>(
        std::move(android_injector));
  }

  void TearDownOnMainThread() override {
    // Necessary to prevent out of order destruction between tab model and
    // the service.
    service_.reset();

    AndroidBrowserTest::TearDownOnMainThread();
  }

 protected:
  static base::PassKey<tabs_api::AndroidTabStripModelAdapter> GetPassKey() {
    return tabs_api::AndroidTabStripModelAdapter::GetPassKey();
  }

  raw_ptr<TabModel> model_;
  std::unique_ptr<tabs_api::TabStripService> service_;
};

class TestTabStripClient
    : public tabs_api::observation::TabStripApiBatchedObserver {
 public:
  void OnTabEvents(const std::vector<mojom::TabsEventPtr>& events) override {
    for (auto& event : events) {
      received.push_back(event.Clone());
    }
  }

  std::vector<mojom::TabsEventPtr> received;
};

IN_PROC_BROWSER_TEST_F(AndroidTabStripApiBrowserTest, Observation) {
  TestTabStripClient client;
  service_->AddObserver(&client);

  auto target_to_close = model_->GetTab(0)->GetHandle();
  model_->DuplicateTab(target_to_close);
  model_->CloseTab(target_to_close);

  // We expect 4 events now:
  // 1. Tab created (from DuplicateTab)
  // 2. Tab deselected (from DuplicateTab selecting the new tab)
  // 3. Tab selected (from DuplicateTab selecting the new tab)
  // 4. Nodes closed (from CloseTab)
  ASSERT_EQ(4u, client.received.size());

  auto& event = client.received.at(3);
  auto& close_event = event->get_nodes_closed_event();
  ASSERT_EQ(1u, close_event->node_ids.size());
  ASSERT_EQ(NodeId::FromTabHandle(target_to_close),
            close_event->node_ids.at(0));

  service_->RemoveObserver(&client);
}

IN_PROC_BROWSER_TEST_F(AndroidTabStripApiBrowserTest, Get) {
  auto tab_strip_id = base::NumberToString(
      model_->GetTabStripCollection(GetPassKey())->GetHandle().raw_value());

  // Initial state test, there should be one tab.
  ASSERT_EQ(1, model_->GetTabCount());
  {
    auto result = service_->GetTabsWithoutObservation();
    ASSERT_TRUE(result.has_value());

    auto& window_container = result.value();
    ASSERT_EQ(base::NumberToString(model_->GetSessionId().id()),
              window_container->data->get_window()->id.Id());
    ASSERT_EQ(1u, window_container->children.size());

    auto& tab_strip_container = window_container->children.at(0);
    ASSERT_EQ(tab_strip_id,
              tab_strip_container->data->get_tab_strip()->id.Id());
    ASSERT_EQ(1u, tab_strip_container->children.size());
    ASSERT_EQ(base::NumberToString(
                  model_->GetAllTabs().at(0)->GetHandle().raw_value()),
              tab_strip_container->children.at(0)->data->get_tab()->id.Id());
  }

  // Now create a new tab and check that it is indeed reflected.
  model_->CreateNewTabForDevTools(GURL("http://somewhere.nowhere"), false);
  ASSERT_EQ(2, model_->GetTabCount());
  {
    // Some of the stuff is repeated, just to make sure we don't mangle the
    // parents.
    auto result = service_->GetTabsWithoutObservation();
    ASSERT_TRUE(result.has_value());

    auto& window_container = result.value();
    ASSERT_EQ(base::NumberToString(model_->GetSessionId().id()),
              window_container->data->get_window()->id.Id());
    ASSERT_EQ(1u, window_container->children.size());

    auto& tab_strip_container = window_container->children.at(0);
    ASSERT_EQ(tab_strip_id,
              tab_strip_container->data->get_tab_strip()->id.Id());
    ASSERT_EQ(2u, tab_strip_container->children.size());

    // Ordering is actually material, we need to ensure that the tab
    // order returned by the API matches the underlying model.
    ASSERT_EQ(base::NumberToString(model_->GetTab(0)->GetHandle().raw_value()),
              tab_strip_container->children.at(0)->data->get_tab()->id.Id());
    ASSERT_EQ(base::NumberToString(model_->GetTab(1)->GetHandle().raw_value()),
              tab_strip_container->children.at(1)->data->get_tab()->id.Id());
  }
}

IN_PROC_BROWSER_TEST_F(AndroidTabStripApiBrowserTest, Create) {
  ASSERT_EQ(1, model_->GetTabCount());

  auto result = service_->CreateTabAt(std::nullopt, GURL("http://there.where"));

  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(2, model_->GetTabCount());
}

IN_PROC_BROWSER_TEST_F(AndroidTabStripApiBrowserTest, Activate) {
  model_->DuplicateTab(model_->GetTab(0)->GetHandle());
  ASSERT_EQ(2, model_->GetTabCount());

  auto* tab0 = model_->GetTab(0);
  auto* tab1 = model_->GetTab(1);

  model_->ActivateTab(tab0->GetHandle());
  ASSERT_EQ(tab0, model_->GetActiveTab());

  auto result =
      service_->ActivateTab(tabs_api::NodeId::FromTabHandle(tab1->GetHandle()));

  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(tab1, model_->GetActiveTab());
}

IN_PROC_BROWSER_TEST_F(AndroidTabStripApiBrowserTest, Close) {
  model_->DuplicateTab(model_->GetTab(0)->GetHandle());
  model_->DuplicateTab(model_->GetTab(0)->GetHandle());
  ASSERT_EQ(3, model_->GetTabCount());

  auto* tab0 = model_->GetTab(0);  // <--- keep open
  auto* tab1 = model_->GetTab(1);  // <--- target to close
  auto* tab2 = model_->GetTab(2);  // <--- target to close

  auto result = service_->CloseNodes({
      tabs_api::NodeId::FromTabHandle(tab1->GetHandle()),
      tabs_api::NodeId::FromTabHandle(tab2->GetHandle()),
  });

  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(1, model_->GetTabCount());
  ASSERT_EQ(tab0, model_->GetTab(0));
}

IN_PROC_BROWSER_TEST_F(AndroidTabStripApiBrowserTest, CloseGroup) {
  model_->DuplicateTab(model_->GetTab(0)->GetHandle());
  model_->DuplicateTab(model_->GetTab(0)->GetHandle());
  ASSERT_EQ(3, model_->GetTabCount());

  // This is similar to the close test, but we will create a new group
  // and move the targets to close into that group. We will then close
  // that group and confirm that both of the targets are closed.
  auto* tab0 = model_->GetTab(0);  // <--- keep open
  auto* tab1 = model_->GetTab(1);  // <--- target to close
  auto* tab2 = model_->GetTab(2);  // <--- target to close

  auto maybe_group_id =
      model_->CreateTabGroup({tab1->GetHandle(), tab2->GetHandle()});
  ASSERT_TRUE(maybe_group_id.has_value());
  auto group_id = maybe_group_id.value();
  auto* collection = model_->GetTabStripCollection(GetPassKey())
                         ->GetTabGroupCollection(group_id);
  ASSERT_TRUE(collection != nullptr);

  auto result = service_->CloseNodes({
      tabs_api::NodeId::FromTabCollectionHandle(collection->GetHandle()),
  });

  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(1, model_->GetTabCount());
  ASSERT_EQ(tab0, model_->GetTab(0));
}

IN_PROC_BROWSER_TEST_F(AndroidTabStripApiBrowserTest, MoveTab) {
  model_->DuplicateTab(model_->GetTab(0)->GetHandle());
  model_->DuplicateTab(model_->GetTab(0)->GetHandle());
  ASSERT_EQ(3, model_->GetTabCount());

  // Order is Tab0, Tab1, Tab2
  auto handle0 = model_->GetTab(0)->GetHandle();
  auto handle1 = model_->GetTab(1)->GetHandle();
  auto handle2 = model_->GetTab(2)->GetHandle();

  // Move Tab0 to the end (index 2 in the unpinned collection)
  auto* tab_strip_collection = model_->GetTabStripCollection(GetPassKey());
  tabs_api::Path path(
      {NodeId::FromWindowId(base::NumberToString(model_->GetSessionId().id())),
       NodeId::FromTabCollectionHandle(tab_strip_collection->GetHandle()),
       NodeId::FromTabCollectionHandle(
           tab_strip_collection->unpinned_collection()->GetHandle())});
  tabs_api::Position pos(2, path);

  auto result = service_->MoveNode(NodeId::FromTabHandle(handle0), pos);
  ASSERT_TRUE(result.has_value());

  // New order should be Tab1, Tab2, Tab0
  ASSERT_EQ(handle1, model_->GetTab(0)->GetHandle());
  ASSERT_EQ(handle2, model_->GetTab(1)->GetHandle());
  ASSERT_EQ(handle0, model_->GetTab(2)->GetHandle());
}

IN_PROC_BROWSER_TEST_F(AndroidTabStripApiBrowserTest, MoveTabIntoGroup) {
  model_->DuplicateTab(model_->GetTab(0)->GetHandle());
  model_->DuplicateTab(model_->GetTab(0)->GetHandle());
  ASSERT_EQ(3, model_->GetTabCount());

  auto handle0 = model_->GetTab(0)->GetHandle();
  auto handle1 = model_->GetTab(1)->GetHandle();
  auto handle2 = model_->GetTab(2)->GetHandle();

  auto group_id = model_->CreateTabGroup({handle1}).value();
  auto* group_collection = model_->GetTabStripCollection(GetPassKey())
                               ->GetTabGroupCollection(group_id);

  // Move Tab0 into the group at index 1
  tabs_api::Path path(
      {NodeId::FromWindowId(base::NumberToString(model_->GetSessionId().id())),
       NodeId::FromTabCollectionHandle(
           model_->GetTabStripCollection(GetPassKey())->GetHandle()),
       NodeId::FromTabCollectionHandle(group_collection->GetHandle())});
  tabs_api::Position pos(1, path);

  auto result = service_->MoveNode(NodeId::FromTabHandle(handle0), pos);
  ASSERT_TRUE(result.has_value());

  // New order should be [ (G: H0, H1), H2 ]
  ASSERT_EQ(group_id, handle0.Get()->GetGroup());
  ASSERT_EQ(group_id, handle1.Get()->GetGroup());
  ASSERT_EQ(handle1, model_->GetTab(0)->GetHandle());
  ASSERT_EQ(handle0, model_->GetTab(1)->GetHandle());
  ASSERT_EQ(handle2, model_->GetTab(2)->GetHandle());

  // Now move H0 out of the group.
  tabs_api::Position move_out_pos(1);

  auto move_out_result =
      service_->MoveNode(NodeId::FromTabHandle(handle0), move_out_pos);
  ASSERT_TRUE(move_out_result.has_value());

  ASSERT_FALSE(handle0.Get()->GetGroup().has_value());
}

IN_PROC_BROWSER_TEST_F(AndroidTabStripApiBrowserTest, PinUnpinTab) {
  auto handle0 = model_->GetTab(0)->GetHandle();
  ASSERT_FALSE(handle0.Get()->IsPinned());

  auto* tab_strip_collection = model_->GetTabStripCollection(GetPassKey());
  auto* pinned_collection = tab_strip_collection->pinned_collection();
  auto* unpinned_collection = tab_strip_collection->unpinned_collection();

  // Pin the tab by moving it to the pinned collection.
  tabs_api::Path pin_path(
      {NodeId::FromWindowId(base::NumberToString(model_->GetSessionId().id())),
       NodeId::FromTabCollectionHandle(tab_strip_collection->GetHandle()),
       NodeId::FromTabCollectionHandle(pinned_collection->GetHandle())});
  tabs_api::Position pin_pos(0, pin_path);

  auto pin_result = service_->MoveNode(NodeId::FromTabHandle(handle0), pin_pos);
  ASSERT_TRUE(pin_result.has_value());
  ASSERT_TRUE(handle0.Get()->IsPinned());

  // Unpin the tab by moving it to the unpinned collection.
  tabs_api::Path unpin_path(
      {NodeId::FromWindowId(base::NumberToString(model_->GetSessionId().id())),
       NodeId::FromTabCollectionHandle(tab_strip_collection->GetHandle()),
       NodeId::FromTabCollectionHandle(unpinned_collection->GetHandle())});
  tabs_api::Position unpin_pos(0, unpin_path);

  auto unpin_result =
      service_->MoveNode(NodeId::FromTabHandle(handle0), unpin_pos);
  ASSERT_TRUE(unpin_result.has_value());
  ASSERT_FALSE(handle0.Get()->IsPinned());
}

IN_PROC_BROWSER_TEST_F(AndroidTabStripApiBrowserTest, MoveGroup) {
  model_->DuplicateTab(model_->GetTab(0)->GetHandle());
  model_->DuplicateTab(model_->GetTab(0)->GetHandle());
  ASSERT_EQ(3, model_->GetTabCount());

  auto handle0 = model_->GetTab(0)->GetHandle();
  auto handle1 = model_->GetTab(1)->GetHandle();
  auto handle2 = model_->GetTab(2)->GetHandle();

  auto group_id = model_->CreateTabGroup({handle0, handle1}).value();
  auto* group_collection = model_->GetTabStripCollection(GetPassKey())
                               ->GetTabGroupCollection(group_id);

  // Initial order: [ (G: H0, H1), H2 ]
  ASSERT_EQ(group_id, model_->GetTab(0)->GetGroup());
  ASSERT_EQ(group_id, model_->GetTab(1)->GetGroup());
  ASSERT_EQ(handle2, model_->GetTab(2)->GetHandle());

  // Move the group after H2 (index 1 in the unpinned collection)
  auto* tab_strip_collection = model_->GetTabStripCollection(GetPassKey());
  tabs_api::Path path(
      {NodeId::FromWindowId(base::NumberToString(model_->GetSessionId().id())),
       NodeId::FromTabCollectionHandle(tab_strip_collection->GetHandle()),
       NodeId::FromTabCollectionHandle(
           tab_strip_collection->unpinned_collection()->GetHandle())});
  tabs_api::Position pos(1, path);

  auto result = service_->MoveNode(
      NodeId::FromTabCollectionHandle(group_collection->GetHandle()), pos);
  ASSERT_TRUE(result.has_value());

  // New order should be [ H2, (G: H0, H1) ]
  ASSERT_EQ(handle2, model_->GetTab(0)->GetHandle());
  ASSERT_EQ(handle0, model_->GetTab(1)->GetHandle());
  ASSERT_EQ(handle1, model_->GetTab(2)->GetHandle());

  ASSERT_FALSE(handle2.Get()->GetGroup().has_value());
  ASSERT_EQ(group_id, handle0.Get()->GetGroup());
  ASSERT_EQ(group_id, handle1.Get()->GetGroup());
}

IN_PROC_BROWSER_TEST_F(AndroidTabStripApiBrowserTest, CreateAtPosition) {
  model_->DuplicateTab(model_->GetTab(0)->GetHandle());
  ASSERT_EQ(2, model_->GetTabCount());

  auto handle0 = model_->GetTab(0)->GetHandle();
  auto handle1 = model_->GetTab(1)->GetHandle();

  auto* tab_strip_collection = model_->GetTabStripCollection(GetPassKey());
  tabs_api::Path path(
      {NodeId::FromWindowId(base::NumberToString(model_->GetSessionId().id())),
       NodeId::FromTabCollectionHandle(tab_strip_collection->GetHandle()),
       NodeId::FromTabCollectionHandle(
           tab_strip_collection->unpinned_collection()->GetHandle())});
  tabs_api::Position pos(1, path);

  auto result = service_->CreateTabAt(pos, GURL("http://there.where"));
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(3, model_->GetTabCount());

  ASSERT_EQ(handle0, model_->GetTab(0)->GetHandle());
  ASSERT_EQ(handle1, model_->GetTab(2)->GetHandle());

  auto new_tab_handle = model_->GetTab(1)->GetHandle();
  ASSERT_NE(handle0, new_tab_handle);
  ASSERT_NE(handle1, new_tab_handle);
}

IN_PROC_BROWSER_TEST_F(AndroidTabStripApiBrowserTest, CreateInGroup) {
  model_->DuplicateTab(model_->GetTab(0)->GetHandle());
  ASSERT_EQ(2, model_->GetTabCount());

  auto handle0 = model_->GetTab(0)->GetHandle();

  auto group_id = model_->CreateTabGroup({handle0}).value();
  auto* group_collection = model_->GetTabStripCollection(GetPassKey())
                               ->GetTabGroupCollection(group_id);

  tabs_api::Path path(
      {NodeId::FromWindowId(base::NumberToString(model_->GetSessionId().id())),
       NodeId::FromTabCollectionHandle(
           model_->GetTabStripCollection(GetPassKey())->GetHandle()),
       NodeId::FromTabCollectionHandle(group_collection->GetHandle())});
  tabs_api::Position pos(1, path);

  auto result = service_->CreateTabAt(pos, GURL("http://there.where"));
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(3, model_->GetTabCount());

  auto new_tab_handle = model_->GetTab(1)->GetHandle();
  ASSERT_EQ(group_id, new_tab_handle.Get()->GetGroup());
}

IN_PROC_BROWSER_TEST_F(AndroidTabStripApiBrowserTest, CreatePinned) {
  auto* tab_strip_collection = model_->GetTabStripCollection(GetPassKey());
  auto* pinned_collection = tab_strip_collection->pinned_collection();

  tabs_api::Path path(
      {NodeId::FromWindowId(base::NumberToString(model_->GetSessionId().id())),
       NodeId::FromTabCollectionHandle(tab_strip_collection->GetHandle()),
       NodeId::FromTabCollectionHandle(pinned_collection->GetHandle())});
  tabs_api::Position pos(0, path);

  auto result = service_->CreateTabAt(pos, GURL("http://there.where"));
  ASSERT_TRUE(result.has_value());

  ASSERT_EQ(2, model_->GetTabCount());
  ASSERT_TRUE(model_->GetTab(0)->IsPinned());
}

IN_PROC_BROWSER_TEST_F(AndroidTabStripApiBrowserTest, CreateWithoutPosition) {
  auto* tab_strip_collection = model_->GetTabStripCollection(GetPassKey());
  auto* unpinned_collection = tab_strip_collection->unpinned_collection();

  tabs_api::Position pos(0, tabs_api::Path());

  auto result = service_->CreateTabAt(pos, GURL("http://there.where"));
  ASSERT_TRUE(result.has_value());

  ASSERT_EQ(2, model_->GetTabCount());
  ASSERT_EQ(2ul, unpinned_collection->ChildCount());
}

IN_PROC_BROWSER_TEST_F(AndroidTabStripApiBrowserTest, CreateUnsupported) {
  auto* tab_strip_collection = model_->GetTabStripCollection(GetPassKey());

  tabs_api::Path bad_path_with_tabstrip(
      {NodeId::FromWindowId(base::NumberToString(model_->GetSessionId().id())),
       NodeId::FromTabCollectionHandle(tab_strip_collection->GetHandle())});
  EXPECT_DEATH_IF_SUPPORTED(
      {
        std::ignore =
            service_->CreateTabAt(tabs_api::Position(0, bad_path_with_tabstrip),
                                  GURL("http://there.where"));
      },
      "Unsupported collection type for insertion");
}

}  // namespace tabs_api
