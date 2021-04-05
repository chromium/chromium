// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/chrome_labs_button.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/ui/toolbar/chrome_labs_prefs.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs_bubble_view_model.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/flags_ui/feature_entry_macros.h"
#include "ui/events/event_utils.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"

namespace {
const char kFirstTestFeatureId[] = "feature-1";
const base::Feature kTestFeature1{"FeatureName1",
                                  base::FEATURE_ENABLED_BY_DEFAULT};
const char kSecondTestFeatureId[] = "feature-2";
const base::Feature kTestFeature2{"FeatureName2",
                                  base::FEATURE_DISABLED_BY_DEFAULT};
}  // namespace

class ChromeLabsButtonTest : public TestWithBrowserView {
 public:
  ChromeLabsButtonTest()
      : scoped_feature_entries_({{kFirstTestFeatureId, "", "",
                                  flags_ui::FlagsState::GetCurrentPlatform(),
                                  FEATURE_VALUE_TYPE(kTestFeature1)}}) {}
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kChromeLabs);

    std::vector<LabInfo> test_feature_info = {
        {kFirstTestFeatureId, u"", u"", "", version_info::Channel::STABLE}};
    scoped_chrome_labs_model_data_.SetModelDataForTesting(test_feature_info);

    TestWithBrowserView::SetUp();
    profile()->GetPrefs()->SetBoolean(chrome_labs_prefs::kBrowserLabsEnabled,
                                      true);
  }

 private:
  about_flags::testing::ScopedFeatureEntries scoped_feature_entries_;
  base::test::ScopedFeatureList scoped_feature_list_;

  ScopedChromeLabsModelDataForTesting scoped_chrome_labs_model_data_;
};

TEST_F(ChromeLabsButtonTest, ShowAndHideChromeLabsBubbleOnPress) {
  ChromeLabsButton* labs_button =
      browser_view()->toolbar()->chrome_labs_button();
  EXPECT_FALSE(ChromeLabsBubbleView::IsShowing());
  ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(labs_button);
  test_api.NotifyClick(e);
  EXPECT_TRUE(ChromeLabsBubbleView::IsShowing());
  views::test::WidgetDestroyedWaiter destroyed_waiter(
      ChromeLabsBubbleView::GetChromeLabsBubbleViewForTesting()->GetWidget());
  test_api.NotifyClick(e);
  destroyed_waiter.Wait();
  EXPECT_FALSE(ChromeLabsBubbleView::IsShowing());
}

TEST_F(ChromeLabsButtonTest, ShouldButtonShowTest) {
  // There are experiments available so the button should not be nullptr.
  EXPECT_NE(browser_view()->toolbar()->chrome_labs_button(), nullptr);
  // Enterprise policy is initially set to true.
  EXPECT_TRUE(browser_view()->toolbar()->chrome_labs_button()->GetVisible());

  // Default enterprise policy value should show the Chrome Labs button.
  profile()->GetPrefs()->ClearPref(chrome_labs_prefs::kBrowserLabsEnabled);
  EXPECT_TRUE(browser_view()->toolbar()->chrome_labs_button()->GetVisible());

  profile()->GetPrefs()->SetBoolean(chrome_labs_prefs::kBrowserLabsEnabled,
                                    false);
  EXPECT_FALSE(browser_view()->toolbar()->chrome_labs_button()->GetVisible());
}

class ChromeLabsButtonNoExperimentsAvailableTest : public TestWithBrowserView {
 public:
  ChromeLabsButtonNoExperimentsAvailableTest()
      : scoped_feature_entries_({{kSecondTestFeatureId, "", "", 0,
                                  FEATURE_VALUE_TYPE(kTestFeature2)}}) {}
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kChromeLabs);

    std::vector<LabInfo> test_feature_info = {
        {kSecondTestFeatureId, u"", u"", "", version_info::Channel::STABLE}};
    scoped_chrome_labs_model_data_.SetModelDataForTesting(test_feature_info);

    TestWithBrowserView::SetUp();
    profile()->GetPrefs()->SetBoolean(chrome_labs_prefs::kBrowserLabsEnabled,
                                      true);
  }

 private:
  about_flags::testing::ScopedFeatureEntries scoped_feature_entries_;
  base::test::ScopedFeatureList scoped_feature_list_;

  ScopedChromeLabsModelDataForTesting scoped_chrome_labs_model_data_;
};

TEST_F(ChromeLabsButtonNoExperimentsAvailableTest, ShouldButtonShowTest) {
  EXPECT_EQ(browser_view()->toolbar()->chrome_labs_button(), nullptr);
}
