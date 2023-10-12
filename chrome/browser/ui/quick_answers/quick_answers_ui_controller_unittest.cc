// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_view.h"

#include "chrome/browser/ui/quick_answers/quick_answers_controller_impl.h"
#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"
#include "chrome/browser/ui/quick_answers/test/chrome_quick_answers_test_base.h"

namespace {

constexpr gfx::Rect kDefaultAnchorBoundsInScreen =
    gfx::Rect(gfx::Point(500, 250), gfx::Size(80, 140));

}  // namespace

class QuickAnswersUiControllerTest : public ChromeQuickAnswersTestBase {
 protected:
  QuickAnswersUiControllerTest() = default;
  QuickAnswersUiControllerTest(const QuickAnswersUiControllerTest&) = delete;
  QuickAnswersUiControllerTest& operator=(const QuickAnswersUiControllerTest&) =
      delete;
  ~QuickAnswersUiControllerTest() override = default;

  // ChromeQuickAnswersTestBase:
  void SetUp() override {
    ChromeQuickAnswersTestBase::SetUp();

    ui_controller_ =
        static_cast<QuickAnswersControllerImpl*>(QuickAnswersController::Get())
            ->quick_answers_ui_controller();
  }

  // Currently instantiated QuickAnswersView instance.
  QuickAnswersUiController* ui_controller() { return ui_controller_; }

 private:
  raw_ptr<QuickAnswersUiController, DanglingUntriaged | ExperimentalAsh>
      ui_controller_ = nullptr;
};

TEST_F(QuickAnswersUiControllerTest, TearDownWhileQuickAnswersViewShowing) {
  EXPECT_FALSE(ui_controller()->IsShowingQuickAnswersView());

  // Set up a companion menu before creating the QuickAnswersView.
  CreateAndShowBasicMenu();

  ui_controller()->CreateQuickAnswersView(GetProfile(),
                                          kDefaultAnchorBoundsInScreen,
                                          "default_title", "default_query",
                                          /*is_internal=*/false);
  EXPECT_TRUE(ui_controller()->IsShowingQuickAnswersView());
}

TEST_F(QuickAnswersUiControllerTest, TearDownWhileConsentViewShowing) {
  EXPECT_FALSE(ui_controller()->IsShowingUserConsentView());

  // Set up a companion menu before creating the QuickAnswersView.
  CreateAndShowBasicMenu();

  ui_controller()->CreateUserConsentView(kDefaultAnchorBoundsInScreen,
                                         std::u16string(), std::u16string());
  EXPECT_TRUE(ui_controller()->IsShowingUserConsentView());
}
