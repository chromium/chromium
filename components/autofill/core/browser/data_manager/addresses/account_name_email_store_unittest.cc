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
#include "components/sync/base/features.h"
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

class AccountNameEmailStoreCoreTest : public testing::Test {
 public:
  AccountNameEmailStoreCoreTest()
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

  void SimulateProfileRemoval(
      const AutofillProfile* profile,
      bool non_permanent_account_profile_removal = false) {
    CHECK(profile);
    // `TestAddressDataManager` does not send `AutofillProfileChange`
    // notifications, so it has to be simulated.
    AutofillProfileChange change(non_permanent_account_profile_removal
                                     ? AutofillProfileChange::HIDE_IN_AUTOFILL
                                     : AutofillProfileChange::REMOVE,
                                 profile->guid(), *profile);
    address_data_manager().RemoveProfile(profile->guid());
    account_name_email_store().ApplyChange(change);
  }

  void SetAutofillSyncToggleStatus(bool syncing) {
    syncer::UserSelectableTypeSet selected_sync_types =
        sync_service().GetUserSettings()->GetSelectedTypes();
    if (syncing) {
      selected_sync_types.Put(syncer::UserSelectableType::kAutofill);
    } else {
      selected_sync_types.Remove(syncer::UserSelectableType::kAutofill);
    }
    sync_service().GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false, selected_sync_types);
    sync_service().FireStateChanged();
  }

  AccountNameEmailStore& account_name_email_store() { return store_; }
  AddressDataManager& address_data_manager() { return test_adm_; }
  signin::IdentityManager& identity_manager() { return *identity_manager_; }
  PrefService& pref_service() { return *prefs_; }
  signin::IdentityTestEnvironment& identity_test_env() {
    return identity_test_env_;
  }
  syncer::TestSyncService& sync_service() { return sync_service_; }

 protected:
  base::test::ScopedFeatureList features_;

 private:
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

class AccountNameEmailStoreTest : public AccountNameEmailStoreCoreTest {
 public:
  AccountNameEmailStoreTest() {
    features_.InitWithFeatures(
        {features::kAutofillEnableSupportForNameAndEmail}, {});
  }
};

// Tests that a new `kAccountNameEmail` profile isn't created when an empty
// `AccountInfo` is passed into the `MaybeUpdateOrCreateAccountNameEmail`
// method.
TEST_F(AccountNameEmailStoreTest, EmptyAccountInfoCreation) {
  account_name_email_store().MaybeUpdateOrCreateAccountNameEmail();
  EXPECT_THAT(address_data_manager().GetProfiles(), IsEmpty());
}

// Tests that a new kAccountNameEmail profile isn't created when the account
// info misses the full_name.
TEST_F(AccountNameEmailStoreTest, EmptyAccountNameCreation) {
  CreatePrimaryAccount(std::string(), kTestEmailAddress1);
  EXPECT_THAT(address_data_manager().GetProfiles(), IsEmpty());
}

// Tests that a new kAccountNameEmail profile isn't created when the account
// info's full_name is an email address.
TEST_F(AccountNameEmailStoreTest, FullNameIsEmailAddress) {
  CreatePrimaryAccount(kTestEmailAddress1, kTestEmailAddress1);
  EXPECT_THAT(address_data_manager().GetProfiles(), IsEmpty());
}

// Tests that a new kAccountNameEmail profile is removed when name was changed
// to an email address.
TEST_F(AccountNameEmailStoreTest, NameUpdatedToEmailAddress) {
  CreatePrimaryAccount(kTestName1, kTestEmailAddress1);
  ASSERT_THAT(address_data_manager().GetProfiles(),
              ElementsAre(IsCorrectAccountNameEmail(
                  base::UTF8ToUTF16(kTestName1),
                  base::UTF8ToUTF16(kTestEmailAddress1))));

  // Update the name to an email address.
  AccountInfo info = GetPrimaryAccountInfo();
  info.full_name = kTestEmailAddress1;
  OnAccountUpdated(info);

  // The old profile should be removed and nothing should be created.
  EXPECT_THAT(address_data_manager().GetProfiles(), IsEmpty());

  // Update the name to a valid value.
  info = GetPrimaryAccountInfo();
  info.full_name = kTestName2;
  OnAccountUpdated(info);

  // A profile with a valid name should be created.
  EXPECT_THAT(address_data_manager().GetProfiles(),
              ElementsAre(IsCorrectAccountNameEmail(
                  base::UTF8ToUTF16(kTestName2),
                  base::UTF8ToUTF16(kTestEmailAddress1))));
}

// Tests that a new kAccountNameEmail profile isn't created when autofill is not
// synced.
TEST_F(AccountNameEmailStoreTest, AutofillNotSynced) {
  sync_service().SetSignedOut();
  CreatePrimaryAccount(kTestName1, kTestEmailAddress1);
  EXPECT_THAT(address_data_manager().GetProfiles(), IsEmpty());
}

// Tests that a new kAccountNameEmail profile is removed if user disabled
// autofill sync toggle.
TEST_F(AccountNameEmailStoreTest, ProfileRemovedAfterSyncOff) {
  CreatePrimaryAccount(kTestName1, kTestEmailAddress1);

  ASSERT_THAT(address_data_manager().GetProfiles(),
              ElementsAre(IsCorrectAccountNameEmail(
                  base::UTF8ToUTF16(kTestName1),
                  base::UTF8ToUTF16(kTestEmailAddress1))));

  SetAutofillSyncToggleStatus(false);

  EXPECT_THAT(address_data_manager().GetProfiles(), IsEmpty());
}

TEST_F(AccountNameEmailStoreTest, ProfileNotCreatedBeforeDataLoaded) {
  sync_service().SetDownloadStatusFor(
      {syncer::DataType::PRIORITY_PREFERENCES},
      syncer::SyncService::DataTypeDownloadStatus::kWaitingForUpdates);

  CreatePrimaryAccount(kTestName1, kTestEmailAddress1);
  EXPECT_THAT(address_data_manager().GetProfiles(), IsEmpty());
}

// Tests that the created profile holds the correct data.
TEST_F(AccountNameEmailStoreTest, SpecificInfoCreationUpdate) {
  CreatePrimaryAccount(kTestName1, kTestEmailAddress1);
  EXPECT_THAT(address_data_manager().GetProfiles(),
              ElementsAre(IsCorrectAccountNameEmail(
                  base::UTF8ToUTF16(kTestName1),
                  base::UTF8ToUTF16(kTestEmailAddress1))));
}

// Tests that the creating the profile correctly updates the
// `kAutofillNameAndEmailProfileSignature` pref.
TEST_F(AccountNameEmailStoreTest, HashPrefSaving) {
  CreatePrimaryAccount(kTestName1, kTestEmailAddress1);

  EXPECT_EQ(
      pref_service().GetString(prefs::kAutofillNameAndEmailProfileSignature),
      test_api(&account_name_email_store())
          .HashAccountInfo(identity_manager().FindExtendedAccountInfo(
              identity_manager().GetPrimaryAccountInfo(
                  signin::ConsentLevel::kSignin))));
}

// Tests that old profile is removed when new primary account is used.
TEST_F(AccountNameEmailStoreTest, RemovingProfile) {
  CreatePrimaryAccount(kTestName1, kTestEmailAddress1);
  CreatePrimaryAccount(kTestName2, kTestEmailAddress2);

  EXPECT_THAT(address_data_manager().GetProfiles(),
              ElementsAre(IsCorrectAccountNameEmail(
                  base::UTF8ToUTF16(kTestName2),
                  base::UTF8ToUTF16(kTestEmailAddress2))));
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

// Tests that the `kAutofillNameAndEmailProfileNotSelectedCounter` pref is set
// to a value greater than `kAutofillNameAndEmailProfileNotSelectedThreshold` if
// there was a hard removal.
TEST_F(AccountNameEmailStoreTest, AccountNameEmailProfileRemoved) {
  CreatePrimaryAccount(kTestName1, kTestEmailAddress1);
  ASSERT_EQ(pref_service().GetInteger(
                prefs::kAutofillNameAndEmailProfileNotSelectedCounter),
            0);

  SimulateProfileRemoval(address_data_manager().GetProfiles()[0]);

  EXPECT_GT(pref_service().GetInteger(
                prefs::kAutofillNameAndEmailProfileNotSelectedCounter),
            features::kAutofillNameAndEmailProfileNotSelectedThreshold.Get());
}

// Tests that the `kAutofillNameAndEmailProfileNotSelectedCounter` pref is not
// set, if the removal was soft.
TEST_F(AccountNameEmailStoreTest, AccountNameEmailProfileHidden) {
  CreatePrimaryAccount(kTestName1, kTestEmailAddress1);
  ASSERT_EQ(pref_service().GetInteger(
                prefs::kAutofillNameAndEmailProfileNotSelectedCounter),
            0);

  SimulateProfileRemoval(address_data_manager().GetProfiles()[0],
                         /*non_permanent_account_profile_removal=*/true);

  EXPECT_EQ(pref_service().GetInteger(
                prefs::kAutofillNameAndEmailProfileNotSelectedCounter),
            0);
}

// Tests that removals of profiles other than kAccountNameEmail do not set the
// pref.
TEST_F(AccountNameEmailStoreTest, NotAccountNameEmailProfileRemoved) {
  CreatePrimaryAccount(kTestName1, kTestEmailAddress1);

  AutofillProfile profile{
      AutofillProfile::RecordType::kAccount,
      address_data_manager().GetDefaultCountryCodeForNewAddress()};
  address_data_manager().AddProfile(profile);

  ASSERT_EQ(pref_service().GetInteger(
                prefs::kAutofillNameAndEmailProfileNotSelectedCounter),
            0);

  SimulateProfileRemoval(&profile);

  EXPECT_EQ(pref_service().GetInteger(
                prefs::kAutofillNameAndEmailProfileNotSelectedCounter),
            0);
}

// Tests that if kAccountNameEmail profile was hard removed (thus pref
// set to a number greater than the threshold), after the name change there will
// be a new kAccountNameEmail profile and the
// `kAutofillNameAndEmailProfileNotSelectedCounter` pref will be set to 0.
TEST_F(AccountNameEmailStoreTest, ProfileReappearsAfterNameChange) {
  CreatePrimaryAccount(kTestName1, kTestEmailAddress1);
  SimulateProfileRemoval(address_data_manager().GetProfiles()[0]);

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

TEST_F(AccountNameEmailStoreTest, OnCounterPrefUpdated) {
  ASSERT_EQ(pref_service().GetInteger(
                prefs::kAutofillNameAndEmailProfileNotSelectedCounter),
            0);
  CreatePrimaryAccount(kTestName1, kTestEmailAddress1);
  ASSERT_THAT(address_data_manager().GetProfiles(),
              ElementsAre(IsCorrectAccountNameEmail(
                  base::UTF8ToUTF16(kTestName1),
                  base::UTF8ToUTF16(kTestEmailAddress1))));

  // Setting the pref to a value smaller or equal to
  // `kAutofillNameAndEmailProfileNotSelectedThreshold`, shouldn't remove the
  // profile.
  pref_service().SetInteger(
      prefs::kAutofillNameAndEmailProfileNotSelectedCounter,
      features::kAutofillNameAndEmailProfileNotSelectedThreshold.Get());
  EXPECT_THAT(address_data_manager().GetProfiles(),
              ElementsAre(IsCorrectAccountNameEmail(
                  base::UTF8ToUTF16(kTestName1),
                  base::UTF8ToUTF16(kTestEmailAddress1))));

  // Setting the pref to a value greater than
  // `kAutofillNameAndEmailProfileNotSelectedThreshold`, should remove the
  // kAccountNameEmail profile.
  pref_service().SetInteger(
      prefs::kAutofillNameAndEmailProfileNotSelectedCounter,
      features::kAutofillNameAndEmailProfileNotSelectedThreshold.Get() + 1);
  EXPECT_THAT(address_data_manager().GetProfiles(), IsEmpty());
}

// ChromeOS does not support signing out
#if !BUILDFLAG(IS_CHROMEOS)

// Tests that the user's gets their `kAutofillNameAndEmailProfileSignature` pref
// cleared on signout and the kAccountNameEmail profile is removed.
TEST_F(AccountNameEmailStoreTest, SignOut) {
  CreatePrimaryAccount(kTestName1, kTestEmailAddress1);
  ASSERT_THAT(
      address_data_manager().GetProfiles(),
      ElementsAre(Property(&AutofillProfile::record_type,
                           AutofillProfile::RecordType::kAccountNameEmail)));

  pref_service().SetInteger(
      prefs::kAutofillNameAndEmailProfileNotSelectedCounter, 5);
  pref_service().SetBoolean(prefs::kAutofillWasNameAndEmailProfileUsed, true);

  identity_test_env().EnableRemovalOfExtendedAccountInfo();
  identity_test_env().ClearPrimaryAccount();
  sync_service().SetSignedOut();
  sync_service().FireStateChanged();

  EXPECT_THAT(address_data_manager().GetProfiles(), testing::IsEmpty());
  EXPECT_THAT(
      pref_service().GetString(prefs::kAutofillNameAndEmailProfileSignature),
      IsEmpty());
  EXPECT_EQ(pref_service().GetInteger(
                prefs::kAutofillNameAndEmailProfileNotSelectedCounter),
            0);
  EXPECT_FALSE(
      pref_service().GetBoolean(prefs::kAutofillWasNameAndEmailProfileUsed));
}

// Tests that the kAccountNameEmail profile will be recreated on sign in after
// it has been deleted on sign out.
TEST_F(AccountNameEmailStoreTest, SignOutAndSignIn) {
  CreatePrimaryAccount(kTestName1, kTestEmailAddress1);
  ASSERT_THAT(address_data_manager().GetProfiles(),
              ElementsAre(IsCorrectAccountNameEmail(
                  base::UTF8ToUTF16(kTestName1),
                  base::UTF8ToUTF16(kTestEmailAddress1))));
  // Sign out.
  identity_test_env().EnableRemovalOfExtendedAccountInfo();
  identity_test_env().ClearPrimaryAccount();
  sync_service().SetSignedOut();
  sync_service().FireStateChanged();
  ASSERT_THAT(address_data_manager().GetProfiles(), IsEmpty());

  // Sign in.
  CreatePrimaryAccount(kTestName1, kTestEmailAddress1);
  sync_service().SetSignedIn(signin::ConsentLevel::kSync);
  sync_service().FireStateChanged();

  EXPECT_EQ(pref_service().GetInteger(
                prefs::kAutofillNameAndEmailProfileNotSelectedCounter),
            0);
  EXPECT_THAT(
      address_data_manager().GetProfiles(),
      ElementsAre(Property(&AutofillProfile::record_type,
                           AutofillProfile::RecordType::kAccountNameEmail)));
}

// Tests that the kAccountNameEmail profile will not be recreated if the
// `kAutofillNameAndEmailProfileNotSelectedCounter` pref exceedes the threshold.
TEST_F(AccountNameEmailStoreTest, SignInAfterHardRemove) {
  CreatePrimaryAccount(kTestName1, kTestEmailAddress1);
  ASSERT_THAT(
      address_data_manager().GetProfiles(),
      ElementsAre(Property(&AutofillProfile::record_type,
                           AutofillProfile::RecordType::kAccountNameEmail)));

  SimulateProfileRemoval(address_data_manager().GetProfiles()[0]);

  identity_test_env().EnableRemovalOfExtendedAccountInfo();
  identity_test_env().ClearPrimaryAccount();
  sync_service().SetSignedOut();
  sync_service().FireStateChanged();

  CreatePrimaryAccount(kTestName1, kTestEmailAddress1);
  sync_service().SetSignedIn(signin::ConsentLevel::kSignin);
  sync_service().SetDownloadStatusFor(
      {syncer::DataType::PRIORITY_PREFERENCES},
      syncer::SyncService::DataTypeDownloadStatus::kWaitingForUpdates);
  sync_service().FireStateChanged();
  EXPECT_THAT(address_data_manager().GetProfiles(), IsEmpty());

  // The `TestSyncService` uses a mock server, so manually simulate the prefs
  // being set to correct (i.e. stored on the server) values.
  pref_service().SetString(
      prefs::kAutofillNameAndEmailProfileSignature,
      test_api(&account_name_email_store())
          .HashAccountInfo(identity_manager().FindExtendedAccountInfo(
              identity_manager().GetPrimaryAccountInfo(
                  signin::ConsentLevel::kSignin))));
  pref_service().SetInteger(
      prefs::kAutofillNameAndEmailProfileNotSelectedCounter,
      features::kAutofillNameAndEmailProfileNotSelectedThreshold.Get() + 1);
  sync_service().SetDownloadStatusFor(
      {syncer::DataType::PRIORITY_PREFERENCES},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  sync_service().FireStateChanged();

  EXPECT_THAT(address_data_manager().GetProfiles(), IsEmpty());
  EXPECT_GT(pref_service().GetInteger(
                prefs::kAutofillNameAndEmailProfileNotSelectedCounter),
            features::kAutofillNameAndEmailProfileNotSelectedThreshold.Get());
}

#endif  // !BUILDFLAG(CHROME_OS)

// Tests that the kAccountNameEmail profile will be recreated after enabling
// autofill sync toggle.
TEST_F(AccountNameEmailStoreTest, AutofillSyncToggleOffAndOn) {
  // Sign in and disable autofill sync toggle.
  CreatePrimaryAccount(kTestName1, kTestEmailAddress1);
  ASSERT_THAT(address_data_manager().GetProfiles(),
              ElementsAre(IsCorrectAccountNameEmail(
                  base::UTF8ToUTF16(kTestName1),
                  base::UTF8ToUTF16(kTestEmailAddress1))));
  SetAutofillSyncToggleStatus(false);
  ASSERT_THAT(address_data_manager().GetProfiles(), IsEmpty());

  // Enable the autofill sync toggle.
  SetAutofillSyncToggleStatus(true);
  EXPECT_EQ(pref_service().GetInteger(
                prefs::kAutofillNameAndEmailProfileNotSelectedCounter),
            0);
  EXPECT_THAT(
      address_data_manager().GetProfiles(),
      ElementsAre(Property(&AutofillProfile::record_type,
                           AutofillProfile::RecordType::kAccountNameEmail)));
}

// Tests that recreating the kAccountNameEmail profile will not alter the
// counter pref.
TEST_F(AccountNameEmailStoreTest,
       SwitchingAutofillSyncToggleDoesntAlterCounter) {
  const int test_counter_value = 5;

  // Sign in and disable autofill sync toggle.
  CreatePrimaryAccount(kTestName1, kTestEmailAddress1);
  pref_service().SetInteger(
      prefs::kAutofillNameAndEmailProfileNotSelectedCounter,
      test_counter_value);
  SetAutofillSyncToggleStatus(false);
  ASSERT_EQ(pref_service().GetInteger(
                prefs::kAutofillNameAndEmailProfileNotSelectedCounter),
            test_counter_value);

  // Enable the autofill sync toggle again, verify that the counter did not
  // change.
  SetAutofillSyncToggleStatus(true);
  EXPECT_EQ(pref_service().GetInteger(
                prefs::kAutofillNameAndEmailProfileNotSelectedCounter),
            test_counter_value);
}

// Tests that the kAccountNameEmail profile will not be recreated if the
// `kAutofillNameAndEmailProfileNotSelectedCounter` pref exceedes the threshold.
TEST_F(AccountNameEmailStoreTest, AutofillSyncToggleOnAfterHardRemove) {
  // Sign in, explicitly remove the kAccountNameEmail profile and turn off
  // autofill sync toggle.
  CreatePrimaryAccount(kTestName1, kTestEmailAddress1);
  ASSERT_THAT(
      address_data_manager().GetProfiles(),
      ElementsAre(Property(&AutofillProfile::record_type,
                           AutofillProfile::RecordType::kAccountNameEmail)));
  SimulateProfileRemoval(address_data_manager().GetProfiles()[0]);
  SetAutofillSyncToggleStatus(false);
  ASSERT_GT(pref_service().GetInteger(
                prefs::kAutofillNameAndEmailProfileNotSelectedCounter),
            features::kAutofillNameAndEmailProfileNotSelectedThreshold.Get());

  // Enabling the sync toggle again does not reset the pref.
  SetAutofillSyncToggleStatus(true);
  EXPECT_THAT(address_data_manager().GetProfiles(), IsEmpty());
  EXPECT_GT(pref_service().GetInteger(
                prefs::kAutofillNameAndEmailProfileNotSelectedCounter),
            features::kAutofillNameAndEmailProfileNotSelectedThreshold.Get());
}

// Tests that the kAccountNameEmail profile will be recreated even if the stored
// hash matches the account info, if the actual profile has outdated data.
// It is a regression for the following bug:
// If there were two devices A,B and the account info got updated on the
// device A, the profile will correctly be recreated there, however since
// the hash of the account info is synced, the device B will download the
// updated hash and detect that its stored hash already matches the account info
// and thus won't update the profile.
TEST_F(AccountNameEmailStoreTest,
       ProfileRecreatedIfNameUpdatedOnDifferentDevice) {
  CreatePrimaryAccount(kTestName1, kTestEmailAddress1);
  ASSERT_THAT(
      address_data_manager().GetProfiles(),
      ElementsAre(Property(&AutofillProfile::record_type,
                           AutofillProfile::RecordType::kAccountNameEmail)));

  AccountInfo info = GetPrimaryAccountInfo();
  info.full_name = kTestName2;

  // Set the hash pref to the updated value before the actual autofill profile
  // data changes.
  pref_service().SetString(
      prefs::kAutofillNameAndEmailProfileSignature,
      test_api(&account_name_email_store()).HashAccountInfo(info));

  // Try recreating the profile.
  OnAccountUpdated(info);

  EXPECT_THAT(
      address_data_manager().GetProfiles(),
      ElementsAre(IsCorrectAccountNameEmail(base::UTF8ToUTF16(info.full_name),
                                            base::UTF8ToUTF16(info.email))));
}

#if BUILDFLAG(IS_IOS)
// TODO(crbug.com/449708427): Remove once `AccountInfo` supports full_name on
// IOS.
TEST_F(AccountNameEmailStoreTest,
       MaybeUpdateOrCreateAccountNameEmail_ExplicitlyPassedNameEmail) {
  // Needed since MaybeUpdateOrCreateAccountNameEmail() takes references.
  std::string test_name = "George Washington";
  std::string test_email = "testing@gmail.com";

  CreatePrimaryAccount(std::string(), kTestEmailAddress1);
  ASSERT_THAT(address_data_manager().GetProfiles(), IsEmpty());

  // On IOS the `AccountInfo` doesn't contain the full name. A dedicated method
  // should create the profile using the explicitly passed account info.
  account_name_email_store().MaybeUpdateOrCreateAccountNameEmail(test_name,
                                                                 test_email);
  EXPECT_THAT(
      address_data_manager().GetProfiles(),
      ElementsAre(IsCorrectAccountNameEmail(base::UTF8ToUTF16(test_name),
                                            base::UTF8ToUTF16(test_email))));
}
#endif  // BUILDFLAG(IS_IOS)

class AccountNameEmailStoreSyncTest : public AccountNameEmailStoreCoreTest {
 public:
  AccountNameEmailStoreSyncTest() {
    features_.InitWithFeatures(
        {features::kAutofillEnableSupportForNameAndEmail},
        {syncer::kReplaceSyncPromosWithSignInPromos});
  }
};

// Tests that kAccountNameEmail profile exists only if sync-the-feature is
// enabled.
// TODO(crbug.com/40066949): Remove once kSync gets removed.
TEST_F(AccountNameEmailStoreSyncTest, SyncTheFeatureState) {
  sync_service().SetSignedIn(signin::ConsentLevel::kSignin);
  sync_service().FireStateChanged();

  CreatePrimaryAccount(kTestName1, kTestEmailAddress1);
  EXPECT_THAT(address_data_manager().GetProfiles(), IsEmpty());

  sync_service().SetSignedIn(signin::ConsentLevel::kSync);
  sync_service().FireStateChanged();
  EXPECT_THAT(address_data_manager().GetProfiles(),
              ElementsAre(IsCorrectAccountNameEmail(
                  base::UTF8ToUTF16(kTestName1),
                  base::UTF8ToUTF16(kTestEmailAddress1))));
}

struct NicknameTestCase {
  std::string account_name_with_nickname;
  std::string expected_autofill_profile_full_name;
};

class AccountNameEmailStoreWithNicknameTest
    : public AccountNameEmailStoreCoreTest,
      public testing::WithParamInterface<NicknameTestCase> {};

// Tests that AutofillProfile is created without the nickname.
TEST_P(AccountNameEmailStoreWithNicknameTest, CreatedProfileMissesNickname) {
  auto test_case = GetParam();
  CreatePrimaryAccount(test_case.account_name_with_nickname,
                       kTestEmailAddress1);
  EXPECT_THAT(
      address_data_manager().GetProfiles(),
      ElementsAre(IsCorrectAccountNameEmail(
          base::UTF8ToUTF16(test_case.expected_autofill_profile_full_name),
          base::UTF8ToUTF16(kTestEmailAddress1))));
}

INSTANTIATE_TEST_SUITE_P(
    AccountNameEmailNicknameTest,
    AccountNameEmailStoreWithNicknameTest,
    testing::Values(
        NicknameTestCase{"John Ben Smith (JJ)", "John Ben Smith"},
        NicknameTestCase{"John Ben Smith (John Smith)", "John Ben Smith"},
        NicknameTestCase{"John Ben \"John Smith\" Smith", "John Ben Smith"},
        NicknameTestCase{"John \"JJ\" Smith", "John Smith"},
        NicknameTestCase{"John Ben (John Smith) Smith", "John Ben Smith"},
        NicknameTestCase{"John (JJ) Smith", "John Smith"},
        NicknameTestCase{"John Smith", "John Smith"}));

}  // namespace

}  // namespace autofill
