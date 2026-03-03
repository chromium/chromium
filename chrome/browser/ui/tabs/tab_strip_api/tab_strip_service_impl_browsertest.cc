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
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/browser_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/browser_adapter_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tab_strip_model_adapter_impl.h"
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
      case tabs_api::mojom::Data::Tag::kWindow:
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
    feature_list_.InitWithFeatures({features::kTabStripBrowserApi}, {});
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

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TabStripServiceMojoHandler> tab_strip_service_mojo_handler_;
};

class TabStripServiceDirectBrowserTest : public InProcessBrowserTest {
 public:
  TabStripServiceDirectBrowserTest() {
    feature_list_.InitWithFeatures({features::kTabStripBrowserApi}, {});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    service_ = std::make_unique<tabs_api::TabStripServiceImpl>(
        std::make_unique<tabs_api::BrowserAdapterImpl>(browser()),
        std::make_unique<tabs_api::TabStripModelAdapterImpl>(
            browser()->tab_strip_model(),
            base::NumberToString(browser()->GetSessionID().id())));
  }

  void TearDownOnMainThread() override { service_.reset(); }

 protected:
  TabStripModel* GetTabStripModel() { return browser()->tab_strip_model(); }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<tabs_api::TabStripServiceImpl> service_;
};

IN_PROC_BROWSER_TEST_F(TabStripServiceDirectBrowserTest, CreateNewTab) {
  TabStripModel* model = GetTabStripModel();

  ASSERT_EQ(1, model->count());

  auto result = service_->CreateTabAt(std::nullopt, std::nullopt);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(2, model->count());

  tabs::TabHandle created_handle = model->GetTabAtIndex(1)->GetHandle();
  ASSERT_EQ(base::NumberToString(created_handle.raw_value()),
            result.value()->id.Id());
  ASSERT_EQ(tabs_api::NodeId::Type::kContent, result.value()->id.Type());
}

IN_PROC_BROWSER_TEST_F(TabStripServiceDirectBrowserTest, GetTabs) {
  auto result = service_->GetTabs();

  ASSERT_TRUE(result.has_value());
  const auto& window = result.value();
  ASSERT_TRUE(window->data->is_window());
  ASSERT_EQ(1u, window->children.size());

  const auto& tab_strip = window->children[0];
  ASSERT_TRUE(tab_strip->data->is_tab_strip());

  // Root collection has Pinned and Unpinned collections.
  ASSERT_EQ(2u, tab_strip->children.size());
  ASSERT_TRUE(tab_strip->children[0]->data->is_pinned_tabs());
  ASSERT_TRUE(tab_strip->children[1]->data->is_unpinned_tabs());

  const auto& unpinned_tabs = tab_strip->children[1];
  // The browser starts with 1 tab by default in the unpinned collection.
  ASSERT_EQ(1u, unpinned_tabs->children.size());
  ASSERT_TRUE(unpinned_tabs->children[0]->data->is_tab());

  auto handle = GetTabStripModel()->GetTabAtIndex(0)->GetHandle();
  ASSERT_EQ(base::NumberToString(handle.raw_value()),
            unpinned_tabs->children[0]->data->get_tab()->id.Id());
  ASSERT_EQ(tabs_api::NodeId::Type::kContent,
            unpinned_tabs->children[0]->data->get_tab()->id.Type());
}

IN_PROC_BROWSER_TEST_F(TabStripServiceDirectBrowserTest, GetTab) {
  auto handle = GetTabStripModel()->GetTabAtIndex(0)->GetHandle();
  tabs_api::NodeId tab_id(tabs_api::NodeId::Type::kContent,
                          base::NumberToString(handle.raw_value()));
  auto result = service_->GetTab(tab_id);

  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result.value()->id.Id(), base::NumberToString(handle.raw_value()));
  ASSERT_EQ(result.value()->id.Type(), tabs_api::NodeId::Type::kContent);
}

IN_PROC_BROWSER_TEST_F(TabStripServiceDirectBrowserTest, GetTab_NotFound) {
  tabs_api::NodeId tab_id(tabs_api::NodeId::Type::kContent, "666");

  auto result = service_->GetTab(tab_id);

  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(result.error()->code, mojo_base::mojom::Code::kNotFound);
}

IN_PROC_BROWSER_TEST_F(TabStripServiceDirectBrowserTest, CloseTabs) {
  // Add a tab so we can close it without closing the browser.
  auto create_result = service_->CreateTabAt(std::nullopt, std::nullopt);
  ASSERT_TRUE(create_result.has_value());
  ASSERT_EQ(2, GetTabStripModel()->count());

  auto result = service_->CloseTabs({create_result.value()->id});

  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(1, GetTabStripModel()->count());
}

IN_PROC_BROWSER_TEST_F(TabStripServiceDirectBrowserTest, ActivateTab) {
  auto tab1_handle = GetTabStripModel()->GetTabAtIndex(0)->GetHandle();

  auto create_result = service_->CreateTabAt(std::nullopt, std::nullopt);
  auto tab2_handle = GetTabStripModel()->GetTabAtIndex(1)->GetHandle();

  // New tab should be activated.
  ASSERT_EQ(GetTabStripModel()->GetActiveTab()->GetHandle(), tab2_handle);

  tabs_api::NodeId tab1_id(tabs_api::NodeId::Type::kContent,
                           base::NumberToString(tab1_handle.raw_value()));

  auto result = service_->ActivateTab(tab1_id);
  ASSERT_TRUE(result.has_value());

  ASSERT_EQ(GetTabStripModel()->GetActiveTab()->GetHandle(), tab1_handle);
}

IN_PROC_BROWSER_TEST_F(TabStripServiceDirectBrowserTest, ActivateTab_NotFound) {
  tabs_api::NodeId tab_id(tabs_api::NodeId::Type::kContent, "111");

  auto result = service_->ActivateTab(tab_id);

  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(result.error()->code, mojo_base::mojom::Code::kNotFound);
}

IN_PROC_BROWSER_TEST_F(TabStripServiceDirectBrowserTest, SetSelectedTabs) {
  auto tab1_handle = GetTabStripModel()->GetTabAtIndex(0)->GetHandle();
  auto create_result = service_->CreateTabAt(std::nullopt, std::nullopt);

  tabs_api::NodeId tab1_id(tabs_api::NodeId::Type::kContent,
                           base::NumberToString(tab1_handle.raw_value()));
  tabs_api::NodeId tab2_id = create_result.value()->id;

  // tab2 is currently active and selected.
  ASSERT_TRUE(GetTabStripModel()->GetTabAtIndex(1)->IsActivated());

  auto result = service_->SetSelectedTabs({tab1_id}, tab1_id);
  ASSERT_TRUE(result.has_value());

  ASSERT_TRUE(GetTabStripModel()->GetTabAtIndex(0)->IsActivated());
  ASSERT_TRUE(GetTabStripModel()->GetTabAtIndex(0)->IsSelected());
  ASSERT_FALSE(GetTabStripModel()->GetTabAtIndex(1)->IsActivated());
  ASSERT_FALSE(GetTabStripModel()->GetTabAtIndex(1)->IsSelected());
}

IN_PROC_BROWSER_TEST_F(TabStripServiceDirectBrowserTest,
                       SetSelectedTabs_MultipleSelection) {
  // Browser starts with 1 tab. Add 3 more for a total of 4.
  auto create_tab_result = service_->CreateTabAt(std::nullopt, std::nullopt);
  ASSERT_TRUE(create_tab_result.has_value());
  create_tab_result = service_->CreateTabAt(std::nullopt, std::nullopt);
  ASSERT_TRUE(create_tab_result.has_value());
  create_tab_result = service_->CreateTabAt(std::nullopt, std::nullopt);
  ASSERT_TRUE(create_tab_result.has_value());

  ASSERT_EQ(4, GetTabStripModel()->count());

  std::vector<tabs_api::NodeId> selection;
  for (int i = 0; i < 4; ++i) {
    selection.push_back(tabs_api::NodeId::FromTabHandle(
        GetTabStripModel()->GetTabAtIndex(i)->GetHandle()));
  }

  tabs_api::NodeId active_id = selection.back();

  auto result = service_->SetSelectedTabs(selection, active_id);
  ASSERT_TRUE(result.has_value());

  for (int i = 0; i < 4; ++i) {
    ASSERT_TRUE(GetTabStripModel()->GetTabAtIndex(i)->IsSelected());
  }
  ASSERT_TRUE(GetTabStripModel()->GetTabAtIndex(3)->IsActivated());
}

IN_PROC_BROWSER_TEST_F(TabStripServiceDirectBrowserTest, MoveTab) {
  // Add 2 more tabs for a total of 3.
  auto create_tab_result = service_->CreateTabAt(std::nullopt, std::nullopt);
  ASSERT_TRUE(create_tab_result.has_value());
  create_tab_result = service_->CreateTabAt(std::nullopt, std::nullopt);
  ASSERT_TRUE(create_tab_result.has_value());

  auto handle_to_move = GetTabStripModel()->GetTabAtIndex(0)->GetHandle();
  tabs_api::NodeId to_move_id(tabs_api::NodeId::Type::kContent,
                              base::NumberToString(handle_to_move.raw_value()));

  // Move tab 0 to index 2.
  auto result = service_->MoveNode(to_move_id, tabs_api::Position(2));
  ASSERT_TRUE(result.has_value());

  ASSERT_EQ(GetTabStripModel()->GetTabAtIndex(2)->GetHandle(), handle_to_move);
}

IN_PROC_BROWSER_TEST_F(TabStripServiceDirectBrowserTest, MoveTab_OutOfRange) {
  auto handle = GetTabStripModel()->GetTabAtIndex(0)->GetHandle();
  tabs_api::NodeId tab_id(tabs_api::NodeId::Type::kContent,
                          base::NumberToString(handle.raw_value()));

  auto result = service_->MoveNode(tab_id, tabs_api::Position(9001));

  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);
}

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
  base::RunLoop pinned_create_loop;
  observation->remote->CreateTabAt(
      tabs_api::Position(0, tabs_api::Path(pinned_path)), url,
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
  EXPECT_EQ(pinned_path.back(),
            pinned_event->tabs[0]->position.path().components().back());
  EXPECT_EQ(0u, pinned_event->tabs[0]->position.index());

  // Test creating the tab in an unpinned collection.
  base::RunLoop unpinned_create_loop;
  observation->remote->CreateTabAt(
      tabs_api::Position(0, tabs_api::Path(unpinned_path)), url,
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

  base::RunLoop group_create_loop;
  observation->remote->CreateTabAt(
      tabs_api::Position(0, tabs_api::Path(group_path)), url,
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
  EXPECT_EQ(group_node_id,
            group_event->tabs[0]->position.path().components().back());
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

  const int starting_num_tabs = GetTabStripModel()->count();

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
  ASSERT_EQ(starting_num_tabs + 1, GetTabStripModel()->count());
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
  ASSERT_EQ(starting_num_tabs, GetTabStripModel()->count());
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
                      std::make_optional(GURL("http://somewhere.nowhere")),
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

// Tests ShowTabContextMenu() api. Currently it only verifies that the api
// call succeeds.
// TODO(crbug.com/470136275): verifies that the context menu is actually shown.
IN_PROC_BROWSER_TEST_F(TabStripServiceImplBrowserTest, ShowTabContextMenu) {
  mojo::Remote<TabStripService> remote;
  mojo::Remote<TabStripExperimentService> experiment_remote;
  tab_strip_service_mojo_handler_->Accept(remote.BindNewPipeAndPassReceiver());
  tab_strip_service_mojo_handler_->AcceptExperimental(
      experiment_remote.BindNewPipeAndPassReceiver());

  tabs_api::NodeId created_id;
  base::RunLoop create_loop;
  remote->CreateTabAt(std::nullopt,
                      std::make_optional(GURL("http://somewhere.nowhere")),
                      base::BindLambdaForTesting(
                          [&](TabStripService::CreateTabAtResult result) {
                            ASSERT_TRUE(result.has_value());
                            created_id = result.value()->id;
                            create_loop.Quit();
                          }));
  create_loop.Run();

  base::RunLoop run_loop;
  experiment_remote->ShowTabContextMenu(
      created_id, gfx::Point(100, 100),
      base::BindLambdaForTesting(
          [&](TabStripExperimentService::ShowTabContextMenuResult result) {
            ASSERT_TRUE(result.has_value())
                << "ShowTabContextMenu failed: " << result.error()->message;
            run_loop.Quit();
          }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(TabStripServiceImplBrowserTest, GetAllTabsForProfile) {
  mojo::Remote<TabStripExperimentService> experiment_remote;
  tab_strip_service_mojo_handler_->AcceptExperimental(
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
