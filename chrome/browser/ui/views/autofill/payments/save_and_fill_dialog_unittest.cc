// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/save_and_fill_dialog.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/autofill/core/browser/ui/payments/save_and_fill_dialog_controller_impl.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"

namespace autofill {

class TestSaveAndFillDialogView : public SaveAndFillDialogView {
 public:
  TestSaveAndFillDialogView() = default;
  ~TestSaveAndFillDialogView() override = default;
  void DismissThrobberAndUpdateMainView() override {}
};

class SaveAndFillDialogTest : public ChromeViewsTestBase {
 public:
  SaveAndFillDialogTest() = default;
  ~SaveAndFillDialogTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    controller_ = std::make_unique<SaveAndFillDialogControllerImpl>();
    controller_->ShowLocalDialog(
        base::BindOnce([]() -> std::unique_ptr<SaveAndFillDialogView> {
          return std::make_unique<TestSaveAndFillDialogView>();
        }),
        base::DoNothing());
    parent_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    parent_widget_->Show();

    auto dialog = std::make_unique<SaveAndFillDialog>(controller_->GetWeakPtr(),
                                                      base::DoNothing());
    dialog_ = dialog.get();
    widget_ = base::WrapUnique(views::DialogDelegate::CreateDialogWidget(
        dialog.release(), GetContext(), parent_widget_->GetNativeView()));
    widget_->Show();
  }

  void TearDown() override {
    dialog_ = nullptr;
    widget_.reset();
    parent_widget_.reset();
    controller_.reset();
    ChromeViewsTestBase::TearDown();
  }

  SaveAndFillDialog* dialog() { return dialog_; }

  void SimulateInput(views::Textfield& textfield, const std::u16string& text) {
    textfield.SetText(text);
    dialog()->ContentsChanged(&textfield, text);
  }

 private:
  std::unique_ptr<SaveAndFillDialogControllerImpl> controller_;
  std::unique_ptr<views::Widget> parent_widget_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<SaveAndFillDialog> dialog_ = nullptr;
};

TEST_F(SaveAndFillDialogTest, InitialState) {
  EXPECT_FALSE(dialog()->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));

  EXPECT_FALSE(dialog()->card_number_data_for_testing().is_valid_input);
  EXPECT_FALSE(
      dialog()->card_number_data_for_testing().error_label->GetVisible());
  EXPECT_FALSE(dialog()
                   ->card_number_data_for_testing()
                   .GetInputTextField()
                   .GetInvalid());

  // CVC is mandatory and starts invalid without showing an error.
  EXPECT_FALSE(dialog()->cvc_data_for_testing().is_valid_input);
  EXPECT_FALSE(dialog()->cvc_data_for_testing().error_label->GetVisible());
  EXPECT_FALSE(
      dialog()->cvc_data_for_testing().GetInputTextField().GetInvalid());
  EXPECT_TRUE(dialog()
                  ->cvc_data_for_testing()
                  .GetInputTextField()
                  .GetPlaceholderText()
                  .empty());

  EXPECT_FALSE(dialog()->expiration_date_data_for_testing().is_valid_input);
  EXPECT_FALSE(
      dialog()->expiration_date_data_for_testing().error_label->GetVisible());
  EXPECT_FALSE(dialog()
                   ->expiration_date_data_for_testing()
                   .GetInputTextField()
                   .GetInvalid());

  EXPECT_FALSE(dialog()->name_on_card_data_for_testing().is_valid_input);
  EXPECT_FALSE(
      dialog()->name_on_card_data_for_testing().error_label->GetVisible());
  EXPECT_FALSE(dialog()
                   ->name_on_card_data_for_testing()
                   .GetInputTextField()
                   .GetInvalid());
}

TEST_F(SaveAndFillDialogTest, TypingInvalidInput_DoesNotShowErrorWhileFocused) {
  views::Textfield& card_number_field =
      dialog()->card_number_data_for_testing().GetInputTextField();

  SimulateInput(card_number_field, u"4111");

  EXPECT_FALSE(dialog()->card_number_data_for_testing().is_valid_input);
  EXPECT_FALSE(
      dialog()->card_number_data_for_testing().error_label->GetVisible());
  EXPECT_FALSE(card_number_field.GetInvalid());
}

TEST_F(SaveAndFillDialogTest, UnfocusingInvalidField_ShowsError) {
  views::Textfield& card_number_field =
      dialog()->card_number_data_for_testing().GetInputTextField();
  views::Textfield& cvc_field =
      dialog()->cvc_data_for_testing().GetInputTextField();

  SimulateInput(card_number_field, u"4111");
  EXPECT_FALSE(
      dialog()->card_number_data_for_testing().error_label->GetVisible());
  EXPECT_FALSE(card_number_field.GetInvalid());

  // Unfocusing the invalid non-empty field should show the error message.
  dialog()->OnDidChangeFocus(&card_number_field, &cvc_field);
  EXPECT_TRUE(
      dialog()->card_number_data_for_testing().error_label->GetVisible());
  EXPECT_TRUE(card_number_field.GetInvalid());
}

TEST_F(SaveAndFillDialogTest, UnfocusingEmptyField_DoesNotShowError) {
  views::Textfield& card_number_field =
      dialog()->card_number_data_for_testing().GetInputTextField();
  views::Textfield& cvc_field =
      dialog()->cvc_data_for_testing().GetInputTextField();

  EXPECT_TRUE(card_number_field.GetText().empty());
  dialog()->OnDidChangeFocus(&card_number_field, &cvc_field);

  EXPECT_FALSE(
      dialog()->card_number_data_for_testing().error_label->GetVisible());
  EXPECT_FALSE(card_number_field.GetInvalid());
}

TEST_F(SaveAndFillDialogTest, SaveButtonRequiresMandatoryCvc) {
  views::Textfield& card_number_field =
      dialog()->card_number_data_for_testing().GetInputTextField();
  views::Textfield& expiration_field =
      dialog()->expiration_date_data_for_testing().GetInputTextField();
  views::Textfield& cvc_field =
      dialog()->cvc_data_for_testing().GetInputTextField();
  views::Textfield& name_field =
      dialog()->name_on_card_data_for_testing().GetInputTextField();

  base::Time::Exploded now_exploded;
  AutofillClock::Now().LocalExplode(&now_exploded);
  std::u16string valid_exp_date = base::UTF8ToUTF16(
      base::StringPrintf("12/%02d", (now_exploded.year + 2) % 100));

  SimulateInput(card_number_field, u"4111111111111111");
  SimulateInput(expiration_field, valid_exp_date);
  SimulateInput(name_field, u"John Doe");

  EXPECT_TRUE(dialog()->card_number_data_for_testing().is_valid_input);
  EXPECT_TRUE(dialog()->expiration_date_data_for_testing().is_valid_input);
  EXPECT_TRUE(dialog()->name_on_card_data_for_testing().is_valid_input);
  EXPECT_FALSE(dialog()->cvc_data_for_testing().is_valid_input);

  // Save button remains disabled while CVC is empty.
  EXPECT_FALSE(dialog()->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));

  // Entering valid CVC immediately enables the Save button.
  SimulateInput(cvc_field, u"123");
  EXPECT_TRUE(dialog()->cvc_data_for_testing().is_valid_input);
  EXPECT_TRUE(dialog()->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
}

}  // namespace autofill
