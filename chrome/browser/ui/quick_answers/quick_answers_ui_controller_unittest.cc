// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_view.h"

#include "chrome/browser/ui/quick_answers/quick_answers_controller_impl.h"
#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"
#include "chrome/browser/ui/quick_answers/test/chrome_quick_answers_test_base.h"
#include "ui/events/test/test_event.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/button_test_api.h"

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

    ui_controller_ = GetQuickAnswersController()->quick_answers_ui_controller();
  }

  QuickAnswersControllerImpl* GetQuickAnswersController() {
    return static_cast<QuickAnswersControllerImpl*>(
        QuickAnswersController::Get());
  }

  void ShowConsentView() {
    GetQuickAnswersController()->ShowUserConsent(/*intent_type=*/u"",
                                                 /*intent_text=*/u"");
  }

  // Currently instantiated QuickAnswersView instance.
  QuickAnswersUiController* ui_controller() { return ui_controller_; }

 private:
  raw_ptr<QuickAnswersUiController, DanglingUntriaged> ui_controller_ = nullptr;
};

TEST_F(QuickAnswersUiControllerTest, TearDownWhileQuickAnswersViewShowing) {
  EXPECT_FALSE(ui_controller()->IsShowingQuickAnswersView());

  // Set up a companion menu before creating the QuickAnswersView.
  CreateAndShowBasicMenu();

  GetQuickAnswersController()->SetVisibility(
      QuickAnswersVisibility::kQuickAnswersVisible);
  ui_controller()->CreateQuickAnswersView(GetProfile(), "default_title",
                                          "default_query",
                                          /*is_internal=*/false);
  EXPECT_TRUE(ui_controller()->IsShowingQuickAnswersView());
}

TEST_F(QuickAnswersUiControllerTest, ShowAndHideConsentView) {
  EXPECT_FALSE(ui_controller()->IsShowingUserConsentView());

  // Set up a companion menu before creating the QuickAnswersView.
  CreateAndShowBasicMenu();
  GetQuickAnswersController()->OnContextMenuShown(/*profile=*/nullptr);

  auto* quick_answers_controller = GetQuickAnswersController();

  ShowConsentView();

  EXPECT_TRUE(ui_controller()->IsShowingUserConsentView());

  auto& read_write_cards_ui_controller =
      quick_answers_controller->read_write_cards_ui_controller();
  auto* user_consent_view = ui_controller()->user_consent_view();

  // The user consent view should appears as the Quick Answers view within
  // `ReadWriteCardsUiController`.
  EXPECT_EQ(user_consent_view,
            read_write_cards_ui_controller.GetQuickAnswersViewForTest());

  // Click on "Allow" button to close the consent view.
  views::test::ButtonTestApi(user_consent_view->allow_button_for_test())
      .NotifyClick(ui::test::TestEvent());

  EXPECT_FALSE(ui_controller()->user_consent_view());
}

TEST_F(QuickAnswersUiControllerTest, TearDownWhileConsentViewShowing) {
  EXPECT_FALSE(ui_controller()->IsShowingUserConsentView());

  // Set up a companion menu before creating the QuickAnswersView.
  CreateAndShowBasicMenu();
  GetQuickAnswersController()->OnContextMenuShown(GetProfile());

  ShowConsentView();

  EXPECT_TRUE(ui_controller()->IsShowingUserConsentView());
}
