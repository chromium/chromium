// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/profiles/profiles_pixel_test_utils.h"
#include "chrome/browser/ui/views/search_ai_mode/signin_promo_controller.h"
#include "chrome/browser/ui/views/search_ai_mode/signin_promo_view.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace {

const PixelTestParam kTestParams[] = {
    {.test_suffix = "Regular"},
    {.test_suffix = "DarkTheme", .use_dark_theme = true},
    {.test_suffix = "Rtl", .use_right_to_left_language = true},
};
class SearchAIModeSignInPromoViewPixelTest
    : public ProfilesPixelTestBaseT<UiBrowserTest>,
      public ::testing::WithParamInterface<PixelTestParam> {
 public:
  SearchAIModeSignInPromoViewPixelTest()
      : ProfilesPixelTestBaseT<UiBrowserTest>(GetParam()) {
    feature_list_.InitAndEnableFeature(
        switches::kEnableSearchAIModeSigninPromo);
  }
  ~SearchAIModeSignInPromoViewPixelTest() override = default;

  // BrowserTestBase:
  void SetUpOnMainThread() override {
    ProfilesPixelTestBaseT::SetUpOnMainThread();
  }

  void DismissUi() override {
    if (promo_view_) {
      promo_view_->GetWidget()->CloseWithReason(
          views::Widget::ClosedReason::kUnspecified);
      promo_view_ = nullptr;
    }
  }

  void ShowUi(const std::string& name) override {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    views::View* anchor_view =
        browser_view->toolbar_button_provider()->GetAvatarToolbarButton();

    auto promo_view = std::make_unique<SearchAIModeSignInPromoView>(
        anchor_view, browser()->tab_strip_model()->GetActiveWebContents(),
        /*controller=*/nullptr);
    promo_view_ = promo_view.get();
    views::BubbleDialogDelegateView::CreateBubble(std::move(promo_view))
        ->Show();
  }

  bool VerifyUi() override {
    if (!promo_view_) {
      return false;
    }
    auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
    return VerifyPixelUi(promo_view_->GetWidget(), test_info->test_suite_name(),
                         test_info->name()) != ui::test::ActionResult::kFailed;
  }

  void WaitForUserDismissal() override {
    base::RunLoop run_loop;
    views::test::WidgetDestroyedWaiter waiter(promo_view_->GetWidget());
    waiter.Wait();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<SearchAIModeSignInPromoView> promo_view_ = nullptr;
};

IN_PROC_BROWSER_TEST_P(SearchAIModeSignInPromoViewPixelTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

std::string ParamToTestSuffix(
    const ::testing::TestParamInfo<PixelTestParam>& info) {
  return info.param.test_suffix;
}

INSTANTIATE_TEST_SUITE_P(All,
                         SearchAIModeSignInPromoViewPixelTest,
                         testing::ValuesIn(kTestParams),
                         &ParamToTestSuffix);

}  // namespace
