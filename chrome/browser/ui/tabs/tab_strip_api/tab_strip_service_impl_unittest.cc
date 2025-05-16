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
  std::vector<tabs::TabHandle> GetTabs() override { return tabs; }
  TabRendererData GetTabRendererData(int index) override {
    return TabRendererData();
  }
  void CloseTab(size_t idx) override {
    CHECK_LT(idx, tabs.size())
        << "invalid idx passed in: " << idx << ", tab size is: " << tabs.size();
    tabs.erase(tabs.begin() + idx);
  }
  std::optional<int> GetIndexForHandle(tabs::TabHandle tab_handle) override {
    for (size_t i = 0; i < tabs.size(); ++i) {
      if (tabs.at(i) == tab_handle) {
        return i;
      }
    }
    return std::nullopt;
  }

  std::vector<tabs::TabHandle> tabs;
};

class FakeBrowserAdapter : public tabs_api::BrowserAdapter {
 public:
  FakeBrowserAdapter() = default;
  FakeBrowserAdapter(const FakeBrowserAdapter&) = delete;
  FakeBrowserAdapter operator=(const FakeBrowserAdapter&) = delete;
  ~FakeBrowserAdapter() override = default;

  tabs::TabHandle AddTabAt(const GURL& url, int index) override {
    // Hard-coded value for now to test success.
    return tabs::TabHandle(1);
  }
};

class TabStripServiceImplTest : public testing::Test {
 protected:
  TabStripServiceImplTest() = default;
  TabStripServiceImplTest(const TabStripServiceImplTest&) = delete;
  TabStripServiceImplTest operator=(const TabStripServiceImplTest&) = delete;
  ~TabStripServiceImplTest() override = default;

  void SetUp() override {
    auto fake_tab_strip_model = std::make_unique<FakeTabStripAdapter>();
    fake_tab_strip_model_ = fake_tab_strip_model.get();
    impl_ = std::make_unique<TabStripServiceImpl>(
        std::make_unique<FakeBrowserAdapter>(),
        std::move(fake_tab_strip_model));
    impl_->Accept(client_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override { fake_tab_strip_model_ = nullptr; }

  mojo::Remote<tabs_api::mojom::TabStripService> client_;

 protected:
  raw_ptr<FakeTabStripAdapter> fake_tab_strip_model_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TabStripServiceImpl> impl_;
};

TEST_F(TabStripServiceImplTest, CreateNewTab) {
  tabs_api::mojom::TabStripService::CreateTabAtResult result;
  bool success = client_->CreateTabAt(nullptr, std::nullopt, &result);

  ASSERT_TRUE(success);
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value());
}

TEST_F(TabStripServiceImplTest, GetTabs) {
  fake_tab_strip_model_->tabs.push_back(tabs::TabHandle(888));

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
  fake_tab_strip_model_->tabs.push_back(tabs::TabHandle(666));

  tabs_api::mojom::TabStripService::GetTabResult result;
  tabs_api::TabId tab_id(TabId::Type::kContent, "666");
  bool success = client_->GetTab(tab_id, &result);

  ASSERT_TRUE(success);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result.value()->id.Id(), "666");
  ASSERT_EQ(result.value()->id.Type(), TabId::Type::kContent);
}

TEST_F(TabStripServiceImplTest, GetTab_NotFound) {
  tabs_api::mojom::TabStripService::GetTabResult result;
  tabs_api::TabId tab_id(TabId::Type::kContent, "666");
  bool success = client_->GetTab(tab_id, &result);

  ASSERT_TRUE(success);
  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(result.error()->code, mojo_base::mojom::Code::kNotFound);
}

TEST_F(TabStripServiceImplTest, GetTab_MalformedId) {
  tabs_api::mojom::TabStripService::GetTabResult result;
  tabs_api::TabId tab_id(TabId::Type::kContent, /* I know my */ "abc");
  bool success = client_->GetTab(tab_id, &result);

  ASSERT_TRUE(success);
  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);
}

TEST_F(TabStripServiceImplTest, GetTab_InvalidType) {
  tabs_api::mojom::TabStripService::GetTabResult result;
  tabs_api::TabId tab_id;
  bool success = client_->GetTab(tab_id, &result);

  ASSERT_TRUE(success);
  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);
}

TEST_F(TabStripServiceImplTest, CloseTabs) {
  tabs_api::TabId tab_id1(TabId::Type::kContent, "123");
  tabs_api::TabId tab_id2(TabId::Type::kContent, "321");

  // insert fake tab entries.
  fake_tab_strip_model_->tabs.push_back(tabs::TabHandle(123));
  fake_tab_strip_model_->tabs.push_back(tabs::TabHandle(321));

  tabs_api::mojom::TabStripService::CloseTabsResult result;
  bool success = client_->CloseTabs({tab_id1, tab_id2}, &result);

  ASSERT_TRUE(success);
  ASSERT_TRUE(result.has_value());
  // tab entries should be removed.
  ASSERT_EQ(0ul, fake_tab_strip_model_->tabs.size());
}

TEST_F(TabStripServiceImplTest, CloseTabs_InvalidType) {
  tabs_api::TabId collection_id(TabId::Type::kCollection, "321");

  tabs_api::mojom::TabStripService::CloseTabsResult result;
  bool success = client_->CloseTabs({collection_id}, &result);

  ASSERT_TRUE(success);
  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(result.error()->code, mojo_base::mojom::Code::kUnimplemented);
}

}  // namespace
}  // namespace tabs_api
