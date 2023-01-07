// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/ui/user_education/reopen_tab_in_product_help.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/user_education/reopen_tab_in_product_help_trigger.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/user_education/test/mock_feature_promo_controller.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Ref;
using ::testing::Return;

namespace {

constexpr base::TimeDelta kTabMinimumActiveDuration = base::Seconds(15);
constexpr base::TimeDelta kNewTabOpenedTimeout = base::Seconds(5);

}  // namespace

class ReopenTabInProductHelpTest : public BrowserWithTestWindowTest {
 protected:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        feature_engagement::kIPHReopenTabFeature,
        ReopenTabInProductHelpTrigger::GetFieldTrialParamsForTest(
            kTabMinimumActiveDuration.InSeconds(),
            kNewTabOpenedTimeout.InSeconds()));
  }

  std::unique_ptr<BrowserWindow> CreateBrowserWindow() override {
    auto test_window = std::make_unique<TestBrowserWindow>();

    // This test only supports one window.
    DCHECK(!mock_promo_controller_);

    mock_promo_controller_ =
        static_cast<user_education::test::MockFeaturePromoController*>(
            test_window->SetFeaturePromoController(
                std::make_unique<
                    user_education::test::MockFeaturePromoController>()));
    return test_window;
  }

  base::SimpleTestTickClock* clock() { return &clock_; }
  user_education::test::MockFeaturePromoController* mock_promo_controller() {
    return mock_promo_controller_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::SimpleTestTickClock clock_;

  raw_ptr<user_education::test::MockFeaturePromoController>
      mock_promo_controller_ = nullptr;
};

TEST_F(ReopenTabInProductHelpTest, TriggersIPH) {
  ReopenTabInProductHelp reopen_tab_iph(profile(), clock());

  EXPECT_CALL(
      *mock_promo_controller(),
      MaybeShowPromo(Ref(feature_engagement::kIPHReopenTabFeature), _, _))
      .Times(1)
      .WillOnce(Return(true));

  AddTab(browser(), GURL("chrome://blank"));
  AddTab(browser(), GURL("chrome://blank"));
  BrowserList::SetLastActive(browser());

  clock()->Advance(kTabMinimumActiveDuration);

  auto* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->ToggleSelectionAt(0);
  ASSERT_TRUE(tab_strip_model->IsTabSelected(0));
  tab_strip_model->CloseSelectedTabs();

  reopen_tab_iph.NewTabOpened();
}
