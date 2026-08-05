// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/controllers/tab_strip_ui_controller_impl.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_api/controllers/tab_strip_ui_controller_injector_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/browser_apis/tab_strip/adapters/context_menu_adapter.h"
#include "components/browser_apis/tab_strip/tab_strip_ui_controller.mojom.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "url/gurl.h"

namespace tabs_api {

class ToyContextMenuAdapter : public ContextMenuAdapter {
 public:
  ToyContextMenuAdapter() = default;
  ~ToyContextMenuAdapter() override = default;

  base::expected<void, mojo_base::mojom::ErrorPtr> ShowTabContextMenu(
      tabs::TabHandle handle,
      const gfx::Point& location) override {
    called_ = true;
    last_handle_ = handle;
    last_location_ = location;
    return base::ok();
  }

  bool called() const { return called_; }
  tabs::TabHandle last_handle() const { return last_handle_; }
  gfx::Point last_location() const { return last_location_; }

 private:
  bool called_ = false;
  tabs::TabHandle last_handle_;
  gfx::Point last_location_;
};

class TestTabStripUIControllerInjector : public TabStripUIControllerInjector {
 public:
  TestTabStripUIControllerInjector(
      std::unique_ptr<TabStripUIControllerInjectorImpl> impl,
      ContextMenuAdapter* toy_adapter)
      : impl_(std::move(impl)), toy_adapter_(toy_adapter) {}

  BrowserAdapter& browser_adapter() override {
    return impl_->browser_adapter();
  }
  TabStripModelAdapter& tab_strip_model_adapter() override {
    return impl_->tab_strip_model_adapter();
  }
  ContextMenuAdapter& context_menu_adapter() override { return *toy_adapter_; }

 private:
  std::unique_ptr<TabStripUIControllerInjectorImpl> impl_;
  raw_ptr<ContextMenuAdapter> toy_adapter_;
};

class TabStripUIControllerImplBrowserTest : public InProcessBrowserTest {
 public:
  TabStripUIControllerImplBrowserTest() = default;
  ~TabStripUIControllerImplBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    toy_context_menu_adapter_ = std::make_unique<ToyContextMenuAdapter>();
    ui_controller_ = std::make_unique<TabStripUIControllerImpl>(
        std::make_unique<TestTabStripUIControllerInjector>(
            std::make_unique<TabStripUIControllerInjectorImpl>(
                browser(), browser()->tab_strip_model()),
            toy_context_menu_adapter_.get()));
  }

  void TearDownOnMainThread() override {
    ui_controller_.reset();
    toy_context_menu_adapter_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  std::unique_ptr<TabStripUIControllerImpl> ui_controller_;
  std::unique_ptr<ToyContextMenuAdapter> toy_context_menu_adapter_;
};

IN_PROC_BROWSER_TEST_F(TabStripUIControllerImplBrowserTest,
                       ShowTabContextMenu) {
  mojo::Remote<mojom::TabStripUIController> ui_remote;
  ui_controller_->Bind(ui_remote.BindNewPipeAndPassReceiver());

  // Create a tab to show the context menu for.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_EQ(tab_strip_model->count(), 1);
  tabs::TabInterface* tab = tab_strip_model->GetTabAtIndex(0);
  tabs::TabHandle tab_handle = tab->GetHandle();
  NodeId tab_id = NodeId::FromTabHandle(tab_handle);

  base::RunLoop run_loop;
  ui_remote->ShowTabContextMenu(
      tab_id, gfx::Point(100, 100),
      base::BindLambdaForTesting(
          [&](mojom::TabStripUIController::ShowTabContextMenuResult result) {
            EXPECT_TRUE(result.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(toy_context_menu_adapter_->called());
  EXPECT_EQ(toy_context_menu_adapter_->last_location(), gfx::Point(100, 100));
  EXPECT_EQ(toy_context_menu_adapter_->last_handle(), tab_handle);
}

}  // namespace tabs_api
