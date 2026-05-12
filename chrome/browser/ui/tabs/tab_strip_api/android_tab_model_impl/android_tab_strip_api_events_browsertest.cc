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

  // Operation 2: Move Tab 1 to position 0.
  auto* tab_strip_collection = model_->GetTabStripCollection(GetPassKey());
  Path path(
      {NodeId::FromWindowId(base::NumberToString(model_->GetSessionId().id())),
       NodeId::FromTabCollectionHandle(tab_strip_collection->GetHandle()),
       NodeId::FromTabCollectionHandle(
           tab_strip_collection->unpinned_collection()->GetHandle())});
  Position pos(0, path);

  auto result = service_->MoveNode(NodeId::FromTabHandle(handle1), pos);
  ASSERT_TRUE(result.has_value());

  // We expect 1 more event: Tab moved.
  ASSERT_EQ(4u, client.received.size());
  ASSERT_TRUE(client.received[3]->is_node_moved_event());

  auto& moved_event = client.received[3]->get_node_moved_event();
  // Tab 1 was at index 1 and moved to index 0.
  ASSERT_EQ(1u, moved_event->from.index());
  ASSERT_EQ(0u, moved_event->to.index());
}

}  // namespace tabs_api
