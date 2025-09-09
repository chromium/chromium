// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/addresses/account_name_email_store.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_manager/addresses/account_name_email_store_test_api.h"
#include "components/autofill/core/browser/data_manager/addresses/test_address_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/test/test_sync_service.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using testing::ContainerEq;
using testing::Contains;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Not;
using testing::Property;

constexpr std::string_view kTestName1 = "George Washington";
constexpr std::string_view kTestEmailAddress1 = "george.washington@gmail.com";
constexpr std::string_view kTestName2 = "Thomas Jefferson";
constexpr std::string_view kTestEmailAddress2 = "thomas.jefferson@gmail.com";
constexpr GaiaId::Literal kFakeGaiaId("1234567890");

class AccountNameEmailStoreTest : public testing::Test {
 public:
  AccountNameEmailStoreTest()
      : prefs_(test::PrefServiceForTesting()),
        identity_manager_(identity_test_env_.identity_manager()),
        store_(test_adm_, *identity_manager_, sync_service_, *prefs_) {}

  void CreatePrimaryAccount(std::string_view name, std::string_view email) {
    // Calling `MakePrimaryAccountAvailable` registers the account with the
    // `AccountTrackerService`. This is a prerequisite for the next call, which
    // would otherwise crash when trying to update an unknown account.
    //
    // This initial call uses an `AccountInfo` with an empty `full_name`, so the
    // full profile update is deferred to the `UpdateAccountInfoForAccount`
    // call.
    AccountInfo info = identity_test_env().MakePrimaryAccountAvailable(
        email.data(), signin::ConsentLevel::kSignin);
    info.full_name = name;
    identity_test_env().UpdateAccountInfoForAccount(info);
  }

  AccountInfo GetPrimaryAccountInfo() {
    const CoreAccountInfo core_info =
        identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
    std::optional<AccountInfo> info =
        identity_manager_->FindExtendedAccountInfo(core_info);
    CHECK(info.has_value());
    return info.value();
  }

  void OnAccountUpdated(const AccountInfo& info) {
    signin::UpdateAccountInfoForAccount(identity_manager_.get(), info);
  }

  AccountNameEmailStore& account_name_email_store() { return store_; }
  AddressDataManager& address_data_manager() { return test_adm_; }
  signin::IdentityManager& identity_manager() { return *identity_manager_; }
  PrefService& pref_service() { return *prefs_; }
  signin::IdentityTestEnvironment& identity_test_env() {
    return identity_test_env_;
  }

 private:
  base::test::ScopedFeatureList feature_{
      features::kAutofillEnableSupportForNameAndEmail};
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<PrefService> prefs_;
  signin::IdentityTestEnvironment identity_test_env_;
  // `identity_manager_` is owned by `identity_test_env_`
  raw_ptr<signin::IdentityManager> identity_manager_;
  TestAddressDataManager test_adm_;
  syncer::TestSyncService sync_service_;
  AccountNameEmailStore store_;
};

// Check whether the passed in `AutofillProfile` has the correct NAME_FULL and
// EMAIL_ADDRESS, `AddressCountryCode` and `RecordType`.
MATCHER_P2(IsCorrectAccountNameEmail, name_full, email, "") {
  return arg->record_type() == AutofillProfile::RecordType::kAccountNameEmail &&
         arg->GetRawInfo(NAME_FULL) == name_full &&
         arg->GetRawInfo(EMAIL_ADDRESS) == email &&
         arg->GetAddressCountryCode().value().empty();
}

// Tests that a new `kAccountNameEmail` profile isn't created when an empty
// `AccountInfo` is passed into the `UpdateOrCreateAccountNameEmail` method.
TEST_F(AccountNameEmailStoreTest, EmptyAccountInfoCreation) {
  account_name_email_store().UpdateOrCreateAccountNameEmail({});
  EXPECT_THAT(address_data_manager().GetProfiles(), IsEmpty());
}

// Tests that a new `kAccountNameEmail` profile isn't created when the
// `full_name` field of passed in `AccountInfo` is empty.
TEST_F(AccountNameEmailStoreTest, EmptyAccountNameCreation) {
  AccountInfo info;
  info.email = kTestEmailAddress1;
  account_name_email_store().UpdateOrCreateAccountNameEmail(info);
  EXPECT_THAT(address_data_manager().GetProfiles(), IsEmpty());
}

// Tests that the `UpdateOrCreateAccountNameEmail` method creates / updates
// `kAccountNameEmail` with the correct info.
TEST_F(AccountNameEmailStoreTest, SpecificInfoCreationUpdate) {
  AccountInfo info;
  info.full_name = kTestName1;
  info.email = kTestEmailAddress1;

  account_name_email_store().UpdateOrCreateAccountNameEmail(info);

  EXPECT_THAT(address_data_manager().GetProfiles(),
              ElementsAre(IsCorrectAccountNameEmail(
                  base::UTF8ToUTF16(kTestName1),
                  base::UTF8ToUTF16(kTestEmailAddress1))));
}

// Tests that the `UpdateOrCreateAccountNameEmail` correctly updates the
// `kAutofillNameAndEmailProfileSignature` pref.
TEST_F(AccountNameEmailStoreTest, HashPrefSaving) {
  AccountInfo info;
  info.full_name = kTestName1;
  info.email = kTestEmailAddress1;
  account_name_email_store().UpdateOrCreateAccountNameEmail(info);

  EXPECT_EQ(
      pref_service().GetString(prefs::kAutofillNameAndEmailProfileSignature),
      test_api(&account_name_email_store()).HashAccountInfo(info));
}

// Tests that the `UpdateOrCreateAccountNameEmail` returns early (does nothing)
// when account info with the same hash as the stored one in pref was passed in.
TEST_F(AccountNameEmailStoreTest, EarlyReturnWhenHashesAreEqual) {
  AccountInfo info;
  info.full_name = kTestName1;
  info.email = kTestEmailAddress1;

  const std::string hash =
      test_api(&account_name_email_store()).HashAccountInfo(info);

  pref_service().SetString(prefs::kAutofillNameAndEmailProfileSignature, hash);
  account_name_email_store().UpdateOrCreateAccountNameEmail(info);

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

  account_name_email_store().UpdateOrCreateAccountNameEmail(info1);
  account_name_email_store().UpdateOrCreateAccountNameEmail(info2);

  EXPECT_THAT(
      address_data_manager().GetProfiles(),
      Contains(IsCorrectAccountNameEmail(base::UTF8ToUTF16(kTestName1),
                                         base::UTF8ToUTF16(kTestEmailAddress1)))
          .Times(0));
}

// Tests that the `OnExtendedAccountInfoUpdated` method will create the
// `kAccountNameEmail` profile on signin.
TEST_F(AccountNameEmailStoreTest, OnExtendedAccountInfoUpdated_CreatePath) {
  ASSERT_THAT(address_data_manager().GetProfiles(), IsEmpty());
  CreatePrimaryAccount(kTestName1, kTestEmailAddress1);
  EXPECT_THAT(address_data_manager().GetProfiles(),
              ElementsAre(IsCorrectAccountNameEmail(
                  base::UTF8ToUTF16(kTestName1),
                  base::UTF8ToUTF16(kTestEmailAddress1))));
}

// Tests that the `OnExtendedAccountInfoUpdated` method will update the already
// existing `kAccountNameEmail` profile.
TEST_F(AccountNameEmailStoreTest, OnExtendedAccountInfoUpdated_UpdatePath) {
  CreatePrimaryAccount(kTestName1, kTestEmailAddress1);
  ASSERT_THAT(address_data_manager().GetProfiles(),
              Not(ElementsAre(IsCorrectAccountNameEmail(
                  base::UTF8ToUTF16(kTestName2),
                  base::UTF8ToUTF16(kTestEmailAddress2)))));

  AccountInfo info = GetPrimaryAccountInfo();
  info.full_name = kTestName2;
  info.email = kTestEmailAddress2;
  // This call should trigger OnExtendedAccountInfoUpdated and update the
  // `kAccountNameEmail` profile with new info.
  OnAccountUpdated(info);

  EXPECT_THAT(address_data_manager().GetProfiles(),
              ElementsAre(IsCorrectAccountNameEmail(
                  base::UTF8ToUTF16(kTestName2),
                  base::UTF8ToUTF16(kTestEmailAddress2))));
}

// Tests that the `OnExtendedAccountInfoUpdated` method will not update
// `kAccountNameEmail` autofill profile if primary account info and passed in
// info have different `GaiaId`s
TEST_F(AccountNameEmailStoreTest, OnExtendedAccountInfoUpdated_WrongGaiaId) {
  CreatePrimaryAccount(kTestName1, kTestEmailAddress1);

  const std::vector<const AutofillProfile*> profiles_before_update =
      address_data_manager().GetProfiles();
  ASSERT_THAT(profiles_before_update,
              ElementsAre(IsCorrectAccountNameEmail(
                  base::UTF8ToUTF16(kTestName1),
                  base::UTF8ToUTF16(kTestEmailAddress1))));

  const AccountInfo info = identity_test_env().MakeAccountAvailable(
      kTestEmailAddress2, {std::nullopt, false, kFakeGaiaId});
  OnAccountUpdated(info);
  EXPECT_THAT(profiles_before_update,
              ContainerEq(address_data_manager().GetProfiles()));
}

// Tests that the `OnAddressDataChanged` method will set
// `kAutofillNameAndEmailProfileNotSelectedCounter` pref to a value greater than
// `kAutofillNameAndEmailProfileNotSelectedThreshold` if the user is logged in
// but `kAccountNameEmail` profile was deleted.
TEST_F(AccountNameEmailStoreTest, OnAddressDataChanged_ProfileDeleted) {
  CreatePrimaryAccount(kTestName1, kTestEmailAddress1);
  ASSERT_EQ(pref_service().GetInteger(
                prefs::kAutofillNameAndEmailProfileNotSelectedCounter),
            0);

  address_data_manager().RemoveProfile(
      address_data_manager().GetProfiles()[0]->guid());

  EXPECT_GT(pref_service().GetInteger(
                prefs::kAutofillNameAndEmailProfileNotSelectedCounter),
            features::kAutofillNameAndEmailProfileNotSelectedThreshold.Get());
}

// Tests that the `OnAddressDataChanged` method will not change the value of the
// `kAutofillNameAndEmailProfileNotSelectedCounter` pref if a profile other than
// the `kAccountNameEmail` was deleted.
TEST_F(AccountNameEmailStoreTest, OnAddressDataChanged_PrefNotChanged) {
  CreatePrimaryAccount(kTestName1, kTestEmailAddress1);

  AutofillProfile profile{
      AutofillProfile::RecordType::kAccount,
      address_data_manager().GetDefaultCountryCodeForNewAddress()};
  address_data_manager().AddProfile(profile);

  ASSERT_EQ(pref_service().GetInteger(
                prefs::kAutofillNameAndEmailProfileNotSelectedCounter),
            0);

  address_data_manager().RemoveProfile(
      address_data_manager()
          .GetProfilesByRecordType(AutofillProfile::RecordType::kAccount)[0]
          ->guid());

  EXPECT_EQ(pref_service().GetInteger(
                prefs::kAutofillNameAndEmailProfileNotSelectedCounter),
            0);
}

// Tests that if `kAccountNameEmail` profile was removed and pref set to a
// number greater than the threshold, after the name change there will be new
// `kAccountNameEmail` profile and pref will be set to 0.
TEST_F(AccountNameEmailStoreTest, ProfileReappearsAfterNameChange) {
  CreatePrimaryAccount(kTestName1, kTestEmailAddress1);

  address_data_manager().RemoveProfile(
      address_data_manager()
          .GetProfilesByRecordType(
              AutofillProfile::RecordType::kAccountNameEmail)[0]
          ->guid());

  ASSERT_THAT(address_data_manager().GetProfiles(), IsEmpty());
  ASSERT_GT(pref_service().GetInteger(
                prefs::kAutofillNameAndEmailProfileNotSelectedCounter),
            features::kAutofillNameAndEmailProfileNotSelectedThreshold.Get());

  AccountInfo info = GetPrimaryAccountInfo();
  info.full_name = kTestName2;
  OnAccountUpdated(info);

  EXPECT_THAT(
      address_data_manager().GetProfiles(),
      ElementsAre(IsCorrectAccountNameEmail(base::UTF8ToUTF16(info.full_name),
                                            base::UTF8ToUTF16(info.email))));

  EXPECT_EQ(pref_service().GetInteger(
                prefs::kAutofillNameAndEmailProfileNotSelectedCounter),
            0);
}

// ChromeOS does not support signing out
#if !BUILDFLAG(IS_CHROMEOS)
// Tests that the `OnExtendedAccountInfoRemoved` method will remove
// `kAccountNameEmail` profile
TEST_F(AccountNameEmailStoreTest, OnExtendedAccountInfoRemoved) {
  CreatePrimaryAccount(kTestName1, kTestEmailAddress1);
  ASSERT_THAT(
      address_data_manager().GetProfiles(),
      ElementsAre(Property(&AutofillProfile::record_type,
                           AutofillProfile::RecordType::kAccountNameEmail)));

  identity_test_env().EnableRemovalOfExtendedAccountInfo();
  identity_test_env().ClearPrimaryAccount();

  EXPECT_THAT(address_data_manager().GetProfiles(), testing::IsEmpty());
}

// Tests that the `OnExtendedAccountInfoRemoved` method will not remove
// `kAccountNameEmail` profile if it is called with info of a wrong profile.
TEST_F(AccountNameEmailStoreTest, OnExtendedAccountInfoRemoved_WrongInfo) {
  CreatePrimaryAccount(kTestName1, kTestEmailAddress1);

  const AccountInfo info2 = identity_test_env().MakeAccountAvailable(
      kTestEmailAddress2, {std::nullopt, false, kFakeGaiaId});

  ASSERT_THAT(address_data_manager().GetProfiles(),
              ElementsAre(IsCorrectAccountNameEmail(
                  base::UTF8ToUTF16(kTestName1),
                  base::UTF8ToUTF16(kTestEmailAddress1))));

  identity_test_env().EnableRemovalOfExtendedAccountInfo();
// On mobile platforms, accounts are stored outside of Chrome, thus
// `AccountsMutator` of an `IdentityManager` is uninitialized.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  CHECK(!identity_manager().GetAccountsMutator());
  identity_test_env().RemoveRefreshTokenForAccount(info2.account_id);
#else
  CHECK(identity_manager().GetAccountsMutator());
  identity_manager().GetAccountsMutator()->RemoveAccount(
      info2.account_id,
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

  EXPECT_THAT(address_data_manager().GetProfiles(),
              ElementsAre(IsCorrectAccountNameEmail(
                  base::UTF8ToUTF16(kTestName1),
                  base::UTF8ToUTF16(kTestEmailAddress1))));
}

#endif  // !BUILDFLAG(CHROME_OS)

}  // namespace

}  // namespace autofill
