// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/toolbar/chrome_labs_model.h"
#include "chrome/browser/ui/toolbar/chrome_labs_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/flags_ui/feature_entry_macros.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_test.h"
#include "ui/events/event_utils.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/test/base/scoped_channel_override.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH) || !BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace {
const char kFirstTestFeatureId[] = "feature-1";
BASE_FEATURE(kTestFeature1, "FeatureName1", base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace

class ChromeLabsUiTest : public DialogBrowserTest {
 public:
  ChromeLabsUiTest()
      :
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        channel_override_(chrome::ScopedChannelOverride(
            chrome::ScopedChannelOverride::Channel::kDev)),
#endif
        scoped_feature_entries_({{kFirstTestFeatureId, "", "",
                                  flags_ui::FlagsState::GetCurrentPlatform(),
                                  FEATURE_VALUE_TYPE(kTestFeature1)}}) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kChromeLabs,
        {{features::kChromeLabsActivationPercentage.name, "100"}});
    std::vector<LabInfo> test_feature_info = {
        {kFirstTestFeatureId, u"Feature 1", u"Feature description", "",
         version_info::Channel::STABLE}};
    scoped_chrome_labs_model_data_.SetModelDataForTesting(test_feature_info);
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    // Bubble bounds may exceed display's work area.
    // https://crbug.com/893292
    set_should_verify_dialog_bounds(false);
    ChromeLabsButton* chrome_labs_button =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar()
            ->chrome_labs_button();

    chrome_labs_button->OnMousePressed(
        ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
    chrome_labs_button->OnMouseReleased(
        ui::MouseEvent(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  }

 private:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  chrome::ScopedChannelOverride channel_override_;
#endif
  about_flags::testing::ScopedFeatureEntries scoped_feature_entries_;
  base::test::ScopedFeatureList scoped_feature_list_;
  ScopedChromeLabsModelDataForTesting scoped_chrome_labs_model_data_;
};

IN_PROC_BROWSER_TEST_F(ChromeLabsUiTest, InvokeUi_default) {
  set_baseline("2810222");
  ShowAndVerifyUi();
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) || !BUILDFLAG(GOOGLE_CHROME_BRANDING)
