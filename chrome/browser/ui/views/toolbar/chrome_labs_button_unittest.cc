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
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kChromeLabs);

    std::vector<flags_ui::FeatureEntry> entries = {
        {kFirstTestFeatureId, "", "",
         flags_ui::FlagsState::GetCurrentPlatform(),
         FEATURE_VALUE_TYPE(kTestFeature1)}};

    about_flags::testing::SetFeatureEntries(entries);

    std::vector<LabInfo> test_feature_info = {
        {kFirstTestFeatureId, base::ASCIIToUTF16(""), base::ASCIIToUTF16(""),
         version_info::Channel::STABLE}};
    scoped_chrome_labs_model_data_.SetModelDataForTesting(test_feature_info);

    TestWithBrowserView::SetUp();
    profile()->GetPrefs()->SetBoolean(chrome_labs_prefs::kBrowserLabsEnabled,
                                      true);
  }

 private:
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
  EXPECT_NE(browser_view()->toolbar()->chrome_labs_button(), nullptr);
}

class ChromeLabsButtonNoExperimentsAvailableTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kChromeLabs);

    std::vector<flags_ui::FeatureEntry> entries = {
        {kSecondTestFeatureId, "", "", 0, FEATURE_VALUE_TYPE(kTestFeature2)}};

    about_flags::testing::SetFeatureEntries(entries);

    std::vector<LabInfo> test_feature_info = {
        {kSecondTestFeatureId, base::ASCIIToUTF16(""), base::ASCIIToUTF16(""),
         version_info::Channel::STABLE}};
    scoped_chrome_labs_model_data_.SetModelDataForTesting(test_feature_info);

    TestWithBrowserView::SetUp();
    profile()->GetPrefs()->SetBoolean(chrome_labs_prefs::kBrowserLabsEnabled,
                                      true);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  ScopedChromeLabsModelDataForTesting scoped_chrome_labs_model_data_;
};

TEST_F(ChromeLabsButtonNoExperimentsAvailableTest, ShouldButtonShowTest) {
  EXPECT_EQ(browser_view()->toolbar()->chrome_labs_button(), nullptr);
}
