// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/navigator/browser_navigator_params_utils.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/tab_list/mock_tab_list_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

using ::testing::_;
using ::testing::Return;
using ::testing::ReturnRef;

class BrowserNavigatorParamsUtilsTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    mock_browser_ =
        std::make_unique<testing::NiceMock<MockBrowserWindowInterface>>();
    mock_tab_list_ =
        std::make_unique<testing::NiceMock<MockTabListInterface>>();

    EXPECT_CALL(*mock_browser_, GetProfile()).WillRepeatedly(Return(profile()));
    const MockBrowserWindowInterface* const_mock_browser = mock_browser_.get();
    EXPECT_CALL(*const_mock_browser, GetUnownedUserDataHost())
        .WillRepeatedly(ReturnRef(unowned_user_data_host_));

    tab_list_registration_ =
        std::make_unique<ui::ScopedUnownedUserData<TabListInterface>>(
            mock_browser_->GetUnownedUserDataHost(), *mock_tab_list_);

    // Setup 3 dummy tabs.
    for (int i = 0; i < 3; ++i) {
      auto web_contents =
          content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
      auto mock_tab = std::make_unique<tabs::MockTabInterface>();
      EXPECT_CALL(*mock_tab, GetContents())
          .WillRepeatedly(Return(web_contents.get()));
      web_contents_list_.push_back(std::move(web_contents));
      mock_tabs_.push_back(std::move(mock_tab));
    }

    content::WebContentsTester::For(web_contents_list_[0].get())
        ->NavigateAndCommit(GURL(kUrl1));
    content::WebContentsTester::For(web_contents_list_[1].get())
        ->NavigateAndCommit(GURL(kUrl2));
    content::WebContentsTester::For(web_contents_list_[2].get())
        ->NavigateAndCommit(GURL(kUrl3));

    EXPECT_CALL(*mock_tab_list_, GetTabCount())
        .WillRepeatedly(Return(mock_tabs_.size()));
    EXPECT_CALL(*mock_tab_list_, GetActiveIndex()).WillRepeatedly(Return(0));

    ON_CALL(*mock_tab_list_, GetTab(_))
        .WillByDefault([this](int index) -> tabs::TabInterface* {
          if (index >= 0 && index < static_cast<int>(mock_tabs_.size())) {
            return mock_tabs_[index].get();
          }
          return nullptr;
        });
  }

  void TearDown() override {
    tab_list_registration_.reset();
    mock_tab_list_.reset();
    mock_browser_.reset();
    mock_tabs_.clear();
    web_contents_list_.clear();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  NavigateParams NavigateParamsForTest(
      GURL url,
      WindowOpenDisposition disposition =
          WindowOpenDisposition::SINGLETON_TAB) {
    NavigateParams params(profile(), url,
                          ui::PageTransition::PAGE_TRANSITION_TYPED);
    params.disposition = disposition;
    return params;
  }

  MockBrowserWindowInterface* browser() { return mock_browser_.get(); }

  const char* kUrl1 = "http://1.chromium.org/1";
  const char* kUrl2 = "http://2.chromium.org/2";
  const char* kUrl3 = "https://3.chromium.org/3";

 private:
  std::unique_ptr<testing::NiceMock<MockBrowserWindowInterface>> mock_browser_;
  std::unique_ptr<testing::NiceMock<MockTabListInterface>> mock_tab_list_;
  std::unique_ptr<ui::ScopedUnownedUserData<TabListInterface>>
      tab_list_registration_;
  ui::UnownedUserDataHost unowned_user_data_host_;

  std::vector<std::unique_ptr<content::WebContents>> web_contents_list_;
  std::vector<std::unique_ptr<tabs::MockTabInterface>> mock_tabs_;
};

TEST_F(BrowserNavigatorParamsUtilsTest, FindsExactMatch) {
  EXPECT_EQ(GetIndexOfExistingTabMatchingURL(
                browser(), NavigateParamsForTest(GURL(kUrl1))),
            0);
  EXPECT_EQ(GetIndexOfExistingTabMatchingURL(
                browser(), NavigateParamsForTest(GURL(kUrl2))),
            1);
}

TEST_F(BrowserNavigatorParamsUtilsTest, FindsWithDifferentRef) {
  auto params = NavigateParamsForTest(GURL(kUrl1).Resolve("/1#ref"));
  EXPECT_EQ(GetIndexOfExistingTabMatchingURL(browser(), params), 0);
}

TEST_F(BrowserNavigatorParamsUtilsTest, DoesNotFindNonSingletonDisposition) {
  auto params = NavigateParamsForTest(
      GURL(kUrl1), WindowOpenDisposition::NEW_FOREGROUND_TAB);
  EXPECT_EQ(GetIndexOfExistingTabMatchingURL(browser(), params), -1);
}

TEST_F(BrowserNavigatorParamsUtilsTest, DoesNotFindViewSource) {
  auto params =
      NavigateParamsForTest(GURL("view-source:http://1.chromium.org/1"));
  EXPECT_EQ(GetIndexOfExistingTabMatchingURL(browser(), params), -1);
}

TEST_F(BrowserNavigatorParamsUtilsTest, DoesNotFindDifferentPath) {
  auto params = NavigateParamsForTest(GURL(kUrl1).Resolve("/a"));
  EXPECT_EQ(GetIndexOfExistingTabMatchingURL(browser(), params), -1);
}
