// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/back_to_opener/back_to_opener_controller.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace back_to_opener {

class FakeBrowserWindowInterface : public MockBrowserWindowInterface {
 public:
  ~FakeBrowserWindowInterface() override = default;
  explicit FakeBrowserWindowInterface(Profile* profile) : profile_(profile) {}
  Profile* GetProfile() override { return profile_; }

 private:
  raw_ptr<Profile> profile_ = nullptr;
};

class BackToOpenerControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    feature_list_.InitAndEnableFeature(tabs::kBackToOpener);

    browser_window_interface_ =
        std::make_unique<FakeBrowserWindowInterface>(profile());
    tab_strip_model_delegate_ = std::make_unique<TestTabStripModelDelegate>();
    tab_strip_model_delegate_->SetBrowserWindowInterface(
        browser_window_interface_.get());
    tab_strip_model_ = std::make_unique<TabStripModel>(
        tab_strip_model_delegate_.get(), profile());
    EXPECT_CALL(*browser_window_interface_, GetTabStripModel())
        .WillRepeatedly(testing::Return(tab_strip_model_.get()));
    EXPECT_CALL(*browser_window_interface_, GetUnownedUserDataHost())
        .WillRepeatedly(testing::ReturnRef(user_data_host_));
  }

  void TearDown() override {
    tab_strip_model_.reset();
    tab_strip_model_delegate_.reset();
    browser_window_interface_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<tabs::TabModel> CreateTabModel(
      content::WebContents* contents) {
    std::unique_ptr<content::WebContents> web_contents(contents);
    return std::make_unique<tabs::TabModel>(std::move(web_contents),
                                            tab_strip_model_.get());
  }

  BackToOpenerController* GetController(tabs::TabModel* tab_model) {
    return BackToOpenerController::From(tab_model);
  }

  TabStripModel* tab_strip_model() { return tab_strip_model_.get(); }

 private:
  std::unique_ptr<TabStripModel> tab_strip_model_;
  base::test::ScopedFeatureList feature_list_;
  ui::UnownedUserDataHost user_data_host_;
  std::unique_ptr<FakeBrowserWindowInterface> browser_window_interface_;
  std::unique_ptr<TestTabStripModelDelegate> tab_strip_model_delegate_;
};

TEST_F(BackToOpenerControllerTest, BasicRelationship) {
  std::unique_ptr<content::WebContents> dest_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      dest_contents.get(), GURL("https://example.com/page"));

  std::unique_ptr<content::WebContents> opener_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      opener_contents.get(), GURL("https://example.com"));

  std::unique_ptr<tabs::TabModel> tab_model =
      CreateTabModel(dest_contents.release());
  BackToOpenerController* controller = GetController(tab_model.get());
  controller->SetOpenerWebContents(opener_contents.get());

  EXPECT_TRUE(controller->HasValidOpener());
  EXPECT_TRUE(controller->CanGoBackToOpener());
  // Note: GetOpenerOriginalURL() is private, but we can verify the relationship
  // is established correctly through HasValidOpener() and CanGoBackToOpener().
}

TEST_F(BackToOpenerControllerTest, OpenerDestroyedClearsRelationship) {
  std::unique_ptr<content::WebContents> dest_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      dest_contents.get(), GURL("https://example.com/page"));

  std::unique_ptr<content::WebContents> opener_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      opener_contents.get(), GURL("https://example.com"));

  std::unique_ptr<tabs::TabModel> tab_model =
      CreateTabModel(dest_contents.release());
  BackToOpenerController* controller = GetController(tab_model.get());
  controller->SetOpenerWebContents(opener_contents.get());

  EXPECT_TRUE(controller->HasValidOpener());
  EXPECT_TRUE(controller->CanGoBackToOpener());

  opener_contents.reset();

  EXPECT_FALSE(controller->HasValidOpener());
  EXPECT_FALSE(controller->CanGoBackToOpener());
}

TEST_F(BackToOpenerControllerTest, OpenerNavigatedAwayClearsRelationship) {
  std::unique_ptr<content::WebContents> dest_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      dest_contents.get(), GURL("https://example.com/page"));

  std::unique_ptr<content::WebContents> opener_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      opener_contents.get(), GURL("https://example.com"));

  std::unique_ptr<tabs::TabModel> tab_model =
      CreateTabModel(dest_contents.release());
  BackToOpenerController* controller = GetController(tab_model.get());
  controller->SetOpenerWebContents(opener_contents.get());

  EXPECT_TRUE(controller->HasValidOpener());
  EXPECT_TRUE(controller->CanGoBackToOpener());

  // Simulate opener navigating away from original URL
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      opener_contents.get(), GURL("https://other.com"));

  EXPECT_FALSE(controller->HasValidOpener());
  EXPECT_FALSE(controller->CanGoBackToOpener());
}

TEST_F(BackToOpenerControllerTest, PinnedTabDisablesBackToOpener) {
  std::unique_ptr<content::WebContents> dest_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      dest_contents.get(), GURL("https://example.com/page"));

  std::unique_ptr<content::WebContents> opener_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      opener_contents.get(), GURL("https://example.com"));

  std::unique_ptr<tabs::TabModel> tab_model =
      CreateTabModel(dest_contents.release());
  BackToOpenerController* controller = GetController(tab_model.get());
  controller->SetOpenerWebContents(opener_contents.get());

  EXPECT_TRUE(controller->CanGoBackToOpener());
  EXPECT_TRUE(controller->HasValidOpener());

  // Pinning should disable back-to-opener but maintain relationship
  controller->OnPinnedStateChanged(tab_model.get(), true);

  EXPECT_FALSE(controller->CanGoBackToOpener());
  EXPECT_TRUE(controller->HasValidOpener());

  // Unpinning should re-enable back-to-opener
  controller->OnPinnedStateChanged(tab_model.get(), false);

  EXPECT_TRUE(controller->CanGoBackToOpener());
  EXPECT_TRUE(controller->HasValidOpener());
}

// Regression test: Back-to-opener is notified from OnPinnedStateChanged when
// tabs are reparented (e.g. during unsplit/close). At that moment the tab
// strip selection can be invalid (active_index() == kNoTab).
// NotifyUIStateChanged must skip NotifyNavigationStateChanged in that case.
TEST_F(BackToOpenerControllerTest, OnPinnedStateChangedWithInvalidSelection) {
  std::unique_ptr<content::WebContents> dest_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      dest_contents.get(), GURL("https://example.com/page"));

  std::unique_ptr<content::WebContents> opener_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      opener_contents.get(), GURL("https://example.com"));

  // Tab is created but never added to the strip.
  std::unique_ptr<tabs::TabModel> tab_model =
      CreateTabModel(dest_contents.release());
  ASSERT_EQ(tab_strip_model()->active_index(), TabStripModel::kNoTab);

  BackToOpenerController* controller = GetController(tab_model.get());
  controller->SetOpenerWebContents(opener_contents.get());
  EXPECT_TRUE(controller->HasValidOpener());

  // Should not crash or DCHECK. NotifyUIStateChanged skips
  // NotifyNavigationStateChanged when active_index() == kNoTab.
  controller->OnPinnedStateChanged(tab_model.get(), true);

  // Controller state should still be updated (pinned disables back-to-opener).
  EXPECT_FALSE(controller->CanGoBackToOpener());
  EXPECT_TRUE(controller->HasValidOpener());
}

}  // namespace back_to_opener
