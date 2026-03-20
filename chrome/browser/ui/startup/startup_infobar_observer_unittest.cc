// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_infobar_observer.h"

#include <memory>
#include <utility>

#include "base/test/test_future.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

using testing::Return;
using testing::ReturnRef;

class StartupInfoBarObserverTest : public testing::Test {
 protected:
  StartupInfoBarObserverTest() = default;
  ~StartupInfoBarObserverTest() override = default;

  void SetUp() override {
    profile_ = TestingProfile::Builder().Build();
    tab_strip_model_ = std::make_unique<TabStripModel>(
        &tab_strip_model_delegate_, profile_.get());
    browser_interface_ =
        std::make_unique<testing::NiceMock<MockBrowserWindowInterface>>();
    tab_strip_model_delegate_.SetBrowserWindowInterface(
        browser_interface_.get());

    ON_CALL(*browser_interface_, GetTabStripModel())
        .WillByDefault(Return(tab_strip_model_.get()));
    ON_CALL(static_cast<const MockBrowserWindowInterface&>(*browser_interface_),
            GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(user_data_host_));
    ON_CALL(*browser_interface_, GetProfile())
        .WillByDefault(Return(profile_.get()));
  }

  StartupInfoBarObserver* CreateObserver(
      StartupInfoBarObserver::AddInfoBarsCallback callback) {
    StartupInfoBarObserver::ObserveProfile(*profile(), std::move(callback));

    return static_cast<StartupInfoBarObserver*>(profile()->GetUserData(
        &StartupInfoBarObserver::kStartupInfoBarObserverKey));
  }

  TestingProfile* profile() { return profile_.get(); }
  MockBrowserWindowInterface* browser() { return browser_interface_.get(); }
  TabStripModel* tab_strip_model() { return tab_strip_model_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
  std::unique_ptr<TestingProfile> profile_;
  ui::UnownedUserDataHost user_data_host_;
  std::unique_ptr<MockBrowserWindowInterface> browser_interface_;
  TestTabStripModelDelegate tab_strip_model_delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
};

TEST_F(StartupInfoBarObserverTest, DoesNotRunOnBrowserCreated) {
  base::test::TestFuture<BrowserWindowInterface*> future;
  StartupInfoBarObserver* observer = CreateObserver(future.GetCallback());

  // Browser created but empty.
  observer->OnBrowserCreated(browser());
  EXPECT_FALSE(future.IsReady());
  ASSERT_NE(nullptr, profile()->GetUserData(
                         &StartupInfoBarObserver::kStartupInfoBarObserverKey));
}

TEST_F(StartupInfoBarObserverTest, RunsCallbackOnTabInserted) {
  base::test::TestFuture<BrowserWindowInterface*> future;
  StartupInfoBarObserver* observer = CreateObserver(future.GetCallback());

  observer->OnBrowserCreated(browser());

  // Adding a tab should trigger the infobar creation and delete the observer.
  std::unique_ptr<content::WebContents> contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  tab_strip_model()->AppendWebContents(std::move(contents), true);

  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(nullptr, profile()->GetUserData(
                         &StartupInfoBarObserver::kStartupInfoBarObserverKey));
}

TEST_F(StartupInfoBarObserverTest, HandlesTabStripModelDestruction) {
  base::test::TestFuture<BrowserWindowInterface*> future;
  StartupInfoBarObserver* observer = CreateObserver(future.GetCallback());

  observer->OnBrowserCreated(browser());

  // Destroying the tab strip model before inserting a tab should delete the
  // observer.
  observer->OnTabStripModelDestroyed(tab_strip_model());

  EXPECT_FALSE(future.IsReady());
  EXPECT_EQ(nullptr, profile()->GetUserData(
                         &StartupInfoBarObserver::kStartupInfoBarObserverKey));
}

TEST_F(StartupInfoBarObserverTest, IgnoresNonInsertionTabStripChanges) {
  base::test::TestFuture<BrowserWindowInterface*> future;
  StartupInfoBarObserver* observer = CreateObserver(future.GetCallback());

  observer->OnBrowserCreated(browser());

  // Simulate a change other than kInserted.
  TabStripSelectionChange selection;
  TabStripModelChange change;

  observer->OnTabStripModelChanged(tab_strip_model(), change, selection);

  // Callback shouldn't trigger and observer should still be alive since the
  // change wasn't kInserted.
  EXPECT_FALSE(future.IsReady());
  EXPECT_NE(nullptr, profile()->GetUserData(
                         &StartupInfoBarObserver::kStartupInfoBarObserverKey));
}
