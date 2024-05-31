// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/address_i18n.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_field.h"

namespace i18n::addressinput {

using ::operator<<;

}  // namespace i18n::addressinput

namespace autofill {
namespace i18n {

using ::i18n::addressinput::AddressData;
using ::i18n::addressinput::AddressField;

using ::i18n::addressinput::ADMIN_AREA;
using ::i18n::addressinput::COUNTRY;
using ::i18n::addressinput::DEPENDENT_LOCALITY;
using ::i18n::addressinput::LOCALITY;
using ::i18n::addressinput::ORGANIZATION;
using ::i18n::addressinput::POSTAL_CODE;
using ::i18n::addressinput::RECIPIENT;
using ::i18n::addressinput::SORTING_CODE;
using ::i18n::addressinput::STREET_ADDRESS;

struct FieldTypeMirrorConversionsTestCase {
  FieldType server_field;
  AddressField address_field;
};

class FieldTypeMirrorConversionsTest
    : public testing::TestWithParam<FieldTypeMirrorConversionsTestCase> {};

TEST_P(FieldTypeMirrorConversionsTest, FieldTypeMirrorConversions) {
  auto test_data = GetParam();
  AddressField address_field;
  EXPECT_TRUE(FieldForType(test_data.server_field, &address_field));
  EXPECT_EQ(test_data.address_field, address_field);

  FieldType server_field = TypeForField(test_data.address_field);
  EXPECT_EQ(test_data.server_field, server_field);
}

INSTANTIATE_TEST_SUITE_P(
    AddressI18nTest,
    FieldTypeMirrorConversionsTest,
    testing::Values(
        FieldTypeMirrorConversionsTestCase{ADDRESS_HOME_COUNTRY, COUNTRY},
        FieldTypeMirrorConversionsTestCase{ADDRESS_HOME_STATE, ADMIN_AREA},
        FieldTypeMirrorConversionsTestCase{ADDRESS_HOME_CITY, LOCALITY},
        FieldTypeMirrorConversionsTestCase{ADDRESS_HOME_DEPENDENT_LOCALITY,
                                           DEPENDENT_LOCALITY},
        FieldTypeMirrorConversionsTestCase{ADDRESS_HOME_SORTING_CODE,
                                           SORTING_CODE},
        FieldTypeMirrorConversionsTestCase{ADDRESS_HOME_ZIP, POSTAL_CODE},
        FieldTypeMirrorConversionsTestCase{ADDRESS_HOME_STREET_ADDRESS,
                                           STREET_ADDRESS},
        FieldTypeMirrorConversionsTestCase{COMPANY_NAME, ORGANIZATION},
        FieldTypeMirrorConversionsTestCase{NAME_FULL, RECIPIENT}));

struct FieldTypeUnidirectionalConversionsTestCase {
  FieldType server_field;
  AddressField expected_address_field;
};

class FieldTypeUnidirectionalConversionsTest
    : public testing::TestWithParam<
          FieldTypeUnidirectionalConversionsTestCase> {};

TEST_P(FieldTypeUnidirectionalConversionsTest,
       FieldTypeUnidirectionalConversions) {
  auto test_data = GetParam();
  AddressField actual_address_field;
  FieldForType(test_data.server_field, &actual_address_field);
  EXPECT_EQ(test_data.expected_address_field, actual_address_field);
}

INSTANTIATE_TEST_SUITE_P(AddressI18nTest,
                         FieldTypeUnidirectionalConversionsTest,
                         testing::Values(
                             FieldTypeUnidirectionalConversionsTestCase{
                                 ADDRESS_HOME_LINE1, STREET_ADDRESS},
                             FieldTypeUnidirectionalConversionsTestCase{
                                 ADDRESS_HOME_LINE2, STREET_ADDRESS}));

TEST(AddressI18nTest, UnconvertableFields) {
  EXPECT_FALSE(FieldForType(PHONE_HOME_NUMBER, nullptr));
  EXPECT_FALSE(FieldForType(EMAIL_ADDRESS, nullptr));
}

TEST(AddressI18nTest, CreateAddressDataFromAutofillProfile) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile, "John", "H.", "Doe", "johndoe@hades.com",
                       "Underworld", "666 Erebus St.", "Apt 8", "Elysium", "CA",
                       "91111", "US", "16502111111");
  profile.set_language_code("en");
  std::unique_ptr<AddressData> actual =
      CreateAddressDataFromAutofillProfile(profile, "en_US");

  AddressData expected;
  expected.region_code = "US";
  expected.address_line.push_back("666 Erebus St.");
  expected.address_line.push_back("Apt 8");
  expected.administrative_area = "CA";
  expected.locality = "Elysium";
  expected.postal_code = "91111";
  expected.language_code = "en";
  expected.organization = "Underworld";
  expected.recipient = "John H. Doe";

  EXPECT_EQ(expected, *actual);
}

TEST(AddressI18nTest, ProfileOnlyWithAddressLine2ReturnsOneAddressLine) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile, "", "", "", "", "", "", "Apt 8", "", "", "",
                       "", "");
  profile.set_language_code("en");
  std::unique_ptr<AddressData> actual =
      CreateAddressDataFromAutofillProfile(profile, "en_US");

  AddressData expected;
  expected.address_line.push_back("Apt 8");
  expected.language_code = "en";

  EXPECT_EQ(expected, *actual);
}

TEST(AddressI18nTest, IsFieldRequired) {
  EXPECT_TRUE(IsFieldRequired(ADDRESS_HOME_LINE1, "CA"));

  EXPECT_FALSE(IsFieldRequired(ADDRESS_HOME_SORTING_CODE, "CA"));
  EXPECT_FALSE(IsFieldRequired(ADDRESS_HOME_SORTING_CODE, "FR"));

  EXPECT_TRUE(IsFieldRequired(ADDRESS_HOME_STATE, "AE"));
  EXPECT_FALSE(IsFieldRequired(ADDRESS_HOME_STATE, "BS"));
}

}  // namespace i18n
}  // namespace autofill
