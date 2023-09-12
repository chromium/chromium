// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/address_field.h"
#include "components/autofill/core/browser/field_types.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

class AddressFieldTest
    : public FormFieldTestBase,
      public ::testing::TestWithParam<PatternProviderFeatureState> {
 public:
  AddressFieldTest() : FormFieldTestBase(GetParam()) {}
  AddressFieldTest(const AddressFieldTest&) = delete;
  AddressFieldTest& operator=(const AddressFieldTest&) = delete;

 protected:
  std::unique_ptr<FormField> Parse(AutofillScanner* scanner,
                                   const GeoIpCountryCode& client_country,
                                   const LanguageCode& page_language) override {
    return AddressField::Parse(scanner, client_country, page_language,
                               *GetActivePatternSource(),
                               /*log_manager=*/nullptr);
  }
};

INSTANTIATE_TEST_SUITE_P(
    AddressFieldTest,
    AddressFieldTest,
    ::testing::ValuesIn(PatternProviderFeatureState::All()));

TEST_P(AddressFieldTest, Empty) {
  ClassifyAndVerify(ParseResult::NOT_PARSED);
}

TEST_P(AddressFieldTest, NonParse) {
  AddTextFormFieldData("", "", UNKNOWN_TYPE);
  ClassifyAndVerify(ParseResult::NOT_PARSED);
}

TEST_P(AddressFieldTest, ParseOneLineAddress) {
  AddTextFormFieldData("address", "Address", ADDRESS_HOME_LINE1);
  ClassifyAndVerify();
}

TEST_P(AddressFieldTest, ParseTwoLineAddress) {
  AddTextFormFieldData("address", "Address", ADDRESS_HOME_LINE1);
  AddTextFormFieldData("address2", "Address", ADDRESS_HOME_LINE2);
  ClassifyAndVerify();
}

TEST_P(AddressFieldTest, ParseThreeLineAddress) {
  AddTextFormFieldData("Address1", "Address Line 1", ADDRESS_HOME_LINE1);
  AddTextFormFieldData("Address1", "Address Line 2", ADDRESS_HOME_LINE2);
  AddTextFormFieldData("Address1", "Address Line 3", ADDRESS_HOME_LINE3);
  ClassifyAndVerify();
}

TEST_P(AddressFieldTest, ParseStreetAddressFromTextArea) {
  AddFormFieldData("textarea", "address", "Address",
                   ADDRESS_HOME_STREET_ADDRESS);
  ClassifyAndVerify();
}

// Tests that fields are classified as |ADDRESS_HOME_STREET_NAME| and
// |ADDRESS_HOME_HOUSE_NUMBER| when they are labeled accordingly and
// both are present.
TEST_P(AddressFieldTest, ParseStreetNameAndHouseNumber) {
  AddTextFormFieldData("street", "Street", ADDRESS_HOME_STREET_NAME);
  AddTextFormFieldData("house-number", "House number",
                       ADDRESS_HOME_HOUSE_NUMBER);
  ClassifyAndVerify();
}

// Tests that fields are classified as |ADDRESS_HOME_STREET_NAME|, and
// |ADDRESS_HOME_HOUSE_NUMBER| |ADDRESS_HOME_APT_NUM| when they are labeled
// accordingly and all are present.
TEST_P(AddressFieldTest, ParseStreetNameAndHouseNumberAndApartmentNumber) {
  // TODO(crbug.com/1125978): Remove once launched.
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(
      features::kAutofillEnableSupportForApartmentNumbers);

  AddTextFormFieldData("street", "Street", ADDRESS_HOME_STREET_NAME);
  AddTextFormFieldData("house-number", "House number",
                       ADDRESS_HOME_HOUSE_NUMBER);
  AddTextFormFieldData("apartment", "apartment", ADDRESS_HOME_APT_NUM);
  ClassifyAndVerify();
}

// Tests that an address field after a |ADDRESS_HOME_STREET_NAME|,
// |ADDRESS_HOME_HOUSE_NUMBER| combination is classified as
// |ADDRESS_HOME_LINE2| instead of |ADDRESS_HOME_LINE1|.
TEST_P(AddressFieldTest, ParseAsAddressLine2AfterStreetNameNotEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillStructuredFieldsDisableAddressLines);
  AddTextFormFieldData("street", "Street", ADDRESS_HOME_STREET_NAME);
  AddTextFormFieldData("house-number", "House no.", ADDRESS_HOME_HOUSE_NUMBER);
  AddTextFormFieldData("address", "Address", ADDRESS_HOME_LINE2);
  ClassifyAndVerify();
}

TEST_P(AddressFieldTest, ParseAsAddressLine2AfterStreetNameEnabled) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillStructuredFieldsDisableAddressLines);
  AddTextFormFieldData("street", "Street", ADDRESS_HOME_STREET_NAME);
  AddTextFormFieldData("house-number", "House no.", ADDRESS_HOME_HOUSE_NUMBER);
  AddTextFormFieldData("address", "Address", UNKNOWN_TYPE);
  ClassifyAndVerify();
}

// Tests that the field is not classified as |ADDRESS_HOME_STREET_NAME| when
// it is labeled accordingly but an adjacent field classified as
// |ADDRESS_HOME_HOUSE_NUMBER| is absent.
TEST_P(AddressFieldTest, NotParseStreetNameWithoutHouseNumber) {
  AddTextFormFieldData("street", "Street", ADDRESS_HOME_LINE1);
  ClassifyAndVerify();
}

// Tests that the field is not classified as |ADDRESS_HOME_HOUSE_NUMBER| when
// it is labeled accordingly but adjacent field classified as
// |ADDRESS_HOME_STREET_NAME| is absent.
TEST_P(AddressFieldTest, NotParseHouseNumberWithoutStreetName) {
  AddTextFormFieldData("house-number", "House number", UNKNOWN_TYPE);
  ClassifyAndVerify(ParseResult::NOT_PARSED);
}

// Tests that the dependent locality is correctly classified with
// an unambiguous field name and label.
TEST_P(AddressFieldTest, ParseDependentLocality) {
  // TODO(crbug.com/1157405): Remove once launched.
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(
      features::kAutofillEnableDependentLocalityParsing);

  AddTextFormFieldData("neighborhood", "Neighborhood",
                       ADDRESS_HOME_DEPENDENT_LOCALITY);
  ClassifyAndVerify();
}

// Tests that the landmark is correctly classified.
TEST_P(AddressFieldTest, ParseLandmark) {
  // TODO(crbug.com/1441904): Remove once launched.
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(features::kAutofillEnableSupportForLandmark);

  AddTextFormFieldData("landmark", "Landmark", ADDRESS_HOME_LANDMARK);
  ClassifyAndVerify();
}

// Tests that between streets field is correctly classified.
TEST_P(AddressFieldTest, ParseBetweenStreets) {
  // TODO(crbug.com/1441904): Remove once launched.
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(
      features::kAutofillEnableSupportForBetweenStreets);

  AddTextFormFieldData("entre-calle", "Entre calle",
                       ADDRESS_HOME_BETWEEN_STREETS);
  ClassifyAndVerify();
}

// Tests that address level 2 field is correctly classified.
TEST_P(AddressFieldTest, ParseAdminLevel2) {
  // TODO(crbug.com/1441904): Remove once launched.
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(features::kAutofillEnableSupportForAdminLevel2);

  AddTextFormFieldData("municipio", "Municipio", ADDRESS_HOME_ADMIN_LEVEL2);
  ClassifyAndVerify();
}

// Tests that overflow field is correctly classified.
TEST_P(AddressFieldTest, ParseOverflow) {
  // TODO(crbug.com/1441904): Remove once launched.
  base::test::ScopedFeatureList enabled(
      features::kAutofillEnableSupportForAddressOverflow);

  AddTextFormFieldData("complemento", "Complemento", ADDRESS_HOME_OVERFLOW);
  ClassifyAndVerify();
}

// Tests that overflow field is correctly classified.
TEST_P(AddressFieldTest, ParseOverflowAndLandmark) {
  // TODO(crbug.com/1441904): Remove once launched.
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/
      {features::kAutofillEnableSupportForAddressOverflow,
       features::kAutofillEnableSupportForAddressOverflowAndLandmark},
      /*disabled_features=*/{});

  AddTextFormFieldData("additional_info", "Complemento e ponto de referência",
                       ADDRESS_HOME_OVERFLOW_AND_LANDMARK);
  ClassifyAndVerify();
}

TEST_P(AddressFieldTest, ParseCity) {
  AddTextFormFieldData("city", "City", ADDRESS_HOME_CITY);
  ClassifyAndVerify();
}

TEST_P(AddressFieldTest, ParseState) {
  AddTextFormFieldData("state", "State", ADDRESS_HOME_STATE);
  ClassifyAndVerify();
}

TEST_P(AddressFieldTest, ParseZip) {
  AddTextFormFieldData("zip", "Zip", ADDRESS_HOME_ZIP);
  ClassifyAndVerify();
}

TEST_P(AddressFieldTest, ParseZipFileExtension) {
  AddTextFormFieldData("filename", "Supported formats: .zip, .rar",
                       UNKNOWN_TYPE);
  ClassifyAndVerify(ParseResult::NOT_PARSED);
}

TEST_P(AddressFieldTest, ParseStateAndZipOneLabel) {
  AddTextFormFieldData("state", "State/Province, Zip/Postal Code",
                       ADDRESS_HOME_STATE);
  AddTextFormFieldData("zip", "State/Province, Zip/Postal Code",
                       ADDRESS_HOME_ZIP);
  ClassifyAndVerify();
}

TEST_P(AddressFieldTest, ParseCountry) {
  AddTextFormFieldData("country", "Country", ADDRESS_HOME_COUNTRY);
  ClassifyAndVerify();
}

TEST_P(AddressFieldTest, ParseCompany) {
  AddTextFormFieldData("company", "Company", COMPANY_NAME);
  ClassifyAndVerify();
}

// Tests that the dependent locality, city, state, country and zip-code
// fields are correctly classfied with unambiguous field names and labels.
TEST_P(AddressFieldTest,
       ParseDependentLocalityCityStateCountryZipcodeTogether) {
  // TODO(crbug.com/1157405): Remove once launched.
  base::test::ScopedFeatureList enabled;
  enabled.InitWithFeatures(
      {
          features::kAutofillEnableSupportForAddressOverflow,
          features::kAutofillEnableDependentLocalityParsing,
          features::kAutofillEnableSupportForLandmark,
          features::kAutofillEnableSupportForBetweenStreets,
          features::kAutofillEnableSupportForAdminLevel2,
      },
      {});

  AddTextFormFieldData("neighborhood", "Neighborhood",
                       ADDRESS_HOME_DEPENDENT_LOCALITY);
  AddTextFormFieldData("city", "City", ADDRESS_HOME_CITY);
  AddTextFormFieldData("state", "State", ADDRESS_HOME_STATE);
  AddTextFormFieldData("country", "Country", ADDRESS_HOME_COUNTRY);
  AddTextFormFieldData("zip", "Zip", ADDRESS_HOME_ZIP);
  AddTextFormFieldData("landmark", "Landmark", ADDRESS_HOME_LANDMARK);
  AddTextFormFieldData("entre-calle", "Entre calle",
                       ADDRESS_HOME_BETWEEN_STREETS);
  AddTextFormFieldData("municipio", "Municipio", ADDRESS_HOME_ADMIN_LEVEL2);
  AddTextFormFieldData("complemento", "Complemento", ADDRESS_HOME_OVERFLOW);
  ClassifyAndVerify();
}

// Tests that the field is classified as |ADDRESS_HOME_COUNTRY| when the field
// contains 'region'.
TEST_P(AddressFieldTest, ParseAmbiguousCountryState) {
  // The strings are ambiguous between country and state. Country should be
  // preferred.
  AddTextFormFieldData("country/region", "asdf", ADDRESS_HOME_COUNTRY);
  ClassifyAndVerify();
}
TEST_P(AddressFieldTest, ParseAmbiguousCountryState2) {
  // The strings are ambiguous between country and state. Country should be
  // preferred.
  AddTextFormFieldData("asdf", "country/region", ADDRESS_HOME_COUNTRY);
  ClassifyAndVerify();
}

// Tests that city and state fields are classified correctly when their names
// contain keywords for different types. This is achieved by giving the priority
// to the label over the name for pages in Turkish.
TEST_P(AddressFieldTest, ParseTurkishCityStateWithLabelPrecedence) {
  // TODO(crbug.com/1156315): Remove once launched.
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(
      features::kAutofillEnableLabelPrecedenceForTurkishAddresses);

  AddTextFormFieldData("city", "Il", ADDRESS_HOME_STATE);
  AddTextFormFieldData("county", "Ilce", ADDRESS_HOME_CITY);
  ClassifyAndVerify(ParseResult::PARSED, LanguageCode("tr"));
}

// Tests that address name is not misclassified as address.
TEST_P(AddressFieldTest, NotParseAddressName) {
  AddTextFormFieldData("address", "Adres Başlığı", UNKNOWN_TYPE);
  ClassifyAndVerify(ParseResult::NOT_PARSED, LanguageCode("tr"));
}

// Tests that the address components sequence in a label is classified
// as |ADDRESS_HOME_LINE1|.
TEST_P(AddressFieldTest, ParseAddressComponentsSequenceAsAddressLine1) {
  AddTextFormFieldData("detail", "Улица, дом, квартира", ADDRESS_HOME_LINE1);
  ClassifyAndVerify(ParseResult::PARSED, LanguageCode("ru"));
}

// Tests that the address components sequence in a label is classified
// as |ADDRESS_HOME_STREET_ADDRESS|.
TEST_P(AddressFieldTest, ParseAddressComponentsSequenceAsStreetAddress) {
  AddFormFieldData("textarea", "detail",
                   "Mahalle, sokak, cadde ve diğer bilgilerinizi girin",
                   ADDRESS_HOME_STREET_ADDRESS);
  ClassifyAndVerify(ParseResult::PARSED, LanguageCode("tr"));
}

}  // namespace autofill
