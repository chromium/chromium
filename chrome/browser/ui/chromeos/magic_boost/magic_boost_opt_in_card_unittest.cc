// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_opt_in_card.h"

#include <memory>

#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_constants.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_controller.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_disclaimer_view.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "ui/events/test/event_generator.h"
#include "ui/lottie/resource.h"
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
  MagicBoostOptInCardTest() {
// Sets the default functions for the test to create image with the lottie
// resource id. Otherwise there's no `g_parse_lottie_as_still_image_` set in the
// `ResourceBundle`.
#if BUILDFLAG(IS_CHROMEOS_ASH)
    ui::ResourceBundle::SetLottieParsingFunctions(
        &lottie::ParseLottieAsStillImage,
        &lottie::ParseLottieAsThemedStillImage);
#endif
  }
};

TEST_F(MagicBoostOptInCardTest, ButtonActions) {
  auto* controller = MagicBoostController::Get();

  // Show the opt-in UI card.
  controller->ShowOptInUi(/*anchor_bounds=*/gfx::Rect());
  auto* opt_in_widget = controller->opt_in_widget_for_test();
  ASSERT_TRUE(opt_in_widget);

  // Test that pressing the primary button closes the card and shows the
  // disclaimer UI.
  auto* primary_button = GetPrimaryButton(opt_in_widget);
  ASSERT_TRUE(primary_button);
  LeftClickOn(primary_button);
  EXPECT_FALSE(controller->opt_in_widget_for_test());
  EXPECT_TRUE(controller->disclaimer_widget_for_test());

  // Close the disclaimer UI directly without pressing its buttons, which won't
  // set the `CanShowOptInUi` pref to false. This may happen during shutdown.
  controller->CloseDisclaimerUi();

  // Attempt re-showing the opt-in UI card. It should show since
  // `CanShowOptInUI` pref is still true.
  controller->ShowOptInUi(/*anchor_bounds=*/gfx::Rect());
  ASSERT_TRUE(controller->opt_in_widget_for_test());

  // Test that pressing the secondary button closes the card and sets the pref
  // so the card won't be able to show again.
  auto* secondary_button = GetSecondaryButton(opt_in_widget);
  ASSERT_TRUE(secondary_button);
  LeftClickOn(secondary_button);
  ASSERT_FALSE(controller->opt_in_widget_for_test());

  // Attempt re-showing the opt-in UI card. It should not show again since the
  // user declined before.
  // TODO(b/341158134): Implement can show opt-in UI pref to test that the card
  // won't show again.
}

}  // namespace chromeos
