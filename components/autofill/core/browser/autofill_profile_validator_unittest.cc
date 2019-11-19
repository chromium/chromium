// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_profile_validator.h"

#include <stddef.h>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/null_storage.h"
#include "third_party/libaddressinput/src/cpp/test/testdata_source.h"

namespace autofill {

using ::i18n::addressinput::NullStorage;
using ::i18n::addressinput::Source;
using ::i18n::addressinput::Storage;
using ::i18n::addressinput::TestdataSource;

using ::i18n::addressinput::COUNTRY;
using ::i18n::addressinput::ADMIN_AREA;
using ::i18n::addressinput::LOCALITY;
using ::i18n::addressinput::DEPENDENT_LOCALITY;
using ::i18n::addressinput::SORTING_CODE;
using ::i18n::addressinput::POSTAL_CODE;
using ::i18n::addressinput::STREET_ADDRESS;
using ::i18n::addressinput::RECIPIENT;

// Used to load region rules for this test.
class ValidationTestDataSource : public TestdataSource {
 public:
  ValidationTestDataSource() : TestdataSource(true) {}

  ~ValidationTestDataSource() override {}

  void Get(const std::string& key, const Callback& data_ready) const override {
    data_ready(
        true, key,
        new std::string(
            "{"
            "\"data/CA\": "
            "{\"lang\": \"en\", \"upper\": \"ACNOSZ\", "
            "\"zipex\": \"H3Z 2Y7,V8X 3X4,T0L 1K0,T0H 1A0\", "
            "\"name\": \"CANADA\", "
            "\"fmt\": \"%N%n%O%n%A%n%C %S %Z\", \"id\": \"data/CA\", "
            "\"languages\": \"en\", \"sub_keys\": \"QC\", \"key\": "
            "\"CA\", "
            "\"require\": \"ACSZ\", \"sub_names\": \"Quebec\", "
            "\"sub_zips\": \"G|H|J\"}, "
            "\"data/CA/QC\": "
            "{\"lang\": \"en\", \"key\": \"QC\", "
            "\"id\": \"data/CA/QC\", \"zip\": \"G|H|J\", \"name\": \"Quebec\"}"
            "}"));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ValidationTestDataSource);
};

class AutofillProfileValidatorTest : public testing::Test {
 public:
  AutofillProfileValidatorTest()
      : validator_(new AutofillProfileValidator(
            std::unique_ptr<Source>(new ValidationTestDataSource()),
            std::unique_ptr<Storage>(new NullStorage))),
        onvalidated_cb_(
            base::BindOnce(&AutofillProfileValidatorTest::OnValidated,
                           base::Unretained(this))) {}

 protected:
  const std::unique_ptr<AutofillProfileValidator> validator_;

  ~AutofillProfileValidatorTest() override {}

  void OnValidated(const AutofillProfile* profile) {
    // Make sure the profile has the expected validity state.
    for (auto expectation : expected_validity_) {
      EXPECT_EQ(expectation.second,
                profile->GetValidityState(expectation.first,
                                          AutofillDataModel::CLIENT));
    }
  }

  bool AreRulesLoadedForRegion(std::string region_code) {
    return validator_->AreRulesLoadedForRegion(region_code);
  }

  void LoadRulesForRegion(std::string region_code) {
    validator_->LoadRulesForRegion(region_code);
  }

  AutofillProfileValidatorCallback onvalidated_cb_;

  std::vector<std::pair<ServerFieldType, AutofillDataModel::ValidityState>>
      expected_validity_;

 private:
  base::test::TaskEnvironment scoped_task_scheduler;

  DISALLOW_COPY_AND_ASSIGN(AutofillProfileValidatorTest);
};

// Validate a valid profile, for which the rules are not loaded, yet.
TEST_F(AutofillProfileValidatorTest, ValidateFullValidProfile_RulesNotLoaded) {
  // This is a valid profile, and the rules are loaded in the constructors
  // Province: "QC", Country: "CA"
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());

  // Make sure that the rules are not pre-loaded.
  std::string country_code =
      base::UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(false, AreRulesLoadedForRegion(country_code));

  // Set up the test expectations.
  expected_validity_ = {
      {ADDRESS_HOME_COUNTRY, AutofillDataModel::VALID},
      {ADDRESS_HOME_STATE, AutofillDataModel::VALID},
      {ADDRESS_HOME_ZIP, AutofillDataModel::VALID},
      {ADDRESS_HOME_CITY, AutofillDataModel::VALID},
      {ADDRESS_HOME_DEPENDENT_LOCALITY, AutofillDataModel::EMPTY},
      {PHONE_HOME_WHOLE_NUMBER, AutofillDataModel::VALID},
      {EMAIL_ADDRESS, AutofillDataModel::VALID}};

  // Start the validator.
  validator_->StartProfileValidation(&profile, std::move(onvalidated_cb_));
}

// Validate a Full Profile, for which the rules are already loaded.
TEST_F(AutofillProfileValidatorTest, ValidateAddress_RulesLoaded) {
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());

  // Pre-load the rules.
  std::string country_code =
      base::UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_COUNTRY));
  LoadRulesForRegion(country_code);
  EXPECT_EQ(true, AreRulesLoadedForRegion(country_code));

  // Set up the test expectations.
  expected_validity_ = {{ADDRESS_HOME_COUNTRY, AutofillDataModel::VALID},
                        {ADDRESS_HOME_STATE, AutofillDataModel::VALID},
                        {ADDRESS_HOME_ZIP, AutofillDataModel::VALID},
                        {PHONE_HOME_WHOLE_NUMBER, AutofillDataModel::VALID},
                        {EMAIL_ADDRESS, AutofillDataModel::VALID}};

  // Start the validator.
  validator_->StartProfileValidation(&profile, std::move(onvalidated_cb_));
}

// When country code is invalid, the coutry code is invalid and the rest of the
// address and the phone are unvalidated.
TEST_F(AutofillProfileValidatorTest,
       StartProfileValidation_CountryCodeNotExists) {
  const std::string country_code = "PP";
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, base::UTF8ToUTF16(country_code));

  EXPECT_EQ(false, AreRulesLoadedForRegion(country_code));

  // Set up the test expectations.
  expected_validity_ = {
      {ADDRESS_HOME_COUNTRY, AutofillDataModel::INVALID},
      {ADDRESS_HOME_STATE, AutofillDataModel::INVALID},
      {ADDRESS_HOME_ZIP, AutofillDataModel::INVALID},
      {PHONE_HOME_WHOLE_NUMBER, AutofillDataModel::UNVALIDATED},
      {EMAIL_ADDRESS, AutofillDataModel::VALID}};

  // Start the validator.
  validator_->StartProfileValidation(&profile, std::move(onvalidated_cb_));
}

// When country code is valid, but the rule is not in the source, the state and
// zip are unvalidated.
TEST_F(AutofillProfileValidatorTest, ValidateAddress_RuleNotExists) {
  const std::string country_code = "US";
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, base::UTF8ToUTF16(country_code));

  EXPECT_EQ(false, AreRulesLoadedForRegion(country_code));

  // Set up the test expectations.
  expected_validity_ = {{ADDRESS_HOME_COUNTRY, AutofillDataModel::VALID},
                        {ADDRESS_HOME_STATE, AutofillDataModel::UNVALIDATED},
                        {ADDRESS_HOME_ZIP, AutofillDataModel::UNVALIDATED},
                        {PHONE_HOME_WHOLE_NUMBER, AutofillDataModel::VALID},
                        {EMAIL_ADDRESS, AutofillDataModel::VALID}};

  // Start the validator.
  validator_->StartProfileValidation(&profile, std::move(onvalidated_cb_));
}

// When country code is empty, the profile is unvalidated.
TEST_F(AutofillProfileValidatorTest, ValidateAddress_EmptyCountryCode) {
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, base::string16());

  EXPECT_EQ(false, AreRulesLoadedForRegion(""));

  // Set up the test expectations.
  // The phone is validated for the US.
  expected_validity_ = {
      {ADDRESS_HOME_COUNTRY, AutofillDataModel::EMPTY},
      {ADDRESS_HOME_STATE, AutofillDataModel::INVALID},
      {ADDRESS_HOME_ZIP, AutofillDataModel::INVALID},
      {PHONE_HOME_WHOLE_NUMBER, AutofillDataModel::UNVALIDATED},
      {EMAIL_ADDRESS, AutofillDataModel::VALID}};

  // Start the validator.
  validator_->StartProfileValidation(&profile, std::move(onvalidated_cb_));
}

// Validate a profile with an invalid phone.
TEST_F(AutofillProfileValidatorTest, StartProfileValidation_InvalidPhone) {
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                     base::UTF8ToUTF16("Invalid Phone"));

  // Set up the test expectations.
  expected_validity_ = {
      {ADDRESS_HOME_COUNTRY, AutofillDataModel::VALID},
      {ADDRESS_HOME_STATE, AutofillDataModel::VALID},
      {ADDRESS_HOME_CITY, AutofillDataModel::VALID},
      {ADDRESS_HOME_DEPENDENT_LOCALITY, AutofillDataModel::EMPTY},
      {ADDRESS_HOME_ZIP, AutofillDataModel::VALID},
      {PHONE_HOME_WHOLE_NUMBER, AutofillDataModel::INVALID},
      {EMAIL_ADDRESS, AutofillDataModel::VALID}};

  // Start the validator.
  validator_->StartProfileValidation(&profile, std::move(onvalidated_cb_));
}

// Validate a profile with a valid phone, valid email and invalid address.
TEST_F(AutofillProfileValidatorTest, StartProfileValidation_InvalidAddress) {
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(ADDRESS_HOME_STATE, base::UTF8ToUTF16("Invalid State"));

  // Set up the test expectations.
  expected_validity_ = {
      {ADDRESS_HOME_COUNTRY, AutofillDataModel::VALID},
      {ADDRESS_HOME_STATE, AutofillDataModel::INVALID},
      {ADDRESS_HOME_CITY, AutofillDataModel::VALID},
      {ADDRESS_HOME_DEPENDENT_LOCALITY, AutofillDataModel::EMPTY},
      {ADDRESS_HOME_ZIP, AutofillDataModel::VALID},
      {PHONE_HOME_WHOLE_NUMBER, AutofillDataModel::VALID},
      {EMAIL_ADDRESS, AutofillDataModel::VALID}};

  // Start the validator.
  validator_->StartProfileValidation(&profile, std::move(onvalidated_cb_));
}

// Validate a profile with an empty phone and invalid address.
TEST_F(AutofillProfileValidatorTest,
       StartProfileValidation_EmptyPhone_InvalidAddress) {
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, base::string16());
  profile.SetRawInfo(ADDRESS_HOME_STATE, base::UTF8ToUTF16("Invalid State"));

  // Set up the test expectations.
  expected_validity_ = {
      {ADDRESS_HOME_COUNTRY, AutofillDataModel::VALID},
      {ADDRESS_HOME_STATE, AutofillDataModel::INVALID},
      {ADDRESS_HOME_CITY, AutofillDataModel::VALID},
      {ADDRESS_HOME_DEPENDENT_LOCALITY, AutofillDataModel::EMPTY},
      {ADDRESS_HOME_ZIP, AutofillDataModel::VALID},
      {PHONE_HOME_WHOLE_NUMBER, AutofillDataModel::EMPTY},
      {EMAIL_ADDRESS, AutofillDataModel::VALID}};

  // Start the validator.
  validator_->StartProfileValidation(&profile, std::move(onvalidated_cb_));
}

// Validate a profile with an invalid email and invalid address.
TEST_F(AutofillProfileValidatorTest,
       StartProfileValidation_InvalidEmail_InvalidAddress) {
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(EMAIL_ADDRESS, base::ASCIIToUTF16("Invalid Email"));
  profile.SetRawInfo(ADDRESS_HOME_ZIP, base::UTF8ToUTF16("Invalid Zip"));

  // Set up the test expectations.
  expected_validity_ = {
      {ADDRESS_HOME_COUNTRY, AutofillDataModel::VALID},
      {ADDRESS_HOME_STATE, AutofillDataModel::VALID},
      {ADDRESS_HOME_CITY, AutofillDataModel::VALID},
      {ADDRESS_HOME_DEPENDENT_LOCALITY, AutofillDataModel::EMPTY},
      {ADDRESS_HOME_ZIP, AutofillDataModel::INVALID},
      {PHONE_HOME_WHOLE_NUMBER, AutofillDataModel::VALID},
      {EMAIL_ADDRESS, AutofillDataModel::INVALID}};

  // Start the validator.
  validator_->StartProfileValidation(&profile, std::move(onvalidated_cb_));
}

// Validate a profile with an empty email and invalid zip.
TEST_F(AutofillProfileValidatorTest,
       StartProfileValidation_EmptyEmail_InvalidZip) {
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(EMAIL_ADDRESS, base::string16());
  profile.SetRawInfo(ADDRESS_HOME_ZIP, base::UTF8ToUTF16("Invalid Zip"));

  // Set up the test expectations.
  expected_validity_ = {
      {ADDRESS_HOME_COUNTRY, AutofillDataModel::VALID},
      {ADDRESS_HOME_STATE, AutofillDataModel::VALID},
      {ADDRESS_HOME_CITY, AutofillDataModel::VALID},
      {ADDRESS_HOME_DEPENDENT_LOCALITY, AutofillDataModel::EMPTY},
      {ADDRESS_HOME_ZIP, AutofillDataModel::INVALID},
      {PHONE_HOME_WHOLE_NUMBER, AutofillDataModel::VALID},
      {EMAIL_ADDRESS, AutofillDataModel::EMPTY}};

  // Start the validator.
  validator_->StartProfileValidation(&profile, std::move(onvalidated_cb_));
}

// Validate a profile with an empty email and empty zip.
TEST_F(AutofillProfileValidatorTest,
       StartProfileValidation_InvalidEmail_EmptyZip) {
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(EMAIL_ADDRESS, base::UTF8ToUTF16("Invalid Email"));
  profile.SetRawInfo(ADDRESS_HOME_ZIP, base::string16());

  // Set up the test expectations.
  expected_validity_ = {
      {ADDRESS_HOME_COUNTRY, AutofillDataModel::VALID},
      {ADDRESS_HOME_STATE, AutofillDataModel::VALID},
      {ADDRESS_HOME_CITY, AutofillDataModel::VALID},
      {ADDRESS_HOME_DEPENDENT_LOCALITY, AutofillDataModel::EMPTY},
      {ADDRESS_HOME_ZIP, AutofillDataModel::EMPTY},
      {PHONE_HOME_WHOLE_NUMBER, AutofillDataModel::VALID},
      {EMAIL_ADDRESS, AutofillDataModel::INVALID}};

  // Start the validator.
  validator_->StartProfileValidation(&profile, std::move(onvalidated_cb_));
}

// Validate a profile with an invalid phone and invalid email.
TEST_F(AutofillProfileValidatorTest,
       StartProfileValidation_InvalidEmail_InvalidPhone) {
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(EMAIL_ADDRESS, base::ASCIIToUTF16("Invalid Email"));
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                     base::UTF8ToUTF16("Invalid Phone"));

  // Set up the test expectations.
  expected_validity_ = {
      {ADDRESS_HOME_COUNTRY, AutofillDataModel::VALID},
      {ADDRESS_HOME_STATE, AutofillDataModel::VALID},
      {ADDRESS_HOME_CITY, AutofillDataModel::VALID},
      {ADDRESS_HOME_DEPENDENT_LOCALITY, AutofillDataModel::EMPTY},
      {ADDRESS_HOME_ZIP, AutofillDataModel::VALID},
      {PHONE_HOME_WHOLE_NUMBER, AutofillDataModel::INVALID},
      {EMAIL_ADDRESS, AutofillDataModel::INVALID}};

  // Start the validator.
  validator_->StartProfileValidation(&profile, std::move(onvalidated_cb_));
}

// Validate a profile with an invalid email.
TEST_F(AutofillProfileValidatorTest, StartProfileValidation_InvalidEmail) {
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(EMAIL_ADDRESS, base::ASCIIToUTF16("Invalid Email"));

  // Set up the test expectations.
  expected_validity_ = {
      {ADDRESS_HOME_COUNTRY, AutofillDataModel::VALID},
      {ADDRESS_HOME_STATE, AutofillDataModel::VALID},
      {ADDRESS_HOME_CITY, AutofillDataModel::VALID},
      {ADDRESS_HOME_DEPENDENT_LOCALITY, AutofillDataModel::EMPTY},
      {ADDRESS_HOME_ZIP, AutofillDataModel::VALID},
      {PHONE_HOME_WHOLE_NUMBER, AutofillDataModel::VALID},
      {EMAIL_ADDRESS, AutofillDataModel::INVALID}};

  // Start the validator.
  validator_->StartProfileValidation(&profile, std::move(onvalidated_cb_));
}

// Validate a profile with an invalid email, invalid phone and invalid state.
TEST_F(AutofillProfileValidatorTest,
       StartProfileValidation_InvalidEmail_InvalidPhone_InvalidAddress) {
  AutofillProfile profile(autofill::test::GetFullValidProfileForCanada());
  profile.SetRawInfo(EMAIL_ADDRESS, base::ASCIIToUTF16("Invalid Email."));
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                     base::UTF8ToUTF16("Invalid Phone"));
  profile.SetRawInfo(ADDRESS_HOME_STATE, base::UTF8ToUTF16("Invalid State"));

  // Set up the test expectations.
  expected_validity_ = {
      {ADDRESS_HOME_COUNTRY, AutofillDataModel::VALID},
      {ADDRESS_HOME_STATE, AutofillDataModel::INVALID},
      {ADDRESS_HOME_CITY, AutofillDataModel::VALID},
      {ADDRESS_HOME_DEPENDENT_LOCALITY, AutofillDataModel::EMPTY},
      {ADDRESS_HOME_ZIP, AutofillDataModel::VALID},
      {PHONE_HOME_WHOLE_NUMBER, AutofillDataModel::INVALID},
      {EMAIL_ADDRESS, AutofillDataModel::INVALID}};

  // Start the validator.
  validator_->StartProfileValidation(&profile, std::move(onvalidated_cb_));
}

}  // namespace autofill
