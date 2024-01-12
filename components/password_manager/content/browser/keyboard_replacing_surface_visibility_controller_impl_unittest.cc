// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/keyboard_replacing_surface_visibility_controller_impl.h"
#include "base/test/scoped_feature_list.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/text_input_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/text_input_type.h"

namespace password_manager {

class KeyboardReplacingSurfaceVisibilityControllerImplTest
    : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    password_mananger_driver_ =
        std::make_unique<password_manager::ContentPasswordManagerDriver>(
            main_rfh(), &client_);
  }

  base::WeakPtr<password_manager::ContentPasswordManagerDriver>
  password_mananger_driver() {
    return password_mananger_driver_->AsWeakPtrImpl();
  }

  void ResetPasswordManagerDriver() { password_mananger_driver_.reset(); }

  // Checks whether the keyboard is displayed.
  void VerifyKeyboardSuppression(bool expected_suppressed) {
    ui::mojom::TextInputStatePtr initial_state =
        ui::mojom::TextInputState::New();
    initial_state->type = ui::TEXT_INPUT_TYPE_PASSWORD;
    // Simulate the TextInputStateChanged call, which triggers the keyboard.
    SendTextInputStateChangedToWidget(rvh()->GetWidget(),
                                      std::move(initial_state));

    EXPECT_EQ(content::GetTextInputStateFromWebContents(web_contents())
                  ->always_hide_ime,
              expected_suppressed);
  }

 private:
  std::unique_ptr<password_manager::ContentPasswordManagerDriver>
      password_mananger_driver_;
  password_manager::StubPasswordManagerClient client_;
};

TEST_F(KeyboardReplacingSurfaceVisibilityControllerImplTest, Visibility) {
  base::test::ScopedFeatureList enable_feature(
      features::kPasswordSuggestionBottomSheetV2);
  KeyboardReplacingSurfaceVisibilityControllerImpl controller;

  EXPECT_TRUE(controller.CanBeShown());

  controller.SetVisible(password_mananger_driver());
  VerifyKeyboardSuppression(true);

  EXPECT_TRUE(controller.IsVisible());
}

TEST_F(KeyboardReplacingSurfaceVisibilityControllerImplTest, Reset) {
  base::test::ScopedFeatureList enable_feature(
      features::kPasswordSuggestionBottomSheetV2);
  KeyboardReplacingSurfaceVisibilityControllerImpl controller;

  EXPECT_TRUE(controller.CanBeShown());

  controller.SetVisible(password_mananger_driver());

  EXPECT_FALSE(controller.CanBeShown());

  controller.Reset();
  VerifyKeyboardSuppression(false);

  EXPECT_TRUE(controller.CanBeShown());
}

TEST_F(KeyboardReplacingSurfaceVisibilityControllerImplTest,
       ResetAfterRemovingFrameDriver) {
  base::test::ScopedFeatureList enable_feature(
      features::kPasswordSuggestionBottomSheetV2);
  KeyboardReplacingSurfaceVisibilityControllerImpl controller;

  controller.SetVisible(password_mananger_driver());

  // Simulates the situation, when the frame is being removed from the web page
  // before resetting the controller.
  ResetPasswordManagerDriver();
  // Should not crash here.
  controller.Reset();

  EXPECT_TRUE(controller.CanBeShown());
}

}  // namespace password_manager
