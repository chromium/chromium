// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_api/testing/toy_tab_strip.h"
#include "chrome/browser/ui/tabs/tab_strip_api/testing/toy_tab_strip_browser_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/testing/toy_tab_strip_model_adapter.h"
#include "components/browser_apis/tab_strip/tab_strip_experiment_api.mojom.h"
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

class TabStripExperimentServiceImplTest : public ::testing::Test {
 protected:
  TabStripExperimentServiceImplTest() = default;
  ~TabStripExperimentServiceImplTest() override = default;

  void SetUp() override {
    tab_strip_ = std::make_unique<testing::ToyTabStrip>();
    service_ = std::make_unique<TabStripServiceImpl>(
        std::make_unique<testing::ToyTabStripBrowserAdapter>(tab_strip_.get()),
        std::make_unique<testing::ToyTabStripModelAdapter>(tab_strip_.get()));
  }

  std::unique_ptr<testing::ToyTabStrip> tab_strip_;
  std::unique_ptr<TabStripServiceImpl> service_;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(TabStripExperimentServiceImplTest, UpdateTabGroupVisual) {
  const tab_groups::TabGroupVisualData initial_visuals(
      u"old title", tab_groups::TabGroupColorId::kGrey);
  const tabs::TabCollectionHandle group_handle =
      tab_strip_->AddGroup(initial_visuals);

  ASSERT_EQ(u"old title",
            tab_strip_->GetGroupVisualData(group_handle)->title());
  ASSERT_EQ(tab_groups::TabGroupColorId::kGrey,
            tab_strip_->GetGroupVisualData(group_handle)->color());

  std::u16string expected = u"new title";
  tabs_api::NodeId group_node_id(
      NodeId::Type::kCollection,
      base::NumberToString(group_handle.raw_value()));
  tab_groups::TabGroupVisualData new_visuals(
      expected, tab_groups::TabGroupColorId::kBlue);

  auto result = service_->UpdateTabGroupVisual(group_node_id, new_visuals);

  ASSERT_TRUE(result.has_value());

  const tab_groups::TabGroupVisualData* updated_visuals =
      tab_strip_->GetGroupVisualData(group_handle);
  ASSERT_EQ(expected, updated_visuals->title());
  ASSERT_EQ(tab_groups::TabGroupColorId::kBlue, updated_visuals->color());
}

TEST_F(TabStripExperimentServiceImplTest,
       UpdateTabGroupVisual_InvalidNodeType) {
  tabs_api::NodeId tab_node_id(NodeId::Type::kContent, "123");
  tab_groups::TabGroupVisualData new_visuals(
      u"title", tab_groups::TabGroupColorId::kBlue);

  auto result = service_->UpdateTabGroupVisual(tab_node_id, new_visuals);

  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);
}

TEST_F(TabStripExperimentServiceImplTest, UpdateTabGroupVisual_NotFound) {
  NodeId group_node_id(NodeId::Type::kCollection, "123");
  tab_groups::TabGroupVisualData new_visuals(
      u"title", tab_groups::TabGroupColorId::kBlue);

  auto result = service_->UpdateTabGroupVisual(group_node_id, new_visuals);

  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(result.error()->code, mojo_base::mojom::Code::kNotFound);
}

}  // namespace
}  // namespace tabs_api
