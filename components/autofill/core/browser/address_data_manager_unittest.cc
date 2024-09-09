// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_data_manager.h"

#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/with_feature_override.h"
#include "base/uuid.h"
#include "build/buildflag.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_test_api.h"
#include "components/autofill/core/browser/personal_data_manager_test_utils.h"
#include "components/autofill/core/browser/profile_token_quality_test_api.h"
#include "components/autofill/core/browser/strike_databases/test_inmemory_strike_database.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/addresses/address_autofill_table.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync/test/test_sync_service.h"
#include "components/webdata/common/web_database_service.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using testing::ElementsAre;
using testing::Pointee;
using testing::UnorderedElementsAre;

const base::Time kArbitraryTime = base::Time::FromSecondsSinceUnixEpoch(25);
const base::Time kSomeLaterTime = base::Time::FromSecondsSinceUnixEpoch(1000);
const base::Time kMuchLaterTime = base::Time::FromSecondsSinceUnixEpoch(5000);

constexpr char kGuid[] = "a21f010a-eac1-41fc-aee9-c06bbedfb292";

class MockAddressDataManagerObserver : public AddressDataManager::Observer {
 public:
  MOCK_METHOD(void, OnAddressDataChanged, (), (override));
};

class AddressDataManagerTest : public testing::Test {
 protected:
  AddressDataManagerTest()
      : prefs_(test::PrefServiceForTesting()),
        os_crypt_(os_crypt_async::GetTestOSCryptAsyncForTesting(
            /*is_sync_for_unittests=*/true)),
        profile_web_database_(base::MakeRefCounted<WebDatabaseService>(
            base::FilePath(WebDatabase::kInMemoryPath),
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            base::SingleThreadTaskRunner::GetCurrentDefault())) {
    profile_web_database_->AddTable(std::make_unique<AddressAutofillTable>());
    profile_web_database_->LoadDatabase(os_crypt_.get());
    profile_database_service_ = base::MakeRefCounted<AutofillWebDataService>(
        profile_web_database_,
        base::SingleThreadTaskRunner::GetCurrentDefault());
    profile_database_service_->Init(base::NullCallback());
    ResetAddressDataManager();
  }

  void TearDown() override { profile_web_database_->ShutdownDatabase(); }

  AddressDataManager& address_data_manager() { return *address_data_manager_; }

  void WaitForOnAddressDataChanged() {
    testing::NiceMock<MockAddressDataManagerObserver> observer;
    base::RunLoop run_loop;
    ON_CALL(observer, OnAddressDataChanged)
        .WillByDefault(base::test::RunClosure(run_loop.QuitClosure()));
    base::ScopedObservation<AddressDataManager, AddressDataManager::Observer>
        observation{&observer};
    observation.Observe(address_data_manager_.get());
    run_loop.Run();
  }

  void ResetAddressDataManager(bool use_sync_transport_mode = false) {
    address_data_manager_.reset();
    MakePrimaryAccountAvailable(use_sync_transport_mode, identity_test_env_,
                                sync_service_);
    address_data_manager_ = std::make_unique<AddressDataManager>(
        profile_database_service_, prefs_.get(), prefs_.get(), &sync_service_,
        identity_test_env_.identity_manager(), &strike_database_,
        GeoIpCountryCode("US"), "en-US");
    address_data_manager_->LoadProfiles();
    WaitForOnAddressDataChanged();
  }

  void AddProfileToAddressDataManager(const AutofillProfile& profile) {
    // When trying to add a duplicate profile, observers are notified
    // synchronously, which is why calling `WaitForOnAddressDataChanged()`
    // after `AddProfile()` doesn't suffice.
    testing::NiceMock<MockAddressDataManagerObserver> observer;
    base::RunLoop run_loop;
    ON_CALL(observer, OnAddressDataChanged)
        .WillByDefault(base::test::RunClosure(run_loop.QuitClosure()));
    base::ScopedObservation<AddressDataManager, AddressDataManager::Observer>
        observation{&observer};
    observation.Observe(address_data_manager_.get());
    address_data_manager().AddProfile(profile);
    run_loop.Run();
  }

  void UpdateProfileOnAddressDataManager(const AutofillProfile& profile) {
    // Like in `AddProfileToAddressDataManager()`, observers are notified
    // synchronously when trying to perform a no-op update.
    testing::NiceMock<MockAddressDataManagerObserver> observer;
    base::RunLoop run_loop;
    ON_CALL(observer, OnAddressDataChanged)
        .WillByDefault(base::test::RunClosure(run_loop.QuitClosure()));
    base::ScopedObservation<AddressDataManager, AddressDataManager::Observer>
        observation{&observer};
    observation.Observe(address_data_manager_.get());
    address_data_manager().UpdateProfile(profile);
    run_loop.Run();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<PrefService> prefs_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_;
  syncer::TestSyncService sync_service_;
  scoped_refptr<AutofillWebDataService> profile_database_service_;

 private:
  scoped_refptr<WebDatabaseService> profile_web_database_;
  std::unique_ptr<AddressDataManager> address_data_manager_;
  TestInMemoryStrikeDatabase strike_database_;
};

TEST_F(AddressDataManagerTest, AddProfile) {
  // Add profile0 to the database.
  AutofillProfile profile0(test::GetFullProfile());
  profile0.SetRawInfo(EMAIL_ADDRESS, u"j@s.com");
  AddProfileToAddressDataManager(profile0);
  // Verify the addition.
  const std::vector<const AutofillProfile*>& results1 =
      address_data_manager().GetProfiles();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(0, profile0.Compare(*results1[0]));

  // Add profile with identical values.  Duplicates should not get saved.
  AutofillProfile profile0a = profile0;
  profile0a.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());

  AddProfileToAddressDataManager(profile0a);
  // Verify the non-addition.
  const std::vector<const AutofillProfile*>& results2 =
      address_data_manager().GetProfiles();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, profile0.Compare(*results2[0]));

  // New profile with different email.
  AutofillProfile profile1 = profile0;
  profile1.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  profile1.SetRawInfo(EMAIL_ADDRESS, u"john@smith.com");

  // Add the different profile.  This should save as a separate profile.
  // Note that if this same profile was "merged" it would collapse to one
  // profile with a multi-valued entry for email.
  AddProfileToAddressDataManager(profile1);
  // Verify the addition.
  EXPECT_THAT(address_data_manager().GetProfiles(),
              UnorderedElementsAre(Pointee(profile0), Pointee(profile1)));
}

TEST_F(AddressDataManagerTest, UpdateProfile_ModificationDate) {
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);
  AutofillProfile profile = test::GetFullProfile();
  AddProfileToAddressDataManager(profile);
  ASSERT_THAT(address_data_manager().GetProfiles(),
              UnorderedElementsAre(Pointee(profile)));

  // Update the profile arbitrarily. Expect that the modification date changes.
  // Note that `AutofillProfile::operator==()` doesn't check the
  // `modification_date()`.
  test_clock.SetNow(kSomeLaterTime);
  profile.SetRawInfo(EMAIL_ADDRESS, u"new" + profile.GetRawInfo(EMAIL_ADDRESS));
  UpdateProfileOnAddressDataManager(profile);
  std::vector<const AutofillProfile*> profiles =
      address_data_manager().GetProfiles();
  ASSERT_THAT(profiles, UnorderedElementsAre(Pointee(profile)));
  EXPECT_EQ(profiles[0]->modification_date(), kSomeLaterTime);

  // If the profile hasn't change, expect that updating is a no-op.
  test_clock.SetNow(kMuchLaterTime);
  UpdateProfileOnAddressDataManager(profile);
  profiles = address_data_manager().GetProfiles();
  ASSERT_THAT(profiles, UnorderedElementsAre(Pointee(profile)));
  EXPECT_EQ(profiles[0]->modification_date(), kSomeLaterTime);
}

// Tests that profiles with record type`kAccount` and `kLocalOrSyncable` are
// loaded, and accessible via `GetProfiles()` and `GetProfilesByRecordType()`.
// If duplicates exist across record types, they should be considered distinct.
TEST_F(AddressDataManagerTest, GetProfiles) {
  AutofillProfile account_profile1 = test::GetFullProfile();
  test_api(account_profile1)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  AutofillProfile account_profile2 = test::GetFullProfile2();
  test_api(account_profile2)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  AutofillProfile local_profile = test::GetFullProfile();
  AddProfileToAddressDataManager(account_profile1);
  AddProfileToAddressDataManager(account_profile2);
  AddProfileToAddressDataManager(local_profile);

  EXPECT_THAT(
      address_data_manager().GetProfiles(),
      UnorderedElementsAre(Pointee(account_profile1), Pointee(account_profile2),
                           Pointee(local_profile)));
  EXPECT_THAT(address_data_manager().GetProfilesByRecordType(
                  AutofillProfile::RecordType::kAccount),
              UnorderedElementsAre(Pointee(account_profile1),
                                   Pointee(account_profile2)));
  EXPECT_THAT(address_data_manager().GetProfilesByRecordType(
                  AutofillProfile::RecordType::kLocalOrSyncable),
              ElementsAre(Pointee(local_profile)));
}

// Tests the different orderings in which profiles can be retrieved.
TEST_F(AddressDataManagerTest, GetProfiles_Order) {
  base::Time now = AutofillClock::Now();
  AutofillProfile profile1 = test::GetFullProfile();
  profile1.set_use_date(now - base::Hours(2));
  profile1.set_use_count(1);
  profile1.set_modification_date(now);
  AutofillProfile profile2 = test::GetFullProfile2();
  profile2.set_use_date(now);
  profile2.set_use_count(1);
  profile2.set_modification_date(now - base::Hours(1));
  AutofillProfile profile3 = test::GetFullCanadianProfile();
  profile3.set_use_date(now - base::Hours(1));
  profile3.set_use_count(1234);
  profile3.set_modification_date(now - base::Hours(2));

  AddProfileToAddressDataManager(profile1);
  AddProfileToAddressDataManager(profile2);
  AddProfileToAddressDataManager(profile3);

  // kNone doesn't guarantee any order.
  EXPECT_THAT(address_data_manager().GetProfiles(
                  AddressDataManager::ProfileOrder::kNone),
              UnorderedElementsAre(Pointee(profile1), Pointee(profile2),
                                   Pointee(profile3)));

  // `profile3` is first, since it has a much higher use count.
  // `profile1` and `profile2` have the same use count, so `profile2` with later
  // use date is second.
  EXPECT_THAT(address_data_manager().GetProfiles(
                  AddressDataManager::ProfileOrder::kHighestFrecencyDesc),
              testing::ElementsAre(Pointee(profile3), Pointee(profile2),
                                   Pointee(profile1)));

  // Ordered by `use_date()`.
  EXPECT_THAT(address_data_manager().GetProfiles(
                  AddressDataManager::ProfileOrder::kMostRecentlyUsedFirstDesc),
              testing::ElementsAre(Pointee(profile2), Pointee(profile3),
                                   Pointee(profile1)));

  // Ordered by `modification_date()`.
  EXPECT_THAT(address_data_manager().GetProfiles(
                  AddressDataManager::ProfileOrder::kMostRecentlyModifiedDesc),
              testing::ElementsAre(Pointee(profile1), Pointee(profile2),
                                   Pointee(profile3)));
}

// Tests that `GetProfiles()` and `GetProfilesByRecordType()` filters incomplete
// H/W addresses.
TEST_F(AddressDataManagerTest, GetProfiles_CompletenessFiltering) {
  AutofillProfile local_profile = test::GetFullProfile();
  AutofillProfile regular_account_profile = test::GetFullProfile2();
  test_api(regular_account_profile)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  AutofillProfile complete_home_profile = test::GetFullCanadianProfile();
  test_api(complete_home_profile)
      .set_record_type(AutofillProfile::RecordType::kAccountHome);
  // `GetIncompleteProfile1()` is only missing a phone number, but is not
  // lacking any address information. `GetIncompleteProfile2()` is.
  AutofillProfile incomplete_work_profile = test::GetIncompleteProfile2();
  ASSERT_FALSE(incomplete_work_profile.HasInfo(ADDRESS_HOME_STREET_ADDRESS));
  test_api(incomplete_work_profile)
      .set_record_type(AutofillProfile::RecordType::kAccountWork);

  AddProfileToAddressDataManager(local_profile);
  AddProfileToAddressDataManager(regular_account_profile);
  AddProfileToAddressDataManager(complete_home_profile);
  AddProfileToAddressDataManager(incomplete_work_profile);

  EXPECT_THAT(address_data_manager().GetProfiles(),
              testing::UnorderedElementsAre(Pointee(local_profile),
                                            Pointee(regular_account_profile),
                                            Pointee(complete_home_profile)));
  EXPECT_THAT(address_data_manager().GetProfilesByRecordType(
                  AutofillProfile::RecordType::kAccountHome),
              testing::UnorderedElementsAre(Pointee(complete_home_profile)));
  EXPECT_THAT(address_data_manager().GetProfilesByRecordType(
                  AutofillProfile::RecordType::kAccountWork),
              testing::IsEmpty());
}

// Test that profiles are not shown if |kAutofillProfileEnabled| is set to
// |false|.
TEST_F(AddressDataManagerTest, GetProfilesToSuggest_ProfileAutofillDisabled) {
  // Add two different profiles, a local and a server one.
  AutofillProfile local_profile(
      i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&local_profile, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5",
                       "Orlando", "FL", "32801", "US", "19482937549");
  AddProfileToAddressDataManager(local_profile);

  // Disable Profile autofill.
  prefs::SetAutofillProfileEnabled(prefs_.get(), false);
  WaitForOnAddressDataChanged();

  // Check that profiles were saved.
  const size_t expected_profiles = 1;
  EXPECT_EQ(expected_profiles, address_data_manager().GetProfiles().size());
  // Expect no autofilled values or suggestions.
  EXPECT_EQ(0U, address_data_manager().GetProfilesToSuggest().size());
}

// Test that local and server profiles are not loaded into memory on start-up if
// |kAutofillProfileEnabled| is set to |false|.
TEST_F(AddressDataManagerTest,
       GetProfilesToSuggest_NoProfilesLoadedIfDisabled) {
  // Add two different profiles, a local and a server one.
  AutofillProfile local_profile(
      i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&local_profile, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5",
                       "Orlando", "FL", "32801", "US", "19482937549");
  AddProfileToAddressDataManager(local_profile);

  address_data_manager().LoadProfiles();
  WaitForOnAddressDataChanged();

  // Expect that all profiles are suggested.
  const size_t expected_profiles = 1;
  EXPECT_EQ(expected_profiles, address_data_manager().GetProfiles().size());
  EXPECT_EQ(expected_profiles,
            address_data_manager().GetProfilesToSuggest().size());

  // Disable Profile autofill.
  prefs::SetAutofillProfileEnabled(prefs_.get(), false);

  // Expect no profile values or suggestions were loaded.
  EXPECT_EQ(0U, address_data_manager().GetProfilesToSuggest().size());
}

// Test that profiles are not added if `kAutofillProfileEnabled` is set to
// false.
TEST_F(AddressDataManagerTest, GetProfilesToSuggest_NoProfilesAddedIfDisabled) {
  prefs::SetAutofillProfileEnabled(prefs_.get(), false);
  AddProfileToAddressDataManager(test::GetFullProfile());
  EXPECT_TRUE(address_data_manager().GetProfiles().empty());
}

// Tests that `GetProfilesForSettings()` orders by descending modification
// dates.
TEST_F(AddressDataManagerTest, GetProfilesForSettings) {
  AutofillProfile account_profile = test::GetFullProfile();
  test_api(account_profile)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  account_profile.set_modification_date(kArbitraryTime);
  AddProfileToAddressDataManager(account_profile);

  AutofillProfile local_profile = test::GetFullProfile2();
  test_api(local_profile)
      .set_record_type(AutofillProfile::RecordType::kLocalOrSyncable);
  local_profile.set_modification_date(kSomeLaterTime);
  AddProfileToAddressDataManager(local_profile);

  EXPECT_THAT(address_data_manager().GetProfilesForSettings(),
              testing::ElementsAre(testing::Pointee(local_profile),
                                   testing::Pointee(account_profile)));
}

// Adding, updating, removing operations without waiting in between.
TEST_F(AddressDataManagerTest, AddRemoveUpdateProfileSequence) {
  AutofillProfile profile(test::GetFullProfile());

  address_data_manager().AddProfile(profile);
  address_data_manager().RemoveProfile(profile.guid());
  address_data_manager().UpdateProfile(profile);
  WaitForOnAddressDataChanged();

  auto profiles = address_data_manager().GetProfiles();
  ASSERT_EQ(0U, profiles.size());

  address_data_manager().AddProfile(profile);
  address_data_manager().RemoveProfile(profile.guid());
  address_data_manager().RemoveProfile(profile.guid());
  WaitForOnAddressDataChanged();

  profiles = address_data_manager().GetProfiles();
  ASSERT_EQ(0U, profiles.size());

  address_data_manager().AddProfile(profile);
  profile.SetRawInfo(EMAIL_ADDRESS, u"new@email.com");
  address_data_manager().UpdateProfile(profile);
  WaitForOnAddressDataChanged();

  profiles = address_data_manager().GetProfiles();
  ASSERT_EQ(1U, profiles.size());
  EXPECT_EQ(profiles[0]->GetRawInfo(EMAIL_ADDRESS), u"new@email.com");

  profile.SetRawInfo(EMAIL_ADDRESS, u"newer@email.com");
  address_data_manager().UpdateProfile(profile);
  profile.SetRawInfo(EMAIL_ADDRESS, u"newest@email.com");
  address_data_manager().UpdateProfile(profile);
  WaitForOnAddressDataChanged();

  profiles = address_data_manager().GetProfiles();
  ASSERT_EQ(1U, profiles.size());
  EXPECT_EQ(profiles[0]->GetRawInfo(EMAIL_ADDRESS), u"newest@email.com");
}

// Test that a new profile has its basic information set.
TEST_F(AddressDataManagerTest, AddProfile_BasicInformation) {
  // Create the test clock and set the time to a specific value.
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);

  // Add a profile to the database.
  AutofillProfile profile(test::GetFullProfile());
  profile.SetRawInfo(EMAIL_ADDRESS, u"j@s.com");
  AddProfileToAddressDataManager(profile);

  // Verify the addition.
  const std::vector<const AutofillProfile*>& results =
      address_data_manager().GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, profile.Compare(*results[0]));

  // Make sure the use count and use date were set.
  EXPECT_EQ(1U, results[0]->use_count());
  EXPECT_EQ(kArbitraryTime, results[0]->use_date());
  EXPECT_EQ(kArbitraryTime, results[0]->modification_date());
}

// Test filling profiles with unicode strings and crazy characters.
TEST_F(AddressDataManagerTest, AddProfile_CrazyCharacters) {
  std::vector<AutofillProfile> profiles;
  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile1.SetRawInfo(NAME_FIRST,
                      u"\u0623\u0648\u0628\u0627\u0645\u0627 "
                      u"\u064a\u0639\u062a\u0630\u0631 "
                      u"\u0647\u0627\u062a\u0641\u064a\u0627 "
                      u"\u0644\u0645\u0648\u0638\u0641\u0629 "
                      u"\u0633\u0648\u062f\u0627\u0621 "
                      u"\u0627\u0633\u062a\u0642\u0627\u0644\u062a "
                      u"\u0628\u0633\u0628\u0628 "
                      u"\u062a\u0635\u0631\u064a\u062d\u0627\u062a "
                      u"\u0645\u062c\u062a\u0632\u0623\u0629");
  profile1.SetRawInfo(NAME_MIDDLE, u"BANK\xcBERF\xc4LLE");
  profile1.SetRawInfo(EMAIL_ADDRESS,
                      u"\uacbd\uc81c \ub274\uc2a4 "
                      u"\ub354\ubcf4\uae30@google.com");
  profile1.SetRawInfo(ADDRESS_HOME_LINE1,
                      u"\uad6d\uc815\uc6d0\xb7\uac80\ucc30, "
                      u"\ub178\ubb34\ud604\uc815\ubd80 "
                      u"\ub300\ubd81\uc811\ucd09 \ub2f4\ub2f9 "
                      u"\uc778\uc0ac\ub4e4 \uc870\uc0ac");
  profile1.SetRawInfo(ADDRESS_HOME_CITY,
                      u"\u653f\u5e9c\u4e0d\u6392\u9664\u7acb\u6cd5"
                      u"\u898f\u7ba1\u5c0e\u904a");
  profile1.SetRawInfo(ADDRESS_HOME_ZIP, u"YOHO_54676");
  profile1.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"861088828000");
  profile1.SetInfo(ADDRESS_HOME_COUNTRY, u"India", "en-US");
  profile1.FinalizeAfterImport();
  profiles.push_back(profile1);

  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile2.SetRawInfo(NAME_FIRST,
                      u"\u4e0a\u6d77\u5e02\u91d1\u5c71\u533a "
                      u"\u677e\u9690\u9547\u4ead\u67ab\u516c"
                      u"\u8def1915\u53f7");
  profile2.SetRawInfo(NAME_LAST, u"aguantó");
  profile2.SetRawInfo(ADDRESS_HOME_ZIP, u"HOME 94043");
  profile2.FinalizeAfterImport();
  profiles.push_back(profile2);

  AutofillProfile profile3(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile3.SetRawInfo(EMAIL_ADDRESS, u"sue@example.com");
  profile3.SetRawInfo(COMPANY_NAME, u"Company X");
  profile3.FinalizeAfterImport();
  profiles.push_back(profile3);

  AutofillProfile profile4(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile4.SetRawInfo(NAME_FIRST, u"Joe 3254");
  profile4.SetRawInfo(NAME_LAST, u"\u8bb0\u8d262\u5e74\u591a");
  profile4.SetRawInfo(ADDRESS_HOME_ZIP,
                      u"\uff08\u90ae\u7f16\uff1a201504\uff09");
  profile4.SetRawInfo(EMAIL_ADDRESS, u"télévision@example.com");
  profile4.SetRawInfo(COMPANY_NAME,
                      u"\u0907\u0932\u0947\u0915\u093f\u091f\u094d"
                      u"\u0930\u0928\u093f\u0915\u094d\u0938, "
                      u"\u0905\u092a\u094b\u0932\u094b "
                      u"\u091f\u093e\u092f\u0930\u094d\u0938 "
                      u"\u0906\u0926\u093f");
  profile4.FinalizeAfterImport();
  profiles.push_back(profile4);

  AutofillProfile profile5(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile5.SetRawInfo(NAME_FIRST, u"Larry");
  profile5.SetRawInfo(NAME_LAST,
                      u"\u0938\u094d\u091f\u093e\u0902\u092a "
                      u"\u0921\u094d\u092f\u0942\u091f\u0940");
  profile5.SetRawInfo(ADDRESS_HOME_ZIP, u"111111111111110000GOOGLE");
  profile5.SetRawInfo(EMAIL_ADDRESS, u"page@000000.com");
  profile5.SetRawInfo(COMPANY_NAME, u"Google");
  profile5.FinalizeAfterImport();
  profiles.push_back(profile5);

  AutofillProfile profile6(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile6.SetRawInfo(NAME_FIRST,
                      u"\u4e0a\u6d77\u5e02\u91d1\u5c71\u533a "
                      u"\u677e\u9690\u9547\u4ead\u67ab\u516c"
                      u"\u8def1915\u53f7");
  profile6.SetRawInfo(NAME_LAST,
                      u"\u0646\u062c\u0627\u0645\u064a\u0646\u0627 "
                      u"\u062f\u0639\u0645\u0647\u0627 "
                      u"\u0644\u0644\u0631\u0626\u064a\u0633 "
                      u"\u0627\u0644\u0633\u0648\u062f\u0627\u0646"
                      u"\u064a \u0639\u0645\u0631 "
                      u"\u0627\u0644\u0628\u0634\u064a\u0631");
  profile6.SetRawInfo(ADDRESS_HOME_ZIP, u"HOME 94043");
  profile6.FinalizeAfterImport();
  profiles.push_back(profile6);

  AutofillProfile profile7(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile7.SetRawInfo(NAME_FIRST, u"&$%$$$ TESTO *&*&^&^& MOKO");
  profile7.SetRawInfo(NAME_MIDDLE, u"WOHOOOO$$$$$$$$****");
  profile7.SetRawInfo(EMAIL_ADDRESS, u"yuvu@example.com");
  profile7.SetRawInfo(ADDRESS_HOME_LINE1, u"34544, anderson ST.(120230)");
  profile7.SetRawInfo(ADDRESS_HOME_CITY, u"Sunnyvale");
  profile7.SetRawInfo(ADDRESS_HOME_STATE, u"CA");
  profile7.SetRawInfo(ADDRESS_HOME_ZIP, u"94086");
  profile7.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"15466784565");
  profile7.SetInfo(ADDRESS_HOME_COUNTRY, u"United States", "en-US");
  profile7.FinalizeAfterImport();
  profiles.push_back(profile7);

  for (const AutofillProfile& profile : profiles) {
    AddProfileToAddressDataManager(profile);
  }
  ASSERT_EQ(profiles.size(), address_data_manager().GetProfiles().size());
  for (size_t i = 0; i < profiles.size(); ++i) {
    EXPECT_TRUE(
        base::Contains(profiles, *address_data_manager().GetProfiles()[i]));
  }
}

// Test filling in invalid values for profiles are saved as-is. Phone
// information entered into the settings UI is not validated or rejected except
// for duplicates.
TEST_F(AddressDataManagerTest, AddProfile_Invalid) {
  // First try profiles with invalid ZIP input.
  AutofillProfile without_invalid(
      i18n_model_definition::kLegacyHierarchyCountryCode);
  without_invalid.SetRawInfo(NAME_FIRST, u"Will");
  without_invalid.SetRawInfo(ADDRESS_HOME_CITY, u"Sunnyvale");
  without_invalid.SetRawInfo(ADDRESS_HOME_STATE, u"CA");
  without_invalid.SetRawInfo(ADDRESS_HOME_ZIP, u"my_zip");
  without_invalid.SetInfo(ADDRESS_HOME_COUNTRY, u"United States", "en-US");

  AutofillProfile with_invalid = without_invalid;
  with_invalid.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"Invalid_Phone_Number");

  AddProfileToAddressDataManager(with_invalid);
  ASSERT_EQ(1u, address_data_manager().GetProfiles().size());
  AutofillProfile profile = *address_data_manager().GetProfiles()[0];
  ASSERT_NE(without_invalid.GetRawInfo(PHONE_HOME_WHOLE_NUMBER),
            profile.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
}

TEST_F(AddressDataManagerTest, AddUpdateRemoveProfiles) {
  AutofillProfile profile0(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile0, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");

  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile1, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "903 Apple Ct.", nullptr,
                       "Orlando", "FL", "32801", "US", "19482937549");

  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile2, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5",
                       "Orlando", "FL", "32801", "US", "19482937549");

  // Add two test profiles to the database.
  AddProfileToAddressDataManager(profile0);
  AddProfileToAddressDataManager(profile1);

  EXPECT_THAT(address_data_manager().GetProfiles(),
              UnorderedElementsAre(Pointee(profile0), Pointee(profile1)));

  // Update, remove, and add.
  profile0.SetRawInfo(NAME_FIRST, u"John");
  UpdateProfileOnAddressDataManager(profile0);
  address_data_manager().RemoveProfile(profile1.guid());
  AddProfileToAddressDataManager(profile2);

  EXPECT_THAT(address_data_manager().GetProfiles(),
              UnorderedElementsAre(Pointee(profile0), Pointee(profile2)));

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the profiles from the web
  // database.
  ResetAddressDataManager();

  // Verify that we've loaded the profiles from the web database.
  EXPECT_THAT(address_data_manager().GetProfiles(),
              UnorderedElementsAre(Pointee(profile0), Pointee(profile2)));
}

TEST_F(AddressDataManagerTest, RemoveLocalProfilesModifiedBetween) {
  const base::Time now = base::Time::Now();
  AutofillProfile local_profile1 = test::GetFullProfile();
  local_profile1.set_modification_date(now - base::Minutes(5));
  AutofillProfile local_profile2 = test::GetFullProfile2();
  local_profile2.set_modification_date(now + base::Minutes(1));
  AutofillProfile account_profile = test::GetFullCanadianProfile();
  test_api(account_profile)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  account_profile.set_modification_date(now + base::Minutes(3));

  AddProfileToAddressDataManager(local_profile1);
  AddProfileToAddressDataManager(local_profile2);
  AddProfileToAddressDataManager(account_profile);

  address_data_manager().RemoveLocalProfilesModifiedBetween(
      now, now + base::Minutes(10));
  WaitForOnAddressDataChanged();
  EXPECT_THAT(
      address_data_manager().GetProfiles(),
      UnorderedElementsAre(Pointee(local_profile1), Pointee(account_profile)));
}

// Tests that `UpdateProfile()` takes changes in the `ProfileTokenQuality`
// observations into considerations.
TEST_F(AddressDataManagerTest, UpdateProfile_NewObservations) {
  // Add a profile without observations at `kArbitraryTime`.
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);
  AutofillProfile profile = test::GetFullProfile();
  AddProfileToAddressDataManager(profile);
  test_clock.SetNow(kSomeLaterTime);

  // Add an observation, as might happen during a form submit.
  test_api(profile.token_quality())
      .AddObservation(NAME_FIRST,
                      ProfileTokenQuality::ObservationType::kAccepted);
  UpdateProfileOnAddressDataManager(profile);

  // Expect that `UpdateProfile()` didn't reject the update as a no-op.
  // Since new observations are considered a metadata change, further expected
  // that the modification date hasn't changed.
  const AutofillProfile* pdm_profile =
      address_data_manager().GetProfileByGUID(profile.guid());
  EXPECT_THAT(
      pdm_profile->token_quality().GetObservationTypesForFieldType(NAME_FIRST),
      UnorderedElementsAre(ProfileTokenQuality::ObservationType::kAccepted));
  EXPECT_EQ(profile.modification_date(), kArbitraryTime);
}

// Tests that when the value for a type changes, `UpdateProfile()` resets the
// observations for that type.
TEST_F(AddressDataManagerTest, UpdateProfile_ResetObservations) {
  // Add a profile with observations for NAME_FIRST and NAME_LAST.
  AutofillProfile profile = test::GetFullProfile();
  test_api(profile.token_quality())
      .AddObservation(NAME_FIRST,
                      ProfileTokenQuality::ObservationType::kAccepted);
  test_api(profile.token_quality())
      .AddObservation(NAME_LAST,
                      ProfileTokenQuality::ObservationType::kEditedFallback);
  AddProfileToAddressDataManager(profile);

  // Modify the NAME_FIRST and update the profile in the PDM.
  profile.SetRawInfo(NAME_FIRST, u"new " + profile.GetRawInfo(NAME_FIRST));
  UpdateProfileOnAddressDataManager(profile);

  // Expect that only the observations for NAME_LAST remain.
  profile = *address_data_manager().GetProfileByGUID(profile.guid());
  EXPECT_TRUE(profile.token_quality()
                  .GetObservationTypesForFieldType(NAME_FIRST)
                  .empty());
  EXPECT_THAT(
      profile.token_quality().GetObservationTypesForFieldType(NAME_LAST),
      UnorderedElementsAre(
          ProfileTokenQuality::ObservationType::kEditedFallback));
}

TEST_F(AddressDataManagerTest, IsEligibleForAddressAccountStorage) {
  // All data types are running by default.
  EXPECT_TRUE(address_data_manager().IsEligibleForAddressAccountStorage());

  // No Sync, no account storage.
  address_data_manager().SetSyncServiceForTest(nullptr);
  EXPECT_FALSE(address_data_manager().IsEligibleForAddressAccountStorage());
}

TEST_F(AddressDataManagerTest, IsCountryEligibleForAccountStorage) {
  EXPECT_TRUE(address_data_manager().IsCountryEligibleForAccountStorage("AT"));
  EXPECT_FALSE(address_data_manager().IsCountryEligibleForAccountStorage("IR"));
}

TEST_F(AddressDataManagerTest, MigrateProfileToAccount) {
  const AutofillProfile kLocalProfile = test::GetFullProfile();
  ASSERT_EQ(kLocalProfile.record_type(),
            AutofillProfile::RecordType::kLocalOrSyncable);
  AddProfileToAddressDataManager(kLocalProfile);

  address_data_manager().MigrateProfileToAccount(kLocalProfile);
  WaitForOnAddressDataChanged();
  const std::vector<const AutofillProfile*> profiles =
      address_data_manager().GetProfiles();

  // `kLocalProfile` should be gone and only the migrated account profile should
  // exist.
  ASSERT_EQ(profiles.size(), 1u);
  const AutofillProfile kAccountProfile = *profiles[0];
  EXPECT_EQ(kAccountProfile.record_type(),
            AutofillProfile::RecordType::kAccount);
  EXPECT_EQ(kAccountProfile.initial_creator_id(),
            AutofillProfile::kInitialCreatorOrModifierChrome);
  EXPECT_EQ(kAccountProfile.last_modifier_id(),
            AutofillProfile::kInitialCreatorOrModifierChrome);
  EXPECT_NE(kLocalProfile.guid(), kAccountProfile.guid());
  EXPECT_EQ(kLocalProfile.Compare(kAccountProfile), 0);
}

// Test for http://crbug.com/50047. Makes sure that guids are populated
// correctly on load.
TEST_F(AddressDataManagerTest, PopulateUniqueIDsOnLoad) {
  AutofillProfile profile0(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile0, "y", "", "", "", "", "", "", "", "", "", "",
                       "");

  // Add the profile0 to the db.
  AddProfileToAddressDataManager(profile0);

  // Verify that we've loaded the profiles from the web database.
  const std::vector<const AutofillProfile*>& results2 =
      address_data_manager().GetProfiles();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, profile0.Compare(*results2[0]));

  // Add a new profile.
  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile1, "z", "", "", "", "", "", "", "", "", "", "",
                       "");
  AddProfileToAddressDataManager(profile1);

  // Make sure the two profiles have different GUIDs, both valid.
  const std::vector<const AutofillProfile*>& results3 =
      address_data_manager().GetProfiles();
  ASSERT_EQ(2U, results3.size());
  EXPECT_NE(results3[0]->guid(), results3[1]->guid());
  EXPECT_TRUE(base::Uuid::ParseCaseInsensitive(results3[0]->guid()).is_valid());
  EXPECT_TRUE(base::Uuid::ParseCaseInsensitive(results3[1]->guid()).is_valid());
}

TEST_F(AddressDataManagerTest, SetEmptyProfile) {
  AutofillProfile profile0(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile0, "", "", "", "", "", "", "", "", "", "", "",
                       "");

  // Add the empty profile to the database.
  AddProfileToAddressDataManager(profile0);

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the profiles from the web
  // database.
  ResetAddressDataManager();

  // Verify that we've loaded the profiles from the web database.
  ASSERT_EQ(0U, address_data_manager().GetProfiles().size());
}

TEST_F(AddressDataManagerTest, Refresh) {
  AutofillProfile profile0(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile0, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");

  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile1, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "903 Apple Ct.", nullptr,
                       "Orlando", "FL", "32801", "US", "19482937549");

  // Add the test profiles to the database.
  AddProfileToAddressDataManager(profile0);
  AddProfileToAddressDataManager(profile1);

  EXPECT_THAT(address_data_manager().GetProfiles(),
              UnorderedElementsAre(Pointee(profile0), Pointee(profile1)));

  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile2, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5",
                       "Orlando", "FL", "32801", "US", "19482937549");

  profile_database_service_->AddAutofillProfile(profile2);

  address_data_manager().LoadProfiles();
  WaitForOnAddressDataChanged();

  EXPECT_THAT(address_data_manager().GetProfiles(),
              UnorderedElementsAre(Pointee(profile0), Pointee(profile1),
                                   Pointee(profile2)));

  profile_database_service_->RemoveAutofillProfile(profile1.guid());
  profile_database_service_->RemoveAutofillProfile(profile2.guid());

  address_data_manager().LoadProfiles();
  WaitForOnAddressDataChanged();

  auto results = address_data_manager().GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(profile0, *results[0]);

  profile0.SetRawInfo(NAME_FIRST, u"Mar");
  profile_database_service_->UpdateAutofillProfile(profile0);

  address_data_manager().LoadProfiles();
  WaitForOnAddressDataChanged();

  results = address_data_manager().GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(profile0, *results[0]);
}

TEST_F(AddressDataManagerTest, UpdateLanguageCodeInProfile) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  AddProfileToAddressDataManager(profile);

  // Make sure everything is set up correctly.
  EXPECT_EQ(1U, address_data_manager().GetProfiles().size());
  EXPECT_EQ(1U, address_data_manager().GetProfiles().size());

  profile.set_language_code("en");
  UpdateProfileOnAddressDataManager(profile);

  const std::vector<const AutofillProfile*>& results =
      address_data_manager().GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, profile.Compare(*results[0]));
  EXPECT_EQ("en", results[0]->language_code());
}

// Tests that the least recently used profile of two existing profiles is
// deleted, when an update of one of the profiles makes it a duplicate of the
// other, already existing profile. Here, the less recently used profile is
// edited to become a duplicate of the more recently used profile.
TEST_F(AddressDataManagerTest, CreateDuplicateWithAnUpdate) {
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);

  AutofillProfile more_recently_used_profile(test::GetFullProfile());
  AutofillProfile less_recently_used_profile(test::GetFullProfile2());

  base::Time older_use_date = AutofillClock::Now();
  less_recently_used_profile.set_use_date(older_use_date);
  test_clock.Advance(base::Days(1));

  // Set more recently used profile to have a use date that is newer than
  // `older_use_date`.
  base::Time newer_use_data = AutofillClock::Now();
  more_recently_used_profile.set_use_date(newer_use_data);

  AddProfileToAddressDataManager(more_recently_used_profile);
  AddProfileToAddressDataManager(less_recently_used_profile);

  EXPECT_EQ(address_data_manager().GetProfiles().size(), 2U);

  // Now make an update to less recently used profile that makes it a duplicate
  // of the more recently used profile.
  AutofillProfile updated_less_recently_used_profile =
      more_recently_used_profile;
  updated_less_recently_used_profile.set_guid(
      less_recently_used_profile.guid());
  // Set the updated profile to have a older use date than it's duplicate.
  updated_less_recently_used_profile.set_use_date(older_use_date);
  UpdateProfileOnAddressDataManager(updated_less_recently_used_profile);

  // Verify that the less recently used profile was removed.
  ASSERT_EQ(address_data_manager().GetProfiles().size(), 1U);
  EXPECT_EQ(*address_data_manager().GetProfiles()[0],
            more_recently_used_profile);
  EXPECT_EQ(address_data_manager().GetProfiles()[0]->use_date(),
            newer_use_data);
}

// Tests that the least recently used profile of two existing profiles is
// deleted, when an update of one of the profiles makes it a duplicate of the
// other, already existing profile. Here, the more recently used profile is
// edited to become a duplicate of the less recently used profile.
TEST_F(AddressDataManagerTest,
       CreateDuplicateWithAnUpdate_UpdatedProfileWasMoreRecentlyUsed) {
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);

  AutofillProfile less_recently_used_profile(test::GetFullProfile());
  AutofillProfile more_recently_used_profile(test::GetFullProfile2());

  less_recently_used_profile.set_use_date(AutofillClock::Now());
  more_recently_used_profile.set_use_date(AutofillClock::Now());

  AddProfileToAddressDataManager(less_recently_used_profile);
  AddProfileToAddressDataManager(more_recently_used_profile);

  EXPECT_EQ(address_data_manager().GetProfiles().size(), 2U);

  // Now make an update to profile2 that makes it a duplicate of profile1,
  // but set the last use time to be more recent than the one of profile1.
  AutofillProfile updated_more_recently_used_profile =
      less_recently_used_profile;
  updated_more_recently_used_profile.set_guid(
      more_recently_used_profile.guid());
  // Set the updated profile to have a newer use date than it's duplicate.
  test_clock.Advance(base::Days(1));
  base::Time newer_use_data = AutofillClock::Now();
  updated_more_recently_used_profile.set_use_date(newer_use_data);
  // Expect an update and a deletion. This only triggers a single notification
  // once both operations have finished.
  address_data_manager().UpdateProfile(updated_more_recently_used_profile);
  WaitForOnAddressDataChanged();

  // Verify that less recently used profile was removed.
  ASSERT_EQ(address_data_manager().GetProfiles().size(), 1U);

  EXPECT_EQ(*address_data_manager().GetProfiles()[0],
            updated_more_recently_used_profile);
  EXPECT_EQ(address_data_manager().GetProfiles()[0]->use_date(),
            newer_use_data);
}

TEST_F(AddressDataManagerTest, RecordUseOf) {
  base::test::ScopedFeatureList feature{
      features::kAutofillTrackMultipleUseDates};
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);
  AutofillProfile profile = test::GetFullProfile();
  ASSERT_EQ(profile.use_count(), 1u);
  ASSERT_EQ(profile.use_date(), kArbitraryTime);
  ASSERT_EQ(profile.modification_date(), kArbitraryTime);
  AddProfileToAddressDataManager(profile);

  test_clock.SetNow(kSomeLaterTime);
  base::HistogramTester histogram_tester;
  address_data_manager().RecordUseOf(profile);
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfLastUsedDatesAfterFilling", 2, 1);
  WaitForOnAddressDataChanged();

  const AutofillProfile* adm_profile =
      address_data_manager().GetProfileByGUID(profile.guid());
  ASSERT_TRUE(adm_profile);
  EXPECT_EQ(adm_profile->use_count(), 2u);
  EXPECT_EQ(adm_profile->use_date(), kSomeLaterTime);
  EXPECT_EQ(adm_profile->modification_date(), kArbitraryTime);
}

TEST_F(AddressDataManagerTest, SaveProfileMigrationStrikes) {
  EXPECT_FALSE(address_data_manager().IsProfileMigrationBlocked(kGuid));

  address_data_manager().AddStrikeToBlockProfileMigration(kGuid);
  EXPECT_FALSE(address_data_manager().IsProfileMigrationBlocked(kGuid));

  address_data_manager().AddStrikeToBlockProfileMigration(kGuid);
  EXPECT_FALSE(address_data_manager().IsProfileMigrationBlocked(kGuid));

  // After the third strike, the guid should be blocked.
  address_data_manager().AddStrikeToBlockProfileMigration(kGuid);
  EXPECT_TRUE(address_data_manager().IsProfileMigrationBlocked(kGuid));

  // Until the strikes are removed again.
  address_data_manager().RemoveStrikesToBlockProfileMigration(kGuid);
  EXPECT_FALSE(address_data_manager().IsProfileMigrationBlocked(kGuid));

  // `AddMaxStrikesToBlockProfileMigration()` should add sufficiently many
  // strikes.
  address_data_manager().AddMaxStrikesToBlockProfileMigration(kGuid);
  EXPECT_TRUE(address_data_manager().IsProfileMigrationBlocked(kGuid));
}

TEST_F(AddressDataManagerTest, SaveProfileUpdateStrikes) {
  EXPECT_FALSE(address_data_manager().IsProfileUpdateBlocked(kGuid));

  address_data_manager().AddStrikeToBlockProfileUpdate(kGuid);
  EXPECT_FALSE(address_data_manager().IsProfileUpdateBlocked(kGuid));

  address_data_manager().AddStrikeToBlockProfileUpdate(kGuid);
  EXPECT_FALSE(address_data_manager().IsProfileUpdateBlocked(kGuid));

  // After the third strike, the guid should be blocked.
  address_data_manager().AddStrikeToBlockProfileUpdate(kGuid);
  EXPECT_TRUE(address_data_manager().IsProfileUpdateBlocked(kGuid));

  // Until the strikes are removed again.
  address_data_manager().RemoveStrikesToBlockProfileUpdate(kGuid);
  EXPECT_FALSE(address_data_manager().IsProfileUpdateBlocked(kGuid));
}

TEST_F(AddressDataManagerTest, SaveProfileSaveStrikes) {
  GURL domain("https://www.block.me/index.html");

  EXPECT_FALSE(
      address_data_manager().IsNewProfileImportBlockedForDomain(domain));

  address_data_manager().AddStrikeToBlockNewProfileImportForDomain(domain);
  EXPECT_FALSE(
      address_data_manager().IsNewProfileImportBlockedForDomain(domain));

  address_data_manager().AddStrikeToBlockNewProfileImportForDomain(domain);
  EXPECT_FALSE(
      address_data_manager().IsNewProfileImportBlockedForDomain(domain));

  // After the third strike, the domain should be blocked.
  address_data_manager().AddStrikeToBlockNewProfileImportForDomain(domain);
  EXPECT_TRUE(
      address_data_manager().IsNewProfileImportBlockedForDomain(domain));

  // Until the strikes are removed again.
  address_data_manager().RemoveStrikesToBlockNewProfileImportForDomain(domain);
  EXPECT_FALSE(
      address_data_manager().IsNewProfileImportBlockedForDomain(domain));
}

TEST_F(AddressDataManagerTest, ClearFullBrowsingHistory) {
  GURL domain("https://www.block.me/index.html");
  address_data_manager().AddStrikeToBlockNewProfileImportForDomain(domain);
  address_data_manager().AddStrikeToBlockNewProfileImportForDomain(domain);
  address_data_manager().AddStrikeToBlockNewProfileImportForDomain(domain);
  EXPECT_TRUE(
      address_data_manager().IsNewProfileImportBlockedForDomain(domain));

  history::DeletionInfo deletion_info = history::DeletionInfo::ForAllHistory();
  address_data_manager().OnHistoryDeletions(deletion_info);

  EXPECT_FALSE(
      address_data_manager().IsNewProfileImportBlockedForDomain(domain));
}

TEST_F(AddressDataManagerTest, ClearUrlsFromBrowsingHistory) {
  GURL first_url("https://www.block.me/index.html");
  GURL second_url("https://www.block.too/index.html");

  // Add strikes to block both domains.
  address_data_manager().AddStrikeToBlockNewProfileImportForDomain(first_url);
  address_data_manager().AddStrikeToBlockNewProfileImportForDomain(first_url);
  address_data_manager().AddStrikeToBlockNewProfileImportForDomain(first_url);
  EXPECT_TRUE(
      address_data_manager().IsNewProfileImportBlockedForDomain(first_url));

  address_data_manager().AddStrikeToBlockNewProfileImportForDomain(second_url);
  address_data_manager().AddStrikeToBlockNewProfileImportForDomain(second_url);
  address_data_manager().AddStrikeToBlockNewProfileImportForDomain(second_url);
  EXPECT_TRUE(
      address_data_manager().IsNewProfileImportBlockedForDomain(second_url));

  history::URLRows deleted_urls = {history::URLRow(first_url)};
  history::DeletionInfo deletion_info =
      history::DeletionInfo::ForUrls(deleted_urls, {});
  address_data_manager().OnHistoryDeletions(deletion_info);

  // The strikes for `domain` should be deleted, but the strikes for
  // `another_domain` should not.
  EXPECT_FALSE(
      address_data_manager().IsNewProfileImportBlockedForDomain(first_url));
  EXPECT_TRUE(
      address_data_manager().IsNewProfileImportBlockedForDomain(second_url));
}

TEST_F(AddressDataManagerTest, ClearUrlsFromBrowsingHistoryInTimeRange) {
  GURL first_url("https://www.block.me/index.html");
  GURL second_url("https://www.block.too/index.html");

  TestAutofillClock test_clock;

  // Add strikes to block both domains.
  address_data_manager().AddStrikeToBlockNewProfileImportForDomain(first_url);
  address_data_manager().AddStrikeToBlockNewProfileImportForDomain(first_url);
  address_data_manager().AddStrikeToBlockNewProfileImportForDomain(first_url);
  address_data_manager().AddStrikeToBlockNewProfileImportForDomain(second_url);
  address_data_manager().AddStrikeToBlockNewProfileImportForDomain(second_url);
  EXPECT_TRUE(
      address_data_manager().IsNewProfileImportBlockedForDomain(first_url));

  test_clock.Advance(base::Hours(1));
  base::Time end_of_deletion = AutofillClock::Now();
  test_clock.Advance(base::Hours(1));

  address_data_manager().AddStrikeToBlockNewProfileImportForDomain(second_url);
  EXPECT_TRUE(
      address_data_manager().IsNewProfileImportBlockedForDomain(second_url));

  history::URLRows deleted_urls = {history::URLRow(first_url),
                                   history::URLRow(second_url)};
  history::DeletionInfo deletion_info(
      history::DeletionTimeRange(base::Time::Min(), end_of_deletion), false,
      deleted_urls, {},
      std::make_optional<std::set<GURL>>({first_url, second_url}));
  address_data_manager().OnHistoryDeletions(deletion_info);

  // The strikes for `first_url` should be deleted because the strikes have been
  // added within the deletion time range.
  EXPECT_FALSE(
      address_data_manager().IsNewProfileImportBlockedForDomain(first_url));
  // The last strike for 'second_url' was collected after the deletion time
  // range and therefore, the blocking should prevail.
  EXPECT_TRUE(
      address_data_manager().IsNewProfileImportBlockedForDomain(second_url));
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

TEST_F(AddressDataManagerTest,
       IsEligibleForAddressAccountStorageSigninPending) {
  // Setup account in auth error.
  CoreAccountInfo account_info =
      identity_test_env_.identity_manager()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);
  identity_test_env_.SimulateSuccessfulFetchOfAccountInfo(
      account_info.account_id, account_info.email, account_info.gaia,
      /*hosted_domain=*/"", "Full Name", "Given Name", /*locale=*/"",
      /*picture_url=*/"");
  identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      account_info.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  sync_service_.SetPersistentAuthError();

  // User is still signed in.
  ASSERT_TRUE(identity_test_env_.identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  // Addresses are selected data type, but not active.
  ASSERT_FALSE(sync_service_.GetActiveDataTypes().Has(syncer::CONTACT_INFO));
  ASSERT_TRUE(sync_service_.GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kAutofill));

  // Account storage is not eligible.
  EXPECT_FALSE(address_data_manager().IsEligibleForAddressAccountStorage());
}

TEST_F(AddressDataManagerTest, AutofillSyncToggleAvailableInTransportMode) {
  ResetAddressDataManager(
      /*use_sync_transport_mode=*/true);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{syncer::
                                kSyncEnableContactInfoDataTypeInTransportMode,
                            ::switches::kExplicitBrowserSigninUIOnDesktop},
      /*disabled_features=*/{});
  const CoreAccountInfo& account = sync_service_.GetAccountInfo();
  identity_test_env_.SimulateSuccessfulFetchOfAccountInfo(
      account.account_id, account.email, account.gaia,
      /*hosted_domain=*/"", "Full Name", "Given Name", "en-US",
      /*picture_url=*/"");

  prefs_->SetBoolean(::prefs::kExplicitBrowserSignin, true);
  EXPECT_TRUE(address_data_manager().IsAutofillSyncToggleAvailable());

  prefs_->SetBoolean(::prefs::kExplicitBrowserSignin, false);
  EXPECT_FALSE(address_data_manager().IsAutofillSyncToggleAvailable());
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

TEST_F(AddressDataManagerTest, ChangeCallbackIsTriggeredOnAddedProfile) {
  ::testing::StrictMock<base::MockOnceClosure> callback;
  EXPECT_CALL(callback, Run);
  address_data_manager().AddChangeCallback(callback.Get());
  AddProfileToAddressDataManager(test::GetFullProfile());
}

}  // namespace

}  // namespace autofill
