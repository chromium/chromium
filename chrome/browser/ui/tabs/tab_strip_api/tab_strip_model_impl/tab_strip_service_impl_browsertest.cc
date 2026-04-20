// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_impl.h"

#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/browser_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/experimental_platform_adapters_provider.h"
#include "chrome/browser/ui/tabs/tab_strip_api/observation/tab_strip_api_batched_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_model_impl/browser_adapter_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_model_impl/tab_strip_model_adapter_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_model_impl/tab_strip_model_injector.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
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
#include "ui/gfx/geometry/point.h"
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

  void OnNodesClosed(tabs_api::mojom::OnNodesClosedEventPtr& event) {
    for (auto& id : event->node_ids) {
      tabs.erase(std::string(id.Id()));
    }
    node_closed_events.push_back(event.Clone());
  }

  void OnNodeMoved(tabs_api::mojom::OnNodeMovedEventPtr event) {
    move_events.push_back(std::move(event));
  }

  void OnDataChanged(tabs_api::mojom::OnDataChangedEventPtr& event) {
    // TODO(crbug.com/412738255): this is a hack, because we are not correctly
    // adding the initial tab that is created by the tab strip. We should have
    // a test for GetTabSnapshot and properly populate the initial tab.
    switch (event->which()) {
      case tabs_api::mojom::OnDataChangedEvent::Tag::kTab: {
        const auto& tab = event->get_tab()->data;
        std::string id_str = std::string(tab->id.Id());
        if (tabs.contains(id_str)) {
          tabs.at(id_str) = tab.Clone();
        }
        break;
      }
      case tabs_api::mojom::OnDataChangedEvent::Tag::kTabGroup:
      case tabs_api::mojom::OnDataChangedEvent::Tag::kSplitTab:
        // TODO(crbug.com/412955607): implement this.
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
        case tabs_api::mojom::TabsEvent::Tag::kNodesClosedEvent:
          OnNodesClosed(event->get_nodes_closed_event());
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
  std::vector<tabs_api::mojom::OnNodesClosedEventPtr> node_closed_events;

  std::map<std::string, tabs_api::mojom::TabPtr> tabs;
};

class TabStripServiceImplBrowserTest : public InProcessBrowserTest {
 public:
  using TabStripService = tabs_api::mojom::TabStripService;
  using TabStripExperimentService = tabs_api::mojom::TabStripExperimentService;

  TabStripServiceImplBrowserTest() = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    tab_strip_service_ = std::make_unique<tabs_api::TabStripServiceImpl>(
        std::make_unique<tabs_api::tab_strip_model::TabStripModelInjector>(
            browser(), browser()->tab_strip_model()),
        std::make_unique<
            tabs_api::tab_strip_model::TabStripModelExperimentalInjector>(
            browser(), browser()->tab_strip_model()));
  }

  void TearDownOnMainThread() override {
    tab_strip_service_.reset();
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
    tab_strip_service_->Accept(
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
        case tabs_api::mojom::Data::Tag::kWindow:
          return data->get_window()->id;
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

  tabs_api::NodeId GetIdFromContainer(
      const tabs_api::mojom::Container& container) {
    const auto& data = container.data;
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
      case tabs_api::mojom::Data::Tag::kWindow:
        return data->get_window()->id;
    }
  }

  std::optional<std::vector<tabs_api::NodeId>> FindPathToContainer(
      const tabs_api::mojom::Container& container,
      tabs_api::mojom::Data::Tag target_tag,
      std::vector<tabs_api::NodeId> current_path = {}) {
    current_path.push_back(GetIdFromContainer(container));

    if (container.data->which() == target_tag) {
      return current_path;
    }

    for (const auto& child : container.children) {
      if (auto found_path =
              FindPathToContainer(*child, target_tag, current_path);
          found_path.has_value()) {
        return found_path;
      }
    }
    return std::nullopt;
  }

  tabs_api::mojom::TabPtr CreateTabAt(
      mojo::Remote<TabStripService>& remote,
      std::optional<tabs_api::Position> position = std::nullopt,
      std::optional<GURL> url = std::nullopt) {
    base::RunLoop run_loop;
    TabStripService::CreateTabAtResult result;
    remote->CreateTabAt(
        position, url,
        base::BindLambdaForTesting([&](TabStripService::CreateTabAtResult in) {
          result = std::move(in);
          run_loop.Quit();
        }));
    run_loop.Run();
    EXPECT_TRUE(result.has_value());
    return result.has_value() ? std::move(result.value()) : nullptr;
  }

  void CreateTabs(mojo::Remote<TabStripService>& remote,
                  int count,
                  std::optional<GURL> url = std::nullopt) {
    for (int i = 0; i < count; ++i) {
      CreateTabAt(remote, std::nullopt, url);
    }
  }

  std::unique_ptr<tabs_api::TabStripServiceImpl> tab_strip_service_;
};

IN_PROC_BROWSER_TEST_F(TabStripServiceImplBrowserTest, SynchronousObserver) {
  ReallyVerySimpleSyncObserver observer;

  tab_strip_service_->AddObserver(&observer);

  ASSERT_EQ(0, observer.num_callbacks);

  auto result = tab_strip_service_->CreateTabAt(
      tabs_api::Position(0), std::make_optional(GURL("www.foo.bear")));

  ASSERT_TRUE(result.has_value());

  ASSERT_EQ(1, observer.num_callbacks);
}

IN_PROC_BROWSER_TEST_F(TabStripServiceImplBrowserTest, PreventsReentrancy) {
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

  ReallyBadObserver observer(tab_strip_service_.get());

  tab_strip_service_->AddObserver(&observer);

  // We have a really bad observer that will attempt to re-enter. Assert that
  // this is disallowed.
  EXPECT_CHECK_DEATH([&] {
    auto _ = tab_strip_service_->CreateTabAt(
        tabs_api::Position(0), std::make_optional(GURL("www.foo.bear")));
  }());
}

IN_PROC_BROWSER_TEST_F(TabStripServiceImplBrowserTest, CreateTabAt) {
  mojo::Remote<TabStripService> remote;
  tab_strip_service_->Accept(remote.BindNewPipeAndPassReceiver());

  TabStripModel* model = GetTabStripModel();
  const int expected_tab_count = model->count() + 1;
  const GURL url("http://example.com/");

  auto tab = CreateTabAt(remote, tabs_api::Position(0), url);

  ASSERT_TRUE(tab);
  EXPECT_EQ(model->count(), expected_tab_count);

  auto handle = model->GetTabAtIndex(0)->GetHandle();
  ASSERT_EQ(base::NumberToString(handle.raw_value()), tab->id.Id());
  // Assert that newly created tabs are also activated.
  ASSERT_EQ(model->GetActiveTab()->GetHandle(), handle);
}

IN_PROC_BROWSER_TEST_F(TabStripServiceImplBrowserTest,
                       CreateTabEventInCollection) {
  auto observation = SetUpObservation();
  TabStripModel* model = GetTabStripModel();

  auto get_snapshot_collection_paths = [&]() {
    std::vector<tabs_api::NodeId> pinned_path;
    std::vector<tabs_api::NodeId> unpinned_path;
    base::RunLoop run_loop;
    observation->remote->GetTabs(
        base::BindLambdaForTesting([&](TabStripService::GetTabsResult result) {
          auto root_container = std::move(result.value()->tab_strip);

          auto maybe_pinned = FindPathToContainer(
              *root_container, tabs_api::mojom::Data::Tag::kPinnedTabs);
          pinned_path = maybe_pinned.value();

          auto maybe_unpinned = FindPathToContainer(
              *root_container, tabs_api::mojom::Data::Tag::kUnpinnedTabs);
          unpinned_path = maybe_unpinned.value();

          run_loop.Quit();
        }));
    run_loop.Run();
    return std::make_pair(pinned_path, unpinned_path);
  };
  std::optional<GURL> url("http://example.com/");
  auto [pinned_path, unpinned_path] = get_snapshot_collection_paths();
  // Test creating a tab in the pinned collection
  CreateTabAt(observation->remote,
              tabs_api::Position(0, tabs_api::Path(pinned_path)), url);
  observation->receiver.FlushForTesting();
  ASSERT_EQ(model->count(), 2);
  ASSERT_TRUE(model->IsTabPinned(0));
  ASSERT_EQ(1u, observation->client.tab_created_events.size());

  const auto& pinned_event = observation->client.tab_created_events.back();
  ASSERT_EQ(1u, pinned_event->tabs.size());
  EXPECT_EQ(pinned_path.back(),
            pinned_event->tabs[0]->position.path().components().back());
  EXPECT_EQ(0u, pinned_event->tabs[0]->position.index());

  // Test creating the tab in an unpinned collection.
  CreateTabAt(observation->remote,
              tabs_api::Position(0, tabs_api::Path(unpinned_path)), url);
  observation->receiver.FlushForTesting();

  ASSERT_EQ(model->count(), 3);
  ASSERT_EQ(2u, observation->client.tab_created_events.size());
  const auto& unpinned_event = observation->client.tab_created_events.back();
  ASSERT_EQ(1u, unpinned_event->tabs.size());
  EXPECT_EQ(unpinned_path.back(),
            unpinned_event->tabs[0]->position.path().components().back());
  EXPECT_EQ(0u, unpinned_event->tabs[0]->position.index());

  // Test creating the tab within a tab group collection.
  const tab_groups::TabGroupId group_id = model->AddToNewGroup({1});
  const TabGroup* group = model->group_model()->GetTabGroup(group_id);
  const tabs_api::NodeId group_node_id(
      tabs_api::NodeId::Type::kCollection,
      base::NumberToString(group->GetCollectionHandle().raw_value()));

  std::vector<tabs_api::NodeId> group_path = unpinned_path;
  group_path.push_back(group_node_id);

  CreateTabAt(observation->remote,
              tabs_api::Position(0, tabs_api::Path(group_path)), url);
  observation->receiver.FlushForTesting();

  ASSERT_EQ(model->count(), 4);
  ASSERT_EQ(3u, observation->client.tab_created_events.size());
  const auto& group_event = observation->client.tab_created_events.back();
  ASSERT_EQ(1u, group_event->tabs.size());
  EXPECT_EQ(group_node_id,
            group_event->tabs[0]->position.path().components().back());
  EXPECT_EQ(0u, group_event->tabs[0]->position.index());
}

IN_PROC_BROWSER_TEST_F(TabStripServiceImplBrowserTest, Observation) {
  mojo::Remote<TabStripService> remote;
  tab_strip_service_->Accept(remote.BindNewPipeAndPassReceiver());
  TestTabStripClient client;
  mojo::AssociatedReceiver<tabs_api::mojom::TabsObserver> receiver(&client);
  const GURL url("http://example.com/");
  uint32_t target_index = 0;
  auto original_tab_id = tabs_api::NodeId::FromTabHandle(
      GetTabStripModel()->GetTabAtIndex(0)->GetHandle());

  base::RunLoop get_tabs_loop;
  remote->GetTabs(
      base::BindLambdaForTesting([&](TabStripService::GetTabsResult result) {
        ASSERT_TRUE(result.has_value());
        // This is where the client sets up the binding!
        receiver.Bind(std::move(result.value()->stream));
        get_tabs_loop.Quit();
      }));
  get_tabs_loop.Run();

  auto created_tab = CreateTabAt(remote, tabs_api::Position(target_index), url);

  // Ensure that we've received the observation callback, which are not
  // guaranteed to happen immediately.
  receiver.FlushForTesting();

  ASSERT_TRUE(created_tab);

  ASSERT_EQ(1ul, client.tabs.size());
  ASSERT_TRUE(client.tabs.contains(std::string(created_tab->id.Id())));

  // Navigate to a new url which will modify the tab state.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://www.google.com/")));
  receiver.FlushForTesting();
  ASSERT_EQ("https://www.google.com/",
            client.tabs.find(std::string(created_tab->id.Id()))->second->url);

  TabStripService::CloseNodesResult close_result;
  base::RunLoop close_tab_loop;
  remote->CloseNodes(
      {created_tab->id},
      base::BindLambdaForTesting([&](TabStripService::CloseNodesResult in) {
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

IN_PROC_BROWSER_TEST_F(TabStripServiceImplBrowserTest, CloseNodes) {
  mojo::Remote<TabStripService> remote;
  tab_strip_service_->Accept(remote.BindNewPipeAndPassReceiver());

  const int starting_num_tabs = GetTabStripModel()->count();

  CreateTabAt(remote, tabs_api::Position(0), GURL("http://dark.web"));

  // We should now have one more tab than when we first started.
  ASSERT_EQ(starting_num_tabs + 1, GetTabStripModel()->count());
  const auto* interface = GetTabStripModel()->GetTabAtIndex(0);

  base::RunLoop close_loop;
  remote->CloseNodes(
      {tabs_api::NodeId(
          tabs_api::NodeId::Type::kContent,
          base::NumberToString(interface->GetHandle().raw_value()))},
      base::BindLambdaForTesting([&](TabStripService::CloseNodesResult result) {
        ASSERT_TRUE(result.has_value());
        close_loop.Quit();
      }));
  close_loop.Run();

  // We should be back to where we started.
  ASSERT_EQ(starting_num_tabs, GetTabStripModel()->count());
}

IN_PROC_BROWSER_TEST_F(TabStripServiceImplBrowserTest, RemoveTabGroup) {
  mojo::Remote<TabStripService> remote;
  tab_strip_service_->Accept(remote.BindNewPipeAndPassReceiver());
  auto observation = SetUpObservation();
  TabStripModel* model = GetTabStripModel();
  CreateTabs(remote, 3, GURL("http://somwewhere.nowhere"));
  ASSERT_EQ(model->count(), 4);

  const tab_groups::TabGroupId group_id = model->AddToNewGroup({0, 1, 2});
  observation->receiver.FlushForTesting();

  model->CloseAllTabsInGroup(group_id);
  observation->receiver.FlushForTesting();

  // Total number of nodes closed (3 tabs + 1 group collection).
  int closed_node_count = 0;
  for (const auto& event : observation->client.node_closed_events) {
    closed_node_count += event->node_ids.size();
  }
  EXPECT_EQ(closed_node_count, 4);
  EXPECT_EQ(model->count(), 1);
}

IN_PROC_BROWSER_TEST_F(TabStripServiceImplBrowserTest, ActivateTab) {
  mojo::Remote<TabStripService> remote;
  tab_strip_service_->Accept(remote.BindNewPipeAndPassReceiver());

  tabs_api::NodeId created_id;
  // Append a new tab to the end, which will also focus it.
  auto tab = CreateTabAt(remote, std::nullopt, GURL("http://dark.web"));
  created_id = tab->id;

  auto old_tab_handle = GetTabStripModel()->GetTabAtIndex(0)->GetHandle();
  // Creating a new tab should have caused the old tab to lose active state.
  ASSERT_NE(GetTabStripModel()->GetActiveTab()->GetHandle(), old_tab_handle);

  auto old_tab_id =
      tabs_api::NodeId(tabs_api::NodeId::Type::kContent,
                       base::NumberToString(old_tab_handle.raw_value()));
  base::RunLoop activate_loop;
  remote->ActivateTab(old_tab_id,
                      base::BindLambdaForTesting(
                          [&](TabStripService::ActivateTabResult result) {
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
  tab_strip_service_->Accept(remote.BindNewPipeAndPassReceiver());

  auto observation = SetUpObservation();

  // Created 5 tabs.
  CreateTabs(remote, 5, GURL("http://some.where/nowhere"));
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
  tab_strip_service_->Accept(remote.BindNewPipeAndPassReceiver());

  auto observation = SetUpObservation();

  // Append a new tab to the end, so we have two tabs to work with.
  CreateTabAt(remote, std::nullopt, GURL("http://somwewhere.nowhere"));

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
  tab_strip_service_->Accept(remote.BindNewPipeAndPassReceiver());

  auto observation = SetUpObservation();

  TabStripModel* model = GetTabStripModel();
  CreateTabs(remote, 3, GURL("http://somwewhere.nowhere"));
  ASSERT_EQ(model->count(), 4);

  const tab_groups::TabGroupId group_id = model->AddToNewGroup({0, 1});
  const TabGroup* group = model->group_model()->GetTabGroup(group_id);
  const tabs_api::NodeId to_group_collection_id(
      tabs_api::NodeId::Type::kCollection,
      base::NumberToString(group->GetCollectionHandle().raw_value()));

  std::vector<tabs_api::NodeId> group_path;
  base::RunLoop get_tabs_loop;
  observation->remote->GetTabs(
      base::BindLambdaForTesting([&](TabStripService::GetTabsResult result) {
        auto root_container = std::move(result.value()->tab_strip);
        auto maybe_group_path = FindPathToContainer(
            *root_container, tabs_api::mojom::Data::Tag::kTabGroup);
        group_path = maybe_group_path.value();
        get_tabs_loop.Quit();
      }));
  get_tabs_loop.Run();

  auto position = tabs_api::Position(1, tabs_api::Path(group_path));
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
    if (event->id == to_move_id && !event->to.path().components().empty() &&
        event->to.path().components().back() == to_group_collection_id) {
      move_event = std::move(event);
      break;
    }
  }
  EXPECT_EQ(to_move_id, move_event->id);
  EXPECT_EQ(to_group_collection_id, move_event->to.path().components().back());
}

IN_PROC_BROWSER_TEST_F(TabStripServiceImplBrowserTest, MoveGroupCollection) {
  mojo::Remote<TabStripService> remote;
  tab_strip_service_->Accept(remote.BindNewPipeAndPassReceiver());

  TabStripModel* model = GetTabStripModel();
  CreateTabs(remote, 3, GURL("http://somwewhere.nowhere"));
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
  tab_strip_service_->Accept(remote.BindNewPipeAndPassReceiver());

  TabStripModel* model = GetTabStripModel();
  CreateTabs(remote, 3, GURL("http://somwewhere.nowhere"));
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

IN_PROC_BROWSER_TEST_F(TabStripServiceImplBrowserTest, ReplaceTabInSplit) {
  mojo::Remote<TabStripService> remote;
  mojo::Remote<TabStripExperimentService> experiment_remote;
  tab_strip_service_->Accept(remote.BindNewPipeAndPassReceiver());
  tab_strip_service_->AcceptExperimental(
      experiment_remote.BindNewPipeAndPassReceiver());

  TabStripModel* model = GetTabStripModel();
  CreateTabs(remote, 3, GURL("http://somewhere.nowhere"));
  ASSERT_EQ(model->count(), 4);

  // Create a split with tabs at index 2 and 3.
  model->ActivateTabAt(2);
  const split_tabs::SplitTabId split_id =
      model->AddToNewSplit({3}, split_tabs::SplitTabVisualData(),
                           split_tabs::SplitTabCreatedSource::kToolbarButton);

  auto split_tab_handle = model->GetTabAtIndex(2)->GetHandle();
  auto replacement_tab_handle = model->GetTabAtIndex(0)->GetHandle();

  tabs_api::NodeId split_tab_id =
      tabs_api::NodeId::FromTabHandle(split_tab_handle);
  tabs_api::NodeId replacement_tab_id =
      tabs_api::NodeId::FromTabHandle(replacement_tab_handle);

  base::RunLoop replace_loop;
  experiment_remote->ReplaceTabInSplit(
      split_tab_id, replacement_tab_id,
      base::BindLambdaForTesting(
          [&](TabStripExperimentService::ReplaceTabInSplitResult result) {
            ASSERT_TRUE(result.has_value());
            replace_loop.Quit();
          }));
  replace_loop.Run();

  // The split tab should have been closed, and the replacement tab should now
  // be part of the split at the same position.
  ASSERT_EQ(model->count(), 3);
  int replacement_index = model->GetIndexOfTab(replacement_tab_handle.Get());
  ASSERT_NE(TabStripModel::kNoTab, replacement_index);
  EXPECT_EQ(model->GetSplitForTab(replacement_index).value(), split_id);
  EXPECT_TRUE(replacement_tab_handle.Get()->IsActivated());
}

IN_PROC_BROWSER_TEST_F(TabStripServiceImplBrowserTest, UpdateTabGroupData) {
  mojo::Remote<TabStripService> remote;
  mojo::Remote<TabStripExperimentService> experiment_remote;
  tab_strip_service_->Accept(remote.BindNewPipeAndPassReceiver());
  tab_strip_service_->AcceptExperimental(
      experiment_remote.BindNewPipeAndPassReceiver());
  TabStripModel* model = GetTabStripModel();

  CreateTabAt(remote, std::nullopt, GURL("http://somewhere.nowhere"));

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
  auto tab_group = tabs_api::mojom::TabGroup::New(group_node_id, new_visuals);
  auto data = tabs_api::mojom::Data::NewTabGroup(std::move(tab_group));
  remote->Update(
      std::move(data), std::nullopt,
      base::BindLambdaForTesting(
          [&](tabs_api::mojom::TabStripService::UpdateResult result) {
            ASSERT_TRUE(result.has_value())
                << "Update failed: " << result.error()->message;
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

// Tests ShowTabContextMenu() api. Currently it only verifies that the api
// call succeeds.
// TODO(crbug.com/470136275): verifies that the context menu is actually shown.
IN_PROC_BROWSER_TEST_F(TabStripServiceImplBrowserTest, ShowTabContextMenu) {
  mojo::Remote<TabStripService> remote;
  mojo::Remote<TabStripExperimentService> experiment_remote;
  tab_strip_service_->Accept(remote.BindNewPipeAndPassReceiver());
  tab_strip_service_->AcceptExperimental(
      experiment_remote.BindNewPipeAndPassReceiver());

  tabs_api::NodeId created_id;
  auto tab =
      CreateTabAt(remote, std::nullopt, GURL("http://somewhere.nowhere"));
  created_id = tab->id;

  base::RunLoop run_loop;
  experiment_remote->ShowTabContextMenu(
      created_id, gfx::Point(100, 100),
      base::BindLambdaForTesting(
          [&](TabStripExperimentService::ShowTabContextMenuResult result) {
            EXPECT_TRUE(result.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(TabStripServiceImplBrowserTest, GetAllTabsForProfile) {
  mojo::Remote<TabStripExperimentService> experiment_remote;
  tab_strip_service_->AcceptExperimental(
      experiment_remote.BindNewPipeAndPassReceiver());

  CreateBrowser(browser()->profile());

  base::RunLoop run_loop;
  experiment_remote->GetAllTabsForProfile(base::BindLambdaForTesting(
      [&](TabStripExperimentService::GetAllTabsForProfileResult result) {
        ASSERT_TRUE(result.has_value());
        const auto& windows = result.value();
        ASSERT_GE(windows.size(), 2u);

        for (const auto& [id, window_container] : windows) {
          ASSERT_TRUE(window_container->data->is_window());
          ASSERT_EQ(1u, window_container->children.size());

          const auto& tab_strip_container = window_container->children[0];
          ASSERT_TRUE(tab_strip_container->data->is_tab_strip());
        }
        run_loop.Quit();
      }));
  run_loop.Run();
}
