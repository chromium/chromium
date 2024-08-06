// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_opt_in_card.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/magic_boost/mock_magic_boost_state.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_card_controller.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_constants.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_metrics.h"
#include "chrome/browser/ui/chromeos/magic_boost/test/mock_magic_boost_controller_crosapi.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace chromeos {

namespace {

views::View* GetPrimaryButton(views::Widget* opt_in_widget) {
  return opt_in_widget->GetContentsView()->GetViewByID(
      magic_boost::ViewId::OptInCardPrimaryButton);
}

views::View* GetSecondaryButton(views::Widget* opt_in_widget) {
  return opt_in_widget->GetContentsView()->GetViewByID(
      magic_boost::ViewId::OptInCardSecondaryButton);
}

const std::u16string& GetTitleText(views::Widget* opt_in_widget) {
  return views::AsViewClass<views::Label>(
             opt_in_widget->GetContentsView()->GetViewByID(
                 magic_boost::ViewId::OptInCardTitleLabel))
      ->GetText();
}

const std::u16string& GetBodyText(views::Widget* opt_in_widget) {
  return views::AsViewClass<views::Label>(
             opt_in_widget->GetContentsView()->GetViewByID(
                 magic_boost::ViewId::OptInCardBodyLabel))
      ->GetText();
}

void LeftClickOn(views::View* view) {
  auto* widget = view->GetWidget();
  ASSERT_TRUE(widget);
  ui::test::EventGenerator event_generator(GetRootWindow(widget),
                                           widget->GetNativeWindow());
  event_generator.MoveMouseTo(view->GetBoundsInScreen().CenterPoint());
  event_generator.ClickLeftButton();
}

}  // namespace

class MagicBoostOptInCardTest : public ChromeViewsTestBase {
 public:
  MagicBoostOptInCardTest() = default;

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

    // Instantiates `MockMagicBoostState` (the real one is created in
    // ChromeBrowserMainExtraPartsAsh::PreProfileInit() which is not called in
    // the unit tests).
    mock_magic_boost_state_ = std::make_unique<ash::MockMagicBoostState>();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

    card_controller_.SetOptInFeature(OptInFeatures::kHmrOnly);
  }

  void TearDown() override {
    mock_magic_boost_state_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  MagicBoostCardController card_controller_;
  testing::NiceMock<MockMagicBoostControllerCrosapi> crosapi_controller_;
  mojo::Receiver<crosapi::mojom::MagicBoostController> receiver_{
      &crosapi_controller_};
  std::unique_ptr<ash::MockMagicBoostState> mock_magic_boost_state_;
};

TEST_F(MagicBoostOptInCardTest, PrimaryButtonActions) {
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  std::string histogram_name = magic_boost::kMagicBoostOptInCardHistogram;
  histogram_tester->ExpectTotalCount(histogram_name + "Total", 0);
  histogram_tester->ExpectTotalCount(histogram_name + "HmrOnly", 0);

  // Show the opt-in UI card.
  EXPECT_CALL(crosapi_controller_, CloseDisclaimerUi);
  card_controller_.ShowOptInUi(/*anchor_view_bounds=*/gfx::Rect());
  auto* opt_in_widget = card_controller_.opt_in_widget_for_test();
  ASSERT_TRUE(opt_in_widget);

  // Test that pressing the primary button closes the card and shows the
  // disclaimer UI.
  auto* primary_button = GetPrimaryButton(opt_in_widget);
  ASSERT_TRUE(primary_button);

  // Records the `kShowCard` metrics.
  histogram_tester->ExpectTotalCount(histogram_name + "Total", 1);
  histogram_tester->ExpectTotalCount(histogram_name + "HmrOnly", 1);
  histogram_tester->ExpectBucketCount(
      histogram_name + "HmrOnly", magic_boost::OptInCardAction::kShowCard, 1);
  histogram_tester->ExpectBucketCount(
      histogram_name + "Total", magic_boost::OptInCardAction::kShowCard, 1);
  histogram_tester->ExpectBucketCount(
      histogram_name + "HmrOnly",
      magic_boost::OptInCardAction::kAcceptButtonPressed, 0);

  EXPECT_EQ(chromeos::HMRConsentStatus::kUnset,
            mock_magic_boost_state_->hmr_consent_status());

  EXPECT_CALL(crosapi_controller_, ShowDisclaimerUi);

  LeftClickOn(primary_button);
  EXPECT_FALSE(card_controller_.opt_in_widget_for_test());

  EXPECT_EQ(chromeos::HMRConsentStatus::kPendingDisclaimer,
            mock_magic_boost_state_->hmr_consent_status());

  // Records the `kAcceptButtonPressed` metrics.
  histogram_tester->ExpectTotalCount(histogram_name + "Total", 2);
  histogram_tester->ExpectBucketCount(
      histogram_name + "HmrOnly", magic_boost::OptInCardAction::kShowCard, 1);
  histogram_tester->ExpectBucketCount(
      histogram_name + "HmrOnly",
      magic_boost::OptInCardAction::kAcceptButtonPressed, 1);
  histogram_tester->ExpectBucketCount(
      histogram_name + "Total",
      magic_boost::OptInCardAction::kAcceptButtonPressed, 1);
}

TEST_F(MagicBoostOptInCardTest, StringVariations_NoOrca) {
  card_controller_.SetOptInFeature(OptInFeatures::kHmrOnly);

  // Show the opt-in UI card.
  EXPECT_CALL(crosapi_controller_, CloseDisclaimerUi);
  card_controller_.ShowOptInUi(/*anchor_view_bounds=*/gfx::Rect());
  auto* opt_in_widget = card_controller_.opt_in_widget_for_test();
  ASSERT_TRUE(opt_in_widget);

  // Test that the appropriate strings were used.
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_MAGIC_BOOST_OPT_IN_CARD_NO_ORCA_TITLE),
      GetTitleText(opt_in_widget));
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_MAGIC_BOOST_OPT_IN_CARD_NO_ORCA_BODY),
      GetBodyText(opt_in_widget));
}

TEST_F(MagicBoostOptInCardTest, StringVariations_OrcaIncluded) {
  card_controller_.SetOptInFeature(OptInFeatures::kOrcaAndHmr);

  // Show the opt-in UI card.
  EXPECT_CALL(crosapi_controller_, CloseDisclaimerUi);
  card_controller_.ShowOptInUi(/*anchor_view_bounds=*/gfx::Rect());
  auto* opt_in_widget = card_controller_.opt_in_widget_for_test();
  ASSERT_TRUE(opt_in_widget);

  // Test that the appropriate strings were used.
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_MAGIC_BOOST_OPT_IN_CARD_TITLE),
            GetTitleText(opt_in_widget));
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_MAGIC_BOOST_OPT_IN_CARD_BODY),
            GetBodyText(opt_in_widget));
}

TEST_F(MagicBoostOptInCardTest, SecondaryButtonActions_NoOrca) {
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  std::string histogram_name = magic_boost::kMagicBoostOptInCardHistogram;
  histogram_tester->ExpectTotalCount(histogram_name + "Total", 0);
  histogram_tester->ExpectTotalCount(histogram_name + "HmrOnly", 0);

  card_controller_.SetOptInFeature(OptInFeatures::kHmrOnly);

  // Show the opt-in UI card.
  EXPECT_CALL(crosapi_controller_, CloseDisclaimerUi);
  card_controller_.ShowOptInUi(/*anchor_view_bounds=*/gfx::Rect());
  auto* opt_in_widget = card_controller_.opt_in_widget_for_test();
  ASSERT_TRUE(opt_in_widget);

  // Records the `kShowCard` metrics.
  histogram_tester->ExpectTotalCount(histogram_name + "Total", 1);
  histogram_tester->ExpectTotalCount(histogram_name + "HmrOnly", 1);
  histogram_tester->ExpectBucketCount(
      histogram_name + "HmrOnly", magic_boost::OptInCardAction::kShowCard, 1);
  histogram_tester->ExpectBucketCount(
      histogram_name + "Total", magic_boost::OptInCardAction::kShowCard, 1);
  histogram_tester->ExpectBucketCount(
      histogram_name + "HmrOnly",
      magic_boost::OptInCardAction::kAcceptButtonPressed, 0);

  // Test that pressing the secondary button closes the card and sets the pref
  // using `MagicBoostState`. Orca functions shouldn't be called.
  EXPECT_CALL(*mock_magic_boost_state_, EnableOrcaFeature).Times(0);
  EXPECT_CALL(*mock_magic_boost_state_, DisableOrcaFeature).Times(0);

  auto* secondary_button = GetSecondaryButton(opt_in_widget);
  ASSERT_TRUE(secondary_button);
  LeftClickOn(secondary_button);

  EXPECT_EQ(chromeos::HMRConsentStatus::kDeclined,
            mock_magic_boost_state_->hmr_consent_status());
  EXPECT_FALSE(mock_magic_boost_state_->hmr_enabled().value());

  EXPECT_FALSE(card_controller_.opt_in_widget_for_test());

  // Records the `kDeclineButtonPressed` metrics.
  histogram_tester->ExpectTotalCount(histogram_name + "Total", 2);
  histogram_tester->ExpectBucketCount(
      histogram_name + "HmrOnly", magic_boost::OptInCardAction::kShowCard, 1);
  histogram_tester->ExpectBucketCount(
      histogram_name + "HmrOnly",
      magic_boost::OptInCardAction::kDeclineButtonPressed, 1);
  histogram_tester->ExpectBucketCount(
      histogram_name + "Total",
      magic_boost::OptInCardAction::kDeclineButtonPressed, 1);
}

TEST_F(MagicBoostOptInCardTest, SecondaryButtonActions_IncludeOrca) {
  card_controller_.SetOptInFeature(OptInFeatures::kOrcaAndHmr);

  // Show the opt-in UI card.
  EXPECT_CALL(crosapi_controller_, CloseDisclaimerUi);
  card_controller_.ShowOptInUi(/*anchor_view_bounds=*/gfx::Rect());
  auto* opt_in_widget = card_controller_.opt_in_widget_for_test();
  ASSERT_TRUE(opt_in_widget);

  // Test that pressing the secondary button closes the card and sets the pref
  // using `MagicBoostState`. Disable Orca function should be called.
  EXPECT_CALL(*mock_magic_boost_state_, EnableOrcaFeature).Times(0);
  EXPECT_CALL(*mock_magic_boost_state_, DisableOrcaFeature);

  auto* secondary_button = GetSecondaryButton(opt_in_widget);
  ASSERT_TRUE(secondary_button);
  LeftClickOn(secondary_button);

  EXPECT_EQ(chromeos::HMRConsentStatus::kDeclined,
            mock_magic_boost_state_->hmr_consent_status());
  EXPECT_FALSE(mock_magic_boost_state_->hmr_enabled().value());

  EXPECT_FALSE(card_controller_.opt_in_widget_for_test());
}

}  // namespace chromeos
