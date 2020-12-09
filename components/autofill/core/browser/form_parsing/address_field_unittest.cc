// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/address_field.h"

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {

class AddressFieldTest : public testing::Test {
 public:
  AddressFieldTest() = default;
  AddressFieldTest(const AddressFieldTest&) = delete;
  AddressFieldTest& operator=(const AddressFieldTest&) = delete;

 protected:
  // Downcast for tests.
  static std::unique_ptr<AddressField> Parse(
      AutofillScanner* scanner,
      const LanguageCode& page_language) {
    std::unique_ptr<FormField> field =
        AddressField::Parse(scanner, page_language, nullptr);
    return std::unique_ptr<AddressField>(
        static_cast<AddressField*>(field.release()));
  }

  static std::unique_ptr<AddressField> Parse(AutofillScanner* scanner) {
    // An empty page_language means the language is unknown and patterns of all
    // languages are used.
    return Parse(scanner, LanguageCode(""));
  }

  FieldRendererId MakeFieldRendererId() {
    return FieldRendererId(++id_counter_);
  }

  std::vector<std::unique_ptr<AutofillField>> list_;
  std::unique_ptr<AddressField> field_;
  FieldCandidatesMap field_candidates_map_;

 private:
  uint64_t id_counter_ = 0;
};

TEST_F(AddressFieldTest, Empty) {
  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_EQ(nullptr, field_.get());
}

TEST_F(AddressFieldTest, NonParse) {
  list_.push_back(std::make_unique<AutofillField>());
  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_EQ(nullptr, field_.get());
}

TEST_F(AddressFieldTest, ParseOneLineAddress) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId addr1 = list_.back()->unique_renderer_id;

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassificationsForTesting(&field_candidates_map_);
  ASSERT_TRUE(field_candidates_map_.find(addr1) != field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_LINE1,
            field_candidates_map_[addr1].BestHeuristicType());
}

TEST_F(AddressFieldTest, ParseTwoLineAddress) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId addr1 = list_.back()->unique_renderer_id;

  field.label = base::string16();
  field.name = ASCIIToUTF16("address2");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId addr2 = list_.back()->unique_renderer_id;

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassificationsForTesting(&field_candidates_map_);
  ASSERT_TRUE(field_candidates_map_.find(addr1) != field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_LINE1,
            field_candidates_map_[addr1].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(addr2) != field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_LINE2,
            field_candidates_map_[addr2].BestHeuristicType());
}

TEST_F(AddressFieldTest, ParseThreeLineAddress) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Address Line1");
  field.name = ASCIIToUTF16("Address1");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId addr1 = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Address Line2");
  field.name = ASCIIToUTF16("Address2");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId addr2 = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Address Line3");
  field.name = ASCIIToUTF16("Address3");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId addr3 = list_.back()->unique_renderer_id;

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassificationsForTesting(&field_candidates_map_);
  ASSERT_TRUE(field_candidates_map_.find(addr1) != field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_LINE1,
            field_candidates_map_[addr1].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(addr2) != field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_LINE2,
            field_candidates_map_[addr2].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(addr3) != field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_LINE3,
            field_candidates_map_[addr3].BestHeuristicType());
}

TEST_F(AddressFieldTest, ParseStreetAddressFromTextArea) {
  FormFieldData field;
  field.form_control_type = "textarea";

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId addr = list_.back()->unique_renderer_id;

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassificationsForTesting(&field_candidates_map_);
  ASSERT_TRUE(field_candidates_map_.find(addr) != field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_STREET_ADDRESS,
            field_candidates_map_[addr].BestHeuristicType());
}

// Tests that fields are classified as |ADDRESS_HOME_STREET_NAME| and
// |ADDRESS_HOME_HOUSE_NUMBER| when they are labeled accordingly and
// both are present.
TEST_F(AddressFieldTest, ParseStreetNameAndHouseNumber) {
  // TODO(crbug.com/1125978): Remove once launched.
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(
      features::kAutofillEnableSupportForMoreStructureInAddresses);

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Street");
  field.name = ASCIIToUTF16("street");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId street = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("House number");
  field.name = ASCIIToUTF16("house-number");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId house = list_.back()->unique_renderer_id;

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassificationsForTesting(&field_candidates_map_);

  ASSERT_TRUE(field_candidates_map_.find(street) !=
              field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_STREET_NAME,
            field_candidates_map_[street].BestHeuristicType());

  ASSERT_TRUE(field_candidates_map_.find(house) != field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_HOUSE_NUMBER,
            field_candidates_map_[house].BestHeuristicType());
}

// Tests that the field is not classified as |ADDRESS_HOME_STREET_NAME| when
// it is labeled accordingly but adjacent field classified as
// |ADDRESS_HOME_HOUSE_NUMBER| is absent.
TEST_F(AddressFieldTest, NotParseStreetNameWithoutHouseNumber) {
  // TODO(crbug.com/1125978): Remove once launched.
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(
      features::kAutofillEnableSupportForMoreStructureInAddresses);

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Street");
  field.name = ASCIIToUTF16("street");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId street = list_.back()->unique_renderer_id;

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);

  if (!field_.get())
    return;
  field_->AddClassificationsForTesting(&field_candidates_map_);
  if (field_candidates_map_.empty())
    return;

  EXPECT_NE(ADDRESS_HOME_STREET_NAME,
            field_candidates_map_[street].BestHeuristicType());
}

// Tests that the field is not classified as |ADDRESS_HOME_HOUSE_NUMBER| when
// it is labeled accordingly but adjacent field classified as
// |ADDRESS_HOME_STREET_NAME| is absent.
TEST_F(AddressFieldTest, NotParseHouseNumberWithoutStreetName) {
  // TODO(crbug.com/1125978): Remove once launched.
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(
      features::kAutofillEnableSupportForMoreStructureInAddresses);

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("House number");
  field.name = ASCIIToUTF16("house-number");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId house = list_.back()->unique_renderer_id;

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);

  if (!field_.get())
    return;
  field_->AddClassificationsForTesting(&field_candidates_map_);
  if (field_candidates_map_.empty())
    return;

  EXPECT_NE(ADDRESS_HOME_HOUSE_NUMBER,
            field_candidates_map_[house].BestHeuristicType());
}

TEST_F(AddressFieldTest, ParseCity) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("City");
  field.name = ASCIIToUTF16("city");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId city1 = list_.back()->unique_renderer_id;

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassificationsForTesting(&field_candidates_map_);
  ASSERT_TRUE(field_candidates_map_.find(city1) != field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_CITY,
            field_candidates_map_[city1].BestHeuristicType());
}

TEST_F(AddressFieldTest, ParseState) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("State");
  field.name = ASCIIToUTF16("state");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId state1 = list_.back()->unique_renderer_id;

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassificationsForTesting(&field_candidates_map_);
  ASSERT_TRUE(field_candidates_map_.find(state1) !=
              field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_STATE,
            field_candidates_map_[state1].BestHeuristicType());
}

TEST_F(AddressFieldTest, ParseZip) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Zip");
  field.name = ASCIIToUTF16("zip");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId zip1 = list_.back()->unique_renderer_id;

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassificationsForTesting(&field_candidates_map_);
  ASSERT_TRUE(field_candidates_map_.find(zip1) != field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_ZIP, field_candidates_map_[zip1].BestHeuristicType());
}

TEST_F(AddressFieldTest, ParseStateAndZipOneLabel) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("State/Province, Zip/Postal Code");
  field.name = ASCIIToUTF16("state");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId state = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("State/Province, Zip/Postal Code");
  field.name = ASCIIToUTF16("zip");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId zip = list_.back()->unique_renderer_id;

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassificationsForTesting(&field_candidates_map_);
  ASSERT_TRUE(field_candidates_map_.find(state) != field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_STATE,
            field_candidates_map_[state].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(zip) != field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_ZIP, field_candidates_map_[zip].BestHeuristicType());
}

TEST_F(AddressFieldTest, ParseCountry) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId country1 = list_.back()->unique_renderer_id;

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassificationsForTesting(&field_candidates_map_);
  ASSERT_TRUE(field_candidates_map_.find(country1) !=
              field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY,
            field_candidates_map_[country1].BestHeuristicType());
}

TEST_F(AddressFieldTest, ParseCompany) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Company");
  field.name = ASCIIToUTF16("company");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId company1 = list_.back()->unique_renderer_id;

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassificationsForTesting(&field_candidates_map_);
  ASSERT_TRUE(field_candidates_map_.find(company1) !=
              field_candidates_map_.end());
  EXPECT_EQ(COMPANY_NAME, field_candidates_map_[company1].BestHeuristicType());
}

// Tests that the city, state, country and zip-code fields are correctly
// classfied with unambiguous field names and labels.
TEST_F(AddressFieldTest, ParseCityStateCountryZipcodeTogether) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("City");
  field.name = ASCIIToUTF16("city");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId city1 = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("State");
  field.name = ASCIIToUTF16("state");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId state1 = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId country1 = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Zip");
  field.name = ASCIIToUTF16("zip");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId zip1 = list_.back()->unique_renderer_id;

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassificationsForTesting(&field_candidates_map_);

  ASSERT_TRUE(field_candidates_map_.find(city1) != field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_CITY,
            field_candidates_map_[city1].BestHeuristicType());

  ASSERT_TRUE(field_candidates_map_.find(state1) !=
              field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_STATE,
            field_candidates_map_[state1].BestHeuristicType());

  ASSERT_TRUE(field_candidates_map_.find(country1) !=
              field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY,
            field_candidates_map_[country1].BestHeuristicType());

  ASSERT_TRUE(field_candidates_map_.find(zip1) != field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_ZIP, field_candidates_map_[zip1].BestHeuristicType());
}

// Tests that the field is classified as |ADDRESS_HOME_COUNTRY| when the field
// label contains 'Region'.
TEST_F(AddressFieldTest, ParseCountryLabelRegion) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Country/Region");
  field.name = ASCIIToUTF16("country");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId country1 = list_.back()->unique_renderer_id;

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassificationsForTesting(&field_candidates_map_);

  ASSERT_TRUE(field_candidates_map_.find(country1) !=
              field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY,
            field_candidates_map_[country1].BestHeuristicType());
}

// Tests that the field is classified as |ADDRESS_HOME_COUNTRY| when the field
// name contains 'region'.
TEST_F(AddressFieldTest, ParseCountryNameRegion) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Land");
  field.name = ASCIIToUTF16("client_region");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId country1 = list_.back()->unique_renderer_id;

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassificationsForTesting(&field_candidates_map_);

  ASSERT_TRUE(field_candidates_map_.find(country1) !=
              field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY,
            field_candidates_map_[country1].BestHeuristicType());
}

// Tests that city and state fields are classified correctly when their names
// contain keywords for different types. This is achieved by giving the priority
// to the label over the name for pages in Turkish.
TEST_F(AddressFieldTest, ParseTurkishCityStateWithLabelPrecedence) {
  // TODO(crbug.com/1156315): Remove once launched.
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(
      features::kAutofillEnableLabelPrecedenceForTurkishAddresses);

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Il");
  field.name = ASCIIToUTF16("city");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId state = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Ilce");
  field.name = ASCIIToUTF16("county");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId city = list_.back()->unique_renderer_id;

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner, LanguageCode("tr"));
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassificationsForTesting(&field_candidates_map_);

  ASSERT_TRUE(field_candidates_map_.find(state) != field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_STATE,
            field_candidates_map_[state].BestHeuristicType());

  ASSERT_TRUE(field_candidates_map_.find(city) != field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_CITY, field_candidates_map_[city].BestHeuristicType());
}

}  // namespace autofill
