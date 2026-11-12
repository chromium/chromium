// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/testing/scoped_test_tab_strip.h"

#include <memory>

#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_feature.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_apis/tab_strip/tab_strip_service_view.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "url/gurl.h"

namespace tabs_api::testing {
namespace {

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

class ScopedTestTabStripTest : public ::testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_enabler_;
  TestingProfile profile_;
  ui::UnownedUserDataHost user_data_host_;
};

TEST_F(ScopedTestTabStripTest, RegistersFeatureAndManagesSyntheticTabs) {
  NiceMock<MockBrowserWindowInterface> mock_browser;
  ON_CALL(mock_browser, GetUnownedUserDataHost())
      .WillByDefault(ReturnRef(user_data_host_));

  ScopedTestTabStrip test_tab_strip(user_data_host_);
  TabStripServiceFeature* feature = TabStripServiceFeature::From(&mock_browser);
  ASSERT_NE(feature, nullptr);
  TabStripServiceView* view =
      TabStripServiceFeature::GetTabStripServiceView(&mock_browser);
  ASSERT_NE(view, nullptr);
  EXPECT_EQ(view, feature->GetTabStripServiceView());
  EXPECT_TRUE(view->GetTabs().empty());

  const tabs::TabHandle first = test_tab_strip.AddTab(
      GURL("https://first.example/"), u"First", /*active=*/true);
  const tabs::TabHandle second = test_tab_strip.AddTab(
      GURL("https://second.example/"), u"Second", /*active=*/false);

  ASSERT_EQ(view->GetTabs().size(), 2u);
  ASSERT_NE(view->GetActiveTab(), nullptr);
  EXPECT_EQ(view->GetActiveTab()->ToTabHandle(), first);
  EXPECT_EQ(view->GetTab(second)->title, u"Second");
  EXPECT_FALSE(view->GetTab(second)->is_active);

  test_tab_strip.SetActiveTab(second);
  ASSERT_NE(view->GetActiveTab(), nullptr);
  EXPECT_EQ(view->GetActiveTab()->ToTabHandle(), second);
  EXPECT_FALSE(view->GetTab(first)->is_active);

  test_tab_strip.CloseTab(first);
  ASSERT_EQ(view->GetTabs().size(), 1u);
  EXPECT_EQ(view->GetTab(first), nullptr);
  EXPECT_NE(view->GetTab(second), nullptr);
}

TEST_F(ScopedTestTabStripTest, ObservesTabWebContentsAndSupportsUpdateTab) {
  ScopedTestTabStrip test_tab_strip(user_data_host_);
  TabStripServiceView* view = test_tab_strip.feature().GetTabStripServiceView();

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
  tabs::MockTabInterface mock_tab;
  ON_CALL(mock_tab, GetContents()).WillByDefault(Return(web_contents.get()));

  test_tab_strip.AddTab(&mock_tab, /*active=*/true);
  ASSERT_NE(view->GetTab(mock_tab.GetHandle()), nullptr);

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents.get(), GURL("https://navigated.example/"));
  EXPECT_EQ(view->GetTab(mock_tab.GetHandle())->url,
            GURL("https://navigated.example/"));

  content::WebContentsTester::For(web_contents.get())->SetTitle(u"Updated");
  test_tab_strip.UpdateTab(&mock_tab);
  EXPECT_EQ(view->GetTab(mock_tab.GetHandle())->title, u"Updated");

  tabs::MockTabInterface unloaded_tab;
  ON_CALL(unloaded_tab, GetContents()).WillByDefault(Return(nullptr));
  ON_CALL(unloaded_tab, GetURL())
      .WillByDefault(Return(GURL("https://unloaded.example/")));
  ON_CALL(unloaded_tab, GetTitle()).WillByDefault(Return(u"Unloaded"));

  test_tab_strip.AddTab(&unloaded_tab, /*active=*/false);
  const Tab* unloaded_snapshot = view->GetTab(unloaded_tab.GetHandle());
  ASSERT_NE(unloaded_snapshot, nullptr);
  EXPECT_EQ(unloaded_snapshot->url, GURL("https://unloaded.example/"));
  EXPECT_EQ(unloaded_snapshot->title, u"Unloaded");
  EXPECT_FALSE(unloaded_snapshot->is_active);
}

}  // namespace
}  // namespace tabs_api::testing
