// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/controllers/tab_strip_ui_controller_impl.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/tabs/tab_strip_api/testing/experimental_injector.h"
#include "chrome/browser/ui/tabs/tab_strip_api/testing/injector.h"
#include "chrome/browser/ui/tabs/tab_strip_api/testing/toy_tab_strip.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"

namespace tabs_api {
namespace {

class TestTabStripUIControllerInjector : public TabStripUIControllerInjector {
 public:
  explicit TestTabStripUIControllerInjector(testing::ToyTabStrip& toy_tab_strip)
      : injector_(toy_tab_strip) {}

  BrowserAdapter& browser_adapter() override {
    return injector_.browser_adapter();
  }
  TabStripModelAdapter& tab_strip_model_adapter() override {
    return injector_.tab_strip_model_adapter();
  }
  ContextMenuAdapter& context_menu_adapter() override {
    return experimental_injector_.context_menu_adapter();
  }

 private:
  testing::Injector injector_;
  testing::ExperimentalInjector experimental_injector_;
};

class TabStripUIControllerImplTest : public ::testing::Test {
 protected:
  TabStripUIControllerImplTest() = default;
  ~TabStripUIControllerImplTest() override = default;

  void SetUp() override {
    tab_strip_ = std::make_unique<testing::ToyTabStrip>();
    controller_ = std::make_unique<TabStripUIControllerImpl>(
        std::make_unique<TestTabStripUIControllerInjector>(*tab_strip_));
  }

  std::unique_ptr<testing::ToyTabStrip> tab_strip_;
  std::unique_ptr<TabStripUIControllerImpl> controller_;
};

TEST_F(TabStripUIControllerImplTest, ShowTabContextMenu_InvalidId) {
  // Use a window ID, which is not convertible to a tab handle.
  tabs_api::NodeId tab_id = NodeId::FromWindowId("window_1");

  auto result = controller_->ShowTabContextMenu(tab_id, gfx::Point(100, 100));
  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);
}

TEST_F(TabStripUIControllerImplTest, ShowTabContextMenu_ValidId) {
  tab_strip_->AddTab({tabs::TabHandle(123), GURL("http://example.com")});
  tabs_api::NodeId tab_id(NodeId::Type::kContent, "123");

  auto result = controller_->ShowTabContextMenu(tab_id, gfx::Point(100, 100));
  ASSERT_FALSE(result.has_value());
  // ToyTabContextMenuAdapter returns kUnimplemented.
  ASSERT_EQ(result.error()->code, mojo_base::mojom::Code::kUnimplemented);
}

}  // namespace
}  // namespace tabs_api
