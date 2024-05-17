// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_controller.h"

#include <memory>

#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_disclaimer_view.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_opt_in_card.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "ui/lottie/resource.h"
#include "ui/views/view_utils.h"

namespace chromeos {

using MagicBoostControllerTest = ChromeViewsTestBase;

TEST_F(MagicBoostControllerTest, ShowDisclaimerUi) {
// Sets the default functions for the test to create image with the lottie
// resource id. Otherwise there's no `g_parse_lottie_as_still_image_` set in the
// `ResourceBundle`.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ui::ResourceBundle::SetLottieParsingFunctions(
      &lottie::ParseLottieAsStillImage, &lottie::ParseLottieAsThemedStillImage);
#endif

  auto* controller = MagicBoostController::Get();
  EXPECT_FALSE(controller->disclaimer_widget_for_test());

  controller->ShowDisclaimerUi();

  EXPECT_TRUE(controller->disclaimer_widget_for_test());
  EXPECT_TRUE(controller->disclaimer_widget_for_test()->IsVisible());
  EXPECT_TRUE(views::IsViewClass<MagicBoostDisclaimerView>(
      controller->disclaimer_widget_for_test()->GetContentsView()));
}

TEST_F(MagicBoostControllerTest, OptInUi) {
  auto* controller = MagicBoostController::Get();

  // Initially the opt-in widget is not visible.
  EXPECT_FALSE(controller->opt_in_widget_for_test());

  // Show the opt-in widget and test that the proper views are set.
  controller->ShowOptInUi(/*anchor_bounds=*/gfx::Rect());
  auto* opt_in_widget = controller->opt_in_widget_for_test();
  ASSERT_TRUE(opt_in_widget);
  EXPECT_TRUE(opt_in_widget->IsVisible());
  EXPECT_TRUE(views::IsViewClass<MagicBoostOptInCard>(
      opt_in_widget->GetContentsView()));

  // Test that the opt-in widget is closed on `CloseOptInUI`.
  controller->CloseOptInUi();
  EXPECT_FALSE(controller->opt_in_widget_for_test());
}

}  // namespace chromeos
