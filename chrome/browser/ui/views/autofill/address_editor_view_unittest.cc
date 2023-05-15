// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "chrome/browser/ui/autofill/address_editor_controller.h"

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/autofill/address_editor_view.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

class AddressEditorViewTest : public ChromeViewsTestBase {
 public:
  AddressEditorViewTest() = default;
  ~AddressEditorViewTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    profile_to_edit_ = test::GetFullProfile();
    test_web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    auto controller = std::make_unique<AddressEditorController>(
        profile_to_edit_, test_web_contents_.get(), true);
    controller_ = controller.get();
    view_ = std::make_unique<AddressEditorView>(std::move(controller));
  }

  void TearDown() override {
    test_web_contents_.reset();
    view_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  // Required for test_web_content.
  content::RenderViewHostTestEnabler test_render_host_factories_;
  autofill::AutofillProfile profile_to_edit_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> test_web_contents_;
  raw_ptr<AddressEditorController> controller_;
  std::unique_ptr<AddressEditorView> view_;
};

TEST_F(AddressEditorViewTest, FieldValidation) {
  EXPECT_TRUE(controller_->IsValid({autofill::PHONE_HOME_WHOLE_NUMBER, u"",
                                    EditorField::LengthHint::HINT_SHORT, false,
                                    EditorField::ControlType::TEXTFIELD_NUMBER},
                                   u""))
      << "Non-validatable field should always be valid.";

  EXPECT_FALSE(
      controller_->IsValid({autofill::PHONE_HOME_WHOLE_NUMBER, u"",
                            EditorField::LengthHint::HINT_SHORT, true,
                            EditorField::ControlType::TEXTFIELD_NUMBER},
                           u""));

  EXPECT_FALSE(
      controller_->IsValid({autofill::PHONE_HOME_WHOLE_NUMBER, u"  ",
                            EditorField::LengthHint::HINT_SHORT, true,
                            EditorField::ControlType::TEXTFIELD_NUMBER},
                           u""))
      << "Whitespaces should be trimmed and the field is considered invalid.";
}

TEST_F(AddressEditorViewTest, FormValidation) {
  EXPECT_TRUE(controller_->get_is_valid())
      << "The form initailized from a full profile should be valid.";

  view_->SetTextInputFieldValueForTesting(
      autofill::ServerFieldType::ADDRESS_HOME_STREET_ADDRESS, u"");
  EXPECT_FALSE(controller_->get_is_valid())
      << "Street address is required for US, the form should be invalid.";
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_EDIT_ADDRESS_REQUIRED_FIELD_FORM_ERROR),
            view_->GetValidationErrorForTesting());

  view_->SetTextInputFieldValueForTesting(
      autofill::ServerFieldType::ADDRESS_HOME_CITY, u"");
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_EDIT_ADDRESS_REQUIRED_FIELDS_FORM_ERROR),
            view_->GetValidationErrorForTesting())
      << "The error message should denote multiple invalid fileds now.";

  view_->SetTextInputFieldValueForTesting(
      autofill::ServerFieldType::ADDRESS_HOME_STREET_ADDRESS, u"Some text");
  view_->SetTextInputFieldValueForTesting(
      autofill::ServerFieldType::ADDRESS_HOME_CITY, u"Some text");
  EXPECT_TRUE(controller_->get_is_valid())
      << "All the required fields are filled in, the form should be valid.";
  EXPECT_EQ(u"", view_->GetValidationErrorForTesting())
      << "The error message should be empty for a valid form.";
}

TEST_F(AddressEditorViewTest, NoValidatableFormValidation) {
  auto controller = std::make_unique<AddressEditorController>(
      profile_to_edit_, test_web_contents_.get(), false);
  controller_ = controller.get();
  view_ = std::make_unique<AddressEditorView>(std::move(controller));

  view_->SetTextInputFieldValueForTesting(
      autofill::ServerFieldType::ADDRESS_HOME_STREET_ADDRESS, u"");
  EXPECT_TRUE(controller_->get_is_valid())
      << "Street address is required for US, but the form is not validatable.";
  EXPECT_EQ(u"", view_->GetValidationErrorForTesting());
}

TEST_F(AddressEditorViewTest, CountryChangeValidity) {
  view_->SetTextInputFieldValueForTesting(
      ServerFieldType::ADDRESS_HOME_STREET_ADDRESS, u"");
  EXPECT_FALSE(controller_->get_is_valid())
      << "Street address is required for US, the form should be invalid.";
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_EDIT_ADDRESS_REQUIRED_FIELD_FORM_ERROR),
            view_->GetValidationErrorForTesting());

  view_->SetCountryCodeForTesting("AF");
  EXPECT_FALSE(controller_->get_is_valid())
      << "Street address is required for AF, the invalid state should not be "
         "reset.";
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_EDIT_ADDRESS_REQUIRED_FIELD_FORM_ERROR),
            view_->GetValidationErrorForTesting());
}

TEST_F(AddressEditorViewTest, CountryChangeValidity2) {
  view_->SetTextInputFieldValueForTesting(ServerFieldType::ADDRESS_HOME_ZIP,
                                          u"");
  EXPECT_FALSE(controller_->get_is_valid())
      << "ZIP code is required for US, the form should be invalid";

  view_->SetCountryCodeForTesting("BY");
  EXPECT_TRUE(controller_->get_is_valid())
      << "ZIP code is not required for BY, the form should be valid";
}

}  // namespace autofill
