// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_data_manager.h"

#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_test_base.h"
#include "components/autofill/core/browser/profile_token_quality_test_api.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
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

class AddressDataManagerTest : public PersonalDataManagerTestBase,
                               public testing::Test {
 protected:
  void SetUp() override {
    SetUpTest();
    ResetPersonalDataManager();
  }
  void TearDown() override { TearDownTest(); }

  ~AddressDataManagerTest() override {
    if (personal_data_) {
      personal_data_->Shutdown();
    }
    personal_data_.reset();
  }

  void ResetPersonalDataManager() {
    if (personal_data_) {
      personal_data_->Shutdown();
    }
    personal_data_ = std::make_unique<PersonalDataManager>("EN", "US");
    PersonalDataManagerTestBase::ResetPersonalDataManager(
        /*use_sync_transport_mode=*/false, personal_data_.get());
  }

  void AddProfileToPersonalDataManager(const AutofillProfile& profile) {
    PersonalDataChangedWaiter waiter(*personal_data_);
    personal_data_->AddProfile(profile);
    std::move(waiter).Wait();
  }

  void UpdateProfileOnPersonalDataManager(const AutofillProfile& profile) {
    PersonalDataChangedWaiter waiter(*personal_data_);
    personal_data_->UpdateProfile(profile);
    std::move(waiter).Wait();
  }

  void RemoveByGUIDFromPersonalDataManager(const std::string& guid) {
    PersonalDataChangedWaiter waiter(*personal_data_);
    personal_data_->RemoveByGUID(guid);
    std::move(waiter).Wait();
  }

  // TODO(b/322170538): Make this an `AddressDataManager`.
  std::unique_ptr<PersonalDataManager> personal_data_;
};

TEST_F(AddressDataManagerTest, AddProfile) {
  // Add profile0 to the database.
  AutofillProfile profile0(test::GetFullProfile());
  profile0.SetRawInfo(EMAIL_ADDRESS, u"j@s.com");
  AddProfileToPersonalDataManager(profile0);
  // Reload the database.
  ResetPersonalDataManager();
  // Verify the addition.
  const std::vector<AutofillProfile*>& results1 = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(0, profile0.Compare(*results1[0]));

  // Add profile with identical values.  Duplicates should not get saved.
  AutofillProfile profile0a = profile0;
  profile0a.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());

  AddProfileToPersonalDataManager(profile0a);

  // Reload the database.
  ResetPersonalDataManager();

  // Verify the non-addition.
  const std::vector<AutofillProfile*>& results2 = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, profile0.Compare(*results2[0]));

  // New profile with different email.
  AutofillProfile profile1 = profile0;
  profile1.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  profile1.SetRawInfo(EMAIL_ADDRESS, u"john@smith.com");

  // Add the different profile.  This should save as a separate profile.
  // Note that if this same profile was "merged" it would collapse to one
  // profile with a multi-valued entry for email.
  AddProfileToPersonalDataManager(profile1);

  // Reload the database.
  ResetPersonalDataManager();

  // Verify the addition.
  EXPECT_THAT(personal_data_->GetProfiles(),
              UnorderedElementsAre(Pointee(profile0), Pointee(profile1)));
}

TEST_F(AddressDataManagerTest, UpdateProfile_ModificationDate) {
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);
  AutofillProfile profile = test::GetFullProfile();
  AddProfileToPersonalDataManager(profile);
  ASSERT_THAT(personal_data_->GetProfiles(),
              UnorderedElementsAre(Pointee(profile)));

  // Update the profile arbitrarily. Expect that the modification date changes.
  // Note that `AutofillProfile::operator==()` doesn't check the
  // `modification_date()`.
  test_clock.SetNow(kSomeLaterTime);
  profile.SetRawInfo(EMAIL_ADDRESS, u"new" + profile.GetRawInfo(EMAIL_ADDRESS));
  UpdateProfileOnPersonalDataManager(profile);
  std::vector<AutofillProfile*> profiles = personal_data_->GetProfiles();
  ASSERT_THAT(profiles, UnorderedElementsAre(Pointee(profile)));
  EXPECT_EQ(profiles[0]->modification_date(), kSomeLaterTime);

  // If the profile hasn't change, expect that updating is a no-op.
  test_clock.SetNow(kMuchLaterTime);
  UpdateProfileOnPersonalDataManager(profile);
  profiles = personal_data_->GetProfiles();
  ASSERT_THAT(profiles, UnorderedElementsAre(Pointee(profile)));
  EXPECT_EQ(profiles[0]->modification_date(), kSomeLaterTime);
}

// Tests that profiles with source `kAccount` and `kLocalOrSyncable` are loaded,
// and accessible via `GetProfiles()` and `GetProfilesFromSource()`.
// If duplicates exist across sources, they should be considered distinct.
TEST_F(AddressDataManagerTest, GetProfiles) {
  AutofillProfile kAccountProfile = test::GetFullProfile();
  kAccountProfile.set_source_for_testing(AutofillProfile::Source::kAccount);
  AutofillProfile kAccountProfile2 = test::GetFullProfile2();
  kAccountProfile2.set_source_for_testing(AutofillProfile::Source::kAccount);
  AutofillProfile kLocalProfile = test::GetFullProfile();

  AddProfileToPersonalDataManager(kAccountProfile);
  AddProfileToPersonalDataManager(kAccountProfile2);
  AddProfileToPersonalDataManager(kLocalProfile);
  ResetPersonalDataManager();

  EXPECT_THAT(
      personal_data_->GetProfiles(),
      UnorderedElementsAre(Pointee(kAccountProfile), Pointee(kAccountProfile2),
                           Pointee(kLocalProfile)));
  EXPECT_THAT(
      personal_data_->GetProfilesFromSource(AutofillProfile::Source::kAccount),
      UnorderedElementsAre(Pointee(kAccountProfile),
                           Pointee(kAccountProfile2)));
  EXPECT_THAT(personal_data_->GetProfilesFromSource(
                  AutofillProfile::Source::kLocalOrSyncable),
              ElementsAre(Pointee(kLocalProfile)));
}

// Tests the different orderings in which profiles can be retrieved.
TEST_F(AddressDataManagerTest, GetProfiles_Order) {
  base::Time now = AutofillClock::Now();
  AutofillProfile profile1 = test::GetFullProfile();
  profile1.set_use_date(now - base::Hours(2));
  profile1.set_use_count(1);
  AutofillProfile profile2 = test::GetFullProfile2();
  profile2.set_use_date(now);
  profile2.set_use_count(1);
  AutofillProfile profile3 = test::GetFullCanadianProfile();
  profile3.set_use_date(now - base::Hours(1));
  profile3.set_use_count(1234);

  AddProfileToPersonalDataManager(profile1);
  AddProfileToPersonalDataManager(profile2);
  AddProfileToPersonalDataManager(profile3);
  ResetPersonalDataManager();

  // kNone doesn't guarantee any order.
  EXPECT_THAT(
      personal_data_->GetProfiles(PersonalDataManager::ProfileOrder::kNone),
      UnorderedElementsAre(Pointee(profile1), Pointee(profile2),
                           Pointee(profile3)));

  // `profile3` is first, since it has a much higher use count.
  // `profile1` and `profile2` have the same use count, so `profile2` with later
  // use date is second.
  EXPECT_THAT(personal_data_->GetProfiles(
                  PersonalDataManager::ProfileOrder::kHighestFrecencyDesc),
              testing::ElementsAre(Pointee(profile3), Pointee(profile2),
                                   Pointee(profile1)));

  std::vector<AutofillProfile*> profiles = personal_data_->GetProfiles(
      PersonalDataManager::ProfileOrder::kMostRecentlyUsedFirstDesc);
  // Ordered by `use_date()`.
  EXPECT_THAT(profiles,
              testing::ElementsAre(Pointee(profile2), Pointee(profile3),
                                   Pointee(profile1)));

  // TODO(crbug.com/1420547): The modification date cannot be set beforehand,
  // since it is overwritten by the database when the profile is initially
  // stored. To test the ordering by modification date, update the `profiles`
  // modification dates such that the order gets reversed. It is necessary to
  // modify the PDM's profiles directly, since any modification involving the
  // database will overwrite the modification date.
  for (int i = 0; i < 3; i++) {
    profiles[i]->set_modification_date(now - base::Hours(2 - i));
  }
  EXPECT_THAT(personal_data_->GetProfiles(
                  PersonalDataManager::ProfileOrder::kMostRecentlyModifiedDesc),
              testing::ElementsAre(Pointee(profile1), Pointee(profile3),
                                   Pointee(profile2)));
}

// Adding, updating, removing operations without waiting in between.
TEST_F(AddressDataManagerTest, AddRemoveUpdateProfileSequence) {
  AutofillProfile profile(test::GetFullProfile());

  personal_data_->AddProfile(profile);
  personal_data_->RemoveByGUID(profile.guid());
  personal_data_->UpdateProfile(profile);
  PersonalDataChangedWaiter(*personal_data_).Wait();

  auto profiles = personal_data_->GetProfiles();
  ASSERT_EQ(0U, profiles.size());

  personal_data_->AddProfile(profile);
  personal_data_->RemoveByGUID(profile.guid());
  personal_data_->RemoveByGUID(profile.guid());
  PersonalDataChangedWaiter(*personal_data_).Wait();

  profiles = personal_data_->GetProfiles();
  ASSERT_EQ(0U, profiles.size());

  personal_data_->AddProfile(profile);
  profile.SetRawInfo(EMAIL_ADDRESS, u"new@email.com");
  personal_data_->UpdateProfile(profile);
  PersonalDataChangedWaiter(*personal_data_).Wait();

  profiles = personal_data_->GetProfiles();
  ASSERT_EQ(1U, profiles.size());
  EXPECT_EQ(profiles[0]->GetRawInfo(EMAIL_ADDRESS), u"new@email.com");

  profile.SetRawInfo(EMAIL_ADDRESS, u"newer@email.com");
  personal_data_->UpdateProfile(profile);
  profile.SetRawInfo(EMAIL_ADDRESS, u"newest@email.com");
  personal_data_->UpdateProfile(profile);
  PersonalDataChangedWaiter(*personal_data_).Wait();

  profiles = personal_data_->GetProfiles();
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
  AddProfileToPersonalDataManager(profile);

  // Reload the database.
  ResetPersonalDataManager();

  // Verify the addition.
  const std::vector<AutofillProfile*>& results = personal_data_->GetProfiles();
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
  profile1.SetInfo(AutofillType(ADDRESS_HOME_COUNTRY), u"India", "en-US");
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
  profile7.SetInfo(AutofillType(ADDRESS_HOME_COUNTRY), u"United States",
                   "en-US");
  profile7.FinalizeAfterImport();
  profiles.push_back(profile7);

  for (const AutofillProfile& profile : profiles) {
    AddProfileToPersonalDataManager(profile);
  }
  ASSERT_EQ(profiles.size(), personal_data_->GetProfiles().size());
  for (size_t i = 0; i < profiles.size(); ++i) {
    EXPECT_TRUE(base::Contains(profiles, *personal_data_->GetProfiles()[i]));
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
  without_invalid.SetInfo(AutofillType(ADDRESS_HOME_COUNTRY), u"United States",
                          "en-US");

  AutofillProfile with_invalid = without_invalid;
  with_invalid.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"Invalid_Phone_Number");

  AddProfileToPersonalDataManager(with_invalid);
  ASSERT_EQ(1u, personal_data_->GetProfiles().size());
  AutofillProfile profile = *personal_data_->GetProfiles()[0];
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
  AddProfileToPersonalDataManager(profile0);
  AddProfileToPersonalDataManager(profile1);

  EXPECT_THAT(personal_data_->GetProfiles(),
              UnorderedElementsAre(Pointee(profile0), Pointee(profile1)));

  // Update, remove, and add.
  profile0.SetRawInfo(NAME_FIRST, u"John");
  UpdateProfileOnPersonalDataManager(profile0);
  RemoveByGUIDFromPersonalDataManager(profile1.guid());
  AddProfileToPersonalDataManager(profile2);

  EXPECT_THAT(personal_data_->GetProfiles(),
              UnorderedElementsAre(Pointee(profile0), Pointee(profile2)));

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the profiles from the web
  // database.
  ResetPersonalDataManager();

  // Verify that we've loaded the profiles from the web database.
  EXPECT_THAT(personal_data_->GetProfiles(),
              UnorderedElementsAre(Pointee(profile0), Pointee(profile2)));
}

// Tests that `UpdateProfile()` takes changes in the `ProfileTokenQuality`
// observations into considerations.
TEST_F(AddressDataManagerTest, UpdateProfile_NewObservations) {
  base::test::ScopedFeatureList feature{
      features::kAutofillTrackProfileTokenQuality};

  // Add a profile without observations at `kArbitraryTime`.
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);
  AutofillProfile profile = test::GetFullProfile();
  AddProfileToPersonalDataManager(profile);
  test_clock.SetNow(kSomeLaterTime);

  // Add an observation, as might happen during a form submit.
  test_api(profile.token_quality())
      .AddObservation(NAME_FIRST,
                      ProfileTokenQuality::ObservationType::kAccepted);
  UpdateProfileOnPersonalDataManager(profile);

  // Expect that `UpdateProfile()` didn't reject the update as a no-op.
  // Since new observations are considered a metadata change, further expected
  // that the modification date hasn't changed.
  const AutofillProfile* pdm_profile =
      personal_data_->GetProfileByGUID(profile.guid());
  EXPECT_THAT(
      pdm_profile->token_quality().GetObservationTypesForFieldType(NAME_FIRST),
      UnorderedElementsAre(ProfileTokenQuality::ObservationType::kAccepted));
  EXPECT_EQ(profile.modification_date(), kArbitraryTime);
}

// Tests that when the value for a type changes, `UpdateProfile()` resets the
// observations for that type.
TEST_F(AddressDataManagerTest, UpdateProfile_ResetObservations) {
  base::test::ScopedFeatureList feature{
      features::kAutofillTrackProfileTokenQuality};

  // Add a profile with observations for NAME_FIRST and NAME_LAST.
  AutofillProfile profile = test::GetFullProfile();
  test_api(profile.token_quality())
      .AddObservation(NAME_FIRST,
                      ProfileTokenQuality::ObservationType::kAccepted);
  test_api(profile.token_quality())
      .AddObservation(NAME_LAST,
                      ProfileTokenQuality::ObservationType::kEditedFallback);
  AddProfileToPersonalDataManager(profile);

  // Modify the NAME_FIRST and update the profile in the PDM.
  profile.SetRawInfo(NAME_FIRST, u"new " + profile.GetRawInfo(NAME_FIRST));
  UpdateProfileOnPersonalDataManager(profile);

  // Expect that only the observations for NAME_LAST remain.
  profile = *personal_data_->GetProfileByGUID(profile.guid());
  EXPECT_TRUE(profile.token_quality()
                  .GetObservationTypesForFieldType(NAME_FIRST)
                  .empty());
  EXPECT_THAT(
      profile.token_quality().GetObservationTypesForFieldType(NAME_LAST),
      UnorderedElementsAre(
          ProfileTokenQuality::ObservationType::kEditedFallback));
}

TEST_F(AddressDataManagerTest, MigrateProfileToAccount) {
  const AutofillProfile kLocalProfile = test::GetFullProfile();
  ASSERT_EQ(kLocalProfile.source(), AutofillProfile::Source::kLocalOrSyncable);
  AddProfileToPersonalDataManager(kLocalProfile);

  personal_data_->MigrateProfileToAccount(kLocalProfile);
  PersonalDataChangedWaiter(*personal_data_).Wait();
  const std::vector<AutofillProfile*> profiles = personal_data_->GetProfiles();

  // `kLocalProfile` should be gone and only the migrated account profile should
  // exist.
  ASSERT_EQ(profiles.size(), 1u);
  const AutofillProfile kAccountProfile = *profiles[0];
  EXPECT_EQ(kAccountProfile.source(), AutofillProfile::Source::kAccount);
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
  AddProfileToPersonalDataManager(profile0);

  // Verify that we've loaded the profiles from the web database.
  const std::vector<AutofillProfile*>& results2 = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, profile0.Compare(*results2[0]));

  // Add a new profile.
  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile1, "z", "", "", "", "", "", "", "", "", "", "",
                       "");
  AddProfileToPersonalDataManager(profile1);

  // Make sure the two profiles have different GUIDs, both valid.
  const std::vector<AutofillProfile*>& results3 = personal_data_->GetProfiles();
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
  AddProfileToPersonalDataManager(profile0);

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the profiles from the web
  // database.
  ResetPersonalDataManager();

  // Verify that we've loaded the profiles from the web database.
  ASSERT_EQ(0U, personal_data_->GetProfiles().size());
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
  AddProfileToPersonalDataManager(profile0);
  AddProfileToPersonalDataManager(profile1);

  EXPECT_THAT(personal_data_->GetProfiles(),
              UnorderedElementsAre(Pointee(profile0), Pointee(profile1)));

  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile2, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5",
                       "Orlando", "FL", "32801", "US", "19482937549");

  profile_database_service_->AddAutofillProfile(profile2);

  personal_data_->Refresh();

  PersonalDataChangedWaiter(*personal_data_).Wait();

  EXPECT_THAT(personal_data_->GetProfiles(),
              UnorderedElementsAre(Pointee(profile0), Pointee(profile1),
                                   Pointee(profile2)));

  profile_database_service_->RemoveAutofillProfile(
      profile1.guid(), AutofillProfile::Source::kLocalOrSyncable);
  profile_database_service_->RemoveAutofillProfile(
      profile2.guid(), AutofillProfile::Source::kLocalOrSyncable);

  personal_data_->Refresh();
  PersonalDataChangedWaiter(*personal_data_).Wait();

  auto results = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(profile0, *results[0]);

  profile0.SetRawInfo(NAME_FIRST, u"Mar");
  profile_database_service_->UpdateAutofillProfile(profile0);

  personal_data_->Refresh();
  PersonalDataChangedWaiter(*personal_data_).Wait();

  results = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(profile0, *results[0]);
}

TEST_F(AddressDataManagerTest, UpdateLanguageCodeInProfile) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  AddProfileToPersonalDataManager(profile);

  // Make sure everything is set up correctly.
  EXPECT_EQ(1U, personal_data_->GetProfiles().size());
  EXPECT_EQ(1U, personal_data_->GetProfiles().size());

  profile.set_language_code("en");
  UpdateProfileOnPersonalDataManager(profile);

  const std::vector<AutofillProfile*>& results = personal_data_->GetProfiles();
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

  AddProfileToPersonalDataManager(more_recently_used_profile);
  AddProfileToPersonalDataManager(less_recently_used_profile);

  EXPECT_EQ(personal_data_->GetProfiles().size(), 2U);

  // Now make an update to less recently used profile that makes it a duplicate
  // of the more recently used profile.
  AutofillProfile updated_less_recently_used_profile =
      more_recently_used_profile;
  updated_less_recently_used_profile.set_guid(
      less_recently_used_profile.guid());
  // Set the updated profile to have a older use date than it's duplicate.
  updated_less_recently_used_profile.set_use_date(older_use_date);
  UpdateProfileOnPersonalDataManager(updated_less_recently_used_profile);

  // Verify that the less recently used profile was removed.
  ASSERT_EQ(personal_data_->GetProfiles().size(), 1U);
  EXPECT_EQ(*personal_data_->GetProfiles()[0], more_recently_used_profile);
  EXPECT_EQ(personal_data_->GetProfiles()[0]->use_date(), newer_use_data);
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

  AddProfileToPersonalDataManager(less_recently_used_profile);
  AddProfileToPersonalDataManager(more_recently_used_profile);

  EXPECT_EQ(personal_data_->GetProfiles().size(), 2U);

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
  PersonalDataChangedWaiter update_waiter(*personal_data_);
  // Expect an update and a deletion. This only triggers a single notification
  // once both operations have finished.
  personal_data_->UpdateProfile(updated_more_recently_used_profile);
  std::move(update_waiter).Wait();

  // Verify that less recently used profile was removed.
  ASSERT_EQ(personal_data_->GetProfiles().size(), 1U);

  EXPECT_EQ(*personal_data_->GetProfiles()[0],
            updated_more_recently_used_profile);
  EXPECT_EQ(personal_data_->GetProfiles()[0]->use_date(), newer_use_data);
}

TEST_F(AddressDataManagerTest, RecordUseOf) {
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);
  AutofillProfile profile = test::GetFullProfile();
  ASSERT_EQ(profile.use_count(), 1u);
  ASSERT_EQ(profile.use_date(), kArbitraryTime);
  ASSERT_EQ(profile.modification_date(), kArbitraryTime);
  AddProfileToPersonalDataManager(profile);

  test_clock.SetNow(kSomeLaterTime);
  personal_data_->RecordUseOf(&profile);
  PersonalDataChangedWaiter(*personal_data_).Wait();

  AutofillProfile* adm_profile =
      personal_data_->GetProfileByGUID(profile.guid());
  ASSERT_TRUE(adm_profile);
  EXPECT_EQ(adm_profile->use_count(), 2u);
  EXPECT_EQ(adm_profile->use_date(), kSomeLaterTime);
  EXPECT_EQ(adm_profile->modification_date(), kArbitraryTime);
}

TEST_F(AddressDataManagerTest, SaveProfileMigrationStrikes) {
  AddressDataManager& adm = personal_data_->address_data_manager();
  EXPECT_FALSE(adm.IsProfileMigrationBlocked(kGuid));

  adm.AddStrikeToBlockProfileMigration(kGuid);
  EXPECT_FALSE(adm.IsProfileMigrationBlocked(kGuid));

  adm.AddStrikeToBlockProfileMigration(kGuid);
  EXPECT_FALSE(adm.IsProfileMigrationBlocked(kGuid));

  // After the third strike, the guid should be blocked.
  adm.AddStrikeToBlockProfileMigration(kGuid);
  EXPECT_TRUE(adm.IsProfileMigrationBlocked(kGuid));

  // Until the strikes are removed again.
  adm.RemoveStrikesToBlockProfileMigration(kGuid);
  EXPECT_FALSE(adm.IsProfileMigrationBlocked(kGuid));

  // `AddMaxStrikesToBlockProfileMigration()` should add sufficiently many
  // strikes.
  adm.AddMaxStrikesToBlockProfileMigration(kGuid);
  EXPECT_TRUE(adm.IsProfileMigrationBlocked(kGuid));
}

TEST_F(AddressDataManagerTest, SaveProfileUpdateStrikes) {
  AddressDataManager& adm = personal_data_->address_data_manager();
  EXPECT_FALSE(adm.IsProfileUpdateBlocked(kGuid));

  adm.AddStrikeToBlockProfileUpdate(kGuid);
  EXPECT_FALSE(adm.IsProfileUpdateBlocked(kGuid));

  adm.AddStrikeToBlockProfileUpdate(kGuid);
  EXPECT_FALSE(adm.IsProfileUpdateBlocked(kGuid));

  // After the third strike, the guid should be blocked.
  adm.AddStrikeToBlockProfileUpdate(kGuid);
  EXPECT_TRUE(adm.IsProfileUpdateBlocked(kGuid));

  // Until the strikes are removed again.
  adm.RemoveStrikesToBlockProfileUpdate(kGuid);
  EXPECT_FALSE(adm.IsProfileUpdateBlocked(kGuid));
}

TEST_F(AddressDataManagerTest, SaveProfileSaveStrikes) {
  AddressDataManager& adm = personal_data_->address_data_manager();
  GURL domain("https://www.block.me/index.html");

  EXPECT_FALSE(adm.IsNewProfileImportBlockedForDomain(domain));

  adm.AddStrikeToBlockNewProfileImportForDomain(domain);
  EXPECT_FALSE(adm.IsNewProfileImportBlockedForDomain(domain));

  adm.AddStrikeToBlockNewProfileImportForDomain(domain);
  EXPECT_FALSE(adm.IsNewProfileImportBlockedForDomain(domain));

  // After the third strike, the domain should be blocked.
  adm.AddStrikeToBlockNewProfileImportForDomain(domain);
  EXPECT_TRUE(adm.IsNewProfileImportBlockedForDomain(domain));

  // Until the strikes are removed again.
  adm.RemoveStrikesToBlockNewProfileImportForDomain(domain);
  EXPECT_FALSE(adm.IsNewProfileImportBlockedForDomain(domain));
}

}  // namespace

}  // namespace autofill
