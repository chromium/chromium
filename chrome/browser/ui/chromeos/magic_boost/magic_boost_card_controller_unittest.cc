// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_card_controller.h"

#include <memory>

#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_disclaimer_view.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_opt_in_card.h"
#include "chrome/browser/ui/chromeos/magic_boost/test/mock_magic_boost_controller_crosapi.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/lottie/resource.h"
#include "ui/views/view_utils.h"

namespace chromeos {

class MagicBoostCardControllerTest : public ChromeViewsTestBase {
 public:
  MagicBoostCardControllerTest() {
// Sets the default functions for the test to create image with the lottie
// resource id. Otherwise there's no `g_parse_lottie_as_still_image_` set in the
// `ResourceBundle`.
#if BUILDFLAG(IS_CHROMEOS_ASH)
    ui::ResourceBundle::SetLottieParsingFunctions(
        &lottie::ParseLottieAsStillImage,
        &lottie::ParseLottieAsThemedStillImage);
#endif
  }

  // ChromeViewsTestBase:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    // Replace the production `MagicBoostController` with a mock for testing
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    MagicBoostCardController::Get()->BindMagicBoostControllerCrosapiForTesting(
        receiver_.BindNewPipeAndPassRemote());
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
    MagicBoostCardController::Get()->SetMagicBoostControllerCrosapiForTesting(
        &crosapi_controller_);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }

 protected:
  testing::StrictMock<MockMagicBoostControllerCrosapi> crosapi_controller_;
  mojo::Receiver<crosapi::mojom::MagicBoostController> receiver_{
      &crosapi_controller_};
};

TEST_F(MagicBoostCardControllerTest, DisclaimerUi) {
  auto* controller = MagicBoostCardController::Get();

  int expected_display_id = 2;
  auto expected_action =
      crosapi::mojom::MagicBoostController::TransitionAction::kShowEditorPanel;

  EXPECT_CALL(crosapi_controller_, ShowDisclaimerUi)
      .WillOnce(
          [expected_display_id, expected_action](
              int display_id,
              crosapi::mojom::MagicBoostController::TransitionAction action) {
            EXPECT_EQ(expected_display_id, display_id);
            EXPECT_EQ(expected_action, action);
          });

  controller->ShowDisclaimerUi(expected_display_id, expected_action);
}

TEST_F(MagicBoostCardControllerTest, OptInUi) {
  auto* controller = MagicBoostCardController::Get();

  // Initially the opt-in widget is not visible.
  EXPECT_FALSE(controller->opt_in_widget_for_test());

  // Show the opt-in widget and test that the proper views are set.
  controller->ShowOptInUi(/*anchor_view_bounds=*/gfx::Rect());
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
