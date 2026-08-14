// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/tab_list/mock_tab_list_interface.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/testing_profile.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace {

using ::testing::NiceMock;
using ::testing::Return;

// Test double for TabInterface supporting WeakPtrFactory and
// BrowserWindowInterface.
class FakeTab : public tabs::MockTabInterface {
 public:
  explicit FakeTab(BrowserWindowInterface* window = nullptr) : window_(window) {
    ON_CALL(*this, GetBrowserWindowInterface()).WillByDefault(Return(window_));
    ON_CALL(*this, GetWeakPtr())
        .WillByDefault(testing::Invoke(this, &FakeTab::GetWeakPtrImpl));
  }
  FakeTab(const FakeTab&) = delete;
  FakeTab& operator=(const FakeTab&) = delete;
  ~FakeTab() override = default;

  base::WeakPtr<tabs::TabInterface> GetWeakPtrImpl() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  raw_ptr<BrowserWindowInterface> window_ = nullptr;
  base::WeakPtrFactory<FakeTab> weak_factory_{this};
};

// Container managing a mock window, its registered TabListInterface, and owned
// tabs.
class FakeBrowserWindow {
 public:
  explicit FakeBrowserWindow(Profile* profile) {
    ON_CALL(window_, GetProfile()).WillByDefault(Return(profile));
    ON_CALL(tab_list_, GetAllTabs()).WillByDefault([this]() {
      std::vector<tabs::TabInterface*> result;
      result.reserve(tabs_.size());
      for (const auto& tab : tabs_) {
        result.push_back(tab.get());
      }
      return result;
    });
  }
  FakeBrowserWindow(const FakeBrowserWindow&) = delete;
  FakeBrowserWindow& operator=(const FakeBrowserWindow&) = delete;
  ~FakeBrowserWindow() = default;

  MockBrowserWindowInterface* window() { return &window_; }

  FakeTab* AddTab() {
    auto tab = std::make_unique<FakeTab>(&window_);
    FakeTab* tab_ptr = tab.get();
    tabs_.push_back(std::move(tab));
    return tab_ptr;
  }

  void RemoveTab(size_t index) {
    if (index < tabs_.size()) {
      tabs_.erase(tabs_.begin() + index);
    }
  }

  void CloseAllTabs() { tabs_.clear(); }

  size_t tab_count() const { return tabs_.size(); }

 private:
  NiceMock<MockBrowserWindowInterface> window_;
  NiceMock<MockTabListInterface> tab_list_;
  ui::ScopedUnownedUserData<TabListInterface> tab_list_registration_{
      window_.GetUnownedUserDataHost(), tab_list_};
  std::vector<std::unique_ptr<FakeTab>> tabs_;
};

// Helper function to iterate and count all tabs using
// tabs::ForEachTabInterface.
size_t CountAllTabs() {
  size_t count = 0;
  tabs::ForEachTabInterface([&count](tabs::TabInterface* tab) {
    ++count;
    return true;
  });
  return count;
}

class TabContentsIteratorTest : public testing::Test {
 public:
  TabContentsIteratorTest() = default;
  ~TabContentsIteratorTest() override = default;

  void SetUp() override { window1_ = CreateAndRegisterWindow(); }

  void TearDown() override {
    for (BrowserWindowInterface* window : active_windows_) {
      static_cast<BrowserCollectionObserver*>(
          GlobalBrowserCollection::GetInstance()->GetPlatformDelegate())
          ->OnBrowserClosed(window);
    }
    active_windows_.clear();
    window1_ = nullptr;
    windows_.clear();
  }

  FakeBrowserWindow* window1() { return window1_; }
  Profile* profile() { return &profile_; }

  FakeBrowserWindow* CreateAndRegisterWindow() {
    auto window = std::make_unique<FakeBrowserWindow>(&profile_);
    FakeBrowserWindow* ptr = window.get();
    RegisterWindow(ptr->window());
    windows_.push_back(std::move(window));
    return ptr;
  }

  void RegisterWindow(BrowserWindowInterface* window) {
    static_cast<BrowserCollectionObserver*>(
        GlobalBrowserCollection::GetInstance()->GetPlatformDelegate())
        ->OnBrowserCreated(window);
    active_windows_.push_back(window);
  }

  void UnregisterWindow(BrowserWindowInterface* window) {
    auto it = std::ranges::find(active_windows_, window);
    if (it != active_windows_.end()) {
      static_cast<BrowserCollectionObserver*>(
          GlobalBrowserCollection::GetInstance()->GetPlatformDelegate())
          ->OnBrowserClosed(window);
      active_windows_.erase(it);
    }
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  raw_ptr<FakeBrowserWindow> window1_ = nullptr;
  std::vector<std::unique_ptr<FakeBrowserWindow>> windows_;
  std::vector<raw_ptr<BrowserWindowInterface>> active_windows_;
};

TEST_F(TabContentsIteratorTest, TabContentsIteratorVerifyCount) {
  // Make sure we have 1 window to start with.
  EXPECT_EQ(1U, GlobalBrowserCollection::GetInstance()->GetSize());
  EXPECT_EQ(0U, CountAllTabs());

  // Create more windows.
  FakeBrowserWindow* window2 = CreateAndRegisterWindow();
  FakeBrowserWindow* window3 = CreateAndRegisterWindow();
  FakeBrowserWindow* window4 = CreateAndRegisterWindow();

  // Sanity checks.
  EXPECT_EQ(4U, GlobalBrowserCollection::GetInstance()->GetSize());
  EXPECT_EQ(0U, window1()->tab_count());
  EXPECT_EQ(0U, window2->tab_count());
  EXPECT_EQ(0U, window3->tab_count());
  EXPECT_EQ(0U, window4->tab_count());
  EXPECT_EQ(0U, CountAllTabs());

  // Add some tabs.
  for (size_t i = 0; i < 3; ++i) {
    window2->AddTab();
  }
  window3->AddTab();

  EXPECT_EQ(4U, CountAllTabs());

  // Close some tabs.
  window2->CloseAllTabs();

  EXPECT_EQ(1U, CountAllTabs());

  // Add lots of tabs.
  for (size_t i = 0; i < 41; ++i) {
    window1()->AddTab();
  }

  EXPECT_EQ(42U, CountAllTabs());
}

TEST_F(TabContentsIteratorTest, TabContentsIteratorVerifyBrowser) {
  // Make sure we have 1 window to start with.
  EXPECT_EQ(1U, GlobalBrowserCollection::GetInstance()->GetSize());

  // Create more windows.
  FakeBrowserWindow* window2 = CreateAndRegisterWindow();
  FakeBrowserWindow* window3 = CreateAndRegisterWindow();

  // Sanity checks.
  EXPECT_EQ(3U, GlobalBrowserCollection::GetInstance()->GetSize());
  EXPECT_EQ(0U, window1()->tab_count());
  EXPECT_EQ(0U, window2->tab_count());
  EXPECT_EQ(0U, window3->tab_count());
  EXPECT_EQ(0U, CountAllTabs());

  // Add some tabs.
  for (size_t i = 0; i < 3; ++i) {
    window2->AddTab();
  }
  for (size_t i = 0; i < 2; ++i) {
    window3->AddTab();
  }

  absl::flat_hash_map<BrowserWindowInterface*, size_t> tab_counts;
  tabs::ForEachTabInterface([&tab_counts](tabs::TabInterface* const tab) {
    ++tab_counts[tab->GetBrowserWindowInterface()];
    return true;
  });
  EXPECT_EQ(3u, tab_counts[window2->window()]);
  EXPECT_EQ(2u, tab_counts[window3->window()]);

  // Close some tabs and unregister window2.
  window2->CloseAllTabs();
  UnregisterWindow(window2->window());
  window3->RemoveTab(1);

  tab_counts.clear();
  tabs::ForEachTabInterface([&tab_counts](tabs::TabInterface* const tab) {
    ++tab_counts[tab->GetBrowserWindowInterface()];
    return true;
  });
  EXPECT_EQ(1u, tab_counts.size());
  EXPECT_EQ(1u, tab_counts[window3->window()]);

  // Add one tab back to window1.
  window1()->AddTab();

  tab_counts.clear();
  tabs::ForEachTabInterface([&tab_counts](tabs::TabInterface* const tab) {
    ++tab_counts[tab->GetBrowserWindowInterface()];
    return true;
  });
  EXPECT_EQ(2u, tab_counts.size());
  EXPECT_EQ(1u, tab_counts[window1()->window()]);
  EXPECT_EQ(1u, tab_counts[window3->window()]);
}

TEST_F(TabContentsIteratorTest, TabContentsIteratorEarlyExit) {
  FakeBrowserWindow* window2 = CreateAndRegisterWindow();
  window1()->AddTab();
  window1()->AddTab();
  window2->AddTab();

  int tab_count = 0;
  tabs::ForEachTabInterface([&tab_count](tabs::TabInterface* tab) {
    ++tab_count;
    return false;  // Stop after the first tab.
  });

  EXPECT_EQ(1, tab_count);
}

TEST_F(TabContentsIteratorTest,
       TabContentsIteratorTabDestructionDuringIteration) {
  FakeBrowserWindow* window2 = CreateAndRegisterWindow();
  FakeTab* tab1 = window1()->AddTab();
  window1()->AddTab();
  window2->AddTab();

  int tab_count = 0;
  tabs::ForEachTabInterface([&](tabs::TabInterface* tab) {
    if (tab == tab1) {
      // Destroy the second tab in window1 during iteration.
      window1()->RemoveTab(1);
    }
    ++tab_count;
    return true;
  });

  EXPECT_EQ(2, tab_count);
}

TEST_F(TabContentsIteratorTest, TabContentsIteratorWindowWithoutTabList) {
  auto bare_window = std::make_unique<NiceMock<MockBrowserWindowInterface>>();
  ON_CALL(*bare_window, GetProfile()).WillByDefault(Return(profile()));
  RegisterWindow(bare_window.get());

  window1()->AddTab();

  int count = 0;
  tabs::ForEachTabInterface([&count](tabs::TabInterface* tab) {
    ++count;
    return true;
  });

  EXPECT_EQ(1, count);
  UnregisterWindow(bare_window.get());
}

}  // namespace
