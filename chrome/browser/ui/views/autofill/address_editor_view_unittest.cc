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
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

// TODO(crbug.com/40277889): write interactive UI tests instead of unit tests.
class AddressEditorViewTest : public ChromeViewsTestBase {
 public:
  AddressEditorViewTest() = default;
  ~AddressEditorViewTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kAutofillProfileEnabled, true);
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kAutofillCreditCardEnabled, true);
    pdm_.SetPrefService(&pref_service_);

    profile_to_edit_ = test::GetFullProfile();
    auto controller = std::make_unique<AddressEditorController>(
        profile_to_edit_, &pdm_, true);
    controller_ = controller.get();
    view_ = std::make_unique<AddressEditorView>(std::move(controller));
  }

  void TearDown() override {
    controller_ = nullptr;
    view_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  // Required for test_web_content.
  content::RenderViewHostTestEnabler test_render_host_factories_;
  autofill::AutofillProfile profile_to_edit_{
      i18n_model_definition::kLegacyHierarchyCountryCode};
  TestingProfile profile_;
  TestingPrefServiceSimple pref_service_;
  TestPersonalDataManager pdm_;
  raw_ptr<AddressEditorController> controller_;
  std::unique_ptr<AddressEditorView> view_;
};

TEST_F(AddressEditorViewTest, FormValidation) {
  view_->ValidateAllFields();
  EXPECT_TRUE(*controller_->is_valid())
      << "The form initailized from a full profile should be valid.";

  view_->SetTextInputFieldValueForTesting(
      autofill::FieldType::ADDRESS_HOME_STREET_ADDRESS, u"");
  EXPECT_FALSE(*controller_->is_valid())
      << "Street address is required for US, the form should be invalid.";
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_EDIT_ADDRESS_REQUIRED_FIELD_FORM_ERROR),
            view_->GetValidationErrorForTesting());

  view_->SetTextInputFieldValueForTesting(
      autofill::FieldType::ADDRESS_HOME_CITY, u"");
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_EDIT_ADDRESS_REQUIRED_FIELDS_FORM_ERROR),
            view_->GetValidationErrorForTesting())
      << "The error message should denote multiple invalid fileds now.";

  view_->SetTextInputFieldValueForTesting(
      autofill::FieldType::ADDRESS_HOME_STREET_ADDRESS, u"Some text");
  view_->SetTextInputFieldValueForTesting(
      autofill::FieldType::ADDRESS_HOME_CITY, u"Some text");
  EXPECT_TRUE(*controller_->is_valid())
      << "All the required fields are filled in, the form should be valid.";
  EXPECT_EQ(u"", view_->GetValidationErrorForTesting())
      << "The error message should be empty for a valid form.";
}

TEST_F(AddressEditorViewTest, NoValidatableFormValidation) {
  auto controller =
      std::make_unique<AddressEditorController>(profile_to_edit_, &pdm_, false);
  controller_ = controller.get();
  view_ = std::make_unique<AddressEditorView>(std::move(controller));

  view_->SetTextInputFieldValueForTesting(
      autofill::FieldType::ADDRESS_HOME_STREET_ADDRESS, u"");
  view_->ValidateAllFields();
  EXPECT_FALSE(controller_->is_valid().has_value())
      << "Street address is required for US, but the form is not validatable.";
  EXPECT_EQ(u"", view_->GetValidationErrorForTesting());
}

TEST_F(AddressEditorViewTest, CountryChangeValidity) {
  view_->SetTextInputFieldValueForTesting(
      FieldType::ADDRESS_HOME_STREET_ADDRESS, u"");
  view_->ValidateAllFields();
  EXPECT_FALSE(*controller_->is_valid())
      << "Street address is required for US, the form should be invalid.";
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_EDIT_ADDRESS_REQUIRED_FIELD_FORM_ERROR),
            view_->GetValidationErrorForTesting());

  view_->SelectCountryForTesting(u"Afghanistan");
  EXPECT_FALSE(*controller_->is_valid())
      << "Street address is required for AF, the invalid state should not be "
         "reset.";
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_EDIT_ADDRESS_REQUIRED_FIELD_FORM_ERROR),
            view_->GetValidationErrorForTesting());
}

TEST_F(AddressEditorViewTest, CountryChangeValidity2) {
  view_->SetTextInputFieldValueForTesting(FieldType::ADDRESS_HOME_ZIP, u"");
  view_->ValidateAllFields();
  EXPECT_FALSE(*controller_->is_valid())
      << "ZIP code is required for US, the form should be invalid";

  view_->SelectCountryForTesting(u"Belarus");
  EXPECT_TRUE(*controller_->is_valid())
      << "ZIP code is not required for BY, the form should be valid";
}

TEST_F(AddressEditorViewTest, WholeFormValidationState) {
  view_->SetTextInputFieldValueForTesting(FieldType::ADDRESS_HOME_STATE, u"");
  view_->SetTextInputFieldValueForTesting(
      FieldType::ADDRESS_HOME_STREET_ADDRESS, u"");
  view_->SetTextInputFieldValueForTesting(FieldType::ADDRESS_HOME_ZIP, u"");

  EXPECT_FALSE(controller_->is_valid().has_value())
      << "Some required field for a US address was cleared, but we didn't call "
         "`ValidateAllFields()` and the validation state is unknown.";
  EXPECT_EQ(view_->GetValidationErrorForTesting(), u"");

  view_->SetTextInputFieldValueForTesting(FieldType::ADDRESS_HOME_STATE,
                                          u"California");
  EXPECT_FALSE(controller_->is_valid().has_value())
      << "The validation state should not be updated by fixing one of the "
         "invalid fields as the whole form validation was not performed yet.";
  EXPECT_EQ(view_->GetValidationErrorForTesting(), u"");

  view_->ValidateAllFields();
  EXPECT_FALSE(*controller_->is_valid());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_EDIT_ADDRESS_REQUIRED_FIELDS_FORM_ERROR),
            view_->GetValidationErrorForTesting());

  view_->SetTextInputFieldValueForTesting(FieldType::ADDRESS_HOME_ZIP, u"1234");
  EXPECT_FALSE(*controller_->is_valid()) << "";
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_EDIT_ADDRESS_REQUIRED_FIELD_FORM_ERROR),
            view_->GetValidationErrorForTesting());

  view_->SetTextInputFieldValueForTesting(
      FieldType::ADDRESS_HOME_STREET_ADDRESS, u"12 Park avenue");
  EXPECT_TRUE(*controller_->is_valid());
  EXPECT_EQ(view_->GetValidationErrorForTesting(), u"");
}

TEST_F(AddressEditorViewTest, InitialFocusViewPointsToCountryCombobox) {
  EXPECT_NE(view_->initial_focus_view(), nullptr);
  EXPECT_EQ(
      std::string(view_->initial_focus_view()->GetClassMetaData()->type_name()),
      "Combobox");
}

TEST_F(AddressEditorViewTest, FocusIsNotLostAfterEditorContentChange) {
  // The view is temporarily added into a Widget to have a FocusManager.
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  widget->SetContentsView(view_.get());
  widget->Show();
  EXPECT_NE(view_->initial_focus_view(), nullptr);

  view_->SelectCountryForTesting(u"Belarus");
  EXPECT_NE(view_->initial_focus_view(), nullptr);
  EXPECT_TRUE(view_->initial_focus_view()->HasFocus());

  // The view is managed by the test class, remove it from the widget.
  widget->GetRootView()->RemoveChildView(view_.get());
}

}  // namespace autofill
