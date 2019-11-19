// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_profile_validation_util.h"

#include <memory>
#include <string>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/null_storage.h"
#include "third_party/libaddressinput/src/cpp/test/testdata_source.h"

namespace autofill {

namespace {

using ::i18n::addressinput::Source;
using ::i18n::addressinput::Storage;
using ::i18n::addressinput::NullStorage;
using ::i18n::addressinput::TestdataSource;

}  // namespace

class AutofillProfileValidationUtilTest : public testing::Test,
                                          public LoadRulesListener {
 protected:
  AutofillProfileValidationUtilTest() {
    base::FilePath file_path;
    CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &file_path));
    file_path = file_path.Append(FILE_PATH_LITERAL("third_party"))
                    .Append(FILE_PATH_LITERAL("libaddressinput"))
                    .Append(FILE_PATH_LITERAL("src"))
                    .Append(FILE_PATH_LITERAL("testdata"))
                    .Append(FILE_PATH_LITERAL("countryinfo.txt"));

    validator_ = std::make_unique<AddressValidator>(
        std::unique_ptr<Source>(
            new TestdataSource(true, file_path.AsUTF8Unsafe())),
        std::unique_ptr<Storage>(new NullStorage), this);
    validator_->LoadRules("CA");
    // China has rules for locality/dependent locality fields.
    validator_->LoadRules("CN");
  }

  void ValidateProfileTest(AutofillProfile* profile) {
    profile_validation_util::ValidateProfile(profile, validator_.get());
  }

  void ValidateAddressTest(AutofillProfile* profile) {
    profile_validation_util::ValidateAddressStrictly(profile, validator_.get());
  }

  void ValidatePhoneTest(AutofillProfile* profile) {
    profile_validation_util::ValidatePhoneNumber(profile);
  }

  void ValidateEmailTest(AutofillProfile* profile) {
    profile_validation_util::ValidateEmailAddress(profile);
  }

  ~AutofillProfileValidationUtilTest() override {}

 private:
  std::unique_ptr<AddressValidator> validator_;

  // LoadRulesListener implementation.
  void OnAddressValidationRulesLoaded(const std::string& country_code,
                                      bool success) override {}

  DISALLOW_COPY_AND_ASSIGN(AutofillProfileValidationUtilTest);
};

TEST_F(AutofillProfileValidationUtilTest, ValidateFullValidProfileForCanada) {
  // This is a valid profile according to the rules in contryinfo.txt:
  // Address Line 1: "666 Notre-Dame Ouest",
  // Address Line 2: "Apt 8", City: "Montreal", Province: "QC",
  // Postal Code: "H3B 2T9", Country Code: "CA",
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  ValidateAddressTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::CLIENT));
  // For Canada, there is no rule and data to validate the city.
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT));
  // Canada doesn't have a dependent locality. It's not filled, and yet the
  // profile is valid.
  EXPECT_EQ(AutofillDataModel::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest,
       ValidateFullProfile_CountryCodeNotExist) {
  // This is a profile with invalid country code, therefore it cannot be
  // validated according to contryinfo.txt.
  const std::string country_code = "PP";
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, base::UTF8ToUTF16(country_code));
  ValidateAddressTest(&profile);
  EXPECT_EQ(AutofillDataModel::INVALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillDataModel::CLIENT));
  // The zip, the state and the city can't be validated, because we don't know
  // the country, in the strict validation this is considered as invalid.
  EXPECT_EQ(
      AutofillDataModel::INVALID,
      profile.GetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::INVALID,
      profile.GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::INVALID,
      profile.GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest,
       ValidateFullProfile_EmptyCountryCode) {
  // This is a profile with no country code, therefore it cannot be validated
  // according to contryinfo.txt.
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, base::UTF8ToUTF16(""));
  ValidateAddressTest(&profile);
  EXPECT_EQ(AutofillDataModel::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillDataModel::CLIENT));
  // The zip, the state and the city can't be validated, because we don't know
  // the country, in the strict validation this is considered as invalid.
  EXPECT_EQ(
      AutofillDataModel::INVALID,
      profile.GetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::INVALID,
      profile.GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::INVALID,
      profile.GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest,
       ValidateFullProfile_RuleNotAvailable) {
  // This is a profile with valid country code, but the rule is not available in
  // the contryinfo.txt.
  const std::string country_code = "US";
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, base::UTF8ToUTF16(country_code));
  ValidateAddressTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::UNVALIDATED,
      profile.GetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::UNVALIDATED,
      profile.GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::UNVALIDATED,
      profile.GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest, ValidateAddress_AdminAreaNotExists) {
  const std::string admin_area_code = "QQ";
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(ADDRESS_HOME_STATE, base::UTF8ToUTF16(admin_area_code));

  ValidateAddressTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::INVALID,
      profile.GetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::CLIENT));
  // The city can't be validated, because we don't know the state, in the strict
  // validation this is considered as invalid.
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest, ValidateAddress_EmptyAdminArea) {
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(ADDRESS_HOME_STATE, base::UTF8ToUTF16(""));

  ValidateAddressTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::EMPTY,
      profile.GetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::CLIENT));
  // The city can't be validated, because we don't know the state, in the strict
  // validation this is considered as invalid.
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest, ValidateAddress_AdminAreaFullName) {
  const std::string admin_area = "Quebec";
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(ADDRESS_HOME_STATE, base::UTF8ToUTF16(admin_area));

  ValidateAddressTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest, ValidateAddress_AdminAreaLowerCase) {
  const std::string admin_area = "qc";
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(ADDRESS_HOME_STATE, base::UTF8ToUTF16(admin_area));

  ValidateAddressTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest,
       ValidateAddress_AdminAreaSpecialLetter) {
  const std::string admin_area = "Québec";
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(ADDRESS_HOME_STATE, base::UTF8ToUTF16(admin_area));

  ValidateAddressTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest,
       ValidateAddress_AdminAreaNonDefaultLanguage) {
  // For this profile, different fields are in different available languages of
  // the country (Canada), and the language is not set. This is considered as
  // valid.
  const std::string admin_area = "Nouveau-Brunswick";
  const std::string postal_code = "E1A 8R5";  // A valid postal code for NB.
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(ADDRESS_HOME_STATE, base::UTF8ToUTF16(admin_area));
  profile.SetRawInfo(ADDRESS_HOME_ZIP, base::UTF8ToUTF16(postal_code));

  ValidateAddressTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest, ValidateAddress_ValidZipNoSpace) {
  const std::string postal_code = "H3C6S3";
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(ADDRESS_HOME_ZIP, base::UTF8ToUTF16(postal_code));

  ValidateAddressTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest, ValidateAddress_ValidZipLowerCase) {
  // Postal codes in lower case letters should also be considered valid.
  const std::string postal_code = "h3c 6s3";
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(ADDRESS_HOME_ZIP, base::UTF8ToUTF16(postal_code));

  ValidateAddressTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest, ValidateAddress_InvalidZip) {
  const std::string postal_code = "ABC 123";
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(ADDRESS_HOME_ZIP, base::UTF8ToUTF16(postal_code));

  ValidateAddressTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::INVALID,
      profile.GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest, ValidateAddress_EmptyZip) {
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(ADDRESS_HOME_ZIP, base::UTF8ToUTF16(""));

  ValidateAddressTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::EMPTY,
      profile.GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest, ValidateAddress_EmptyCity) {
  // Although, for Canada, there is no rule to validate the city (aka locality)
  // field, the field is required. Therefore, a profile without a city field
  // would be an invalid profile.
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(ADDRESS_HOME_CITY, base::UTF8ToUTF16(""));

  ValidateAddressTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::EMPTY,
      profile.GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest, ValidateFullProfile_EmptyFields) {
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, base::UTF8ToUTF16(""));
  profile.SetRawInfo(ADDRESS_HOME_STATE, base::UTF8ToUTF16(""));
  profile.SetRawInfo(ADDRESS_HOME_CITY, base::UTF8ToUTF16(""));
  profile.SetRawInfo(ADDRESS_HOME_ZIP, base::UTF8ToUTF16(""));

  ValidateAddressTest(&profile);
  EXPECT_EQ(AutofillDataModel::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::EMPTY,
      profile.GetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::EMPTY,
      profile.GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::EMPTY,
      profile.GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest, ValidateFullValidProfileForChina) {
  // This is a valid profile according to the rules in countryinfo.txt:
  // Address Address: "100 Century Avenue",
  // District: "赫章县", City: "毕节地区", Province: "贵州省",
  // Postal Code: "200120", Country Code: "CN",
  AutofillProfile profile(autofill::test::GetFullValidProfileForChina());

  ValidateAddressTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest,
       ValidateFullValidProfile_InvalidCity) {
  const std::string invalid_city = "毕节";
  AutofillProfile profile(autofill::test::GetFullValidProfileForChina());
  profile.SetRawInfo(ADDRESS_HOME_CITY, base::UTF8ToUTF16(invalid_city));

  ValidateAddressTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillDataModel::CLIENT));
  // The city which is the only dependent field on state is invalid, in the
  // strict validation the state would also be considered as invalid.
  EXPECT_EQ(
      AutofillDataModel::INVALID,
      profile.GetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::INVALID,
      profile.GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT));
  // The dependent locality can't be validated, because we don't know the city,
  // in the strict validation this is considered as invalid.
  EXPECT_EQ(AutofillDataModel::INVALID,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest,
       ValidateFullValidProfile_MisplacedCity) {
  // "揭阳市" is a valid city name, but not in the "贵州省" province. Therefore,
  // the city would be considered as INVALID.

  const std::string city = "揭阳市";
  AutofillProfile profile(autofill::test::GetFullValidProfileForChina());
  profile.SetRawInfo(ADDRESS_HOME_CITY, base::UTF8ToUTF16(city));

  ValidateAddressTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillDataModel::CLIENT));
  // The city which is the only dependent field on state is invalid, in the
  // strict validation the state would also be considered as invalid.
  EXPECT_EQ(
      AutofillDataModel::INVALID,
      profile.GetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::INVALID,
      profile.GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT));
  // The dependent locality can't be validated, because we don't know the city,
  // in the strict validation this is considered as invalid.
  EXPECT_EQ(AutofillDataModel::INVALID,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest,
       ValidateFullValidProfile_LatinNameForCity) {
  const std::string admin_area = "Guizhou Sheng";
  const std::string city = "Bijie Diqu";
  const std::string district = "Weining Xian";
  AutofillProfile profile(autofill::test::GetFullValidProfileForChina());
  profile.SetRawInfo(ADDRESS_HOME_STATE, base::UTF8ToUTF16(admin_area));
  profile.SetRawInfo(ADDRESS_HOME_CITY, base::UTF8ToUTF16(city));
  profile.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY,
                     base::UTF8ToUTF16(district));

  ValidateAddressTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest,
       ValidateFullValidProfile_EmptyDistrict) {
  // China has a dependent locality field (aka district), but it's not required.

  AutofillProfile profile(autofill::test::GetFullValidProfileForChina());
  profile.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY, base::UTF8ToUTF16(""));

  ValidateAddressTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest,
       ValidateFullValidProfile_InvalidDistrict) {
  // Though the dependent locality (aka district) field is not a required field,
  // but we should still validate it.

  AutofillProfile profile(autofill::test::GetFullValidProfileForChina());
  profile.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY, base::UTF8ToUTF16("赫"));

  ValidateAddressTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::CLIENT));
  // The dependent locality which is the only dependent field on city is
  // invalid, in the strict validation the city would also be invalid.
  EXPECT_EQ(
      AutofillDataModel::INVALID,
      profile.GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::INVALID,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest,
       ValidateFullValidProfile_MisplacedDistrict) {
  // "蒙城县" is a valid district name, but not in the "揭阳市" city. Therefore,
  // the district should be considered as INVALID.

  AutofillProfile profile(autofill::test::GetFullValidProfileForChina());
  profile.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY,
                     base::UTF8ToUTF16("蒙城县"));

  ValidateAddressTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::CLIENT));
  // The only that depend on city (dependent locality) is invalid,
  //  in the strict validation city would also be considered as invalid.
  EXPECT_EQ(
      AutofillDataModel::INVALID,
      profile.GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::INVALID,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest, ValidatePhone_FullValidProfile) {
  // This is a full valid profile:
  // Country Code: "CA", Phone Number: "15141112233"
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  ValidatePhoneTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER,
                                     AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest, ValidatePhone_EmptyPhoneNumber) {
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, base::string16());
  ValidatePhoneTest(&profile);
  EXPECT_EQ(AutofillDataModel::EMPTY,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER,
                                     AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest,
       ValidatePhone_ValidPhoneCountryCodeNotExist) {
  // This is a profile with invalid country code, therefore the phone number
  // cannot be validated.
  const std::string country_code = "PP";
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, base::UTF8ToUTF16(country_code));
  ValidatePhoneTest(&profile);
  EXPECT_EQ(AutofillDataModel::UNVALIDATED,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER,
                                     AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest,
       ValidatePhone_EmptyPhoneCountryCodeNotExist) {
  // This is a profile with invalid country code, but a missing phone number.
  // Therefore, it's an invalid phone number.
  const std::string country_code = "PP";
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, base::UTF8ToUTF16(country_code));
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, base::string16());
  ValidatePhoneTest(&profile);
  EXPECT_EQ(AutofillDataModel::EMPTY,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER,
                                     AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest, ValidatePhone_InvalidPhoneNumber) {
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, base::ASCIIToUTF16("33"));
  ValidatePhoneTest(&profile);
  EXPECT_EQ(AutofillDataModel::INVALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER,
                                     AutofillDataModel::CLIENT));

  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                     base::ASCIIToUTF16("151411122334"));
  ValidatePhoneTest(&profile);
  EXPECT_EQ(AutofillDataModel::INVALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER,
                                     AutofillDataModel::CLIENT));

  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                     base::ASCIIToUTF16("1(514)111-22-334"));
  ValidatePhoneTest(&profile);
  EXPECT_EQ(AutofillDataModel::INVALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER,
                                     AutofillDataModel::CLIENT));

  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                     base::ASCIIToUTF16("251411122334"));
  ValidatePhoneTest(&profile);
  EXPECT_EQ(AutofillDataModel::INVALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER,
                                     AutofillDataModel::CLIENT));

  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, base::ASCIIToUTF16("Hello!"));
  ValidatePhoneTest(&profile);
  EXPECT_EQ(AutofillDataModel::INVALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER,
                                     AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest, ValidatePhone_ValidPhoneNumber) {
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, base::ASCIIToUTF16("5141112233"));
  ValidatePhoneTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER,
                                     AutofillDataModel::CLIENT));

  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                     base::ASCIIToUTF16("514-111-2233"));
  ValidatePhoneTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER,
                                     AutofillDataModel::CLIENT));

  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                     base::ASCIIToUTF16("1(514)111-22-33"));
  ValidatePhoneTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER,
                                     AutofillDataModel::CLIENT));

  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                     base::ASCIIToUTF16("+1 514 111 22 33"));
  ValidatePhoneTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER,
                                     AutofillDataModel::CLIENT));

  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                     base::ASCIIToUTF16("+1 (514)-111-22-33"));
  ValidatePhoneTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER,
                                     AutofillDataModel::CLIENT));

  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                     base::ASCIIToUTF16("(514)-111-22-33"));
  ValidatePhoneTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER,
                                     AutofillDataModel::CLIENT));

  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                     base::ASCIIToUTF16("+1 650 GOO OGLE"));
  ValidatePhoneTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER,
                                     AutofillDataModel::CLIENT));

  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                     base::ASCIIToUTF16("778 111 22 33"));
  ValidatePhoneTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER,
                                     AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest, ValidateEmail_FullValidProfile) {
  // This is a full valid profile:
  // Email: "alice@wonderland.ca"
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  ValidateEmailTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(EMAIL_ADDRESS, AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest, ValidateEmail_EmptyEmailAddress) {
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(EMAIL_ADDRESS, base::string16());
  ValidateEmailTest(&profile);
  EXPECT_EQ(AutofillDataModel::EMPTY,
            profile.GetValidityState(EMAIL_ADDRESS, AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest,
       ValidateEmail_ValidateInvalidEmailAddress) {
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(EMAIL_ADDRESS, base::ASCIIToUTF16("Hello!"));
  ValidateEmailTest(&profile);
  EXPECT_EQ(AutofillDataModel::INVALID,
            profile.GetValidityState(EMAIL_ADDRESS, AutofillDataModel::CLIENT));

  profile.SetRawInfo(EMAIL_ADDRESS, base::ASCIIToUTF16("alice.wonderland"));
  ValidateEmailTest(&profile);
  EXPECT_EQ(AutofillDataModel::INVALID,
            profile.GetValidityState(EMAIL_ADDRESS, AutofillDataModel::CLIENT));

  profile.SetRawInfo(EMAIL_ADDRESS, base::ASCIIToUTF16("alice@"));
  ValidateEmailTest(&profile);
  EXPECT_EQ(AutofillDataModel::INVALID,
            profile.GetValidityState(EMAIL_ADDRESS, AutofillDataModel::CLIENT));

  profile.SetRawInfo(EMAIL_ADDRESS,
                     base::ASCIIToUTF16("alice@=wonderland.com"));
  ValidateEmailTest(&profile);
  EXPECT_EQ(AutofillDataModel::INVALID,
            profile.GetValidityState(EMAIL_ADDRESS, AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest, ValidateEmail_ValidEmailAddress) {
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());

  profile.SetRawInfo(EMAIL_ADDRESS, base::ASCIIToUTF16("alice@wonderland"));
  ValidateEmailTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(EMAIL_ADDRESS, AutofillDataModel::CLIENT));

  profile.SetRawInfo(EMAIL_ADDRESS,
                     base::ASCIIToUTF16("alice@wonderland.fiction"));
  ValidateEmailTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(EMAIL_ADDRESS, AutofillDataModel::CLIENT));

  profile.SetRawInfo(EMAIL_ADDRESS,
                     base::ASCIIToUTF16("alice+cat@wonderland.fiction.book"));
  ValidateEmailTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(EMAIL_ADDRESS, AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest, ValidateProfile_FullValidProfile) {
  // This is a full valid profile:
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  ValidateProfileTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(EMAIL_ADDRESS, AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER,
                                     AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest,
       ValidateProfile_FullValidProfileWithInvalidZip) {
  // This is a full valid profile:
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(ADDRESS_HOME_ZIP, base::UTF8ToUTF16("ABC 123"));
  ValidateProfileTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::INVALID,
      profile.GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(EMAIL_ADDRESS, AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER,
                                     AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest,
       ValidateProfile_FullValidProfileWithInvalidPhone) {
  // This is a full valid profile:
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, base::ASCIIToUTF16("33"));
  ValidateProfileTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::INVALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(EMAIL_ADDRESS, AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest,
       ValidateProfile_FullValidProfileWithInvalidEmail) {
  // This is a full valid profile:
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(EMAIL_ADDRESS, base::ASCIIToUTF16("fakeaddress"));
  ValidateProfileTest(&profile);
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::INVALID,
            profile.GetValidityState(EMAIL_ADDRESS, AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest,
       ValidateProfile_TopToBottomValidationCanada) {
  // This is a full valid profile, with the wrong country:
  // Address Line 1: "666 Notre-Dame Ouest",
  // Address Line 2: "Apt 8", City: "Montreal", Province: "QC",
  // Postal Code: "H3B 2T9", Country Code: "CN",
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, base::ASCIIToUTF16("CN"));

  ValidateProfileTest(&profile);
  // The fields that depend on country (state and zip) are both invalid,
  // therefore in the strict validation this is considered as invalid.
  EXPECT_EQ(AutofillDataModel::INVALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillDataModel::CLIENT));
  // The state is not a Chinese state, so it's considered as invalid.
  EXPECT_EQ(
      AutofillDataModel::INVALID,
      profile.GetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::CLIENT));
  // The city can't be validated, because the state value is not
  // valid, in the strict validation this is considered as invalid.
  EXPECT_EQ(
      AutofillDataModel::INVALID,
      profile.GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT));

  EXPECT_EQ(AutofillDataModel::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::UNSUPPORTED,
            profile.GetValidityState(ADDRESS_HOME_STREET_ADDRESS,
                                     AutofillDataModel::CLIENT));
  // The zip is not a Chinese one, therefore it's invalid.
  EXPECT_EQ(
      AutofillDataModel::INVALID,
      profile.GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT));
  // Phone number is validated regardless of the country.
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER,
                                     AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest,
       ValidateProfile_TopToBottomValidationChina) {
  // This is a full valid profile, with the wrong country:
  // Address Address: "100 Century Avenue",
  // District: "赫章县", City: "毕节地区", Province: "贵州省",
  // Postal Code: "200120", Country Code: "CA",
  AutofillProfile profile(autofill::test::GetFullValidProfileForChina());
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, base::ASCIIToUTF16("CA"));

  ValidateProfileTest(&profile);

  // The fields that depend on Country (state and zip) are both invalid,
  // therefore in the strict validation this is considered as invalid.
  EXPECT_EQ(AutofillDataModel::INVALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillDataModel::CLIENT));
  // The state is not a Canadian state, so it's considered as invalid.
  EXPECT_EQ(
      AutofillDataModel::INVALID,
      profile.GetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::CLIENT));
  // We can't validate city, because state is not valid, in the strict
  // validation this is considered as invalid.
  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT));
  // The dependent locality is not a Canadian field, so it's considered as
  // invalid.
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::UNSUPPORTED,
            profile.GetValidityState(ADDRESS_HOME_STREET_ADDRESS,
                                     AutofillDataModel::CLIENT));

  // The zip is not a Canadian one, therefore it's invalid.
  EXPECT_EQ(
      AutofillDataModel::INVALID,
      profile.GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT));
  // Phone number is validated regardless of the country.
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER,
                                     AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest,
       ValidateProfile_TopToBottomValidationChina_StateWrong) {
  // This is a full valid profile, with the wrong province:
  // Address Address: "100 Century Avenue",
  // District: "赫章县", City: "毕节地区", Province: "海南省",
  // Postal Code: "200120", Country Code: "CN",
  AutofillProfile profile(autofill::test::GetFullValidProfileForChina());
  profile.SetRawInfo(ADDRESS_HOME_STATE, base::UTF8ToUTF16("海南省"));

  ValidateProfileTest(&profile);

  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillDataModel::CLIENT));
  // The only field that depends on state (city) is invalid, in the strict
  // validation this makes state also invalid.
  EXPECT_EQ(
      AutofillDataModel::INVALID,
      profile.GetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::CLIENT));
  // The city is in another province.
  EXPECT_EQ(
      AutofillDataModel::INVALID,
      profile.GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT));
  // The dependent locality can't be validated, because the city value is not
  // valid, in the strict validation this is considered as invalid.
  EXPECT_EQ(AutofillDataModel::INVALID,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::UNSUPPORTED,
            profile.GetValidityState(ADDRESS_HOME_STREET_ADDRESS,
                                     AutofillDataModel::CLIENT));

  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT));
  // Phone number is validated regardless of the country.
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER,
                                     AutofillDataModel::CLIENT));
}

TEST_F(AutofillProfileValidationUtilTest,
       ValidateProfile_TopToBottomValidationChina_StateMissing) {
  // This is a full valid profile, with the empty province:
  // Address Address: "100 Century Avenue",
  // District: "赫章县", City: "毕节地区", Province: "",
  // Postal Code: "200120", Country Code: "CN",
  AutofillProfile profile(autofill::test::GetFullValidProfileForChina());
  profile.SetRawInfo(ADDRESS_HOME_STATE, base::UTF8ToUTF16(""));

  ValidateProfileTest(&profile);

  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(
      AutofillDataModel::EMPTY,
      profile.GetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::CLIENT));
  // City can't be validated, because the state is missing, in the strict
  // validation this is considered as invalid.
  EXPECT_EQ(
      AutofillDataModel::INVALID,
      profile.GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT));
  // The dependent locality can't be validated, because we don't know the city,
  // in the strict validation this is considered as invalid.
  EXPECT_EQ(AutofillDataModel::INVALID,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillDataModel::CLIENT));
  EXPECT_EQ(AutofillDataModel::UNSUPPORTED,
            profile.GetValidityState(ADDRESS_HOME_STREET_ADDRESS,
                                     AutofillDataModel::CLIENT));

  EXPECT_EQ(
      AutofillDataModel::VALID,
      profile.GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT));
  // Phone number is validated regardless of the country.
  EXPECT_EQ(AutofillDataModel::VALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER,
                                     AutofillDataModel::CLIENT));
}

}  // namespace autofill
