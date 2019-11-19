// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_ie_toolbar_import_win.h"

#include <stddef.h>

#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/win/registry.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/os_crypt/os_crypt.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <windows.h>

using base::win::RegKey;

namespace autofill {

// Defined in autofill_ie_toolbar_import_win.cc. Not exposed in the header file.
bool ImportCurrentUserProfiles(const std::string& app_locale,
                               std::vector<AutofillProfile>* profiles,
                               std::vector<CreditCard>* credit_cards);

namespace {

const wchar_t kUnitTestRegistrySubKey[] = L"SOFTWARE\\Chromium Unit Tests";
const wchar_t kUnitTestUserOverrideSubKey[] =
    L"SOFTWARE\\Chromium Unit Tests\\HKCU Override";

const wchar_t kProfileKey[] =
    L"Software\\Google\\Google Toolbar\\4.0\\Autofill\\Profiles";
const wchar_t kCreditCardKey[] =
    L"Software\\Google\\Google Toolbar\\4.0\\Autofill\\Credit Cards";
const wchar_t kPasswordHashValue[] = L"password_hash";
const wchar_t kSaltValue[] = L"salt";

struct ValueDescription {
  wchar_t const* const value_name;
  wchar_t const* const value;
};

ValueDescription profile1[] = {
  { L"name_first", L"John" },
  { L"name_middle", L"Herman" },
  { L"name_last", L"Doe" },
  { L"email", L"jdoe@test.com" },
  { L"company_name", L"Testcompany" },
  { L"phone_home_number", L"555-5555" },
  { L"phone_home_city_code", L"650" },
  { L"phone_home_country_code", L"1" },
};

ValueDescription profile2[] = {
  { L"name_first", L"Jane" },
  { L"name_last", L"Doe" },
  { L"email", L"janedoe@test.com" },
  { L"company_name", L"Testcompany" },
};

ValueDescription credit_card[] = {
    {L"credit_card_name_full", L"Tommy Gun"},
    // "4111111111111111" encrypted:
    {L"credit_card_number",
     L"\xE53F\x19AB\xC1BF\xC9EB\xECCC\x9BDA\x8515"
     L"\xE14D\x6852\x80A8\x50A3\x4375\xFD9F\x1E07"
     L"\x790E\x7336\xB773\xAF33\x93EA\xB846\xEC89"
     L"\x265C\xD0E6\x4E23\xB75F\x7983"},
    {L"credit_card_exp_month", L"11"},
    {L"credit_card_exp_4_digit_year", L"2011"},
};

ValueDescription empty_salt = {
  kSaltValue,
  L"\x1\x2\x3\x4\x5\x6\x7\x8\x9\xA\xB\xC\xD\xE\xF\x10\x11\x12\x13\x14"
};

ValueDescription empty_password = {
  kPasswordHashValue, L""
};

ValueDescription protected_salt = {
  kSaltValue, L"\x4854\xB906\x9C7C\x50A6\x4376\xFD9D\x1E02"
};

ValueDescription protected_password = {
  kPasswordHashValue, L"\x18B7\xE586\x459B\x7457\xA066\x3842\x71DA"
};

void EncryptAndWrite(RegKey* key, const ValueDescription* value) {
  std::string data;
  size_t data_size = (lstrlen(value->value) + 1) * sizeof(wchar_t);
  data.resize(data_size);
  memcpy(&data[0], value->value, data_size);

  std::string encrypted_data;
  OSCrypt::EncryptString(data, &encrypted_data);
  EXPECT_EQ(ERROR_SUCCESS, key->WriteValue(value->value_name,
      &encrypted_data[0], encrypted_data.size(), REG_BINARY));
}

void CreateSubkey(RegKey* key, wchar_t const* subkey_name,
                  const ValueDescription* values, size_t values_size) {
  RegKey subkey;
  subkey.Create(key->Handle(), subkey_name, KEY_ALL_ACCESS);
  EXPECT_TRUE(subkey.Valid());
  for (size_t i = 0; i < values_size; ++i)
    EncryptAndWrite(&subkey, values + i);
}

}  // namespace

class AutofillIeToolbarImportTest : public testing::Test {
 public:
  AutofillIeToolbarImportTest();

  // testing::Test method overrides:
  void SetUp() override;
  void TearDown() override;

 private:
  RegKey temp_hkcu_hive_key_;

  DISALLOW_COPY_AND_ASSIGN(AutofillIeToolbarImportTest);
};

AutofillIeToolbarImportTest::AutofillIeToolbarImportTest() {
}

void AutofillIeToolbarImportTest::SetUp() {
  OSCryptMocker::SetUp();
  temp_hkcu_hive_key_.Create(HKEY_CURRENT_USER,
                             kUnitTestUserOverrideSubKey,
                             KEY_ALL_ACCESS);
  EXPECT_TRUE(temp_hkcu_hive_key_.Valid());
  EXPECT_EQ(ERROR_SUCCESS, RegOverridePredefKey(HKEY_CURRENT_USER,
                                                temp_hkcu_hive_key_.Handle()));
}

void AutofillIeToolbarImportTest::TearDown() {
  EXPECT_EQ(ERROR_SUCCESS, RegOverridePredefKey(HKEY_CURRENT_USER, nullptr));
  temp_hkcu_hive_key_.Close();
  RegKey key(HKEY_CURRENT_USER, kUnitTestRegistrySubKey, KEY_ALL_ACCESS);
  key.DeleteKey(L"");
  OSCryptMocker::TearDown();
}

TEST_F(AutofillIeToolbarImportTest, TestAutofillImport) {
  RegKey profile_key;
  profile_key.Create(HKEY_CURRENT_USER, kProfileKey, KEY_ALL_ACCESS);
  EXPECT_TRUE(profile_key.Valid());

  CreateSubkey(&profile_key, L"0", profile1, base::size(profile1));
  CreateSubkey(&profile_key, L"1", profile2, base::size(profile2));

  RegKey cc_key;
  cc_key.Create(HKEY_CURRENT_USER, kCreditCardKey, KEY_ALL_ACCESS);
  EXPECT_TRUE(cc_key.Valid());
  CreateSubkey(&cc_key, L"0", credit_card, base::size(credit_card));
  EncryptAndWrite(&cc_key, &empty_password);
  EncryptAndWrite(&cc_key, &empty_salt);

  profile_key.Close();
  cc_key.Close();

  std::vector<AutofillProfile> profiles;
  std::vector<CreditCard> credit_cards;
  EXPECT_TRUE(ImportCurrentUserProfiles("en-US", &profiles, &credit_cards));
  ASSERT_EQ(2U, profiles.size());
  // The profiles are read in reverse order.
  EXPECT_EQ(profile1[0].value, profiles[1].GetRawInfo(NAME_FIRST));
  EXPECT_EQ(profile1[1].value, profiles[1].GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(profile1[2].value, profiles[1].GetRawInfo(NAME_LAST));
  EXPECT_EQ(profile1[3].value, profiles[1].GetRawInfo(EMAIL_ADDRESS));
  EXPECT_EQ(profile1[4].value, profiles[1].GetRawInfo(COMPANY_NAME));
  EXPECT_EQ(profile1[7].value,
            profiles[1].GetInfo(AutofillType(PHONE_HOME_COUNTRY_CODE), "US"));
  EXPECT_EQ(profile1[6].value,
            profiles[1].GetInfo(AutofillType(PHONE_HOME_CITY_CODE), "US"));
  EXPECT_EQ(L"5555555",
            profiles[1].GetInfo(AutofillType(PHONE_HOME_NUMBER), "US"));
  EXPECT_EQ(L"1 650-555-5555", profiles[1].GetRawInfo(PHONE_HOME_WHOLE_NUMBER));

  EXPECT_EQ(profile2[0].value, profiles[0].GetRawInfo(NAME_FIRST));
  EXPECT_EQ(profile2[1].value, profiles[0].GetRawInfo(NAME_LAST));
  EXPECT_EQ(profile2[2].value, profiles[0].GetRawInfo(EMAIL_ADDRESS));
  EXPECT_EQ(profile2[3].value, profiles[0].GetRawInfo(COMPANY_NAME));

  ASSERT_EQ(1U, credit_cards.size());
  EXPECT_EQ(credit_card[0].value,
            credit_cards[0].GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(L"4111111111111111",
            credit_cards[0].GetRawInfo(CREDIT_CARD_NUMBER));
  EXPECT_EQ(credit_card[2].value,
            credit_cards[0].GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(credit_card[3].value,
            credit_cards[0].GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));

  // Mock password encrypted cc.
  cc_key.Open(HKEY_CURRENT_USER, kCreditCardKey, KEY_ALL_ACCESS);
  EXPECT_TRUE(cc_key.Valid());
  EncryptAndWrite(&cc_key, &protected_password);
  EncryptAndWrite(&cc_key, &protected_salt);
  cc_key.Close();

  profiles.clear();
  credit_cards.clear();
  EXPECT_TRUE(ImportCurrentUserProfiles("en-US", &profiles, &credit_cards));
  // Profiles are not protected.
  EXPECT_EQ(2U, profiles.size());
  // Credit cards are.
  EXPECT_EQ(0U, credit_cards.size());
}

}  // namespace autofill
