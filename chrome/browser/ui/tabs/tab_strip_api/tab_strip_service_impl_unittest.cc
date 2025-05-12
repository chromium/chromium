// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_impl.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/browser_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tab_strip_model_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_id.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/test/base/testing_profile.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tabs_api {
namespace {

class FakeTabStripAdapter : public tabs_api::TabStripModelAdapter {
 public:
  FakeTabStripAdapter() = default;
  FakeTabStripAdapter(const FakeTabStripAdapter&) = delete;
  FakeTabStripAdapter operator=(const FakeTabStripAdapter&) = delete;
  ~FakeTabStripAdapter() override = default;
  void AddObserver(TabStripModelObserver*) override {}
  void RemoveObserver(TabStripModelObserver*) override {}
  std::vector<tabs::TabHandle> GetTabs() override {
    return {tabs::TabHandle(888)};
  }
  TabRendererData GetTabRendererData(int index) override {
    return TabRendererData();
  }
};

class FakeBrowserAdapter : public tabs_api::BrowserAdapter {
 public:
  FakeBrowserAdapter() = default;
  FakeBrowserAdapter(const FakeBrowserAdapter&) = delete;
  FakeBrowserAdapter operator=(const FakeBrowserAdapter&) = delete;
  ~FakeBrowserAdapter() override = default;

  content::WebContents* AddTabAt(const GURL& url, int index) override {
    return nullptr;
  }
};

class TabStripServiceImplTest : public testing::Test {
 protected:
  TabStripServiceImplTest() = default;
  TabStripServiceImplTest(const TabStripServiceImplTest&) = delete;
  TabStripServiceImplTest operator=(const TabStripServiceImplTest&) = delete;
  ~TabStripServiceImplTest() override = default;

  void SetUp() override {
    impl_ = std::make_unique<TabStripServiceImpl>(
        std::make_unique<FakeBrowserAdapter>(),
        std::make_unique<FakeTabStripAdapter>());
    impl_->Accept(client_.BindNewPipeAndPassReceiver());
  }

  mojo::Remote<tabs_api::mojom::TabStripService> client_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TabStripServiceImpl> impl_;
};

TEST_F(TabStripServiceImplTest, CreateNewTab) {
  tabs_api::mojom::TabStripService::CreateTabAtResult result;
  bool success = client_->CreateTabAt(nullptr, std::nullopt, &result);

  ASSERT_TRUE(success);
  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(result.error()->code, mojo_base::mojom::Code::kFailedPrecondition);
}

TEST_F(TabStripServiceImplTest, GetTabs) {
  tabs_api::mojom::TabStripService::GetTabsResult result;
  bool success = client_->GetTabs(&result);

  ASSERT_TRUE(success);
  ASSERT_EQ(1u, result.value()->tabs.size());
  ASSERT_EQ("888", result.value()->tabs[0]->id.Id());
  ASSERT_EQ(TabId::Type::kContent, result.value()->tabs[0]->id.Type());
  // TODO(crbug.com/412709270): we can probably easily test the observation
  // in unit test as well. But it is already covered by the browser
  // test, so skipping for now.
}

TEST_F(TabStripServiceImplTest, GetTab) {
  tabs_api::mojom::TabStripService::GetTabResult result;
  tabs_api::TabId tab_id;
  bool success = client_->GetTab(tab_id, &result);

  ASSERT_TRUE(success);
  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);
}

}  // namespace
}  // namespace tabs_api
