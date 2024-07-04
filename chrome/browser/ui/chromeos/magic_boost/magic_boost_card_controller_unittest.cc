// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_card_controller.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/magic_boost/magic_boost_state_ash.h"
#include "chrome/browser/ash/magic_boost/mock_magic_boost_state.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_metrics.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_opt_in_card.h"
#include "chrome/browser/ui/chromeos/magic_boost/test/mock_magic_boost_controller_crosapi.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
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
    card_controller_.BindMagicBoostControllerCrosapiForTesting(
        receiver_.BindNewPipeAndPassRemote());
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
    card_controller_.SetMagicBoostControllerCrosapiForTesting(
        &crosapi_controller_);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
    card_controller_.SetOptInFeature(OptInFeatures::kHmrOnly);

    magic_boost_state_ = std::make_unique<ash::MockMagicBoostState>();
  }

  void TearDown() override {
    magic_boost_state_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  std::unique_ptr<ash::MockMagicBoostState> magic_boost_state_;
  MagicBoostCardController card_controller_;
  testing::NiceMock<MockMagicBoostControllerCrosapi> crosapi_controller_;
  mojo::Receiver<crosapi::mojom::MagicBoostController> receiver_{
      &crosapi_controller_};
};

// Tests the behavior of the controller when `OnTextAvailable()` and
// `OnDismiss()` are triggered.
TEST_F(MagicBoostCardControllerTest, OnTextAvailableAndDismiss) {
  // Initially the opt-in widget is not visible.
  EXPECT_FALSE(card_controller_.opt_in_widget_for_test());

  // Show the opt-in widget and test that the proper views are set.
  EXPECT_CALL(crosapi_controller_, CloseDisclaimerUi);
  card_controller_.OnTextAvailable(/*anchor_bounds=*/gfx::Rect(),
                                   /*selected_text=*/"",
                                   /*surrounding_text=*/"");
  auto* opt_in_widget = card_controller_.opt_in_widget_for_test();
  ASSERT_TRUE(opt_in_widget);
  EXPECT_TRUE(opt_in_widget->IsVisible());
  EXPECT_TRUE(views::IsViewClass<MagicBoostOptInCard>(
      opt_in_widget->GetContentsView()));

  // Test that the opt-in widget is closed on `CloseOptInUI`.
  card_controller_.OnDismiss(/*is_other_command_executed=*/false);
  EXPECT_FALSE(card_controller_.opt_in_widget_for_test());
}

// Tests the behavior of the controller when `OnAnchorBoundsChanged()` is
// triggered.
TEST_F(MagicBoostCardControllerTest, BoundsChanged) {
  EXPECT_FALSE(card_controller_.opt_in_widget_for_test());

  EXPECT_CALL(crosapi_controller_, CloseDisclaimerUi);
  gfx::Rect anchor_bounds = gfx::Rect(50, 50, 25, 100);
  card_controller_.OnTextAvailable(anchor_bounds,
                                   /*selected_text=*/"",
                                   /*surrounding_text=*/"");
  auto* widget = card_controller_.opt_in_widget_for_test();
  EXPECT_TRUE(widget);

  // Correct bounds should be set.
  EXPECT_EQ(editor_menu::GetEditorMenuBounds(anchor_bounds,
                                             widget->GetContentsView()),
            widget->GetRestoredBounds());

  anchor_bounds = gfx::Rect(0, 50, 55, 80);

  // Widget should change bounds accordingly.
  card_controller_.OnAnchorBoundsChanged(anchor_bounds);
  EXPECT_EQ(editor_menu::GetEditorMenuBounds(anchor_bounds,
                                             widget->GetContentsView()),
            widget->GetRestoredBounds());
}

TEST_F(MagicBoostCardControllerTest, DisclaimerUi) {
  int expected_display_id = 2;
  auto expected_action =
      crosapi::mojom::MagicBoostController::TransitionAction::kShowEditorPanel;
  auto expected_features =
      crosapi::mojom::MagicBoostController::OptInFeatures::kOrcaAndHmr;

  card_controller_.set_transition_action(expected_action);
  card_controller_.SetOptInFeature(expected_features);

  EXPECT_CALL(crosapi_controller_, ShowDisclaimerUi)
      .WillOnce(
          [expected_display_id, expected_action, expected_features](
              int display_id,
              crosapi::mojom::MagicBoostController::TransitionAction action,
              /*opt_in_features=*/
              crosapi::mojom::MagicBoostController::OptInFeatures features) {
            EXPECT_EQ(expected_display_id, display_id);
            EXPECT_EQ(expected_action, action);
            EXPECT_EQ(expected_features, features);
          });

  card_controller_.ShowDisclaimerUi(expected_display_id);
}

TEST_F(MagicBoostCardControllerTest, ShowOptInCardAgain) {
  // Shows the disclaimer view.
  EXPECT_CALL(crosapi_controller_, ShowDisclaimerUi);
  card_controller_.ShowDisclaimerUi(
      /*display_id=*/1);
  EXPECT_FALSE(card_controller_.opt_in_widget_for_test());

  // Shows the opt-in widget. It should close the discalimer view.
  EXPECT_CALL(crosapi_controller_, CloseDisclaimerUi);
  card_controller_.OnTextAvailable(/*anchor_bounds=*/gfx::Rect(),
                                   /*selected_text=*/"",
                                   /*surrounding_text=*/"");
  ASSERT_TRUE(card_controller_.opt_in_widget_for_test());
}

TEST_F(MagicBoostCardControllerTest, ShowOptInCardMetrics) {
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  std::string histogram_name = magic_boost::kMagicBoostOptInCardHistogram;
  histogram_tester->ExpectTotalCount(histogram_name + "Total", 0);
  histogram_tester->ExpectTotalCount(histogram_name + "HmrOnly", 0);
  histogram_tester->ExpectTotalCount(histogram_name + "OrcaAndHmr", 0);

  // Shows the opt-in widget from hmr feature.
  card_controller_.SetOptInFeature(OptInFeatures::kHmrOnly);
  card_controller_.ShowOptInUi(/*anchor_bounds=*/gfx::Rect());
  histogram_tester->ExpectTotalCount(histogram_name + "Total", 1);
  histogram_tester->ExpectTotalCount(histogram_name + "HmrOnly", 1);
  histogram_tester->ExpectBucketCount(
      histogram_name + "HmrOnly", magic_boost::OptInCardAction::kShowCard, 1);
  card_controller_.CloseOptInUi();

  // Shows the opt-in widget from both hmr and orca feature.
  card_controller_.SetOptInFeature(OptInFeatures::kOrcaAndHmr);
  card_controller_.ShowOptInUi(/*anchor_bounds=*/gfx::Rect());
  histogram_tester->ExpectTotalCount(histogram_name + "Total", 2);
  histogram_tester->ExpectTotalCount(histogram_name + "OrcaAndHmr", 1);
  card_controller_.CloseOptInUi();
}

}  // namespace chromeos
