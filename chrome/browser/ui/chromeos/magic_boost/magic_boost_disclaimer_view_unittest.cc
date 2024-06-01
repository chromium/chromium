// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_disclaimer_view.h"

#include <memory>

#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_card_controller.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_constants.h"
#include "chrome/browser/ui/chromeos/magic_boost/test/mock_magic_boost_card_controller.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"
#include "ui/lottie/resource.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace chromeos {
namespace {

views::View* GetAcceptButton(views::Widget* disclaimer_view_widget) {
  return disclaimer_view_widget->GetContentsView()->GetViewByID(
      magic_boost::ViewId::DisclaimerViewAcceptButton);
}

views::View* GetDeclineButton(views::Widget* disclaimer_view_widget) {
  return disclaimer_view_widget->GetContentsView()->GetViewByID(
      magic_boost::ViewId::DisclaimerViewDeclineButton);
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

class MagicBoostDisclaimerViewTest : public ChromeViewsTestBase {
 public:
  MagicBoostDisclaimerViewTest() {
// Sets the default functions for the test to create image with the lottie
// resource id. Otherwise there's no `g_parse_lottie_as_still_image_` set in the
// `ResourceBundle`.
#if BUILDFLAG(IS_CHROMEOS_ASH)
    ui::ResourceBundle::SetLottieParsingFunctions(
        &lottie::ParseLottieAsStillImage,
        &lottie::ParseLottieAsThemedStillImage);
#endif

    scoped_magic_boost_card_controller_ =
        std::make_unique<ScopedMagicBoostCardControllerForTesting>(
            &mock_magic_boost_card_controller_);
  }

  MockMagicBoostCardController& mock_magic_boost_card_controller() {
    return mock_magic_boost_card_controller_;
  }

 private:
  testing::NiceMock<MockMagicBoostCardController>
      mock_magic_boost_card_controller_;
  std::unique_ptr<ScopedMagicBoostCardControllerForTesting>
      scoped_magic_boost_card_controller_;
};

TEST_F(MagicBoostDisclaimerViewTest, ButtonActions) {
  auto* controller = &(mock_magic_boost_card_controller());

  // Show the disclaimer view.
  controller->ShowDisclaimerUi();
  auto* diclaimer_view_widget = controller->disclaimer_widget_for_test();
  ASSERT_TRUE(diclaimer_view_widget);

  // Pressing the accept button without `is_orca_included` should call
  // `SetQuickAnswersAndMahiFeaturesState(true)`.
  EXPECT_FALSE(controller->is_orca_included());
  EXPECT_CALL(mock_magic_boost_card_controller(),
              SetQuickAnswersAndMahiFeaturesState(true));
  auto* accept_button = GetAcceptButton(diclaimer_view_widget);
  ASSERT_TRUE(accept_button);
  LeftClickOn(accept_button);

  // Pressing the accept button with `is_orca_included` should call
  // `SetQuickAnswersAndMahiFeaturesState(true)`.
  controller->SetIsOrcaIncludedForTest(true);
  EXPECT_TRUE(controller->is_orca_included());

  EXPECT_CALL(mock_magic_boost_card_controller(), SetAllFeaturesState(true));
  controller->ShowDisclaimerUi();
  accept_button = GetAcceptButton(diclaimer_view_widget);
  ASSERT_TRUE(accept_button);
  LeftClickOn(accept_button);

  // Pressing the decline button without `is_orca_included` should call
  // `SetQuickAnswersAndMahiFeaturesState(false)`.
  controller->SetIsOrcaIncludedForTest(false);
  EXPECT_FALSE(controller->is_orca_included());
  EXPECT_CALL(mock_magic_boost_card_controller(),
              SetQuickAnswersAndMahiFeaturesState(false));
  controller->ShowDisclaimerUi();
  auto* decline_button = GetDeclineButton(diclaimer_view_widget);
  ASSERT_TRUE(decline_button);
  LeftClickOn(decline_button);

  // Pressing the decline button with `is_orca_included` should call
  // `SetQuickAnswersAndMahiFeaturesState(false)`.
  controller->SetIsOrcaIncludedForTest(true);
  EXPECT_TRUE(controller->is_orca_included());
  EXPECT_CALL(mock_magic_boost_card_controller(), SetAllFeaturesState(false));
  controller->ShowDisclaimerUi();
  decline_button = GetDeclineButton(diclaimer_view_widget);
  ASSERT_TRUE(decline_button);
  LeftClickOn(decline_button);
}
}  // namespace chromeos
