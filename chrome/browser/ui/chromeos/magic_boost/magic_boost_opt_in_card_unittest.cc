// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_opt_in_card.h"

#include <memory>

#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_card_controller.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_constants.h"
#include "chrome/browser/ui/chromeos/magic_boost/test/mock_magic_boost_controller_crosapi.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/magic_boost/magic_boost_state_ash.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

    // Instantiates `MagicBoostStateAsh` (the real one is created in
    // ChromeBrowserMainExtraPartsAsh::PreProfileInit() which is not called in
    // the unit tests).
    magic_boost_state_ = std::make_unique<ash::MagicBoostStateAsh>();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }

  void TearDown() override {
    magic_boost_state_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  MagicBoostCardController card_controller_;
  testing::StrictMock<MockMagicBoostControllerCrosapi> crosapi_controller_;
  mojo::Receiver<crosapi::mojom::MagicBoostController> receiver_{
      &crosapi_controller_};
  std::unique_ptr<MagicBoostState> magic_boost_state_;
};

TEST_F(MagicBoostOptInCardTest, PrimaryButtonActions) {
  // Show the opt-in UI card.
  EXPECT_CALL(crosapi_controller_, CloseDisclaimerUi);
  card_controller_.ShowOptInUi(/*anchor_view_bounds=*/gfx::Rect());
  auto* opt_in_widget = card_controller_.opt_in_widget_for_test();
  ASSERT_TRUE(opt_in_widget);

  // Test that pressing the primary button closes the card and shows the
  // disclaimer UI.
  auto* primary_button = GetPrimaryButton(opt_in_widget);
  ASSERT_TRUE(primary_button);

  EXPECT_CALL(crosapi_controller_, ShowDisclaimerUi);

  LeftClickOn(primary_button);
  EXPECT_FALSE(card_controller_.opt_in_widget_for_test());
}

TEST_F(MagicBoostOptInCardTest, SecondaryButtonActions) {
  // Show the opt-in UI card.
  EXPECT_CALL(crosapi_controller_, CloseDisclaimerUi);
  card_controller_.ShowOptInUi(/*anchor_view_bounds=*/gfx::Rect());
  auto* opt_in_widget = card_controller_.opt_in_widget_for_test();
  ASSERT_TRUE(opt_in_widget);

  // Test that pressing the secondary button closes the card and sets the pref
  // so the card won't be able to show again.
  auto* secondary_button = GetSecondaryButton(opt_in_widget);
  ASSERT_TRUE(secondary_button);
  LeftClickOn(secondary_button);
  ASSERT_FALSE(card_controller_.opt_in_widget_for_test());

  // Attempt re-showing the opt-in UI card. It should not show again since the
  // user declined before.
  // TODO(b/341158134): Implement can show opt-in UI pref to test that the card
  // won't show again.
}

}  // namespace chromeos
