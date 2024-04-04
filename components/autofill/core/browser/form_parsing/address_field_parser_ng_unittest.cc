// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/address_field_parser_ng.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

namespace {
void InitFeaturesForIN(base::test::ScopedFeatureList& features) {
  features.InitWithFeatures(
      {
          features::kAutofillEnableDependentLocalityParsing,
          features::kAutofillEnableSupportForLandmark,
          features::kAutofillEnableSupportForAdminLevel2,
          features::kAutofillUseINAddressModel,
          features::kAutofillStructuredFieldsDisableAddressLines,
      },
      {});
}
}  // namespace

class AddressFieldParserTestNG
    : public FormFieldParserTestBase,
      public ::testing::TestWithParam<PatternProviderFeatureState> {
 public:
  AddressFieldParserTestNG() : FormFieldParserTestBase(GetParam()) {}
  AddressFieldParserTestNG(const AddressFieldParserTestNG&) = delete;
  AddressFieldParserTestNG& operator=(const AddressFieldParserTestNG&) = delete;

 protected:
  std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                         AutofillScanner* scanner) override {
    return AddressFieldParserNG::Parse(context, scanner);
  }

  base::test::ScopedFeatureList default_features{
      features::kAutofillUseI18nAddressModel};
};

INSTANTIATE_TEST_SUITE_P(
    AddressFieldParserTestNG,
    AddressFieldParserTestNG,
    ::testing::ValuesIn(PatternProviderFeatureState::All()));

TEST_P(AddressFieldParserTestNG, Empty) {
  ClassifyAndVerify(ParseResult::kNotParsed);
}

TEST_P(AddressFieldParserTestNG, NonParse) {
  AddTextFormFieldData("", "", UNKNOWN_TYPE);
  ClassifyAndVerify(ParseResult::kNotParsed);
}

TEST_P(AddressFieldParserTestNG, ParseOneLineAddress) {
  AddTextFormFieldData("address", "Address", ADDRESS_HOME_STREET_ADDRESS);
  ClassifyAndVerify();
}

TEST_P(AddressFieldParserTestNG, ParseTwoLineAddress) {
  AddTextFormFieldData("address", "Address", ADDRESS_HOME_LINE1);
  AddTextFormFieldData("address2", "Address", ADDRESS_HOME_LINE2);
  ClassifyAndVerify();
}

TEST_P(AddressFieldParserTestNG, ParseThreeLineAddress) {
  AddTextFormFieldData("Address1", "Address Line 1", ADDRESS_HOME_LINE1);
  AddTextFormFieldData("Address1", "Address Line 2", ADDRESS_HOME_LINE2);
  AddTextFormFieldData("Address1", "Address Line 3", ADDRESS_HOME_LINE3);
  ClassifyAndVerify();
}

TEST_P(AddressFieldParserTestNG, ParseStreetAddressFromTextArea) {
  AddFormFieldData(FormControlType::kTextArea, "address", "Address",
                   ADDRESS_HOME_STREET_ADDRESS);
  ClassifyAndVerify();
}

// Tests that fields are classified as |ADDRESS_HOME_STREET_NAME| and
// |ADDRESS_HOME_HOUSE_NUMBER| when they are labeled accordingly and
// both are present.
TEST_P(AddressFieldParserTestNG, ParseStreetNameAndHouseNumber) {
  AddTextFormFieldData("street", "Street", ADDRESS_HOME_STREET_NAME);
  AddTextFormFieldData("house-number", "House number",
                       ADDRESS_HOME_HOUSE_NUMBER);
  ClassifyAndVerify();
}

// Tests that fields are classified as |ADDRESS_HOME_STREET_NAME|, and
// |ADDRESS_HOME_HOUSE_NUMBER| |ADDRESS_HOME_APT_NUM| when they are labeled
// accordingly and all are present.
TEST_P(AddressFieldParserTestNG,
       ParseStreetNameAndHouseNumberAndApartmentNumber) {
  // TODO(crbug.com/1125978): Remove once launched.
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(
      features::kAutofillEnableSupportForApartmentNumbers);

  AddTextFormFieldData("street", "Street", ADDRESS_HOME_STREET_NAME);
  AddTextFormFieldData("house-number", "House number",
                       ADDRESS_HOME_HOUSE_NUMBER);
  AddTextFormFieldData("apartment", "apartment", ADDRESS_HOME_APT_NUM);
  ClassifyAndVerify(ParseResult::kParsed, GeoIpCountryCode("XX"),
                    LanguageCode("en"));
}

// Tests that an address field after a |ADDRESS_HOME_STREET_NAME|,
// |ADDRESS_HOME_HOUSE_NUMBER| combination is classified as
// |ADDRESS_HOME_LINE2| instead of |ADDRESS_HOME_LINE1|.
TEST_P(AddressFieldParserTestNG, ParseAsAddressLine2AfterStreetNameNotEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillStructuredFieldsDisableAddressLines);
  AddTextFormFieldData("street", "Street", ADDRESS_HOME_STREET_NAME);
  AddTextFormFieldData("house-number", "House no.", ADDRESS_HOME_HOUSE_NUMBER);
  AddTextFormFieldData("address", "Address", ADDRESS_HOME_LINE2);
  ClassifyAndVerify();
}

TEST_P(AddressFieldParserTestNG, ParseAsAddressLine2AfterStreetNameEnabled) {
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
TEST_P(AddressFieldParserTestNG, NotParseStreetNameWithoutHouseNumber) {
  AddTextFormFieldData("street", "Street", ADDRESS_HOME_STREET_ADDRESS);
  ClassifyAndVerify();
}

// Tests that the field is not classified as |ADDRESS_HOME_HOUSE_NUMBER| when
// it is labeled accordingly but adjacent field classified as
// |ADDRESS_HOME_STREET_NAME| is absent.
TEST_P(AddressFieldParserTestNG, NotParseHouseNumberWithoutStreetName) {
  AddTextFormFieldData("house-number", "House number", UNKNOWN_TYPE);
  ClassifyAndVerify(ParseResult::kNotParsed);
}

// Tests that the dependent locality is correctly classified with
// an unambiguous field name and label.
TEST_P(AddressFieldParserTestNG, ParseDependentLocality) {
  // TODO(crbug.com/1157405): Remove once launched.
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(
      features::kAutofillEnableDependentLocalityParsing);

  AddTextFormFieldData("neighborhood", "Neighborhood",
                       ADDRESS_HOME_DEPENDENT_LOCALITY);
  ClassifyAndVerify();
}

// Tests that the landmark is correctly classified.
TEST_P(AddressFieldParserTestNG, ParseLandmark) {
  // TODO(crbug.com/1441904): Remove once launched.
  base::test::ScopedFeatureList enabled;
  enabled.InitWithFeatures({features::kAutofillUseBRAddressModel,
                            features::kAutofillEnableSupportForLandmark},
                           {});

  AddTextFormFieldData("landmark", "Landmark", ADDRESS_HOME_LANDMARK);
  ClassifyAndVerify(ParseResult::kParsed, GeoIpCountryCode("BR"),
                    LanguageCode("pt"));
}

// Tests that between streets field is correctly classified.
TEST_P(AddressFieldParserTestNG, ParseBetweenStreets) {
  // TODO(crbug.com/1441904): Remove once launched.
  base::test::ScopedFeatureList enabled;
  enabled.InitWithFeatures({features::kAutofillUseMXAddressModel,
                            features::kAutofillEnableSupportForBetweenStreets},
                           {});

  AddTextFormFieldData("entre-calles", "Entre calles",
                       ADDRESS_HOME_BETWEEN_STREETS);
  ClassifyAndVerify(ParseResult::kParsed, GeoIpCountryCode("MX"),
                    LanguageCode("es"));
}

// Tests that multiple between streets field are correctly classified.
TEST_P(AddressFieldParserTestNG, ParseBetweenStreetsLines) {
  // TODO(crbug.com/1441904): Remove once launched.
  base::test::ScopedFeatureList enabled;
  enabled.InitWithFeatures({features::kAutofillUseMXAddressModel,
                            features::kAutofillEnableSupportForBetweenStreets},
                           {});

  std::vector<std::pair<std::pair<std::string, std::string>,
                        std::pair<std::string, std::string>>>
      // "Name", "Label" for ADDRESS_HOME_BETWEEN_STREETS_1
      // "Name", "Label" for ADDRESS_HOME_BETWEEN_STREETS_2
      instances = {{{"entre-calle1", "Entre calle 1"},
                    {"entre-calle2", "Entre calle 2"}},
                   {{"entre-calle1", ""}, {"entre-calle2", ""}},
                   {{"entre-calle", ""}, {"entre-calle", ""}},
                   {{"entre-calle", ""}, {"y-calle", ""}},
                   {{"", "Entre calle 1"}, {"", "Entre calle 2"}}};

  for (const auto& [first_field, second_field] : instances) {
    ClearFieldsAndExpectations();
    AddTextFormFieldData(first_field.first, first_field.second,
                         ADDRESS_HOME_BETWEEN_STREETS_1);
    AddTextFormFieldData(second_field.first, second_field.second,
                         ADDRESS_HOME_BETWEEN_STREETS_2);
    ClassifyAndVerify(ParseResult::kParsed, GeoIpCountryCode("MX"),
                      LanguageCode("es"));
  }
}

// Tests that address level 2 field is correctly classified.
TEST_P(AddressFieldParserTestNG, ParseAdminLevel2) {
  // TODO(crbug.com/1441904): Remove once launched.
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(features::kAutofillEnableSupportForAdminLevel2);

  AddTextFormFieldData("municipio", "Municipio", ADDRESS_HOME_ADMIN_LEVEL2);
  ClassifyAndVerify(ParseResult::kParsed, GeoIpCountryCode("MX"),
                    LanguageCode("es"));
}

// Tests that overflow field is correctly classified.
TEST_P(AddressFieldParserTestNG, ParseOverflow) {
  // TODO(crbug.com/1441904): Remove once launched.
  base::test::ScopedFeatureList enabled;
  enabled.InitWithFeatures({features::kAutofillUseBRAddressModel,
                            features::kAutofillEnableSupportForAddressOverflow},
                           {});

  AddTextFormFieldData("complemento", "Complemento", ADDRESS_HOME_OVERFLOW);
  ClassifyAndVerify(ParseResult::kParsed, GeoIpCountryCode("BR"),
                    LanguageCode("pt"));
}

// Tests that overflow field is correctly classified.
TEST_P(AddressFieldParserTestNG, ParseOverflowAndLandmark) {
  // TODO(crbug.com/1441904): Remove once launched.
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/
      {features::kAutofillUseBRAddressModel,
       features::kAutofillEnableSupportForAddressOverflow,
       features::kAutofillEnableSupportForAddressOverflowAndLandmark},
      /*disabled_features=*/{});

  AddTextFormFieldData("additional_info", "Complemento e ponto de referência",
                       ADDRESS_HOME_OVERFLOW_AND_LANDMARK);
  ClassifyAndVerify(ParseResult::kParsed, GeoIpCountryCode("BR"),
                    LanguageCode("pt"));
}

TEST_P(AddressFieldParserTestNG, ParseCity) {
  AddTextFormFieldData("city", "City", ADDRESS_HOME_CITY);
  ClassifyAndVerify();
}

TEST_P(AddressFieldParserTestNG, ParseState) {
  AddTextFormFieldData("state", "State", ADDRESS_HOME_STATE);
  ClassifyAndVerify();
}

TEST_P(AddressFieldParserTestNG, ParseZip) {
  AddTextFormFieldData("zip", "Zip", ADDRESS_HOME_ZIP);
  ClassifyAndVerify();
}

TEST_P(AddressFieldParserTestNG, ParseZipFileExtension) {
  AddTextFormFieldData("filename", "Supported formats: .zip, .rar",
                       UNKNOWN_TYPE);
  ClassifyAndVerify(ParseResult::kNotParsed);
}

TEST_P(AddressFieldParserTestNG, ParseStateAndZipOneLabel) {
  AddTextFormFieldData("state", "State/Province, Zip/Postal Code",
                       ADDRESS_HOME_STATE);
  AddTextFormFieldData("zip", "State/Province, Zip/Postal Code",
                       ADDRESS_HOME_ZIP);
  ClassifyAndVerify();
}

TEST_P(AddressFieldParserTestNG, ParseCountry) {
  AddTextFormFieldData("country", "Country", ADDRESS_HOME_COUNTRY);
  ClassifyAndVerify();
}

TEST_P(AddressFieldParserTestNG, ParseCompany) {
  AddTextFormFieldData("company", "Company", COMPANY_NAME);
  ClassifyAndVerify();
}

// Tests that the dependent locality, city, state, country and zip-code
// fields are correctly classfied with unambiguous field names and labels.
TEST_P(AddressFieldParserTestNG,
       ParseDependentLocalityCityStateCountryZipcodeTogether) {
  // TODO(crbug.com/1157405): Remove once launched.
  base::test::ScopedFeatureList enabled;
  enabled.InitWithFeatures(
      {
          features::kAutofillUseMXAddressModel,
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
  AddTextFormFieldData("entre-calles", "Entre calles",
                       ADDRESS_HOME_BETWEEN_STREETS);
  AddTextFormFieldData("municipio", "Municipio", ADDRESS_HOME_ADMIN_LEVEL2);
  ClassifyAndVerify(ParseResult::kParsed, GeoIpCountryCode("MX"),
                    LanguageCode("es"));
}

// Tests that the field is classified as |ADDRESS_HOME_COUNTRY| when the field
// contains 'region'.
TEST_P(AddressFieldParserTestNG, ParseAmbiguousCountryState) {
  // The strings are ambiguous between country and state. Country should be
  // preferred.
  AddTextFormFieldData("country/region", "asdf", ADDRESS_HOME_COUNTRY);
  ClassifyAndVerify();
}
TEST_P(AddressFieldParserTestNG, ParseAmbiguousCountryState2) {
  // The strings are ambiguous between country and state. Country should be
  // preferred.
  AddTextFormFieldData("asdf", "country/region", ADDRESS_HOME_COUNTRY);
  ClassifyAndVerify();
}

// Tests that city and state fields are classified correctly when their names
// contain keywords for different types. This is achieved by giving the priority
// to the label over the name for pages in Turkish.
TEST_P(AddressFieldParserTestNG, ParseTurkishCityStateWithLabelPrecedence) {
  // TODO(crbug.com/1156315): Remove once launched.
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(
      features::kAutofillEnableLabelPrecedenceForTurkishAddresses);

  AddTextFormFieldData("city", "Il", ADDRESS_HOME_STATE);
  AddTextFormFieldData("county", "Ilce", ADDRESS_HOME_CITY);
  ClassifyAndVerify(ParseResult::kParsed, GeoIpCountryCode("TR"),
                    LanguageCode("tr"));
}

// Tests that address name is not misclassified as address.
TEST_P(AddressFieldParserTestNG, NotParseAddressName_TR) {
  AddTextFormFieldData("address", "Adres Başlığı", UNKNOWN_TYPE);
  ClassifyAndVerify(ParseResult::kNotParsed, GeoIpCountryCode("TR"),
                    LanguageCode("tr"));
}

// Tests that an address name does not lead to a classification even if the
// field mentions the word city.
TEST_P(AddressFieldParserTestNG, NotParseAddressName_BR) {
  AddTextFormFieldData("-", "nombre de la dirección, city", UNKNOWN_TYPE);
  ClassifyAndVerify(ParseResult::kNotParsed, GeoIpCountryCode("BR"),
                    LanguageCode("es"));
}

// Tests that the address components sequence in a label is classified
// as |ADDRESS_HOME_STREET_ADDRESS| if no further fields for address line 2
// exist.
TEST_P(AddressFieldParserTestNG, ParseAddressComponentsSequenceAsAddressLine1) {
  AddTextFormFieldData("detail", "Улица, дом, квартира",
                       ADDRESS_HOME_STREET_ADDRESS);
  ClassifyAndVerify(ParseResult::kParsed, GeoIpCountryCode("RU"),
                    LanguageCode("ru"));
}

// Tests that the address components sequence in a label is classified
// as |ADDRESS_HOME_STREET_ADDRESS|.
TEST_P(AddressFieldParserTestNG,
       ParseAddressComponentsSequenceAsStreetAddress) {
  AddFormFieldData(FormControlType::kTextArea, "detail",
                   "Mahalle, sokak, cadde ve diğer bilgilerinizi girin",
                   ADDRESS_HOME_STREET_ADDRESS);
  ClassifyAndVerify(ParseResult::kParsed, GeoIpCountryCode("TR"),
                    LanguageCode("tr"));
}

// Tests that the three street address components are correctly classified.
TEST_P(AddressFieldParserTestNG, IN_Building_Locality_Landmark) {
  base::test::ScopedFeatureList enabled;
  InitFeaturesForIN(enabled);
  AddTextFormFieldData("building", "building", ADDRESS_HOME_STREET_LOCATION);
  AddTextFormFieldData("locality", "locality", ADDRESS_HOME_DEPENDENT_LOCALITY);
  AddTextFormFieldData("landmark", "landmark", ADDRESS_HOME_LANDMARK);
  ClassifyAndVerify(ParseResult::kParsed, GeoIpCountryCode("IN"),
                    LanguageCode("en"));
}

// Tests that the three street address components are correctly classified even
// if they are labeled address line 1, 2, and landmark.
TEST_P(AddressFieldParserTestNG, IN_Building_Locality_Landmark_AsAddressLines) {
  base::test::ScopedFeatureList enabled;
  InitFeaturesForIN(enabled);
  AddTextFormFieldData("address1", "Building", ADDRESS_HOME_STREET_LOCATION);
  AddTextFormFieldData("address2", "Locality", ADDRESS_HOME_DEPENDENT_LOCALITY);
  AddTextFormFieldData("landmark", "Landmark", ADDRESS_HOME_LANDMARK);
  ClassifyAndVerify(ParseResult::kParsed, GeoIpCountryCode("IN"),
                    LanguageCode("en"));
}

// Tests that the street address in India are correctly classified if the
// locality and landmark are combined in one field.
TEST_P(AddressFieldParserTestNG, IN_Building_LocalityAndLandmark) {
  base::test::ScopedFeatureList enabled;
  InitFeaturesForIN(enabled);
  AddTextFormFieldData("address1", "Building", ADDRESS_HOME_STREET_LOCATION);
  AddTextFormFieldData("address2", "Locality/Landmark",
                       ADDRESS_HOME_DEPENDENT_LOCALITY_AND_LANDMARK);
  ClassifyAndVerify(ParseResult::kParsed, GeoIpCountryCode("IN"),
                    LanguageCode("en"));
}

// Tests that the street address in India are correctly classified if the
// building and locality are combined in one field.
TEST_P(AddressFieldParserTestNG, IN_BuildingAndLocality_Landmark) {
  base::test::ScopedFeatureList enabled;
  InitFeaturesForIN(enabled);
  AddTextFormFieldData("address1", "Building/Locality",
                       ADDRESS_HOME_STREET_LOCATION_AND_LOCALITY);
  AddTextFormFieldData("address2", "Landmark", ADDRESS_HOME_LANDMARK);
  ClassifyAndVerify(ParseResult::kParsed, GeoIpCountryCode("IN"),
                    LanguageCode("en"));
}

// Tests that the street address in India are correctly classified if the
// building and landmark are combined in one field.
TEST_P(AddressFieldParserTestNG, IN_BuildingAndLandmark_Locality) {
  base::test::ScopedFeatureList enabled;
  InitFeaturesForIN(enabled);
  AddTextFormFieldData("address1", "Building/Landmark",
                       ADDRESS_HOME_STREET_LOCATION_AND_LANDMARK);
  AddTextFormFieldData("address2", "Locality", ADDRESS_HOME_DEPENDENT_LOCALITY);
  ClassifyAndVerify(ParseResult::kParsed, GeoIpCountryCode("IN"),
                    LanguageCode("en"));
}

}  // namespace autofill
