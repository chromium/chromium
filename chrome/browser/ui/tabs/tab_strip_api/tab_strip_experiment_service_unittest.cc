// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_api/testing/experimental_injector.h"
#include "chrome/browser/ui/tabs/tab_strip_api/testing/injector.h"
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
        std::make_unique<testing::Injector>(*tab_strip_),
        std::make_unique<testing::ExperimentalInjector>());
  }

  std::unique_ptr<testing::ToyTabStrip> tab_strip_;
  std::unique_ptr<TabStripServiceImpl> service_;
};

TEST_F(TabStripExperimentServiceImplTest, ReplaceTabInSplit_InvalidTabs) {
  tabs_api::NodeId split_id(NodeId::Type::kContent, "999");
  tabs_api::NodeId insert_id(NodeId::Type::kContent, "888");

  auto result = service_->ReplaceTabInSplit(split_id, insert_id);
  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);
}

TEST_F(TabStripExperimentServiceImplTest, ShowTabContextMenu) {
  tab_strip_->AddTab({tabs::TabHandle(123), GURL("title")});
  tabs_api::NodeId tab_id(NodeId::Type::kContent, "123");

  auto result = service_->ShowTabContextMenu(tab_id, gfx::Point(100, 100));
  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(result.error()->code, mojo_base::mojom::Code::kUnimplemented);
}

TEST_F(TabStripExperimentServiceImplTest, GetAllTabsForProfile_Empty) {
  auto result = service_->GetAllTabsForProfile();
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(1u, result.value().size());
  auto it = result.value().find("1");
  ASSERT_NE(it, result.value().end());
  ASSERT_EQ(1u, it->second->children.size());
  ASSERT_EQ(0u, it->second->children[0]->children.size());
}

}  // namespace
}  // namespace tabs_api
