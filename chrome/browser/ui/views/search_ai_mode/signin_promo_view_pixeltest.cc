// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/profiles_pixel_test_utils.h"
#include "chrome/browser/ui/views/search_ai_mode/signin_promo_controller.h"
#include "chrome/browser/ui/views/search_ai_mode/signin_promo_view.h"
#include "chrome/browser/ui/views/toolbar/avatar_toolbar_button_interface.h"
#include "components/contextual_tasks/public/features.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"

namespace {

const PixelTestParam kTestParams[] = {
    {.test_suffix = "Regular"},
    {.test_suffix = "DarkTheme", .use_dark_theme = true},
    {.test_suffix = "Rtl", .use_right_to_left_language = true},
};

std::string ParamToTestSuffix(
    const ::testing::TestParamInfo<PixelTestParam>& info) {
  return info.param.test_suffix;
}

class AIModeSignInPromoViewPixelTestBase
    : public ProfilesPixelTestBaseT<UiBrowserTest>,
      public ::testing::WithParamInterface<PixelTestParam> {
 public:
  AIModeSignInPromoViewPixelTestBase()
      : ProfilesPixelTestBaseT<UiBrowserTest>(GetParam()) {}
  ~AIModeSignInPromoViewPixelTestBase() override = default;

  // BrowserTestBase:
  void SetUpOnMainThread() override {
    ProfilesPixelTestBaseT::SetUpOnMainThread();
  }

  void DismissUi() override {
    if (promo_view_tracker_.view()) {
      promo_view_tracker_.view()->GetWidget()->CloseWithReason(
          views::Widget::ClosedReason::kUnspecified);
    }
  }

  bool VerifyUi() override {
    if (!promo_view_tracker_.view()) {
      return false;
    }
    auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
    return VerifyPixelUi(promo_view_tracker_.view()->GetWidget(),
                         test_info->test_suite_name(),
                         test_info->name()) != ui::test::ActionResult::kFailed;
  }

  void WaitForUserDismissal() override {
    if (!promo_view_tracker_.view()) {
      return;
    }
    views::test::WidgetDestroyedWaiter waiter(
        promo_view_tracker_.view()->GetWidget());
    waiter.Wait();
  }

 protected:
  views::BubbleAnchor GetAvatarBubbleAnchor() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view->toolbar_button_provider()
        ->GetAvatarToolbarButtonInterface()
        ->GetBubbleAnchor(*browser());
  }

  void ShowPromoView(std::unique_ptr<AIModeSignInPromoViewBase> promo_view) {
    promo_view_tracker_.SetView(promo_view.get());
    views::BubbleDialogDelegateView::CreateBubble(std::move(promo_view))
        ->Show();
  }

 private:
  views::ViewTracker promo_view_tracker_;
};

// Pixel tests for SearchAIModeSignInPromoView.
class SearchAIModeSignInPromoViewPixelTest
    : public AIModeSignInPromoViewPixelTestBase {
 public:
  SearchAIModeSignInPromoViewPixelTest() {
    feature_list_.InitWithFeatures(
        // The UI depends on the ContextualTasksUiService which is only enabled
        // with the kContextualTasks flag. The flag is not strictly required to
        // enable the view.
        /*enabled_features=*/{switches::kEnableSearchAIModeSigninPromo,
                              contextual_tasks::kContextualTasks},
        /*disabled_features=*/{});
  }

  void ShowUi(const std::string& name) override {
    ShowPromoView(std::make_unique<SearchAIModeSignInPromoView>(
        GetAvatarBubbleAnchor(),
        browser()->GetActiveTabInterface()->GetContents(),
        /*controller=*/nullptr));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(SearchAIModeSignInPromoViewPixelTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         SearchAIModeSignInPromoViewPixelTest,
                         testing::ValuesIn(kTestParams),
                         &ParamToTestSuffix);

// Pixel tests for ComposeboxDriveSignInPromoView.
class ComposeboxDriveSignInPromoViewPixelTest
    : public AIModeSignInPromoViewPixelTestBase {
 public:
  ComposeboxDriveSignInPromoViewPixelTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{omnibox::
                                  kComposeboxDriveContextMenuOptionSigninPromo},
        /*disabled_features=*/{});
  }

  void ShowUi(const std::string& name) override {
    if (name == "SignInPending") {
      SignInWithAccount();
      identity_test_env()->SetInvalidRefreshTokenForPrimaryAccount();
    }
    ShowPromoView(std::make_unique<ComposeboxDriveSignInPromoView>(
        GetAvatarBubbleAnchor(),
        browser()->GetActiveTabInterface()->GetContents(),
        /*controller=*/nullptr));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(ComposeboxDriveSignInPromoViewPixelTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(ComposeboxDriveSignInPromoViewPixelTest,
                       InvokeUi_SignInPending) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         ComposeboxDriveSignInPromoViewPixelTest,
                         testing::ValuesIn(kTestParams),
                         &ParamToTestSuffix);

}  // namespace
