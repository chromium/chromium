// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_structure_rationalizer.h"

#include <utility>

#include "base/base64.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;

namespace autofill {
namespace {

std::string SerializeAndEncode(const AutofillQueryResponse& response) {
  std::string unencoded_response_string;
  if (!response.SerializeToString(&unencoded_response_string)) {
    LOG(ERROR) << "Cannot serialize the response proto";
    return "";
  }
  std::string response_string;
  base::Base64Encode(unencoded_response_string, &response_string);
  return response_string;
}

// The key information from which we build FormFieldData objects and an
// AutofillQueryResponse for tests.
struct FieldTemplate {
  base::StringPiece label;
  base::StringPiece name;
  // This is a field type we assume the autofill server would provide for
  // the given field.
  ServerFieldType field_type;
  // Section name of a field.
  base::StringPiece section = "";
  base::StringPiece form_control_type = "text";
  bool is_focusable = true;
  FormFieldData::RoleAttribute role = FormFieldData::RoleAttribute::kOther;
};

// These are helper functions that set a special flag in a field_template.
// They only exist because the output of clang-format for function calls is
// more compact than if we switched to designated initializer lists.
FieldTemplate ToNotFocusable(FieldTemplate field_template) {
  // This is often set because a field is hidden.
  field_template.is_focusable = false;
  return field_template;
}

FieldTemplate ToSelectOne(FieldTemplate field_template) {
  field_template.form_control_type = "select-one";
  return field_template;
}

FieldTemplate SetRolePresentation(FieldTemplate field_template) {
  field_template.role = FormFieldData::RoleAttribute::kPresentation;
  return field_template;
}

std::pair<FormData, std::string> CreateFormAndServerClassification(
    std::vector<FieldTemplate> fields) {
  FormData form;
  form.url = GURL("http://foo.com");

  // Build the fields for the form.
  for (const auto& field_template : fields) {
    FormFieldData field;
    field.label = base::UTF8ToUTF16(field_template.label);
    field.name = base::UTF8ToUTF16(field_template.name);
    if (!field_template.section.empty()) {
      field.section = Section::FromAutocomplete(
          {.section = std::string(field_template.section)});
    }
    field.form_control_type = std::string(field_template.form_control_type);
    field.is_focusable = field_template.is_focusable;
    field.role = field_template.role;
    field.unique_renderer_id = test::MakeFieldRendererId();
    form.fields.push_back(std::move(field));
  }

  // Build the response of the Autofill Server with field classifications.
  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  for (size_t i = 0; i < fields.size(); ++i) {
    auto* field_suggestion = form_suggestion->add_field_suggestions();
    field_suggestion->set_field_signature(
        CalculateFieldSignatureForField(form.fields[i]).value());
    *field_suggestion->add_predictions() =
        ::autofill::test::CreateFieldPrediction(fields[i].field_type);
  }
  std::string response_string = SerializeAndEncode(response);

  return std::make_pair(form, response_string);
}

std::vector<ServerFieldType> GetTypes(const FormStructure& form_structure) {
  std::vector<ServerFieldType> server_types;
  server_types.reserve(form_structure.field_count());
  for (size_t i = 0; i < form_structure.field_count(); ++i) {
    server_types.emplace_back(
        form_structure.field(i)->Type().GetStorableType());
  }
  return server_types;
}

FormStructureTestApi test_api(FormStructure* form_structure) {
  return FormStructureTestApi(form_structure);
}

class FormStructureRationalizerTest : public testing::Test {
 public:
  FormStructureRationalizerTest();

 protected:
  base::test::ScopedFeatureList scoped_features_;
  test::AutofillEnvironment autofill_environment_;
};

FormStructureRationalizerTest::FormStructureRationalizerTest() {
  scoped_features_.InitAndEnableFeature(
      features::kAutofillRationalizeStreetAddressAndHouseNumber);
}

TEST_F(FormStructureRationalizerTest, ParseQueryResponse_RationalizeLoneField) {
  auto [form, response_string] = CreateFormAndServerClassification(
      {{"fullname", "fullname", NAME_FULL},
       {"address", "address", ADDRESS_HOME_LINE1},
       {"height", "height", CREDIT_CARD_EXP_MONTH},  // Uh-oh!
       {"email", "email", EMAIL_ADDRESS}});

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms = {&form_structure};
  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);

  EXPECT_THAT(
      GetTypes(form_structure),
      ElementsAre(NAME_FULL, ADDRESS_HOME_LINE1, UNKNOWN_TYPE, EMAIL_ADDRESS));
}

TEST_F(FormStructureRationalizerTest, ParseQueryResponse_RationalizeCCName) {
  auto [form, response_string] = CreateFormAndServerClassification(
      {{"First Name", "fname", CREDIT_CARD_NAME_FIRST},
       {"Last Name", "lname", CREDIT_CARD_NAME_LAST},
       {"email", "email", EMAIL_ADDRESS}});

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms = {&form_structure};
  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);

  EXPECT_THAT(GetTypes(form_structure),
              ElementsAre(NAME_FIRST, NAME_LAST, EMAIL_ADDRESS));
}
TEST_F(FormStructureRationalizerTest,
       ParseQueryResponse_RationalizeMultiMonth_1) {
  auto [form, response_string] = CreateFormAndServerClassification({
      {"Cardholder", "fullname", CREDIT_CARD_NAME_FULL},
      {"Card Number", "address", CREDIT_CARD_NUMBER},
      {"Month", "expiry_month", CREDIT_CARD_EXP_MONTH},
      {"Year", "expiry_year", CREDIT_CARD_EXP_2_DIGIT_YEAR},
      {"Quantity", "quantity", CREDIT_CARD_EXP_MONTH}  // Uh-oh!
  });

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms = {&form_structure};
  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);

  EXPECT_THAT(GetTypes(form_structure),
              ElementsAre(CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
                          CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR,
                          UNKNOWN_TYPE));
}

TEST_F(FormStructureRationalizerTest,
       ParseQueryResponse_RationalizeMultiMonth_2) {
  auto [form, response_string] = CreateFormAndServerClassification({
      {"Cardholder", "fullname", CREDIT_CARD_NAME_FULL},
      {"Card Number", "address", CREDIT_CARD_NUMBER},
      {"Expiry Date (MMYY)", "expiry", CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
      {"Quantity", "quantity", CREDIT_CARD_EXP_MONTH},  // Uh-oh!
  });

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms = {&form_structure};
  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);

  EXPECT_THAT(GetTypes(form_structure),
              ElementsAre(CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
                          CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, UNKNOWN_TYPE));
}

TEST_F(FormStructureRationalizerTest,
       RationalizePhoneNumber_RunsOncePerSection) {
  auto [form, response_string] = CreateFormAndServerClassification({
      {"Full Name", "fullName", NAME_FULL},
      {"Address", "address", ADDRESS_HOME_STREET_ADDRESS},
      {"Home Phone", "homePhoneNumber", PHONE_HOME_WHOLE_NUMBER},
      {"Cell Phone", "cellPhoneNumber", PHONE_HOME_WHOLE_NUMBER},
  });

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms = {&form_structure};
  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);

  Section s = forms[0]->field(0)->section;
  EXPECT_FALSE(test_api(&form_structure).phone_rationalized(s));
  form_structure.RationalizePhoneNumbersInSection(s);
  EXPECT_TRUE(test_api(&form_structure).phone_rationalized(s));

  EXPECT_THAT(GetTypes(form_structure),
              ElementsAre(NAME_FULL, ADDRESS_HOME_STREET_ADDRESS,
                          PHONE_HOME_WHOLE_NUMBER, PHONE_HOME_WHOLE_NUMBER));

  EXPECT_FALSE(forms[0]->field(2)->only_fill_when_focused());
  EXPECT_TRUE(forms[0]->field(3)->only_fill_when_focused());
}

TEST_F(FormStructureRationalizerTest, RationalizeStreetAddressAndAddressLine) {
  auto [form, response_string] = CreateFormAndServerClassification({
      {"Full Name", "fullName", NAME_FULL},
      {"Address1", "address1", ADDRESS_HOME_STREET_ADDRESS},
      {"Address2", "address2", ADDRESS_HOME_LINE2},
  });

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms = {&form_structure};
  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);

  EXPECT_THAT(GetTypes(form_structure),
              ElementsAre(NAME_FULL, ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2));
}

// Ensure that a tuple of (street-address, house number) is rewritten to (street
// name, house number). We have seen several cases where the field preceding the
// house number was not classified as a street name.
TEST_F(FormStructureRationalizerTest, RationalizeStreetAddressAndHouseNumber) {
  auto [form, response_string] = CreateFormAndServerClassification({
      {"Full Name", "fullName", NAME_FULL},
      {"Address1", "address1", ADDRESS_HOME_STREET_ADDRESS},
      {"Address2", "address2", ADDRESS_HOME_HOUSE_NUMBER},
  });

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms = {&form_structure};
  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);

  EXPECT_THAT(GetTypes(form_structure),
              ElementsAre(NAME_FULL, ADDRESS_HOME_STREET_NAME,
                          ADDRESS_HOME_HOUSE_NUMBER));
}

// Ensure that a tuple of (address-line1, house number) is rewritten to (street
// name, house number). We have seen several cases where the field preceding the
// house number was not classified as a street name.
TEST_F(FormStructureRationalizerTest, RationalizeAddressLine1AndHouseNumber) {
  auto [form, response_string] = CreateFormAndServerClassification({
      {"Full Name", "fullName", NAME_FULL},
      {"Address1", "address1", ADDRESS_HOME_LINE1},
      {"Address2", "address2", ADDRESS_HOME_HOUSE_NUMBER},
  });

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms = {&form_structure};
  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);

  EXPECT_THAT(GetTypes(form_structure),
              ElementsAre(NAME_FULL, ADDRESS_HOME_STREET_NAME,
                          ADDRESS_HOME_HOUSE_NUMBER));
}

// Tests that a form that has only one address predicted as
// ADDRESS_HOME_STREET_ADDRESS is not modified by the address rationalization.
TEST_F(FormStructureRationalizerTest, RationalizeRepeatedFields_OneAddress) {
  auto [form, response_string] = CreateFormAndServerClassification({
      {"Full Name", "fullName", NAME_FULL},
      {"Address", "address", ADDRESS_HOME_STREET_ADDRESS},
      {"City", "city", ADDRESS_HOME_CITY},
  });

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms = {&form_structure};
  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);

  EXPECT_THAT(
      GetTypes(form_structure),
      ElementsAre(NAME_FULL, ADDRESS_HOME_STREET_ADDRESS, ADDRESS_HOME_CITY));
}

// Tests that a form that has two address predicted as
// ADDRESS_HOME_STREET_ADDRESS is modified by the address rationalization to be
// ADDRESS_HOME_LINE1 and ADDRESS_HOME_LINE2 instead.
TEST_F(FormStructureRationalizerTest, RationalizeRepreatedFields_TwoAddresses) {
  auto [form, response_string] = CreateFormAndServerClassification({
      {"Full Name", "fullName", NAME_FULL},
      {"Address", "address", ADDRESS_HOME_STREET_ADDRESS},
      {"Address", "address", ADDRESS_HOME_STREET_ADDRESS},
      {"City", "city", ADDRESS_HOME_CITY},
  });

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms = {&form_structure};
  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);

  EXPECT_THAT(GetTypes(form_structure),
              ElementsAre(NAME_FULL, ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2,
                          ADDRESS_HOME_CITY));
}

// Tests that a form that has three address lines predicted as
// ADDRESS_HOME_STREET_ADDRESS is modified by the address rationalization to be
// ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2 and ADDRESS_HOME_LINE3 instead.
TEST_F(FormStructureRationalizerTest,
       RationalizeRepreatedFields_ThreeAddresses) {
  auto [form, response_string] = CreateFormAndServerClassification({
      {"Full Name", "fullName", NAME_FULL},
      {"Address", "address", ADDRESS_HOME_STREET_ADDRESS},
      {"Address", "address", ADDRESS_HOME_STREET_ADDRESS},
      {"Address", "address", ADDRESS_HOME_STREET_ADDRESS},
      {"City", "city", ADDRESS_HOME_CITY},
  });

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms = {&form_structure};
  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);

  EXPECT_THAT(GetTypes(form_structure),
              ElementsAre(NAME_FULL, ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2,
                          ADDRESS_HOME_LINE3, ADDRESS_HOME_CITY));
}

// Tests that a form that has four address lines predicted as
// ADDRESS_HOME_STREET_ADDRESS is not modified by the address rationalization.
// This doesn't happen in real world, because four address lines mean multiple
// sections according to the heuristics.
TEST_F(FormStructureRationalizerTest,
       RationalizeRepreatedFields_FourAddresses) {
  auto [form, response_string] = CreateFormAndServerClassification({
      {"Full Name", "fullName", NAME_FULL},
      {"Address", "address", ADDRESS_HOME_STREET_ADDRESS},
      {"Address", "address", ADDRESS_HOME_STREET_ADDRESS},
      {"Address", "address", ADDRESS_HOME_STREET_ADDRESS},
      {"Address", "address", ADDRESS_HOME_STREET_ADDRESS},
      {"City", "city", ADDRESS_HOME_CITY},
  });

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms = {&form_structure};
  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);

  EXPECT_THAT(
      GetTypes(form_structure),
      ElementsAre(NAME_FULL, ADDRESS_HOME_STREET_ADDRESS,
                  ADDRESS_HOME_STREET_ADDRESS, ADDRESS_HOME_STREET_ADDRESS,
                  ADDRESS_HOME_STREET_ADDRESS, ADDRESS_HOME_CITY));
}

// Tests that a form that has only one address in each section predicted as
// ADDRESS_HOME_STREET_ADDRESS is not modified by the address rationalization.
TEST_F(FormStructureRationalizerTest,
       RationalizeRepreatedFields_OneAddressEachSection) {
  auto [form, response_string] = CreateFormAndServerClassification({
      // Billing
      {"Full Name", "fullName", NAME_FULL, "Billing"},
      {"Address", "address", ADDRESS_HOME_STREET_ADDRESS, "Billing"},
      {"City", "city", ADDRESS_HOME_CITY, "Billing"},
      // Shipping
      {"Full Name", "fullName", NAME_FULL, "Shipping"},
      {"Address", "address", ADDRESS_HOME_STREET_ADDRESS, "Shipping"},
      {"City", "city", ADDRESS_HOME_CITY, "Shipping"},
  });

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms = {&form_structure};
  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);

  EXPECT_THAT(GetTypes(form_structure),
              ElementsAre(
                  // Billing:
                  NAME_FULL, ADDRESS_HOME_STREET_ADDRESS, ADDRESS_HOME_CITY,
                  // Shipping:
                  NAME_FULL, ADDRESS_HOME_STREET_ADDRESS, ADDRESS_HOME_CITY));
}

// Tests a form that has multiple sections with multiple number of address
// fields predicted as ADDRESS_HOME_STREET_ADDRESS. The last section
// doesn't happen in real world, because it is in fact two sections according to
// heuristics, and is only made for testing.
TEST_F(
    FormStructureRationalizerTest,
    RationalizeRepreatedFields_SectionTwoAddress_SectionThreeAddress_SectionFourAddresses) {
  auto [form, response_string] = CreateFormAndServerClassification({
      // Shipping.
      {"Full Name", "fullName", NAME_FULL, "Shipping"},
      {"Address", "address", ADDRESS_HOME_STREET_ADDRESS, "Shipping"},
      {"Address", "address", ADDRESS_HOME_STREET_ADDRESS, "Shipping"},
      {"City", "city", ADDRESS_HOME_CITY, "Shipping"},
      // Billing.
      {"Full Name", "fullName", NAME_FULL, "Billing"},
      {"Address", "address", ADDRESS_HOME_STREET_ADDRESS, "Billing"},
      {"Address", "address", ADDRESS_HOME_STREET_ADDRESS, "Billing"},
      {"Address", "address", ADDRESS_HOME_STREET_ADDRESS, "Billing"},
      {"City", "city", ADDRESS_HOME_CITY, "Billing"},
      // Work address (not realistic).
      {"Full Name", "fullName", NAME_FULL, "Work"},
      {"Address", "address", ADDRESS_HOME_STREET_ADDRESS, "Work"},
      {"Address", "address", ADDRESS_HOME_STREET_ADDRESS, "Work"},
      {"Address", "address", ADDRESS_HOME_STREET_ADDRESS, "Work"},
      {"Address", "address", ADDRESS_HOME_STREET_ADDRESS, "Work"},
      {"City", "city", ADDRESS_HOME_CITY, "Work"},
  });

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms = {&form_structure};
  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);

  EXPECT_THAT(
      GetTypes(form_structure),
      ElementsAre(
          // Shipping.
          NAME_FULL, ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2, ADDRESS_HOME_CITY,
          // Billing.
          NAME_FULL, ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2, ADDRESS_HOME_LINE3,
          ADDRESS_HOME_CITY,
          // Work address.
          NAME_FULL, ADDRESS_HOME_STREET_ADDRESS, ADDRESS_HOME_STREET_ADDRESS,
          ADDRESS_HOME_STREET_ADDRESS, ADDRESS_HOME_STREET_ADDRESS,
          ADDRESS_HOME_CITY));
}

// Tests that a form that has only one address in each section predicted as
// ADDRESS_HOME_STREET_ADDRESS is not modified by the address rationalization,
// while the sections are previously determined by the heuristics.
TEST_F(FormStructureRationalizerTest,
       RationalizeRepreatedFields_MultipleSectionsByHeuristics_OneAddressEach) {
  auto [form, response_string] = CreateFormAndServerClassification({
      // Billing.
      {"Full Name", "fullName", NAME_FULL},
      {"Address", "address", ADDRESS_HOME_STREET_ADDRESS},
      {"City", "city", ADDRESS_HOME_CITY},
      // Shipping.
      {"Full Name", "fullName", NAME_FULL},
      {"Address", "address", ADDRESS_HOME_STREET_ADDRESS},
      {"City", "city", ADDRESS_HOME_CITY},
  });

  FormStructure form_structure(form);
  // Will identify the sections based on the heuristics types.
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::vector<FormStructure*> forms = {&form_structure};
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);

  EXPECT_THAT(GetTypes(form_structure),
              ElementsAre(
                  // Billing.
                  NAME_FULL, ADDRESS_HOME_STREET_ADDRESS, ADDRESS_HOME_CITY,
                  // Shipping.
                  NAME_FULL, ADDRESS_HOME_STREET_ADDRESS, ADDRESS_HOME_CITY));
}

// Tests a form that has multiple sections with multiple number of address
// fields predicted as ADDRESS_HOME_STREET_ADDRESS, while the sections are
// identified by heuristics.
TEST_F(
    FormStructureRationalizerTest,
    RationalizeRepreatedFields_MultipleSectionsByHeuristics_TwoAddress_ThreeAddress) {
  auto [form, response_string] = CreateFormAndServerClassification({
      // Shipping
      {"Full Name", "fullName", NAME_FULL},
      {"Address", "address", ADDRESS_HOME_STREET_ADDRESS},
      {"Address", "address", ADDRESS_HOME_STREET_ADDRESS},
      {"City", "city", ADDRESS_HOME_CITY},
      // Billing
      {"Full Name", "fullName", NAME_FULL},
      {"Address", "address", ADDRESS_HOME_STREET_ADDRESS},
      {"Address", "address", ADDRESS_HOME_STREET_ADDRESS},
      {"Address", "address", ADDRESS_HOME_STREET_ADDRESS},
      {"City", "city", ADDRESS_HOME_CITY},
  });

  FormStructure form_structure(form);
  // Will identify the sections based on the heuristics types.
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::vector<FormStructure*> forms = {&form_structure};
  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);

  EXPECT_THAT(
      GetTypes(form_structure),
      ElementsAre(
          // Shipping.
          NAME_FULL, ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2, ADDRESS_HOME_CITY,
          // Billing.
          NAME_FULL, ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2, ADDRESS_HOME_LINE3,
          ADDRESS_HOME_CITY));
}

TEST_F(FormStructureRationalizerTest,
       RationalizeRepreatedFields_StateCountry_NoRationalization) {
  auto [form, response_string] = CreateFormAndServerClassification({
      // First Section
      {"Full Name", "fullName", NAME_FULL},
      {"State", "state", ADDRESS_HOME_STATE},
      {"Country", "country", ADDRESS_HOME_COUNTRY},
      // Second Section
      {"Country", "country", ADDRESS_HOME_COUNTRY},
      {"Full Name", "fullName", NAME_FULL},
      {"State", "state", ADDRESS_HOME_STATE},
      // Third Section
      {"Full Name", "fullName", NAME_FULL},
      {"State", "state", ADDRESS_HOME_STATE},
      // Fourth Section
      {"Full Name", "fullName", NAME_FULL},
      {"Country", "country", ADDRESS_HOME_COUNTRY},
  });

  FormStructure form_structure(form);
  // Will identify the sections based on the heuristics types.
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::vector<FormStructure*> forms = {&form_structure};
  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);

  EXPECT_THAT(GetTypes(form_structure),
              ElementsAre(
                  // First section.
                  NAME_FULL, ADDRESS_HOME_STATE, ADDRESS_HOME_COUNTRY,
                  // Second section.
                  ADDRESS_HOME_COUNTRY, NAME_FULL, ADDRESS_HOME_STATE,
                  // Third section.
                  NAME_FULL, ADDRESS_HOME_STATE,
                  // Fourth section.
                  NAME_FULL, ADDRESS_HOME_COUNTRY));
}

TEST_F(FormStructureRationalizerTest,
       RationalizeRepreatedFields_CountryStateNoHeuristics) {
  auto [form, response_string] = CreateFormAndServerClassification({
      // Shipping.
      {"Full Name", "fullName", NAME_FULL, "shipping"},
      {"City", "city", ADDRESS_HOME_CITY, "shipping"},
      {"State", "state", ADDRESS_HOME_STATE, "shipping"},
      {"Country", "country", ADDRESS_HOME_STATE, "shipping"},

      // Billing.
      ToSelectOne(ToNotFocusable(
          {"Country", "country2", ADDRESS_HOME_STATE, "billing"})),
      ToSelectOne({"Country", "country", ADDRESS_HOME_STATE, "billing"}),
      ToSelectOne(ToNotFocusable(
          {"Country", "country2", ADDRESS_HOME_STATE, "billing"})),
      ToSelectOne(ToNotFocusable(
          {"Country", "country2", ADDRESS_HOME_STATE, "billing"})),
      ToSelectOne(ToNotFocusable(
          {"Country", "country2", ADDRESS_HOME_STATE, "billing"})),
      ToSelectOne({"Full Name", "fullName", NAME_FULL, "billing"}),
      {"State", "state", ADDRESS_HOME_STATE, "billing"},

      // Billing-2.
      {"Country", "country", ADDRESS_HOME_STATE, "billing-2"},
      {"Full Name", "fullName", NAME_FULL, "billing-2"},
      {"State", "state", ADDRESS_HOME_STATE, "billing-2"},
  });

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms = {&form_structure};
  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);

  EXPECT_THAT(GetTypes(form_structure),
              ElementsAre(
                  // Shipping.
                  NAME_FULL, ADDRESS_HOME_CITY, ADDRESS_HOME_STATE,
                  ADDRESS_HOME_COUNTRY,
                  // Billing.
                  ADDRESS_HOME_COUNTRY, ADDRESS_HOME_COUNTRY,
                  ADDRESS_HOME_COUNTRY, ADDRESS_HOME_COUNTRY,
                  ADDRESS_HOME_COUNTRY, NAME_FULL, ADDRESS_HOME_STATE,
                  // Billing-2.
                  ADDRESS_HOME_COUNTRY, NAME_FULL, ADDRESS_HOME_STATE));
}

TEST_F(FormStructureRationalizerTest,
       RationalizeRepreatedFields_StateCountryWithHeuristics) {
  auto [form, response_string] = CreateFormAndServerClassification({
      // First section.
      {"Full Name", "fullName", NAME_FULL},
      ToSelectOne(ToNotFocusable({"Country", "country", ADDRESS_HOME_COUNTRY})),
      {"Country", "country2", ADDRESS_HOME_COUNTRY},
      {"city", "City", ADDRESS_HOME_CITY},
      ToSelectOne(
          SetRolePresentation({"State", "state2", ADDRESS_HOME_COUNTRY})),
      {"State", "state", ADDRESS_HOME_COUNTRY},

      // Second Section
      {"Country", "country", ADDRESS_HOME_COUNTRY},
      {"city", "City", ADDRESS_HOME_CITY},
      {"State", "state", ADDRESS_HOME_COUNTRY},

      // Third Section
      {"city", "City", ADDRESS_HOME_CITY},
      ToSelectOne(
          SetRolePresentation({"State", "state2", ADDRESS_HOME_COUNTRY})),
      {"State", "state", ADDRESS_HOME_COUNTRY},
      {"Country", "country", ADDRESS_HOME_COUNTRY},
      ToSelectOne(
          ToNotFocusable({"Country", "country2", ADDRESS_HOME_COUNTRY})),
  });

  FormStructure form_structure(form);
  // Will identify the sections based on the heuristics types.
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::vector<FormStructure*> forms = {&form_structure};
  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);

  EXPECT_THAT(GetTypes(form_structure),
              ElementsAre(
                  // First section.
                  NAME_FULL, ADDRESS_HOME_COUNTRY, ADDRESS_HOME_COUNTRY,
                  ADDRESS_HOME_CITY, ADDRESS_HOME_STATE, ADDRESS_HOME_STATE,
                  // Second section
                  ADDRESS_HOME_COUNTRY, ADDRESS_HOME_CITY, ADDRESS_HOME_STATE,
                  // Third section
                  ADDRESS_HOME_CITY, ADDRESS_HOME_STATE, ADDRESS_HOME_STATE,
                  ADDRESS_HOME_COUNTRY, ADDRESS_HOME_COUNTRY));
}

TEST_F(FormStructureRationalizerTest,
       RationalizeRepreatedFields_FirstFieldRationalized) {
  auto [form, response_string] = CreateFormAndServerClassification({
      {"Country", "country", ADDRESS_HOME_STATE, "billing"},
      ToSelectOne(ToNotFocusable(
          {"Country", "country2", ADDRESS_HOME_STATE, "billing"})),
      ToSelectOne(ToNotFocusable(
          {"Country", "country3", ADDRESS_HOME_STATE, "billing"})),
      {"Full Name", "fullName", NAME_FULL, "billing"},
      {"State", "state", ADDRESS_HOME_STATE, "billing"},
  });

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms = {&form_structure};
  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);

  EXPECT_THAT(GetTypes(form_structure),
              ElementsAre(ADDRESS_HOME_COUNTRY, ADDRESS_HOME_COUNTRY,
                          ADDRESS_HOME_COUNTRY, NAME_FULL, ADDRESS_HOME_STATE));
}

TEST_F(FormStructureRationalizerTest,
       RationalizeRepreatedFields_LastFieldRationalized) {
  auto [form, response_string] = CreateFormAndServerClassification({
      {"Country", "country", ADDRESS_HOME_COUNTRY, "billing"},
      ToSelectOne(ToNotFocusable(
          {"Country", "country2", ADDRESS_HOME_COUNTRY, "billing"})),
      ToSelectOne(ToNotFocusable(
          {"Country", "country3", ADDRESS_HOME_COUNTRY, "billing"})),
      ToSelectOne({"Full Name", "fullName", NAME_FULL, "billing"}),
      ToSelectOne(
          ToNotFocusable({"State", "state", ADDRESS_HOME_COUNTRY, "billing"})),
      ToSelectOne({"State", "state2", ADDRESS_HOME_COUNTRY, "billing"}),
  });

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms = {&form_structure};
  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);

  EXPECT_THAT(GetTypes(form_structure),
              ElementsAre(ADDRESS_HOME_COUNTRY, ADDRESS_HOME_COUNTRY,
                          ADDRESS_HOME_COUNTRY, NAME_FULL, ADDRESS_HOME_STATE,
                          ADDRESS_HOME_STATE));
}

struct RationalizationTypeRelationshipsTestParams {
  ServerFieldType server_type;
  ServerFieldType required_type;
};
class RationalizationFieldTypeFilterTest
    : public testing::Test,
      public testing::WithParamInterface<ServerFieldType> {
  test::AutofillEnvironment autofill_environment_;
};
class RationalizationFieldTypeRelationshipsTest
    : public testing::Test,
      public testing::WithParamInterface<
          RationalizationTypeRelationshipsTestParams> {
  test::AutofillEnvironment autofill_environment_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         RationalizationFieldTypeFilterTest,
                         testing::Values(PHONE_HOME_COUNTRY_CODE));

INSTANTIATE_TEST_SUITE_P(All,
                         RationalizationFieldTypeRelationshipsTest,
                         testing::Values(
                             RationalizationTypeRelationshipsTestParams{
                                 PHONE_HOME_COUNTRY_CODE, PHONE_HOME_NUMBER},
                             RationalizationTypeRelationshipsTestParams{
                                 PHONE_HOME_COUNTRY_CODE,
                                 PHONE_HOME_CITY_AND_NUMBER}));

// Tests that the rationalization logic will filter out fields of type |param|
// when there is no other required type.
TEST_P(RationalizationFieldTypeFilterTest, Rationalization_Rules_Filter_Out) {
  ServerFieldType filtered_off_field = GetParam();

  // Just adding >=3 random fields to trigger rationalization.
  auto [form, response_string] = CreateFormAndServerClassification(
      {{"First Name", "firstName", NAME_FIRST},
       {"Last Name", "lastName", NAME_LAST},
       {"Address", "address", ADDRESS_HOME_LINE1},
       {"Something under test", "tested-thing", filtered_off_field}});

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms = {&form_structure};
  // Will identify the sections based on the heuristics types.
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);

  EXPECT_THAT(
      GetTypes(form_structure),
      ElementsAre(NAME_FIRST, NAME_LAST, ADDRESS_HOME_LINE1,
                  // Last field's type should have been overwritten to expected.
                  UNKNOWN_TYPE));
}

// Tests that the rationalization logic will not filter out fields of type
// |param| when there is another field with a required type.
TEST_P(RationalizationFieldTypeRelationshipsTest,
       Rationalization_Rules_Relationships) {
  RationalizationTypeRelationshipsTestParams test_params = GetParam();

  // Just adding >=3 random fields to trigger rationalization.
  auto [form, response_string] = CreateFormAndServerClassification(
      {{"First Name", "firstName", NAME_FIRST},
       {"Last Name", "lastName", NAME_LAST},
       {"Some field with required type", "some-name",
        test_params.required_type},
       {"Something under test", "tested-thing", test_params.server_type}});

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms = {&form_structure};
  // Will identify the sections based on the heuristics types.
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);

  EXPECT_THAT(
      GetTypes(form_structure),
      ElementsAre(NAME_FIRST, NAME_LAST, test_params.required_type,
                  // Last field's type should have been overwritten to expected.
                  test_params.server_type));
}

}  // namespace
}  // namespace autofill
