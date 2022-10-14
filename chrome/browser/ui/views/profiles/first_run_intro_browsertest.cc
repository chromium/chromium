// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/widget/widget.h"

#if !BUILDFLAG(ENABLE_DICE_SUPPORT)
#error Platform not supported
#endif

// Tests for the chrome://intro WebUI page. They live here and not in the webui
// directory because they manipulate views.

class FirstRunIntroPixelTest : public UiBrowserTest {
 public:
  void ShowUi(const std::string& name) override {
    ui::ScopedAnimationDurationScaleMode disable_animation(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
    GURL url(chrome::kChromeUIIntroURL + std::string("?noAnimations"));

    // TODO(crbug.com/1347507): Render the page in the profile management view
    // instead of a full browser window.
    ASSERT_TRUE(
        AddTabAtIndex(0, url, ui::PageTransition::PAGE_TRANSITION_FIRST));
  }

  bool VerifyUi() override {
    views::Widget* widget = GetWidgetForScreenshot();

    auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
    const std::string screenshot_name =
        base::StrCat({test_info->test_case_name(), "_", test_info->name()});

    return VerifyPixelUi(widget, "FirstRunIntroPixelTest", screenshot_name);
  }

  void WaitForUserDismissal() override {
    DCHECK(GetWidgetForScreenshot());
    ui_test_utils::WaitForBrowserToClose(browser());
  }

 private:
  views::Widget* GetWidgetForScreenshot() {
    return BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();
  }

  base::test::ScopedFeatureList scoped_feature_list_{kForYouFre};
};

IN_PROC_BROWSER_TEST_F(FirstRunIntroPixelTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
