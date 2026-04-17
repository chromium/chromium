// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/experimental_platform_adapters_provider.h"
#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/android_tab_strip_api_injector.h"
#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/android_tab_strip_model_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/testing/utils.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_impl.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_strip_collection.h"
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
        std::move(android_injector), nullptr);
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

  ASSERT_EQ(1u, client.received.size());

  auto& event = client.received.at(0);
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

}  // namespace tabs_api
