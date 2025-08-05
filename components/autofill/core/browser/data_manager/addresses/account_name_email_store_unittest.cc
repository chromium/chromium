// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/addresses/account_name_email_store.h"

#include <string_view>

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_manager/addresses/account_name_email_store_test_api.h"
#include "components/autofill/core/browser/data_manager/addresses/test_address_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using testing::ElementsAre;
using testing::Property;

constexpr std::string_view kTestName1 = "George Washington";
constexpr std::string_view kTestEmailAddress1 = "george.washington@gmail.com";
constexpr std::string_view kTestName2 = "Thomas Jefferson";
constexpr std::string_view kTestEmailAddress2 = "thomas.jefferson@gmail.com";

class AccountNameEmailStoreTest : public testing::Test {
 public:
  AccountNameEmailStoreTest()
      : prefs_(test::PrefServiceForTesting()), store_(test_adm_, *prefs_) {}

  AddressDataManager& address_data_manager() { return test_adm_; }
  PrefService& pref_service() { return *prefs_; }

  AccountNameEmailStore* account_name_email_store() { return &store_; }

 private:
  base::test::ScopedFeatureList feature_{
      features::kAutofillEnableSupportForNameAndEmail};
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<PrefService> prefs_;
  TestAddressDataManager test_adm_;
  AccountNameEmailStore store_;
};

// Tests that a new `kAccountNameEmail` profile isn't created when an empty
// `AccountInfo` is passed into the `UpdateOrCreateAccountNameEmail` method.
TEST_F(AccountNameEmailStoreTest, EmptyAccountInfoCreation) {
  account_name_email_store()->UpdateOrCreateAccountNameEmail({});
  EXPECT_THAT(address_data_manager().GetProfiles(), testing::IsEmpty());
}

// Check whether the passed in `AutofillProfile` has the correct NAME_FULL and
// EMAIL_ADDRESS.
MATCHER_P2(IsCorrectNameEmailInfo, name_full, email, "") {
  return arg->record_type() == AutofillProfile::RecordType::kAccountNameEmail &&
         arg->GetRawInfo(NAME_FULL) == name_full &&
         arg->GetRawInfo(EMAIL_ADDRESS) == email;
}

// Tests that the `UpdateOrCreateAccountNameEmail` method creates / updates
// `kAccountNameEmail` with the correct info.
TEST_F(AccountNameEmailStoreTest, SpecificInfoCreationUpdate) {
  AccountInfo info;
  info.full_name = kTestName1;
  info.email = kTestEmailAddress1;

  account_name_email_store()->UpdateOrCreateAccountNameEmail(info);

  EXPECT_THAT(address_data_manager().GetProfiles(),
              ElementsAre(IsCorrectNameEmailInfo(
                  base::UTF8ToUTF16(kTestName1),
                  base::UTF8ToUTF16(kTestEmailAddress1))));
}

// Tests that the `UpdateOrCreateAccountNameEmail` correctly updates the
// `kAutofillNameAndEmailProfileSignature` pref.
TEST_F(AccountNameEmailStoreTest, HashPrefSaving) {
  AccountInfo info;
  info.full_name = kTestName1;
  info.email = kTestEmailAddress1;
  account_name_email_store()->UpdateOrCreateAccountNameEmail(info);

  EXPECT_EQ(
      pref_service().GetString(prefs::kAutofillNameAndEmailProfileSignature),
      test_api(account_name_email_store()).HashAccountInfo(info));
}

// Tests that the `UpdateOrCreateAccountNameEmail` returns early (does nothing)
// when account info with the same hash as the stored one in pref was passed in.
TEST_F(AccountNameEmailStoreTest, EarlyReturnWhenHashesAreEqual) {
  AccountInfo info;
  info.full_name = kTestName1;
  info.email = kTestEmailAddress1;

  const std::string hash =
      test_api(account_name_email_store()).HashAccountInfo(info);

  pref_service().SetString(prefs::kAutofillNameAndEmailProfileSignature, hash);
  account_name_email_store()->UpdateOrCreateAccountNameEmail(info);

  EXPECT_EQ(hash, pref_service().GetString(
                      prefs::kAutofillNameAndEmailProfileSignature));
}

// Tests that the `UpdateOrCreateAccountNameEmail` removes the Account Name
// Email profile when updating.
TEST_F(AccountNameEmailStoreTest, RemovingProfile) {
  AccountInfo info1;
  info1.full_name = kTestName1;
  info1.email = kTestEmailAddress1;

  AccountInfo info2;
  info2.full_name = kTestName2;
  info2.email = kTestEmailAddress2;

  account_name_email_store()->UpdateOrCreateAccountNameEmail(info1);
  account_name_email_store()->UpdateOrCreateAccountNameEmail(info2);

  EXPECT_THAT(address_data_manager().GetProfiles(),
              testing::Contains(
                  IsCorrectNameEmailInfo(base::UTF8ToUTF16(kTestName1),
                                         base::UTF8ToUTF16(kTestEmailAddress1)))
                  .Times(0));
}

}  // namespace

}  // namespace autofill
