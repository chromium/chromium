// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_impl.h"

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

// TODO(ffred): refactor this stuff. Maybe it makes more sense to have an
// accumulator here instead of a test impl.
class TestTabStripClient : public tabs_api::mojom::TabsObserver {
 public:
  void OnTabsCreated(tabs_api::mojom::OnTabsCreatedEventPtr event) override {
    for (auto& tab_created_container : event->tabs) {
      auto& tab = tab_created_container->tab;
      auto tab_id = tab->id;
      tabs.push_back({tab_id, tab->url.spec()});
    }
  }

  void OnTabsClosed(tabs_api::mojom::OnTabsClosedEventPtr event) override {
    for (auto& id : event->tabs) {
      auto found = std::find_if(
          tabs.begin(), tabs.end(),
          [&](const std::pair<tabs_api::NodeId, std::string>& element) {
            return element.first == id;
          });

      if (found != tabs.end()) {
        tabs.erase(found);
      }
    }
  }

  void OnTabMoved(tabs_api::mojom::OnTabMovedEventPtr event) override {
    move_events.push_back(std::move(event));
  }

  void OnTabDataChanged(
      tabs_api::mojom::OnTabDataChangedEventPtr event) override {
    auto& id = event->tab->id;
    auto found = std::find_if(
        tabs.begin(), tabs.end(),
        [&](const std::pair<tabs_api::NodeId, std::string>& element) {
          return element.first == id;
        });

    if (found != tabs.end()) {
      *found = {id, event->tab->url.spec()};
    }
  }

  void OnTabGroupCreated(
      tabs_api::mojom::OnTabGroupCreatedEventPtr event) override {
    // TODO(crbug.com/412955607): implement this.
    group_events.push_back(std::move(event));
  }

  void OnTabGroupVisualsChanged(
      tabs_api::mojom::OnTabGroupVisualsChangedEventPtr event) override {
    // TODO(crbug.com/412955607): implement this.
  }

  std::vector<tabs_api::mojom::OnTabMovedEventPtr> move_events;
  std::vector<tabs_api::mojom::OnTabGroupCreatedEventPtr> group_events;
  // Tabs is a vector containing a tab id and a url in the form of a string.
  std::vector<std::pair<tabs_api::NodeId, std::string>> tabs;
};

class TabStripServiceImplBrowserTest : public InProcessBrowserTest {
 public:
  using TabStripService = tabs_api::mojom::TabStripService;

  TabStripServiceImplBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kTabStripBrowserApi);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    tab_strip_service_impl_ = std::make_unique<TabStripServiceImpl>(
        browser(), browser()->tab_strip_model());
  }

  void TearDownOnMainThread() override {
    tab_strip_service_impl_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  TabStripModel* GetTabStripModel() { return browser()->tab_strip_model(); }

  struct Observation {
    mojo::Remote<TabStripService> remote;
    TestTabStripClient client;
    mojo::AssociatedReceiver<tabs_api::mojom::TabsObserver> receiver{&client};
  };

  std::unique_ptr<Observation> SetUpObservation() {
    auto observation = std::make_unique<Observation>();
    tab_strip_service_impl_->Accept(
        observation->remote.BindNewPipeAndPassReceiver());

    base::RunLoop run_loop;
    observation->remote->GetTabs(
        base::BindLambdaForTesting([&](TabStripService::GetTabsResult result) {
          ASSERT_TRUE(result.has_value());
          // This is where the client sets up the binding!
          observation->receiver.Bind(std::move(result.value()->stream));
          run_loop.Quit();
        }));
    run_loop.Run();

    return observation;
  }

  tabs_api::mojom::PositionPtr CreatePosition(int index) {
    auto position = tabs_api::mojom::Position::New();
    position->index = index;
    return position;
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TabStripServiceImpl> tab_strip_service_impl_;
};

IN_PROC_BROWSER_TEST_F(TabStripServiceImplBrowserTest, CreateTabAt) {
  mojo::Remote<TabStripService> remote;
  tab_strip_service_impl_->Accept(remote.BindNewPipeAndPassReceiver());

  TabStripModel* model = GetTabStripModel();
  const int expected_tab_count = model->count() + 1;
  const GURL url("http://example.com/");

  base::RunLoop run_loop;
  tabs_api::mojom::PositionPtr position = CreatePosition(0);

  TabStripService::CreateTabAtResult result;
  remote->CreateTabAt(
      std::move(position), std::make_optional(url),
      base::BindLambdaForTesting([&](TabStripService::CreateTabAtResult in) {
        result = std::move(in);
        run_loop.Quit();
      }));
  run_loop.Run();

  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.value());
  EXPECT_EQ(model->count(), expected_tab_count);

  auto handle = model->GetTabAtIndex(0)->GetHandle();
  ASSERT_EQ(base::NumberToString(handle.raw_value()), result.value()->id.Id());
  // Assert that newly created tabs are also activated.
  ASSERT_EQ(model->GetActiveTab()->GetHandle(), handle);
}

IN_PROC_BROWSER_TEST_F(TabStripServiceImplBrowserTest, Observation) {
  mojo::Remote<TabStripService> remote;
  tab_strip_service_impl_->Accept(remote.BindNewPipeAndPassReceiver());
  TestTabStripClient client;
  mojo::AssociatedReceiver<tabs_api::mojom::TabsObserver> receiver(&client);
  const GURL url("http://example.com/");
  uint32_t target_index = 0;

  base::RunLoop run_loop;
  tabs_api::mojom::PositionPtr position = CreatePosition(target_index);

  base::RunLoop get_tabs_loop;
  remote->GetTabs(
      base::BindLambdaForTesting([&](TabStripService::GetTabsResult result) {
        ASSERT_TRUE(result.has_value());
        // This is where the client sets up the binding!
        receiver.Bind(std::move(result.value()->stream));
        get_tabs_loop.Quit();
      }));
  get_tabs_loop.Run();

  TabStripService::CreateTabAtResult result;
  remote->CreateTabAt(
      std::move(position), std::make_optional(url),
      base::BindLambdaForTesting([&](TabStripService::CreateTabAtResult in) {
        result = std::move(in);
        run_loop.Quit();
      }));
  run_loop.Run();

  // Ensure that we've received the observation callback, which are not
  // guaranteed to happen immediately.
  receiver.FlushForTesting();

  ASSERT_TRUE(result.has_value())
      << "CreateTabAt failed: " << (result.error()->message);
  auto created_tab = std::move(result.value());

  ASSERT_EQ(1ul, client.tabs.size());
  ASSERT_EQ(created_tab->id, client.tabs.at(0).first);

  // Navigate to a new url which will modify the tab state.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://www.google.com/")));
  receiver.FlushForTesting();
  ASSERT_EQ(client.tabs[0].second, "https://www.google.com/");

  TabStripService::CloseTabsResult close_result;
  base::RunLoop close_tab_loop;
  remote->CloseTabs(
      {created_tab->id},
      base::BindLambdaForTesting([&](TabStripService::CloseTabsResult in) {
        close_result = std::move(in);
        close_tab_loop.Quit();
      }));
  close_tab_loop.Run();

  // Wait for observation.
  receiver.FlushForTesting();

  ASSERT_TRUE(close_result.has_value());
  // Observation should have caused the tab to be removed.
  ASSERT_EQ(0ul, client.tabs.size());
}

IN_PROC_BROWSER_TEST_F(TabStripServiceImplBrowserTest, CloseTabs) {
  mojo::Remote<TabStripService> remote;
  tab_strip_service_impl_->Accept(remote.BindNewPipeAndPassReceiver());

  const int starting_num_tabs = GetTabStripModel()->GetTabCount();

  base::RunLoop create_loop;
  remote->CreateTabAt(CreatePosition(0),
                      std::make_optional(GURL("http://dark.web")),
                      base::BindLambdaForTesting(
                          [&](TabStripService::CreateTabAtResult result) {
                            ASSERT_TRUE(result.has_value());
                            create_loop.Quit();
                          }));
  create_loop.Run();

  // We should now have one more tab than when we first started.
  ASSERT_EQ(starting_num_tabs + 1, GetTabStripModel()->GetTabCount());
  const auto* interface = GetTabStripModel()->GetTabAtIndex(0);

  base::RunLoop close_loop;
  remote->CloseTabs(
      {tabs_api::NodeId(
          tabs_api::NodeId::Type::kContent,
          base::NumberToString(interface->GetHandle().raw_value()))},
      base::BindLambdaForTesting([&](TabStripService::CloseTabsResult result) {
        ASSERT_TRUE(result.has_value());
        close_loop.Quit();
      }));
  close_loop.Run();

  // We should be back to where we started.
  ASSERT_EQ(starting_num_tabs, GetTabStripModel()->GetTabCount());
}

IN_PROC_BROWSER_TEST_F(TabStripServiceImplBrowserTest, ActivateTab) {
  mojo::Remote<TabStripService> remote;
  tab_strip_service_impl_->Accept(remote.BindNewPipeAndPassReceiver());

  tabs_api::NodeId created_id;
  base::RunLoop create_loop;
  // Append a new tab to the end, which will also focus it.
  remote->CreateTabAt(nullptr, std::make_optional(GURL("http://dark.web")),
                      base::BindLambdaForTesting(
                          [&](TabStripService::CreateTabAtResult result) {
                            ASSERT_TRUE(result.has_value());
                            created_id = result.value()->id;
                            create_loop.Quit();
                          }));
  create_loop.Run();

  auto old_tab_handle = GetTabStripModel()->GetTabAtIndex(0)->GetHandle();
  // Creating a new tab should have caused the old tab to lose active state.
  ASSERT_NE(GetTabStripModel()->GetActiveTab()->GetHandle(), old_tab_handle);

  auto old_tab_id =
      tabs_api::NodeId(tabs_api::NodeId::Type::kContent,
                      base::NumberToString(old_tab_handle.raw_value()));
  base::RunLoop activate_loop;
  remote->ActivateTab(
      old_tab_id,
      base::BindLambdaForTesting([&](TabStripService::CloseTabsResult result) {
        ASSERT_TRUE(result.has_value());
        activate_loop.Quit();
      }));
  activate_loop.Run();

  // Old tab should now be re-activated.
  ASSERT_EQ(GetTabStripModel()->GetActiveTab()->GetHandle(), old_tab_handle);
}

IN_PROC_BROWSER_TEST_F(TabStripServiceImplBrowserTest, MoveTab) {
  mojo::Remote<TabStripService> remote;
  tab_strip_service_impl_->Accept(remote.BindNewPipeAndPassReceiver());

  auto observation = SetUpObservation();

  // Append a new tab to the end, so we have two tabs to work with.
  base::RunLoop create_loop;
  remote->CreateTabAt(nullptr,
                      std::make_optional(GURL("http://somwewhere.nowhere")),
                      base::BindLambdaForTesting(
                          [&](TabStripService::CreateTabAtResult result) {
                            ASSERT_TRUE(result.has_value());
                            create_loop.Quit();
                          }));
  create_loop.Run();

  auto handle_to_move = GetTabStripModel()->GetTabAtIndex(0)->GetHandle();
  auto to_move_id =
      tabs_api::NodeId(tabs_api::NodeId::Type::kContent,
                      base::NumberToString(handle_to_move.raw_value()));

  size_t target_idx = 1;

  auto position = tabs_api::mojom::Position::New();
  position->index = target_idx;

  base::RunLoop move_loop;
  remote->MoveTab(
      to_move_id, std::move(position),
      base::BindLambdaForTesting([&](TabStripService::MoveTabResult result) {
        ASSERT_TRUE(result.has_value());
        move_loop.Quit();
      }));
  move_loop.Run();
  observation->receiver.FlushForTesting();

  // Tab should now have been moved to target idx.
  ASSERT_EQ(GetTabStripModel()->GetTabAtIndex(target_idx)->GetHandle(),
            handle_to_move);

  ASSERT_EQ(1ul, observation->client.move_events.size());

  auto event = observation->client.move_events.at(0).Clone();
  ASSERT_EQ(to_move_id, event->id);
  ASSERT_EQ(0u, event->from->index);
  ASSERT_EQ(1u, event->to->index);
}
