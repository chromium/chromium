// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_group_focus_iph_controller.h"

#include <memory>
#include <optional>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/user_education/mock_browser_user_education_interface.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_interface.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

class TabGroupFocusIphControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  TabGroupFocusIphControllerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    feature_list_.InitAndEnableFeature(features::kTabGroupsFocusing);
  }

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
        .WillByDefault(testing::Return(profile()));

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

  MockBrowserUserEducationInterface* user_education() {
    return static_cast<MockBrowserUserEducationInterface*>(
        BrowserUserEducationInterface::From(browser_window_interface()));
  }

  void AddTabs(int count) {
    for (int i = 0; i < count; ++i) {
      std::unique_ptr<content::WebContents> contents =
          content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
      content::WebContentsTester::For(contents.get())
          ->NavigateAndCommit(GURL("http://test_url"));
      tab_strip_model_->AppendWebContents(std::move(contents), true);
    }
  }

  void ExpectFocusPromoShown(int times) {
    EXPECT_CALL(*user_education(),
                MaybeShowFeaturePromo(testing::Truly([](const auto& params) {
                  return &params.feature.get() ==
                         &feature_engagement::kIPHTabGroupsFocusingFeature;
                })))
        .Times(times);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface_;
  TestTabStripModelDelegate test_tab_strip_model_delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  ui::UnownedUserDataHost user_data_host_;
  std::optional<MockBrowserUserEducationInterface> user_education_;
  const tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
};

TEST_F(TabGroupFocusIphControllerTest, DoesNotShowPromoWithoutTabGroup) {
  TabGroupFocusIphController controller(browser_window_interface());

  ExpectFocusPromoShown(0);
  AddTabs(features::kTabGroupsFocusingIphMinTabs.Get() + 1);
  task_environment()->FastForwardBy(
      features::kTabGroupsFocusingIphPromoDelay.Get());
}

TEST_F(TabGroupFocusIphControllerTest, DoesNotShowPromoBelowTabThreshold) {
  TabGroupFocusIphController controller(browser_window_interface());

  ExpectFocusPromoShown(0);
  AddTabs(features::kTabGroupsFocusingIphMinTabs.Get());
  tab_strip_model()->AddToNewGroup({0});
  task_environment()->FastForwardBy(
      features::kTabGroupsFocusingIphPromoDelay.Get());
}

TEST_F(TabGroupFocusIphControllerTest,
       ShowsPromoWhenTabGroupAndTabCountExceedThreshold) {
  TabGroupFocusIphController controller(browser_window_interface());

  ExpectFocusPromoShown(1);
  AddTabs(features::kTabGroupsFocusingIphMinTabs.Get() + 1);
  tab_strip_model()->AddToNewGroup({0});
  task_environment()->FastForwardBy(
      features::kTabGroupsFocusingIphPromoDelay.Get());
}

TEST_F(TabGroupFocusIphControllerTest,
       FocusAndUnfocusGroupManagesPromosAndBadges) {
  TabGroupFocusIphController controller(browser_window_interface());

  AddTabs(2);
  tab_groups::TabGroupId group_id = tab_strip_model()->AddToNewGroup({0});

  EXPECT_CALL(
      *user_education(),
      NotifyFeaturePromoFeatureUsed(
          testing::Ref(feature_engagement::kIPHTabGroupsFocusingFeature),
          FeaturePromoFeatureUsedAction::kClosePromoIfPresent))
      .Times(1);
  EXPECT_CALL(
      *user_education(),
      NotifyNewBadgeFeatureUsed(testing::Ref(features::kTabGroupsFocusing)))
      .Times(1);
  EXPECT_CALL(*user_education(),
              MaybeShowFeaturePromo(testing::Truly([](const auto& params) {
                return &params.feature.get() ==
                       &feature_engagement::kIPHTabGroupsUnfocusingFeature;
              })))
      .Times(1);

  tab_strip_model()->EnterFocusMode(group_id);
  testing::Mock::VerifyAndClearExpectations(user_education());

  EXPECT_CALL(
      *user_education(),
      NotifyFeaturePromoFeatureUsed(
          testing::Ref(feature_engagement::kIPHTabGroupsUnfocusingFeature),
          FeaturePromoFeatureUsedAction::kClosePromoIfPresent))
      .Times(1);

  tab_strip_model()->ExitFocusMode();
}
