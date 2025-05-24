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
#include "chrome/browser/ui/tabs/tab_strip_api/testing/toy_tab_strip.h"
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
  explicit FakeTabStripAdapter(testing::ToyTabStrip* tab_strip)
      : tab_strip_(tab_strip) {}
  FakeTabStripAdapter(const FakeTabStripAdapter&) = delete;
  FakeTabStripAdapter operator=(const FakeTabStripAdapter&) = delete;
  ~FakeTabStripAdapter() override = default;
  void AddObserver(TabStripModelObserver*) override {}
  void RemoveObserver(TabStripModelObserver*) override {}

  std::vector<tabs::TabHandle> GetTabs() override {
    return tab_strip_->GetTabs();
  }

  TabRendererData GetTabRendererData(int index) override {
    return TabRendererData();
  }

  void CloseTab(size_t idx) override { tab_strip_->CloseTab(idx); }

  std::optional<int> GetIndexForHandle(tabs::TabHandle tab_handle) override {
    return tab_strip_->GetIndexForHandle(tab_handle);
  }

  void ActivateTab(size_t idx) override {
    const auto tab = tab_strip_->GetTabs().at(idx);
    tab_strip_->ActivateTab(tab);
  }

 private:
  raw_ptr<testing::ToyTabStrip> tab_strip_;
};

class FakeBrowserAdapter : public tabs_api::BrowserAdapter {
 public:
  explicit FakeBrowserAdapter(testing::ToyTabStrip* tab_strip)
      : tab_strip_(tab_strip) {}
  FakeBrowserAdapter(const FakeBrowserAdapter&) = delete;
  FakeBrowserAdapter operator=(const FakeBrowserAdapter&) = delete;
  ~FakeBrowserAdapter() override = default;

  tabs::TabHandle AddTabAt(const GURL& url, std::optional<int> index) override {
    return tab_strip_->AddTabAt(url, index);
  }

 private:
  raw_ptr<testing::ToyTabStrip> tab_strip_;
};

class TabStripServiceImplTest : public ::testing::Test {
 protected:
  TabStripServiceImplTest() = default;
  TabStripServiceImplTest(const TabStripServiceImplTest&) = delete;
  TabStripServiceImplTest operator=(const TabStripServiceImplTest&) = delete;
  ~TabStripServiceImplTest() override = default;

  void SetUp() override {
    tab_strip_ = std::make_unique<testing::ToyTabStrip>();
    impl_ = std::make_unique<TabStripServiceImpl>(
        std::make_unique<FakeBrowserAdapter>(tab_strip_.get()),
        std::make_unique<FakeTabStripAdapter>(tab_strip_.get()));
    impl_->Accept(client_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override { fake_tab_strip_model_ = nullptr; }

  mojo::Remote<tabs_api::mojom::TabStripService> client_;

 protected:
  std::unique_ptr<testing::ToyTabStrip> tab_strip_;
  raw_ptr<FakeTabStripAdapter> fake_tab_strip_model_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TabStripServiceImpl> impl_;
};

TEST_F(TabStripServiceImplTest, CreateNewTab) {
  tabs_api::mojom::TabStripService::CreateTabAtResult result;
  // We should start with nothing.
  ASSERT_EQ(0ul, tab_strip_->GetTabs().size());

  bool success = client_->CreateTabAt(nullptr, std::nullopt, &result);

  ASSERT_TRUE(success);
  ASSERT_TRUE(result.has_value());

  // One tab should have been created. Now we assert its shape.
  ASSERT_EQ(1ul, tab_strip_->GetTabs().size());
  auto created = tab_strip_->GetTabs().at(0);
  ASSERT_EQ(base::NumberToString(created.raw_value()), result.value()->id.Id());
  ASSERT_EQ(TabId::Type::kContent, result.value()->id.Type());
}

TEST_F(TabStripServiceImplTest, GetTabs) {
  tab_strip_->AddTab({GURL("hihi"), tabs::TabHandle(888)});

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
  tab_strip_->AddTab({GURL("hihi"), tabs::TabHandle(666)});

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
  tab_strip_->AddTab({GURL("1"), tabs::TabHandle(123)});
  tab_strip_->AddTab({GURL("2"), tabs::TabHandle(321)});

  tabs_api::mojom::TabStripService::CloseTabsResult result;
  bool success = client_->CloseTabs({tab_id1, tab_id2}, &result);

  ASSERT_TRUE(success);
  ASSERT_TRUE(result.has_value());
  // tab entries should be removed.
  ASSERT_EQ(0ul, tab_strip_->GetTabs().size());
}

TEST_F(TabStripServiceImplTest, CloseTabs_InvalidType) {
  tabs_api::TabId collection_id(TabId::Type::kCollection, "321");

  tabs_api::mojom::TabStripService::CloseTabsResult result;
  bool success = client_->CloseTabs({collection_id}, &result);

  ASSERT_TRUE(success);
  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(result.error()->code, mojo_base::mojom::Code::kUnimplemented);
}

TEST_F(TabStripServiceImplTest, ActivateTab) {
  // We start with this being active.
  auto tab1 = testing::ToyTab{
      GURL("1"),
      tabs::TabHandle(1),
  };

  // And end with this one being active.
  auto tab2 = testing::ToyTab{
      GURL("1"),
      tabs::TabHandle(2),
  };

  tab_strip_->AddTab(tab1);
  tab_strip_->AddTab(tab2);
  tab_strip_->ActivateTab(tab1.tab_handle);
  ASSERT_EQ(tab_strip_->FindActiveTab(), tab1.tab_handle);

  tabs_api::TabId tab2_id(TabId::Type::kContent,
                          base::NumberToString(tab2.tab_handle.raw_value()));

  tabs_api::mojom::TabStripService::ActivateTabResult result;
  bool success = client_->ActivateTab(tab2_id, &result);

  ASSERT_TRUE(success);
  ASSERT_EQ(tab_strip_->FindActiveTab(), tab2.tab_handle);
}

TEST_F(TabStripServiceImplTest, ActivateTab_WrongType) {
  tabs_api::TabId tab2_id(TabId::Type::kCollection, "111");

  tabs_api::mojom::TabStripService::ActivateTabResult result;
  bool success = client_->ActivateTab(tab2_id, &result);

  ASSERT_TRUE(success);
  ASSERT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);
}

TEST_F(TabStripServiceImplTest, ActivateTab_Malformed) {
  tabs_api::TabId tab2_id(TabId::Type::kContent, "aaa");

  tabs_api::mojom::TabStripService::ActivateTabResult result;
  bool success = client_->ActivateTab(tab2_id, &result);

  ASSERT_TRUE(success);
  ASSERT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);
}

TEST_F(TabStripServiceImplTest, ActivateTab_NotFound) {
  tabs_api::TabId tab2_id(TabId::Type::kContent, "111");

  tabs_api::mojom::TabStripService::ActivateTabResult result;
  bool success = client_->ActivateTab(tab2_id, &result);

  ASSERT_TRUE(success);
  ASSERT_EQ(result.error()->code, mojo_base::mojom::Code::kNotFound);
}

}  // namespace
}  // namespace tabs_api
