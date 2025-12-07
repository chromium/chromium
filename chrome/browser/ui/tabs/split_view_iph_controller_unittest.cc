// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/split_view_iph_controller.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/test/gtest_util.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/user_education/mock_browser_user_education_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/test/mock_feature_promo_controller.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"

class SplitViewIphControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    test_tab_strip_model_delegate_.SetBrowserWindowInterface(
        &mock_browser_window_interface_);
    tab_strip_model_ = std::make_unique<TabStripModel>(
        &test_tab_strip_model_delegate_, profile());

    ON_CALL(mock_browser_window_interface_, GetTabStripModel())
        .WillByDefault(testing::Return(tab_strip_model_.get()));
    ON_CALL(mock_browser_window_interface_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(user_data_host_));
    ON_CALL(mock_browser_window_interface_, GetProfile())
        .WillByDefault(::testing::Return(profile()));
    user_education_.emplace(&mock_browser_window_interface_);
    ON_CALL(*user_education_, MaybeShowFeaturePromo(testing::_))
        .WillByDefault([](user_education::FeaturePromoParams params) {
          return user_education::FeaturePromoResult::Success();
        });
  }

  void TearDown() override {
    DeleteContents();
    tab_strip_model_.reset();
    test_tab_strip_model_delegate_.SetBrowserWindowInterface(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TabStripModel* tab_strip_model() { return tab_strip_model_.get(); }
  MockBrowserWindowInterface* browser_window_interface() {
    return &mock_browser_window_interface_;
  }

  std::unique_ptr<content::WebContents> CreateWebContents() {
    return content::WebContentsTester::CreateTestWebContents(profile(),
                                                             nullptr);
  }

  tabs::TabInterface* AddTab(TabStripModel* tab_strip_model = nullptr,
                             std::optional<GURL> url = std::nullopt) {
    std::unique_ptr<content::WebContents> contents_unique_ptr =
        CreateWebContents();
    content::WebContentsTester::For(contents_unique_ptr.get())
        ->NavigateAndCommit(url.has_value() ? url.value()
                                            : GURL("http://test_url"));
    content::WebContents* content_ptr = contents_unique_ptr.get();
    if (!tab_strip_model) {
      tab_strip_model = tab_strip_model_.get();
    }
    tab_strip_model->AppendWebContents(std::move(contents_unique_ptr), true);
    return tab_strip_model->GetTabForWebContents(content_ptr);
  }

  MockBrowserUserEducationInterface* user_education() {
    return static_cast<MockBrowserUserEducationInterface*>(
        BrowserUserEducationInterface::From(browser_window_interface()));
  }

 private:
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface_;
  TestTabStripModelDelegate test_tab_strip_model_delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  ui::UnownedUserDataHost user_data_host_;
  ui::UserDataFactory::ScopedOverride user_ed_override_;
  std::optional<MockBrowserUserEducationInterface> user_education_;
  const tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
};

TEST_F(SplitViewIphControllerTest, RemoveAllTabs) {
  SplitViewIphController iphController{browser_window_interface()};

  AddTab(tab_strip_model(), GURL("test_tab_1"));
  AddTab(tab_strip_model(), GURL("test_tab_2"));

  EXPECT_EQ(iphController.get_recent_tabs_size(), 2);

  tab_strip_model()->CloseAllTabs();
  // All tabs but 1 will be closed, which will remain in recent_tabs_.
  EXPECT_EQ(iphController.get_recent_tabs_size(), 1);
}

TEST_F(SplitViewIphControllerTest, AddMoreThanTwoTabs) {
  SplitViewIphController iphController{browser_window_interface()};

  AddTab(tab_strip_model(), GURL("test_tab_1"));
  AddTab(tab_strip_model(), GURL("test_tab_2"));

  EXPECT_EQ(iphController.get_recent_tabs_size(), 2);

  AddTab(tab_strip_model(), GURL("test_tab_3"));
  AddTab(tab_strip_model(), GURL("test_tab_4"));
  AddTab(tab_strip_model(), GURL("test_tab_5"));

  EXPECT_EQ(iphController.get_recent_tabs_size(), 2);
}

TEST_F(SplitViewIphControllerTest, SelectingDifferentTabs) {
  SplitViewIphController iphController{browser_window_interface()};

  AddTab(tab_strip_model(), GURL("test_tab_1"));
  AddTab(tab_strip_model(), GURL("test_tab_2"));

  EXPECT_EQ(iphController.get_recent_tabs_size(), 2);

  AddTab(tab_strip_model(), GURL("test_tab_3"));
  AddTab(tab_strip_model(), GURL("test_tab_4"));
  AddTab(tab_strip_model(), GURL("test_tab_5"));

  EXPECT_EQ(iphController.get_recent_tabs_size(), 2);

  // First two tab switches will be a new set of tabs, so tab_switch_count will
  // not be incremented.
  tab_strip_model()->SelectTabAt(0);
  tab_strip_model()->SelectTabAt(1);
  tab_strip_model()->SelectTabAt(0);
  tab_strip_model()->SelectTabAt(1);
  tab_strip_model()->SelectTabAt(0);
  tab_strip_model()->SelectTabAt(1);

  EXPECT_EQ(iphController.get_tab_switch_count(), 4);
}
