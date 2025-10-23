// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_impl.h"

#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_api/observation/tab_strip_api_batched_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_mojo_handler.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "components/browser_apis/tab_strip/tab_strip_experiment_api.mojom.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

// TODO(ffred): refactor this stuff. Maybe it makes more sense to have an
// accumulator here instead of a test impl.
// TODO(ffred): this is actually a e2e test. We should break up the tests into
// integration (sync API) and e2e (mojo stuff).
class ReallyVerySimpleSyncObserver
    : public tabs_api::observation::TabStripApiBatchedObserver {
 public:
  ReallyVerySimpleSyncObserver() = default;
  ~ReallyVerySimpleSyncObserver() override = default;

  void OnTabEvents(
      const std::vector<tabs_api::mojom::TabsEventPtr>& events) override {
    num_callbacks++;
  }

  int num_callbacks = 0;
};

class TestTabStripClient : public tabs_api::mojom::TabsObserver {
 public:
  void OnTabsCreated(tabs_api::mojom::OnTabsCreatedEventPtr event) {
    for (auto& tab_created_container : event->tabs) {
      auto& tab = tab_created_container->tab;
      auto tab_id = tab->id;
      tabs.emplace(std::string(tab_id.Id()), std::move(tab));
    }
    tab_created_events.push_back(event.Clone());
  }

  void OnTabsClosed(tabs_api::mojom::OnTabsClosedEventPtr& event) {
    for (auto& id : event->tabs) {
      tabs.erase(std::string(id.Id()));
    }
  }

  void OnNodeMoved(tabs_api::mojom::OnNodeMovedEventPtr event) {
    move_events.push_back(std::move(event));
  }

  void OnDataChanged(tabs_api::mojom::OnDataChangedEventPtr& event) {
    // TODO(crbug.com/412738255): this is a hack, because we are not correctly
    // adding the initial tab that is created by the tab strip. We should have
    // a test for GetTabSnapshot and properly populate the initial tab.
    const auto& data = event->data;
    switch (data->which()) {
      case tabs_api::mojom::Data::Tag::kTab: {
        const auto& tab = data->get_tab();
        std::string id_str = std::string(tab->id.Id());
        if (tabs.contains(id_str)) {
          tabs.at(id_str) = tab.Clone();
        }
        break;
      }
      case tabs_api::mojom::Data::Tag::kTabGroup:
      case tabs_api::mojom::Data::Tag::kSplitTab:
        // TODO(crbug.com/412955607): implement this.
        break;
      case tabs_api::mojom::Data::Tag::kTabStrip:
      case tabs_api::mojom::Data::Tag::kPinnedTabs:
      case tabs_api::mojom::Data::Tag::kUnpinnedTabs:
        NOTIMPLEMENTED();
        break;
    }
  }

  void OnCollectionCreated(tabs_api::mojom::OnCollectionCreatedEventPtr event) {
    // TODO(crbug.com/412955607): implement this.
    created_events.push_back(std::move(event));
  }

  void OnTabEvents(std::vector<tabs_api::mojom::TabsEventPtr> events) override {
    for (auto& event : events) {
      switch (event->which()) {
        case tabs_api::mojom::TabsEvent::Tag::kTabsCreatedEvent:
          OnTabsCreated(std::move(event->get_tabs_created_event()));
          break;
        case tabs_api::mojom::TabsEvent::Tag::kTabsClosedEvent:
          OnTabsClosed(event->get_tabs_closed_event());
          break;
        case tabs_api::mojom::TabsEvent::Tag::kNodeMovedEvent:
          OnNodeMoved(std::move(event->get_node_moved_event()));
          break;
        case tabs_api::mojom::TabsEvent::Tag::kDataChangedEvent:
          OnDataChanged(event->get_data_changed_event());
          break;
        case tabs_api::mojom::TabsEvent::Tag::kCollectionCreatedEvent:
          OnCollectionCreated(std::move(event->get_collection_created_event()));
          break;
      }
    }
  }

  std::vector<tabs_api::mojom::OnNodeMovedEventPtr> move_events;
  std::vector<tabs_api::mojom::OnCollectionCreatedEventPtr> created_events;
  std::vector<tabs_api::mojom::OnTabsCreatedEventPtr> tab_created_events;

  std::map<std::string, tabs_api::mojom::TabPtr> tabs;
};

class TabStripServiceImplBrowserTest : public InProcessBrowserTest {
 public:
  using TabStripService = tabs_api::mojom::TabStripService;
  using TabStripExperimentService = tabs_api::mojom::TabStripExperimentService;

  TabStripServiceImplBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kTabStripBrowserApi, features::kSideBySide}, {});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    tab_strip_service_mojo_handler_ =
        std::make_unique<TabStripServiceMojoHandler>(
            browser(), browser()->tab_strip_model());
  }

  void TearDownOnMainThread() override {
    tab_strip_service_mojo_handler_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void FindNodeId(tabs_api::mojom::Data::Tag target_tag,
                  tabs_api::NodeId* out_node_id,
                  base::RunLoop* run_loop,
                  TabStripService::GetTabsResult result) {
    const auto& root_container = result.value()->tab_strip;

    if (auto found_id = FindContainer(*root_container, target_tag);
        found_id.has_value()) {
      *out_node_id = found_id.value();
    }
    run_loop->Quit();
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
    tab_strip_service_mojo_handler_->Accept(
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

  std::optional<tabs_api::NodeId> FindContainer(
      const tabs_api::mojom::Container& container,
      tabs_api::mojom::Data::Tag target_tag) {
    const auto& data = container.data;
    if (data->which() == target_tag) {
      switch (data->which()) {
        case tabs_api::mojom::Data::Tag::kTab:
          return data->get_tab()->id;
        case tabs_api::mojom::Data::Tag::kTabStrip:
          return data->get_tab_strip()->id;
        case tabs_api::mojom::Data::Tag::kPinnedTabs:
          return data->get_pinned_tabs()->id;
        case tabs_api::mojom::Data::Tag::kUnpinnedTabs:
          return data->get_unpinned_tabs()->id;
        case tabs_api::mojom::Data::Tag::kTabGroup:
          return data->get_tab_group()->id;
        case tabs_api::mojom::Data::Tag::kSplitTab:
          return data->get_split_tab()->id;
      }
    }

    for (const auto& child : container.children) {
      if (auto found_id = FindContainer(*child, target_tag);
          found_id.has_value()) {
        return found_id;
      }
    }
    return std::nullopt;
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TabStripServiceMojoHandler> tab_strip_service_mojo_handler_;
};

IN_PROC_BROWSER_TEST_F(TabStripServiceImplBrowserTest, SynchronousObserver) {
  ReallyVerySimpleSyncObserver observer;

  auto* service = tab_strip_service_mojo_handler_->GetTabStripService();
  service->AddObserver(&observer);

  ASSERT_EQ(0, observer.num_callbacks);

  auto result = service->CreateTabAt(tabs_api::Position(0),
                                     std::make_optional(GURL("www.foo.bear")));

  ASSERT_TRUE(result.has_value());

  ASSERT_EQ(1, observer.num_callbacks);
}

IN_PROC_BROWSER_TEST_F(TabStripServiceImplBrowserTest, PreventsReentrancy) {
  auto* service = tab_strip_service_mojo_handler_->GetTabStripService();

  class ReallyBadObserver
      : public tabs_api::observation::TabStripApiBatchedObserver {
   public:
    explicit ReallyBadObserver(tabs_api::TabStripService* service)
        : service_(service) {}
    ~ReallyBadObserver() override = default;

    void OnTabEvents(
        const std::vector<tabs_api::mojom::TabsEventPtr>& events) override {
      auto _ = service_->GetTabs();
    }

   private:
    raw_ptr<tabs_api::TabStripService> service_;
  };

  ReallyBadObserver observer(service);

  service->AddObserver(&observer);

  // We have a really bad observer that will attempt to re-enter. Assert that
  // this is disallowed.
  EXPECT_CHECK_DEATH([&] {
    auto _ = service->CreateTabAt(tabs_api::Position(0),
                                  std::make_optional(GURL("www.foo.bear")));
  }());
}

IN_PROC_BROWSER_TEST_F(TabStripServiceImplBrowserTest, CreateTabAt) {
  mojo::Remote<TabStripService> remote;
  tab_strip_service_mojo_handler_->Accept(remote.BindNewPipeAndPassReceiver());

  TabStripModel* model = GetTabStripModel();
  const int expected_tab_count = model->count() + 1;
  const GURL url("http://example.com/");

  base::RunLoop run_loop;

  TabStripService::CreateTabAtResult result;
  remote->CreateTabAt(
      tabs_api::Position(0), std::make_optional(url),
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

IN_PROC_BROWSER_TEST_F(TabStripServiceImplBrowserTest,
                       CreateTabEventInCollection) {
  auto observation = SetUpObservation();
  TabStripModel* model = GetTabStripModel();

  auto get_snapshot_collection_ids = [&]() {
    tabs_api::NodeId pinned_id;
    tabs_api::NodeId unpinned_id;
    base::RunLoop run_loop;
    observation->remote->GetTabs(
        base::BindLambdaForTesting([&](TabStripService::GetTabsResult result) {
          auto root_container = std::move(result.value()->tab_strip);

          auto maybe_pinned = FindContainer(
              *root_container, tabs_api::mojom::Data::Tag::kPinnedTabs);
          pinned_id = maybe_pinned.value();

          auto maybe_unpinned = FindContainer(
              *root_container, tabs_api::mojom::Data::Tag::kUnpinnedTabs);
          unpinned_id = maybe_unpinned.value();

          run_loop.Quit();
        }));
    run_loop.Run();
    return std::make_pair(pinned_id, unpinned_id);
  };
  std::optional<GURL> url("http://example.com/");
  auto [pinned_node_id, unpinned_node_id] = get_snapshot_collection_ids();
  // Test creating a tab in the pinned collection
  base::RunLoop pinned_create_loop;
  observation->remote->CreateTabAt(
      tabs_api::Position(0, pinned_node_id), url,
      base::BindLambdaForTesting(
          [&](TabStripService::CreateTabAtResult result) {
            ASSERT_TRUE(result.has_value());
            pinned_create_loop.Quit();
          }));
  pinned_create_loop.Run();
  observation->receiver.FlushForTesting();
  ASSERT_EQ(model->count(), 2);
  ASSERT_TRUE(model->IsTabPinned(0));
  ASSERT_EQ(1u, observation->client.tab_created_events.size());

  const auto& pinned_event = observation->client.tab_created_events.back();
  ASSERT_EQ(1u, pinned_event->tabs.size());
  EXPECT_EQ(pinned_node_id, pinned_event->tabs[0]->position.parent_id());
  EXPECT_EQ(0u, pinned_event->tabs[0]->position.index());

  // Test creating the tab in an unpinned collection.
  base::RunLoop unpinned_create_loop;
  observation->remote->CreateTabAt(
      tabs_api::Position(0, unpinned_node_id), url,
      base::BindLambdaForTesting(
          [&](TabStripService::CreateTabAtResult result) {
            ASSERT_TRUE(result.has_value());
            unpinned_create_loop.Quit();
          }));
  unpinned_create_loop.Run();
  observation->receiver.FlushForTesting();

  ASSERT_EQ(model->count(), 3);
  ASSERT_EQ(2u, observation->client.tab_created_events.size());
  const auto& unpinned_event = observation->client.tab_created_events.back();
  ASSERT_EQ(1u, unpinned_event->tabs.size());
  EXPECT_EQ(unpinned_node_id, unpinned_event->tabs[0]->position.parent_id());
  EXPECT_EQ(0u, unpinned_event->tabs[0]->position.index());

  // Test creating the tab within a tab group collection.
  const tab_groups::TabGroupId group_id = model->AddToNewGroup({1});
  const TabGroup* group = model->group_model()->GetTabGroup(group_id);
  const tabs_api::NodeId group_node_id(
      tabs_api::NodeId::Type::kCollection,
      base::NumberToString(group->GetCollectionHandle().raw_value()));
  base::RunLoop group_create_loop;
  observation->remote->CreateTabAt(
      tabs_api::Position(0, group_node_id), url,
      base::BindLambdaForTesting(
          [&](TabStripService::CreateTabAtResult result) {
            ASSERT_TRUE(result.has_value());
            group_create_loop.Quit();
          }));
  group_create_loop.Run();
  observation->receiver.FlushForTesting();

  ASSERT_EQ(model->count(), 4);
  ASSERT_EQ(3u, observation->client.tab_created_events.size());
  const auto& group_event = observation->client.tab_created_events.back();
  ASSERT_EQ(1u, group_event->tabs.size());
  EXPECT_EQ(group_node_id, group_event->tabs[0]->position.parent_id());
  EXPECT_EQ(0u, group_event->tabs[0]->position.index());
}

IN_PROC_BROWSER_TEST_F(TabStripServiceImplBrowserTest, Observation) {
  mojo::Remote<TabStripService> remote;
  tab_strip_service_mojo_handler_->Accept(remote.BindNewPipeAndPassReceiver());
  TestTabStripClient client;
  mojo::AssociatedReceiver<tabs_api::mojom::TabsObserver> receiver(&client);
  const GURL url("http://example.com/");
  uint32_t target_index = 0;
  auto original_tab_id = tabs_api::NodeId::FromTabHandle(
      GetTabStripModel()->GetTabAtIndex(0)->GetHandle());
  base::RunLoop run_loop;

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
      tabs_api::Position(target_index), std::make_optional(url),
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
  ASSERT_TRUE(client.tabs.contains(std::string(created_tab->id.Id())));

  // Navigate to a new url which will modify the tab state.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://www.google.com/")));
  receiver.FlushForTesting();
  ASSERT_EQ("https://www.google.com/",
            client.tabs.find(std::string(created_tab->id.Id()))->second->url);

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
  tab_strip_service_mojo_handler_->Accept(remote.BindNewPipeAndPassReceiver());

  const int starting_num_tabs = GetTabStripModel()->GetTabCount();

  base::RunLoop create_loop;
  remote->CreateTabAt(tabs_api::Position(0),
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
  tab_strip_service_mojo_handler_->Accept(remote.BindNewPipeAndPassReceiver());

  tabs_api::NodeId created_id;
  base::RunLoop create_loop;
  // Append a new tab to the end, which will also focus it.
  remote->CreateTabAt(std::nullopt, std::make_optional(GURL("http://dark.web")),
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

// Create 5 tabs and select 3 random ones.
IN_PROC_BROWSER_TEST_F(TabStripServiceImplBrowserTest, SetSelectedTabs) {
  mojo::Remote<TabStripService> remote;
  tab_strip_service_mojo_handler_->Accept(remote.BindNewPipeAndPassReceiver());

  auto observation = SetUpObservation();

  // Created 5 tabs.
  for (int i = 0; i < 5; ++i) {
    base::RunLoop create_loop;
    remote->CreateTabAt(std::nullopt,
                        std::make_optional(GURL("http://some.where/nowhere")),
                        base::BindLambdaForTesting(
                            [&](TabStripService::CreateTabAtResult result) {
                              ASSERT_TRUE(result.has_value());
                              create_loop.Quit();
                            }));
    create_loop.Run();
  }
  observation->receiver.FlushForTesting();

  // TODO(crbug.com/412738255): need to account for the initial tab.
  ASSERT_EQ(6, GetTabStripModel()->count());

  // Now select 3 of the tabs.
  std::vector<tabs_api::NodeId> selection;
  do {
    auto tab_handle =
        GetTabStripModel()->GetTabAtIndex(selection.size())->GetHandle();
    selection.push_back(tabs_api::NodeId::FromTabHandle(tab_handle));
  } while (selection.size() < 3);

  base::RunLoop select_loop;
  remote->SetSelectedTabs(
      selection, selection.at(0),
      base::BindLambdaForTesting(
          [&](TabStripService::SetSelectedTabsResult result) {
            ASSERT_TRUE(result.has_value());
            select_loop.Quit();
          }));
  select_loop.Run();

  observation->receiver.FlushForTesting();

  ASSERT_EQ(5ul, observation->client.tabs.size());

  // Now check that the underlying model is correct and matches with the
  // observation.
  for (auto* tab : *GetTabStripModel()) {
    auto tab_id = base::NumberToString(tab->GetHandle().raw_value());
    if (!observation->client.tabs.contains(tab_id)) {
      continue;
    }
    auto node_id = tabs_api::NodeId::FromTabHandle(tab->GetHandle());

    bool should_be_active = node_id == selection.at(0);
    bool should_be_selected = std::find(selection.begin(), selection.end(),
                                        node_id) != selection.end();

    ASSERT_EQ(should_be_active, tab->IsActivated());
    ASSERT_EQ(should_be_selected, tab->IsSelected());

    auto& observation_tab = observation->client.tabs.at(tab_id);
    ASSERT_EQ(should_be_active, observation_tab->is_active)
        << "bad id was: " << tab_id;
    // TODO(crbug.com/412738255): there is a race that is preventing this from
    // reliably completing. Fix then retest.
  }
}

IN_PROC_BROWSER_TEST_F(TabStripServiceImplBrowserTest, MoveTab) {
  mojo::Remote<TabStripService> remote;
  tab_strip_service_mojo_handler_->Accept(remote.BindNewPipeAndPassReceiver());

  auto observation = SetUpObservation();

  // Append a new tab to the end, so we have two tabs to work with.
  base::RunLoop create_loop;
  remote->CreateTabAt(std::nullopt,
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
  base::RunLoop move_loop;
  remote->MoveNode(
      to_move_id, tabs_api::Position(target_idx),
      base::BindLambdaForTesting([&](TabStripService::MoveNodeResult result) {
        ASSERT_TRUE(result.has_value());
        move_loop.Quit();
      }));
  move_loop.Run();
  observation->receiver.FlushForTesting();

  // Tab should now have been moved to target idx.
  ASSERT_EQ(GetTabStripModel()->GetTabAtIndex(target_idx)->GetHandle(),
            handle_to_move);

  ASSERT_EQ(1ul, observation->client.move_events.size());

  auto& event = observation->client.move_events.at(0);
  ASSERT_EQ(to_move_id, event->id);
  ASSERT_EQ(0u, event->from.index());
  ASSERT_EQ(1u, event->to.index());
}

IN_PROC_BROWSER_TEST_F(TabStripServiceImplBrowserTest, MoveTabIntoGroup) {
  mojo::Remote<TabStripService> remote;
  tab_strip_service_mojo_handler_->Accept(remote.BindNewPipeAndPassReceiver());

  auto observation = SetUpObservation();

  TabStripModel* model = GetTabStripModel();
  for (int i = 0; i < 3; i++) {
    base::RunLoop create_loop;
    remote->CreateTabAt(std::nullopt,
                        std::make_optional(GURL("http://somwewhere.nowhere")),
                        base::BindLambdaForTesting(
                            [&](TabStripService::CreateTabAtResult result) {
                              ASSERT_TRUE(result.has_value());
                              create_loop.Quit();
                            }));
    create_loop.Run();
  }
  ASSERT_EQ(model->count(), 4);

  const tab_groups::TabGroupId group_id = model->AddToNewGroup({0, 1});
  const TabGroup* group = model->group_model()->GetTabGroup(group_id);
  const tabs_api::NodeId to_group_collection_id(
      tabs_api::NodeId::Type::kCollection,
      base::NumberToString(group->GetCollectionHandle().raw_value()));
  auto position = tabs_api::Position(1, to_group_collection_id);
  auto* tab_to_move = GetTabStripModel()->GetTabAtIndex(2);
  auto to_move_id = tabs_api::NodeId(
      tabs_api::NodeId::Type::kContent,
      base::NumberToString(tab_to_move->GetHandle().raw_value()));
  base::RunLoop move_loop;
  remote->MoveNode(
      to_move_id, position,
      base::BindLambdaForTesting([&](TabStripService::MoveNodeResult result) {
        ASSERT_TRUE(result.has_value());
        move_loop.Quit();
      }));
  move_loop.Run();
  observation->receiver.FlushForTesting();

  // Previously tab was at index 2. Now should be at index 1 of the TabGroup.
  EXPECT_EQ(model->GetTabAtIndex(1), tab_to_move);
  std::optional<tab_groups::TabGroupId> moved_tab_group =
      model->GetTabGroupForTab(1);
  ASSERT_TRUE(moved_tab_group.has_value());
  EXPECT_EQ(moved_tab_group.value(), group_id);
  EXPECT_EQ(model->group_model()->GetTabGroup(group_id)->tab_count(), 3);

  ASSERT_FALSE(observation->client.move_events.empty());
  tabs_api::mojom::OnNodeMovedEventPtr move_event;
  for (auto& event : observation->client.move_events) {
    if (event->id == to_move_id && event->to.parent_id().has_value() &&
        event->to.parent_id().value() == to_group_collection_id) {
      move_event = std::move(event);
      break;
    }
  }
  EXPECT_EQ(to_move_id, move_event->id);
  EXPECT_EQ(to_group_collection_id, move_event->to.parent_id().value());
}

IN_PROC_BROWSER_TEST_F(TabStripServiceImplBrowserTest, MoveGroupCollection) {
  mojo::Remote<TabStripService> remote;
  tab_strip_service_mojo_handler_->Accept(remote.BindNewPipeAndPassReceiver());

  TabStripModel* model = GetTabStripModel();
  for (int i = 0; i < 3; i++) {
    base::RunLoop create_loop;
    remote->CreateTabAt(std::nullopt,
                        std::make_optional(GURL("http://somwewhere.nowhere")),
                        base::BindLambdaForTesting(
                            [&](TabStripService::CreateTabAtResult result) {
                              ASSERT_TRUE(result.has_value());
                              create_loop.Quit();
                            }));
    create_loop.Run();
  }
  ASSERT_EQ(model->count(), 4);

  const tab_groups::TabGroupId group_id = model->AddToNewGroup({2, 3});
  const TabGroup* group = model->group_model()->GetTabGroup(group_id);
  const tabs_api::NodeId group_node_id(
      tabs_api::NodeId::Type::kCollection,
      base::NumberToString(group->GetCollectionHandle().raw_value()));

  // Move the group to the beginning of the unpinned tabs at index 0.
  base::RunLoop move_loop;
  remote->MoveNode(
      group_node_id, tabs_api::Position(0),
      base::BindLambdaForTesting([&](TabStripService::MoveNodeResult result) {
        ASSERT_TRUE(result.has_value());
        move_loop.Quit();
      }));
  move_loop.Run();

  // Expect the tab group to be at the first index: [g(t2, t3), t0, t1].
  std::optional<tab_groups::TabGroupId> moved_tab_group_id =
      model->GetTabGroupForTab(0);
  ASSERT_TRUE(moved_tab_group_id.has_value());
  EXPECT_EQ(moved_tab_group_id.value(), group_id);
  const TabGroup* moved_tab_group =
      model->group_model()->GetTabGroup(moved_tab_group_id.value());
  EXPECT_EQ(moved_tab_group->tab_count(), 2);
  EXPECT_EQ(model->count(), 4);
}

IN_PROC_BROWSER_TEST_F(TabStripServiceImplBrowserTest, MoveSplitCollection) {
  mojo::Remote<TabStripService> remote;
  tab_strip_service_mojo_handler_->Accept(remote.BindNewPipeAndPassReceiver());

  TabStripModel* model = GetTabStripModel();
  for (int i = 0; i < 3; i++) {
    base::RunLoop create_loop;
    remote->CreateTabAt(std::nullopt,
                        std::make_optional(GURL("http://somwewhere.nowhere")),
                        base::BindLambdaForTesting(
                            [&](TabStripService::CreateTabAtResult result) {
                              ASSERT_TRUE(result.has_value());
                              create_loop.Quit();
                            }));
    create_loop.Run();
  }
  ASSERT_EQ(model->count(), 4);

  model->ActivateTabAt(2);
  const split_tabs::SplitTabId split_id =
      model->AddToNewSplit({3}, split_tabs::SplitTabVisualData(),
                           split_tabs::SplitTabCreatedSource::kToolbarButton);

  // Unlike TabGroup, find the split node id by iterating through the TabStrip
  // through the GetTabs api.
  tabs_api::NodeId split_node_id;
  base::RunLoop get_tabs_loop;
  remote->GetTabs(base::BindOnce(
      &TabStripServiceImplBrowserTest::FindNodeId, base::Unretained(this),
      tabs_api::mojom::Data::Tag::kSplitTab, &split_node_id, &get_tabs_loop));
  get_tabs_loop.Run();
  ASSERT_FALSE(split_node_id.Id().empty());

  base::RunLoop move_loop;
  remote->MoveNode(
      split_node_id, tabs_api::Position(0),
      base::BindLambdaForTesting([&](TabStripService::MoveNodeResult result) {
        ASSERT_TRUE(result.has_value());
        move_loop.Quit();
      }));
  move_loop.Run();

  EXPECT_EQ(model->GetSplitForTab(0).value(), split_id);
  EXPECT_EQ(model->GetSplitForTab(1).value(), split_id);
  EXPECT_EQ(model->count(), 4);
}

IN_PROC_BROWSER_TEST_F(TabStripServiceImplBrowserTest,
                       UpdateTabGroupVisualData) {
  mojo::Remote<TabStripService> remote;
  mojo::Remote<TabStripExperimentService> experiment_remote;
  tab_strip_service_mojo_handler_->Accept(remote.BindNewPipeAndPassReceiver());
  tab_strip_service_mojo_handler_->AcceptExperimental(
      experiment_remote.BindNewPipeAndPassReceiver());
  TabStripModel* model = GetTabStripModel();

  base::RunLoop create_loop;
  remote->CreateTabAt(std::nullopt,
                      std::make_optional(GURL("http://somwewhere.nowhere")),
                      base::BindLambdaForTesting(
                          [&](TabStripService::CreateTabAtResult result) {
                            ASSERT_TRUE(result.has_value());
                            create_loop.Quit();
                          }));
  create_loop.Run();

  ASSERT_EQ(model->count(), 2);
  const tab_groups::TabGroupId group_id = model->AddToNewGroup({0, 1});
  const TabGroup* group = model->group_model()->GetTabGroup(group_id);
  ASSERT_NE(group, nullptr);

  const tabs_api::NodeId group_node_id(
      tabs_api::NodeId::Type::kCollection,
      base::NumberToString(group->GetCollectionHandle().raw_value()));

  std::u16string expected_title = u"super cool title";
  tab_groups::TabGroupVisualData new_visuals(
      expected_title, tab_groups::TabGroupColorId::kRed, false);
  base::RunLoop run_loop;
  experiment_remote->UpdateTabGroupVisual(
      group_node_id, new_visuals,
      base::BindLambdaForTesting(
          [&](TabStripExperimentService::UpdateTabGroupVisualResult result) {
            ASSERT_TRUE(result.has_value())
                << "UpdateTabGroupVisual failed: " << result.error()->message;
            run_loop.Quit();
          }));
  run_loop.Run();

  const TabGroup* updated_group = model->group_model()->GetTabGroup(group_id);
  ASSERT_NE(updated_group, nullptr);
  const tab_groups::TabGroupVisualData* updated_data =
      updated_group->visual_data();

  ASSERT_EQ(expected_title, updated_data->title());
  ASSERT_EQ(tab_groups::TabGroupColorId::kRed, updated_data->color());
}
