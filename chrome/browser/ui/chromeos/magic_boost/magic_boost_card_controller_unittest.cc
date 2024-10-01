// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_card_controller.h"

#include <memory>

#include "ash/system/mahi/test/mock_mahi_media_app_events_proxy.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/magic_boost/magic_boost_state_ash.h"
#include "chrome/browser/ash/magic_boost/mock_editor_panel_manager.h"
#include "chrome/browser/ash/magic_boost/mock_magic_boost_state.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_metrics.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_opt_in_card.h"
#include "chrome/browser/ui/chromeos/magic_boost/test/mock_magic_boost_controller_crosapi.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/components/mahi/public/cpp/mahi_media_app_events_proxy.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/lottie/resource.h"
#include "ui/views/view_utils.h"

namespace chromeos {

using OptInFeatures = crosapi::mojom::MagicBoostController::OptInFeatures;

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
    magic_boost_state_->set_editor_panel_manager_for_test(
        &mock_editor_manager_);
  }

  void TearDown() override {
    magic_boost_state_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Providing a mock MahiMediaAppEvnetsProxy to satisfy
  // MagicBoostCardController.
  testing::NiceMock<::ash::MockMahiMediaAppEventsProxy>
      mock_mahi_media_app_events_proxy_;
  chromeos::ScopedMahiMediaAppEventsProxySetter
      scoped_mahi_media_app_events_proxy_{&mock_mahi_media_app_events_proxy_};

  testing::NiceMock<ash::MockEditorPanelManager> mock_editor_manager_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

TEST_F(MagicBoostCardControllerTest, PdfContextMenuConsentStatus) {
  ON_CALL(*magic_boost_state_, ShouldIncludeOrcaInOptIn)
      .WillByDefault([](base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });

  auto consent_statuses = {chromeos::HMRConsentStatus::kUnset,
                           chromeos::HMRConsentStatus::kApproved,
                           chromeos::HMRConsentStatus::kDeclined,
                           chromeos::HMRConsentStatus::kPendingDisclaimer};

  for (auto consent_status : consent_statuses) {
    magic_boost_state_->AsyncWriteConsentStatus(consent_status);

    // The opt-in widget should show when HMR card is not shown.
    card_controller_.OnPdfContextMenuShown(/*anchor=*/gfx::Rect());

    auto* opt_in_widget = card_controller_.opt_in_widget_for_test();
    if (magic_boost_state_->ShouldShowHmrCard()) {
      ASSERT_FALSE(opt_in_widget);
    } else {
      ASSERT_TRUE(opt_in_widget);
      EXPECT_TRUE(opt_in_widget->IsVisible());
    }

    // Test that the opt-in widget is closed on `OnPdfContextMenuHide`.
    card_controller_.OnPdfContextMenuHide();
    EXPECT_FALSE(card_controller_.opt_in_widget_for_test());
  }
}

TEST_F(MagicBoostCardControllerTest, PdfContextMenuIncludeOrca) {
  magic_boost_state_->AsyncWriteConsentStatus(
      chromeos::HMRConsentStatus::kUnset);

  ON_CALL(*magic_boost_state_, ShouldIncludeOrcaInOptIn)
      .WillByDefault([](base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });

  // The opt-in features should include Orca if needed.
  card_controller_.OnPdfContextMenuShown(/*anchor=*/gfx::Rect());
  EXPECT_EQ(OptInFeatures::kOrcaAndHmr, card_controller_.GetOptInFeatures());
}

TEST_F(MagicBoostCardControllerTest, PdfContextMenuNotIncludeOrca) {
  magic_boost_state_->AsyncWriteConsentStatus(
      chromeos::HMRConsentStatus::kUnset);

  ON_CALL(*magic_boost_state_, ShouldIncludeOrcaInOptIn)
      .WillByDefault([](base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(false);
      });

  // The opt-in features should not include Orca if not needed.
  card_controller_.OnPdfContextMenuShown(/*anchor=*/gfx::Rect());
  EXPECT_EQ(OptInFeatures::kHmrOnly, card_controller_.GetOptInFeatures());
}

}  // namespace chromeos
