// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/quick_answers/quick_answers_controller_impl.h"
#include "chrome/browser/ui/quick_answers/test/chrome_quick_answers_test_base.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_view.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/test/test_event.h"
#include "ui/views/accessibility/view_accessibility.h"
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

  bool MaybeShowConsentView() {
    return GetQuickAnswersController()->MaybeShowUserConsent(
        quick_answers::IntentType::kUnknown,
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

  EXPECT_TRUE(MaybeShowConsentView());

  EXPECT_TRUE(ui_controller()->IsShowingUserConsentView());

  auto& read_write_cards_ui_controller =
      quick_answers_controller->read_write_cards_ui_controller();
  auto* user_consent_view = ui_controller()->user_consent_view();

  // The user consent view should appears as the Quick Answers view within
  // `ReadWriteCardsUiController`.
  EXPECT_EQ(user_consent_view,
            read_write_cards_ui_controller.GetQuickAnswersUiForTest());

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

  EXPECT_TRUE(MaybeShowConsentView());

  EXPECT_TRUE(ui_controller()->IsShowingUserConsentView());
}

TEST_F(QuickAnswersUiControllerTest, QuickAnswersViewAccessibleProperties) {
  CreateAndShowBasicMenu();
  GetQuickAnswersController()->SetVisibility(
      QuickAnswersVisibility::kQuickAnswersVisible);
  ui_controller()->CreateQuickAnswersView(GetProfile(), "default_title",
                                          "default_query",
                                          /*is_internal=*/false);
  quick_answers::QuickAnswersView* quick_answers_view =
      ui_controller()->quick_answers_view();
  ui::AXNodeData data;

  ASSERT_TRUE(quick_answers_view);
  quick_answers_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kDialog);
}

TEST_F(QuickAnswersUiControllerTest, UserConsentViewAccessibleProperties) {
  CreateAndShowBasicMenu();
  GetQuickAnswersController()->OnContextMenuShown(/*profile=*/nullptr);
  EXPECT_TRUE(MaybeShowConsentView());

  quick_answers::UserConsentView* user_consent_view =
      ui_controller()->user_consent_view();
  ui::AXNodeData data;
  auto expected_name =
      l10n_util::GetStringUTF16(IDS_QUICK_ANSWERS_USER_NOTICE_VIEW_TITLE_TEXT);
  auto expected_desc = l10n_util::GetStringFUTF8(
      IDS_QUICK_ANSWERS_USER_NOTICE_VIEW_A11Y_INFO_DESC_TEMPLATE,
      l10n_util::GetStringUTF16(IDS_QUICK_ANSWERS_USER_CONSENT_VIEW_DESC_TEXT));

  ASSERT_TRUE(user_consent_view);
  user_consent_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kDialog);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            expected_name);
  EXPECT_EQ(data.GetStringAttribute(ax::mojom::StringAttribute::kDescription),
            expected_desc);
}
