// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/boca/chrome_tab_strip_delegate.h"

#include <memory>
#include <string>

#include "ash/public/cpp/tab_strip_delegate.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "content/public/test/test_web_ui.h"

class ChromeTabStripDelegateTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    delegate_ = std::make_unique<ChromeTabStripDelegate>();
  }

  void TearDown() override {
    delegate_ = nullptr;
    BrowserWithTestWindowTest::TearDown();
  }

  void ExpectTab(GURL url, std::string title, ash::TabInfo tab) {
    EXPECT_EQ(base::UTF8ToUTF16(title), tab.title);
    EXPECT_EQ(url, tab.url);
    EXPECT_GT(tab.last_access_timetick, base::TimeTicks());
  }

  ChromeTabStripDelegate* delegate() { return delegate_.get(); }

 private:
  std::unique_ptr<ChromeTabStripDelegate> delegate_;
};

TEST_F(ChromeTabStripDelegateTest, NullWindowShouldReturnEmptyData) {
  base::test::TestFuture<std::vector<ash::TabInfo>> future;
  auto tab_list = delegate()->GetTabsListForWindow(
      /*window=*/nullptr);
  EXPECT_EQ(0u, tab_list.size());
}

TEST_F(ChromeTabStripDelegateTest, EmptyWindowShouldReturnEmptyData) {
  base::test::TestFuture<std::vector<ash::TabInfo>> future;
  auto tab_list = delegate()->GetTabsListForWindow(
      /*window=*/BrowserList::GetInstance()
          ->get(0)
          ->window()
          ->GetNativeWindow());
  EXPECT_EQ(0u, tab_list.size());
}
