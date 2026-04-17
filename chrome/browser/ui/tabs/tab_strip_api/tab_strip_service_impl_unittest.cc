// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_impl.h"

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/browser_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/experimental_platform_adapters_provider.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tab_strip_model_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/testing/injector.h"
#include "chrome/browser/ui/tabs/tab_strip_api/testing/toy_tab_strip.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tabs_api {
namespace {

// Really a hermatic integration test.
class TabStripServiceImplTest : public ::testing::Test {
 protected:
  TabStripServiceImplTest() = default;
  ~TabStripServiceImplTest() override = default;

  void SetUp() override {
    tab_strip_ = std::make_unique<testing::ToyTabStrip>();
    service_ = std::make_unique<TabStripServiceImpl>(
        std::make_unique<testing::Injector>(*tab_strip_), nullptr);
  }

 protected:
  std::unique_ptr<testing::ToyTabStrip> tab_strip_;
  std::unique_ptr<TabStripServiceImpl> service_;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(TabStripServiceImplTest, CreateNewTab) {
  // We should start with nothing.
  ASSERT_EQ(0ul, tab_strip_->GetTabs().size());

  auto result = service_->CreateTabAt(std::nullopt, std::nullopt);

  ASSERT_TRUE(result.has_value());

  // One tab should have been created. Now we assert its shape.
  ASSERT_EQ(1ul, tab_strip_->GetTabs().size());
  auto created = tab_strip_->GetTabs().at(0);
  ASSERT_EQ(base::NumberToString(created.raw_value()), result.value()->id.Id());
  ASSERT_EQ(NodeId::Type::kContent, result.value()->id.Type());
}

TEST_F(TabStripServiceImplTest, GetTabs) {
  tab_strip_->AddTab({tabs::TabHandle(888), GURL("hihi")});

  auto result = service_->GetTabsWithoutObservation();

  const auto& window = result.value();
  ASSERT_TRUE(window->data->is_window());
  ASSERT_EQ(1u, window->children.size());
  const auto& tab_strip = window->children[0];
  ASSERT_TRUE(tab_strip->data->is_tab_strip());
  ASSERT_EQ(1u, tab_strip->children.size());
  ASSERT_TRUE(tab_strip->children[0]->data->is_tab());
  ASSERT_EQ("888", tab_strip->children[0]->data->get_tab()->id.Id());
  ASSERT_EQ(NodeId::Type::kContent,
            tab_strip->children[0]->data->get_tab()->id.Type());
}

TEST_F(TabStripServiceImplTest, GetTab) {
  tab_strip_->AddTab({tabs::TabHandle(666), GURL("hihi")});

  tabs_api::NodeId tab_id(NodeId::Type::kContent, "666");
  auto result = service_->GetTab(tab_id);

  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result.value()->id.Id(), "666");
  ASSERT_EQ(result.value()->id.Type(), NodeId::Type::kContent);
}

TEST_F(TabStripServiceImplTest, GetTab_NotFound) {
  tabs_api::NodeId tab_id(NodeId::Type::kContent, "666");

  auto result = service_->GetTab(tab_id);

  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(result.error()->code, mojo_base::mojom::Code::kNotFound);
}

TEST_F(TabStripServiceImplTest, CloseNodes) {
  tabs_api::NodeId tab_id1(NodeId::Type::kContent, "123");
  tabs_api::NodeId tab_id2(NodeId::Type::kContent, "321");

  // insert fake tab entries.
  tab_strip_->AddTab({tabs::TabHandle(123), GURL("1")});
  tab_strip_->AddTab({tabs::TabHandle(321), GURL("2")});

  auto result = service_->CloseNodes({tab_id1, tab_id2});

  ASSERT_TRUE(result.has_value());
  // tab entries should be removed.
  ASSERT_EQ(0ul, tab_strip_->GetTabs().size());
}

TEST_F(TabStripServiceImplTest, ActivateTab) {
  // We start with this being active.
  auto tab1 = testing::ToyTab{
      tabs::TabHandle(1),
      GURL("1"),
  };

  // And end with this one being active.
  auto tab2 = testing::ToyTab{
      tabs::TabHandle(2),
      GURL("1"),
  };

  tab_strip_->AddTab(tab1);
  tab_strip_->AddTab(tab2);
  tab_strip_->ActivateTab(tab1.tab_handle);
  ASSERT_EQ(tab_strip_->FindActiveTab(), tab1.tab_handle);

  tabs_api::NodeId tab2_id(NodeId::Type::kContent,
                           base::NumberToString(tab2.tab_handle.raw_value()));

  auto result = service_->ActivateTab(tab2_id);

  ASSERT_EQ(tab_strip_->FindActiveTab(), tab2.tab_handle);
}

TEST_F(TabStripServiceImplTest, ActivateTab_NotFound) {
  tabs_api::NodeId tab2_id(NodeId::Type::kContent, "111");

  auto result = service_->ActivateTab(tab2_id);

  ASSERT_EQ(result.error()->code, mojo_base::mojom::Code::kNotFound);
}

TEST_F(TabStripServiceImplTest, SetSelectedTabs) {
  // We start with this being active (and therefore selected).
  auto tab1 = testing::ToyTab{
      tabs::TabHandle(1),
      GURL("1"),
  };

  // And end with this one being active.
  auto tab2 = testing::ToyTab{
      tabs::TabHandle(2),
      GURL("1"),
  };

  tab_strip_->AddTab(tab1);
  tab_strip_->AddTab(tab2);
  tab_strip_->ActivateTab(tab1.tab_handle);

  ASSERT_FALSE(tab_strip_->GetToyTabFor(tab2.tab_handle)->active);
  ASSERT_FALSE(tab_strip_->GetToyTabFor(tab2.tab_handle)->selected);

  ASSERT_TRUE(tab_strip_->GetToyTabFor(tab1.tab_handle)->active);
  ASSERT_TRUE(tab_strip_->GetToyTabFor(tab1.tab_handle)->selected);

  tabs_api::NodeId tab2_id(NodeId::Type::kContent,
                           base::NumberToString(tab2.tab_handle.raw_value()));

  auto result = service_->SetSelectedTabs({tab2_id}, tab2_id);

  ASSERT_TRUE(tab_strip_->GetToyTabFor(tab2.tab_handle)->active);
  ASSERT_TRUE(tab_strip_->GetToyTabFor(tab2.tab_handle)->selected);

  ASSERT_FALSE(tab_strip_->GetToyTabFor(tab1.tab_handle)->active);
  ASSERT_FALSE(tab_strip_->GetToyTabFor(tab1.tab_handle)->selected);
}

TEST_F(TabStripServiceImplTest, SetSelectedTabs_MultipleSelection) {
  auto tab1 = testing::ToyTab{
      tabs::TabHandle(1),
      GURL("1"),
  };

  auto tab2 = testing::ToyTab{
      tabs::TabHandle(2),
      GURL("1"),
  };

  auto tab3 = testing::ToyTab{
      tabs::TabHandle(3),
      GURL("1"),
  };

  auto tab4 = testing::ToyTab{
      tabs::TabHandle(4),
      GURL("1"),
  };

  tab_strip_->AddTab(tab1);
  tab_strip_->AddTab(tab2);
  tab_strip_->AddTab(tab3);
  tab_strip_->AddTab(tab4);

  tabs_api::NodeId tab1_id(NodeId::Type::kContent,
                           base::NumberToString(tab1.tab_handle.raw_value()));
  tabs_api::NodeId tab2_id(NodeId::Type::kContent,
                           base::NumberToString(tab2.tab_handle.raw_value()));
  tabs_api::NodeId tab3_id(NodeId::Type::kContent,
                           base::NumberToString(tab3.tab_handle.raw_value()));
  tabs_api::NodeId tab4_id(NodeId::Type::kContent,
                           base::NumberToString(tab4.tab_handle.raw_value()));

  auto result =
      service_->SetSelectedTabs({tab1_id, tab2_id, tab3_id, tab4_id}, tab4_id);

  ASSERT_FALSE(tab_strip_->GetToyTabFor(tab1.tab_handle)->active);
  ASSERT_TRUE(tab_strip_->GetToyTabFor(tab1.tab_handle)->selected);

  ASSERT_FALSE(tab_strip_->GetToyTabFor(tab2.tab_handle)->active);
  ASSERT_TRUE(tab_strip_->GetToyTabFor(tab2.tab_handle)->selected);

  ASSERT_FALSE(tab_strip_->GetToyTabFor(tab3.tab_handle)->active);
  ASSERT_TRUE(tab_strip_->GetToyTabFor(tab3.tab_handle)->selected);

  ASSERT_TRUE(tab_strip_->GetToyTabFor(tab4.tab_handle)->active);
  ASSERT_TRUE(tab_strip_->GetToyTabFor(tab4.tab_handle)->selected);
}

TEST_F(TabStripServiceImplTest, MoveTab) {
  // Move the first tab to the last spot.
  tab_strip_->AddTab(testing::ToyTab{
      tabs::TabHandle(1),
      GURL("1"),
  });
  tab_strip_->AddTab(testing::ToyTab{
      tabs::TabHandle(2),
      GURL("2"),
  });
  tab_strip_->AddTab(testing::ToyTab{
      tabs::TabHandle(3),
      GURL("3"),
  });

  tabs_api::NodeId tab_id(NodeId::Type::kContent, "1");

  auto position = tabs_api::Position(2);

  auto target_handle = tabs::TabHandle(1);
  // Check that the target is at the beginning before the move.
  ASSERT_EQ(0, tab_strip_->GetIndexForHandle(target_handle).value());

  auto result = service_->MoveNode(tab_id, std::move(position));

  ASSERT_TRUE(result.has_value());

  // Check that the target is now at the end.
  ASSERT_EQ(2, tab_strip_->GetIndexForHandle(target_handle).value());
}

// TODO(crbug.com/422263248): figure out a better way to test for common
// validations. No point covering each of them in the test (or maybe just
// a common framework to ensure that it is being checked?).

TEST_F(TabStripServiceImplTest, MoveTab_OutOfRange) {
  tab_strip_->AddTab(testing::ToyTab{
      tabs::TabHandle(1),
      GURL("1"),
  });

  tabs_api::NodeId tab_id(NodeId::Type::kContent, "1");

  auto position = tabs_api::Position(9001);

  auto result = service_->MoveNode(tab_id, std::move(position));

  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);
}

TEST_F(TabStripServiceImplTest, CloseNodes_TabGroup) {
  auto group_id = tab_groups::TabGroupId::GenerateNew();
  auto group_handle = tab_strip_->AddGroup(group_id, {});
  NodeId group_node = NodeId::FromTabCollectionHandle(group_handle);

  tab_strip_->AddTab({tabs::TabHandle(1), GURL("1"), false, false, group_id});
  tab_strip_->AddTab({tabs::TabHandle(2), GURL("2"), false, false, group_id});
  tab_strip_->AddTab({tabs::TabHandle(3), GURL("3")});

  ASSERT_EQ(3ul, tab_strip_->GetTabs().size());

  auto result = service_->CloseNodes({group_node});

  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(1ul, tab_strip_->GetTabs().size());
  ASSERT_EQ(3, tab_strip_->GetTabs()[0].raw_value());
}

TEST_F(TabStripServiceImplTest, CloseNodes_Mixed) {
  auto group_id = tab_groups::TabGroupId::GenerateNew();
  auto group_handle = tab_strip_->AddGroup(group_id, {});
  NodeId group_node = NodeId::FromTabCollectionHandle(group_handle);

  tab_strip_->AddTab({tabs::TabHandle(1), GURL("1"), false, false, group_id});
  tab_strip_->AddTab({tabs::TabHandle(2), GURL("2"), false, false, group_id});
  tab_strip_->AddTab({tabs::TabHandle(3), GURL("3")});
  tab_strip_->AddTab({tabs::TabHandle(4), GURL("4")});

  ASSERT_EQ(4ul, tab_strip_->GetTabs().size());

  NodeId tab1_node(NodeId::Type::kContent, "1");
  NodeId tab3_node(NodeId::Type::kContent, "3");

  // Close group (containing tab 1 and 2) AND tab 1 (redundant) AND tab 3.
  auto result = service_->CloseNodes({group_node, tab1_node, tab3_node});

  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(1ul, tab_strip_->GetTabs().size());
  ASSERT_EQ(4, tab_strip_->GetTabs()[0].raw_value());
}

TEST_F(TabStripServiceImplTest, UpdateTabGroup) {
  tab_groups::TabGroupVisualData initial_visuals(
      u"group", tab_groups::TabGroupColorId::kGrey);
  auto group_handle = tab_strip_->AddGroup(initial_visuals);
  NodeId group_node = NodeId::FromTabCollectionHandle(group_handle);

  mojom::TabGroupPtr tab_group_mojom = mojom::TabGroup::New();
  tab_group_mojom->id = group_node;
  tab_group_mojom->data = tab_groups::TabGroupVisualData(
      u"super duper group", tab_groups::TabGroupColorId::kBlue);

  auto data = mojom::Data::NewTabGroup(std::move(tab_group_mojom));

  auto result = service_->Update(std::move(data), std::nullopt);
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value()->is_tab_group());
  ASSERT_EQ(result.value()->get_tab_group()->data.title(),
            u"super duper group");
  ASSERT_EQ(result.value()->get_tab_group()->data.color(),
            tab_groups::TabGroupColorId::kBlue);

  const auto* updated_visuals = tab_strip_->GetGroupVisualData(group_handle);
  ASSERT_NE(nullptr, updated_visuals);
  ASSERT_EQ(u"super duper group", updated_visuals->title());
  ASSERT_EQ(tab_groups::TabGroupColorId::kBlue, updated_visuals->color());
}

TEST_F(TabStripServiceImplTest, Update_Unimplemented) {
  tabs_api::NodeId tab_id(NodeId::Type::kContent, "123");
  mojom::TabPtr tab_mojom = mojom::Tab::New();
  tab_mojom->id = tab_id;

  auto data = mojom::Data::NewTab(std::move(tab_mojom));
  auto result = service_->Update(std::move(data), std::nullopt);

  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(result.error()->code, mojo_base::mojom::Code::kUnimplemented);
}

TEST_F(TabStripServiceImplTest, UpdateTabGroupPartial) {
  tab_groups::TabGroupVisualData initial_visuals(
      u"group", tab_groups::TabGroupColorId::kGrey, false);
  auto group_handle = tab_strip_->AddGroup(initial_visuals);
  NodeId group_node = NodeId::FromTabCollectionHandle(group_handle);

  mojom::TabGroupPtr tab_group_mojom = mojom::TabGroup::New();
  tab_group_mojom->id = group_node;
  // We want to update title but NOT color or collapsed state.
  tab_group_mojom->data = tab_groups::TabGroupVisualData(
      u"super duper group", tab_groups::TabGroupColorId::kBlue, true);

  auto data = mojom::Data::NewTabGroup(std::move(tab_group_mojom));
  std::vector<std::string> mask = {"title"};

  auto result = service_->Update(std::move(data), mask);
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value()->is_tab_group());
  ASSERT_EQ(result.value()->get_tab_group()->data.title(),
            u"super duper group");
  // Color should still be grey.
  ASSERT_EQ(result.value()->get_tab_group()->data.color(),
            tab_groups::TabGroupColorId::kGrey);
  // Is collapsed should still be false.
  ASSERT_FALSE(result.value()->get_tab_group()->data.is_collapsed());

  const auto* updated_visuals = tab_strip_->GetGroupVisualData(group_handle);
  ASSERT_NE(nullptr, updated_visuals);
  ASSERT_EQ(u"super duper group", updated_visuals->title());
  ASSERT_EQ(tab_groups::TabGroupColorId::kGrey, updated_visuals->color());
  ASSERT_FALSE(updated_visuals->is_collapsed());
}

}  // namespace
}  // namespace tabs_api
