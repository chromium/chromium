// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/address_editor_controller.h"

#include <memory>

#include "base/callback_list.h"
#include "base/ranges/algorithm.h"
#include "base/test/mock_callback.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/ui/country_combobox_model.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

// TODO(crbug.com/40277889): write more unit tests for AddressEditorController.
class AddressEditorControllerTest : public testing::Test {
 public:
  AddressEditorControllerTest() = default;

  void SetUp() override {
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kAutofillProfileEnabled, true);
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kAutofillCreditCardEnabled, true);
    pdm_.SetPrefService(&pref_service_);
    pdm_.test_address_data_manager().SetDefaultCountryCode(
        AddressCountryCode("US"));
  }

 protected:
  void CreateController(bool is_validatable) {
    controller_ = std::make_unique<AddressEditorController>(profile_, &pdm_,
                                                            is_validatable);
  }

  TestingPrefServiceSimple pref_service_;
  TestPersonalDataManager pdm_;
  AutofillProfile profile_ = test::GetFullProfile();
  std::unique_ptr<AddressEditorController> controller_;
};

TEST_F(AddressEditorControllerTest, SmokeTest) {
  CreateController(/*is_validatable=*/false);
  EXPECT_FALSE(controller_->is_validatable());
  EXPECT_FALSE(controller_->is_valid().has_value());

  controller_->SetIsValid(/*is_valid=*/false);
  EXPECT_TRUE(controller_->is_valid().has_value());
  EXPECT_FALSE(*controller_->is_valid());
}

TEST_F(AddressEditorControllerTest, FieldValidation) {
  CreateController(/*is_validatable=*/true);
  EXPECT_TRUE(controller_->IsValid({autofill::PHONE_HOME_WHOLE_NUMBER, u"",
                                    EditorField::LengthHint::HINT_SHORT, false,
                                    EditorField::ControlType::TEXTFIELD_NUMBER},
                                   u""))
      << "Non-validatable field should always be valid.";

  EXPECT_FALSE(
      controller_->IsValid({autofill::PHONE_HOME_WHOLE_NUMBER, u"",
                            EditorField::LengthHint::HINT_SHORT, true,
                            EditorField::ControlType::TEXTFIELD_NUMBER},
                           u""))
      << "Empty value for validatable field is considered invalid.";

  EXPECT_FALSE(
      controller_->IsValid({autofill::PHONE_HOME_WHOLE_NUMBER, u"  ",
                            EditorField::LengthHint::HINT_SHORT, true,
                            EditorField::ControlType::TEXTFIELD_NUMBER},
                           u""))
      << "Whitespaces should be trimmed and the field is considered invalid.";

  EXPECT_TRUE(controller_->IsValid({autofill::PHONE_HOME_WHOLE_NUMBER, u"",
                                    EditorField::LengthHint::HINT_SHORT, true,
                                    EditorField::ControlType::TEXTFIELD_NUMBER},
                                   u"abc"))
      << "Non-empty string is considered valid";
}

TEST_F(AddressEditorControllerTest, ValidityChanges) {
  CreateController(/*is_validatable=*/true);

  base::MockRepeatingCallback<void(bool)> on_validity_changed;
  InSequence seq;
  EXPECT_CALL(on_validity_changed, Run(false));
  EXPECT_CALL(on_validity_changed, Run(true));

  base::CallbackListSubscription subscription =
      controller_->AddIsValidChangedCallback(on_validity_changed.Get());
  controller_->SetIsValid(/*is_valid=*/false);
  controller_->SetIsValid(/*is_valid=*/true);
}

TEST_F(AddressEditorControllerTest, ValidityRemainsSame) {
  CreateController(/*is_validatable=*/true);

  base::MockRepeatingCallback<void(bool)> on_validity_changed;
  InSequence seq;
  EXPECT_CALL(on_validity_changed, Run).Times(1);

  base::CallbackListSubscription subscription =
      controller_->AddIsValidChangedCallback(on_validity_changed.Get());
  controller_->SetIsValid(/*is_valid=*/true);
  controller_->SetIsValid(/*is_valid=*/true);
}

TEST_F(AddressEditorControllerTest, GetCountryComboboxModel) {
  CreateController(/*is_validatable=*/false);

  EXPECT_GT(controller_->GetCountryComboboxModel().GetItemCount(), 0ul);
  // `country` is null when it represents a separator. There must be exactly 1
  // separator in the country list.
  EXPECT_EQ(
      base::ranges::count_if(controller_->GetCountryComboboxModel().countries(),
                             [](const auto& country) { return !country; }),
      1l);
}

// TODO(crbug.com/40263955): remove this test once unsupported countries
// filtering is removed.
TEST_F(AddressEditorControllerTest, NonZeroCountriesFiltered) {
  auto non_validatable_controller = std::make_unique<AddressEditorController>(
      profile_, &pdm_, /*is_validatable=*/false);
  auto validatable_controller = std::make_unique<AddressEditorController>(
      profile_, &pdm_, /*is_validatable=*/true);

  // Country list should be reduced in size after unsupported countries are
  // filtered out.
  EXPECT_GT(
      non_validatable_controller->GetCountryComboboxModel().GetItemCount(),
      validatable_controller->GetCountryComboboxModel().GetItemCount());
}

TEST_F(AddressEditorControllerTest, SetProfileInfo) {
  CreateController(/*is_validatable=*/false);

  for (const auto& type_value_pair :
       std::vector<std::pair<FieldType, std::u16string>>{
           {ADDRESS_HOME_COUNTRY, u"Germany"},
           {NAME_FULL, u"John Doe"},
           {ADDRESS_HOME_STREET_ADDRESS, u"Lake St. 123"},
           {PHONE_HOME_WHOLE_NUMBER, u"+11111111111"},
           {EMAIL_ADDRESS, u"example@gmail.com"}}) {
    controller_->SetProfileInfo(type_value_pair.first, type_value_pair.second);
    EXPECT_EQ(controller_->GetProfileInfo(type_value_pair.first),
              type_value_pair.second);
  }
  // First and last names must be reset, see crbug.com/1496322.
  EXPECT_EQ(controller_->GetProfileInfo(NAME_FIRST), u"");
  EXPECT_EQ(controller_->GetProfileInfo(NAME_LAST), u"");
}

TEST_F(AddressEditorControllerTest, StaticEditorFields) {
  CreateController(/*is_validatable=*/false);
  // Country, phone number and email address fields are added unconditionally
  // to the set of editor fields.
  for (auto type : std::vector<FieldType>{
           ADDRESS_HOME_COUNTRY, PHONE_HOME_WHOLE_NUMBER, EMAIL_ADDRESS}) {
    EXPECT_EQ(base::ranges::count_if(
                  controller_->editor_fields(),
                  [type](auto field) { return field.type == type; }),
              1);
  }
}

TEST_F(AddressEditorControllerTest, UpdateEditorFields) {
  CreateController(/*is_validatable=*/false);
  size_t us_fields_size = controller_->editor_fields().size();

  controller_->UpdateEditorFields("BR");
  // Verify that the number of fields shown for the Brazil is greater than
  // number of fields for the US address.
  EXPECT_GT(controller_->editor_fields().size(), us_fields_size);
}

}  // namespace autofill
