// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_strip/tab_strip_service_view.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "components/browser_apis/tab_strip/tab_strip_api_data_model.mojom.h"
#include "components/browser_apis/tab_strip/tab_strip_api_events.mojom.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "components/browser_apis/tab_strip/types/path.h"
#include "components/browser_apis/tab_strip/types/position.h"
#include "components/browser_apis/tab_strip/types/tab.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace tabs_api {
namespace {

class FakeTabStripService : public TabStripService {
 public:
  explicit FakeTabStripService(mojom::ContainerPtr initial_root)
      : initial_root_(std::move(initial_root)) {}
  ~FakeTabStripService() override = default;

  void Emit(mojom::TabsEventPtr event) {
    std::vector<mojom::TabsEventPtr> batch;
    batch.push_back(std::move(event));
    for (auto& observer : observers_) {
      observer.OnTabEvents(batch);
    }
  }

  bool HasObservers() const { return !observers_.empty(); }

  // TabStripService:
  GetTabsResult GetTabsWithoutObservation() override {
    if (!initial_root_) {
      return base::unexpected(mojo_base::mojom::Error::New(
          mojo_base::mojom::Code::kInternal, "no root"));
    }
    return initial_root_.Clone();
  }
  void AddObserver(observation::TabStripApiBatchedObserver* observer) override {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(
      observation::TabStripApiBatchedObserver* observer) override {
    observers_.RemoveObserver(observer);
  }
  void Accept(mojo::PendingReceiver<mojom::TabStripService>) override {}
#if !BUILDFLAG(IS_ANDROID)
  void AcceptExperimental(
      mojo::PendingReceiver<mojom::TabStripExperimentService>) override {}
#endif

  // mojom::TabStripServiceDirectReturnStub:
  mojom::TabStripService::GetTabsResult GetTabs() override {
    return base::unexpected(mojo_base::mojom::Error::New());
  }
  mojom::TabStripService::GetTabResult GetTab(const NodeId&) override {
    return base::unexpected(mojo_base::mojom::Error::New());
  }
  mojom::TabStripService::CreateTabAtResult CreateTabAt(
      const std::optional<Position>&,
      const std::optional<GURL>&) override {
    return base::unexpected(mojo_base::mojom::Error::New());
  }
  mojom::TabStripService::CloseNodesResult CloseNodes(
      const std::vector<NodeId>&) override {
    return base::unexpected(mojo_base::mojom::Error::New());
  }
  mojom::TabStripService::ActivateTabResult ActivateTab(
      const NodeId&) override {
    return base::unexpected(mojo_base::mojom::Error::New());
  }
  mojom::TabStripService::SetSelectedTabsResult SetSelectedTabs(
      const std::vector<NodeId>&,
      const NodeId&) override {
    return base::unexpected(mojo_base::mojom::Error::New());
  }
  mojom::TabStripService::MoveNodeResult MoveNode(const NodeId&,
                                                  const Position&) override {
    return base::unexpected(mojo_base::mojom::Error::New());
  }
  mojom::TabStripService::UpdateResult Update(
      mojom::DataPtr,
      const std::optional<std::vector<std::string>>&) override {
    return base::unexpected(mojo_base::mojom::Error::New());
  }

#if !BUILDFLAG(IS_ANDROID)
  // mojom::TabStripExperimentServiceDirectReturnStub:
  mojom::TabStripExperimentService::GetAllTabsForProfileResult
  GetAllTabsForProfile() override {
    return base::unexpected(mojo_base::mojom::Error::New());
  }
  mojom::TabStripExperimentService::ReplaceTabInSplitResult ReplaceTabInSplit(
      const NodeId&,
      const NodeId&) override {
    return base::unexpected(mojo_base::mojom::Error::New());
  }
#endif

 private:
  mojom::ContainerPtr initial_root_;
  base::ObserverList<observation::TabStripApiBatchedObserver> observers_;
};

Tab MakeTab(int32_t handle_id,
            std::u16string title,
            std::string_view url,
            bool is_active) {
  Tab tab;
  tab.id = NodeId::FromTabHandle(tabs::TabHandle(handle_id));
  tab.title = std::move(title);
  tab.url = GURL(url);
  tab.is_active = is_active;
  tab.last_active = base::TimeTicks() + base::Seconds(handle_id);
  return tab;
}

mojom::ContainerPtr BuildInitialTree() {
  auto window = mojom::Container::New();
  window->data = mojom::Data::NewWindow(
      mojom::Window::New(NodeId(NodeId::Type::kWindow, "w1")));

  auto strip = mojom::Container::New();
  strip->data = mojom::Data::NewTabStrip(
      mojom::TabStrip::New(NodeId(NodeId::Type::kCollection, "s1")));

  auto unpinned = mojom::Container::New();
  unpinned->data = mojom::Data::NewUnpinnedTabs(
      mojom::UnpinnedTabs::New(NodeId(NodeId::Type::kCollection, "u1")));

  auto t1 = mojom::Container::New();
  t1->data = mojom::Data::NewTab(
      MakeTab(10, u"First", "https://one.example/", /*is_active=*/false));
  unpinned->children.push_back(std::move(t1));

  auto t2 = mojom::Container::New();
  t2->data = mojom::Data::NewTab(
      MakeTab(20, u"Second", "https://two.example/", /*is_active=*/true));
  unpinned->children.push_back(std::move(t2));

  strip->children.push_back(std::move(unpinned));
  window->children.push_back(std::move(strip));
  return window;
}

Path UnpinnedPath() {
  return Path({
      NodeId(NodeId::Type::kWindow, "w1"),
      NodeId(NodeId::Type::kCollection, "s1"),
      NodeId(NodeId::Type::kCollection, "u1"),
  });
}

}  // namespace

TEST(TabStripServiceViewTest, InitialSnapshotAndObserverLifecycle) {
  FakeTabStripService service(BuildInitialTree());
  EXPECT_FALSE(service.HasObservers());

  {
    TabStripServiceView view(service);
    EXPECT_TRUE(service.HasObservers());
    ASSERT_NE(view.root(), nullptr);

    const Tab* active = view.GetActiveTab();
    ASSERT_NE(active, nullptr);
    EXPECT_EQ(active->ToTabHandle().raw_value(), 20);
    EXPECT_EQ(active->title, u"Second");

    const Tab* by_handle = view.GetTab(tabs::TabHandle(10));
    ASSERT_NE(by_handle, nullptr);
    EXPECT_EQ(by_handle->title, u"First");
    EXPECT_EQ(view.GetParentId(by_handle->id),
              NodeId(NodeId::Type::kCollection, "u1"));

    ASSERT_EQ(view.GetTabs().size(), 2u);
    EXPECT_EQ(view.GetTabs()[0]->ToTabHandle().raw_value(), 10);
    EXPECT_EQ(view.GetTabs()[1]->ToTabHandle().raw_value(), 20);
  }

  EXPECT_FALSE(service.HasObservers());
}

TEST(TabStripServiceViewTest, CreateMoveUpdateAndCloseEvents) {
  FakeTabStripService service(BuildInitialTree());
  TabStripServiceView view(service);

  // 1. Insert a new active tab at index 1 in unpinned tabs.
  auto created_event = mojom::OnTabsCreatedEvent::New();
  auto created = mojom::TabCreatedContainer::New();
  created->tab =
      MakeTab(30, u"Third", "https://three.example/", /*is_active=*/true);
  created->position = Position(1, UnpinnedPath());
  created_event->tabs.push_back(std::move(created));
  service.Emit(mojom::TabsEvent::NewTabsCreatedEvent(std::move(created_event)));

  ASSERT_EQ(view.GetTabs().size(), 3u);
  EXPECT_EQ(view.GetTabs()[0]->ToTabHandle().raw_value(), 10);
  EXPECT_EQ(view.GetTabs()[1]->ToTabHandle().raw_value(), 30);
  EXPECT_EQ(view.GetTabs()[2]->ToTabHandle().raw_value(), 20);
  ASSERT_NE(view.GetActiveTab(), nullptr);
  EXPECT_EQ(view.GetActiveTab()->ToTabHandle().raw_value(), 30);
  EXPECT_FALSE(view.GetTab(tabs::TabHandle(20))->is_active);

  // 2. Create a TabGroup collection at index 0 and move tab 20 into it.
  const NodeId group_id(NodeId::Type::kCollection, "g1");
  auto group_created = mojom::OnCollectionCreatedEvent::New();
  group_created->position = Position(0, UnpinnedPath());
  group_created->collection = mojom::Container::New();
  group_created->collection->data = mojom::Data::NewTabGroup(
      mojom::TabGroup::New(group_id,
                           tab_groups::TabGroupVisualData(
                               u"Work", tab_groups::TabGroupColorId::kBlue),
                           /*is_ephemeral=*/false));
  service.Emit(
      mojom::TabsEvent::NewCollectionCreatedEvent(std::move(group_created)));

  Path group_path({
      NodeId(NodeId::Type::kWindow, "w1"),
      NodeId(NodeId::Type::kCollection, "s1"),
      NodeId(NodeId::Type::kCollection, "u1"),
      group_id,
  });
  auto moved = mojom::OnNodeMovedEvent::New();
  moved->id = NodeId::FromTabHandle(tabs::TabHandle(20));
  moved->from = Position(3, UnpinnedPath());
  moved->to = Position(0, group_path);
  service.Emit(mojom::TabsEvent::NewNodeMovedEvent(std::move(moved)));

  ASSERT_EQ(view.GetTabs().size(), 3u);
  EXPECT_EQ(view.GetTabs()[0]->ToTabHandle().raw_value(), 20);
  EXPECT_EQ(view.GetTabs()[1]->ToTabHandle().raw_value(), 10);
  EXPECT_EQ(view.GetTabs()[2]->ToTabHandle().raw_value(), 30);
  EXPECT_EQ(view.GetParentId(NodeId::FromTabHandle(tabs::TabHandle(20))),
            group_id);

  // 3. Update tab 10 data (title + activation).
  auto tab_change = mojom::TabChange::New();
  tab_change->data =
      MakeTab(10, u"First Renamed", "https://one.example/", /*is_active=*/true);
  tab_change->mask = mojom::TabFieldMask::New();
  service.Emit(mojom::TabsEvent::NewDataChangedEvent(
      mojom::OnDataChangedEvent::NewTab(std::move(tab_change))));

  ASSERT_NE(view.GetActiveTab(), nullptr);
  EXPECT_EQ(view.GetActiveTab()->ToTabHandle().raw_value(), 10);
  EXPECT_EQ(view.GetActiveTab()->title, u"First Renamed");

  // 4. Close the group collection: removes both the group and nested tab 20.
  auto closed = mojom::OnNodesClosedEvent::New();
  closed->node_ids.push_back(group_id);
  service.Emit(mojom::TabsEvent::NewNodesClosedEvent(std::move(closed)));

  EXPECT_EQ(view.GetNodeData(group_id), nullptr);
  EXPECT_EQ(view.GetTab(tabs::TabHandle(20)), nullptr);
  ASSERT_EQ(view.GetTabs().size(), 2u);
  EXPECT_EQ(view.GetTabs()[0]->ToTabHandle().raw_value(), 10);
  EXPECT_EQ(view.GetTabs()[1]->ToTabHandle().raw_value(), 30);
}

}  // namespace tabs_api
