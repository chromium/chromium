// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/personal_data_manager_cleaner.h"

#include "base/guid.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_test_base.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Matcher;
using ::testing::Truly;

namespace autofill {

namespace {

const base::Time kArbitraryTime = base::Time::FromDoubleT(25);
const base::Time kSomeLaterTime = base::Time::FromDoubleT(1000);
const base::Time kMuchLaterTime = base::Time::FromDoubleT(5000);

ACTION_P(QuitMessageLoop, loop) {
  loop->Quit();
}

template <typename T>
auto HasSameElements(const std::vector<T*>& expectations) {
  std::vector<Matcher<T*>> matchers;
  for (const auto& e : expectations)
    matchers.push_back(Truly([e](T* a) { return a->Compare(*e) == 0; }));
  return UnorderedElementsAreArray(matchers);
}

}  // anonymous namespace

class PersonalDataManagerCleanerTest : public PersonalDataManagerTestBase,
                                       public testing::Test {
 public:
  PersonalDataManagerCleanerTest() = default;
  ~PersonalDataManagerCleanerTest() override = default;

  void SetUp() override {
    SetUpTest();
    personal_data_ = std::make_unique<PersonalDataManager>("EN", "US");
    ResetPersonalDataManager(/*is_incognito=*/false,
                             /*use_sync_transport_mode=*/false,
                             personal_data_.get());
    personal_data_manager_cleaner_ =
        std::make_unique<PersonalDataManagerCleaner>(personal_data_.get(),
                                                     nullptr, prefs_.get());
  }

  void TearDown() override {
    if (personal_data_)
      personal_data_->Shutdown();
    personal_data_.reset();
    TearDownTest();
  }

 protected:
  // Verifies that the web database has been updated and the notification sent.
  void WaitForOnPersonalDataChanged(
      absl::optional<AutofillProfile> profile = absl::nullopt) {
    base::RunLoop run_loop;
    EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
        .WillOnce(QuitMessageLoop(&run_loop));
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
        .Times(testing::AnyNumber());
    if (profile)
      personal_data_->AddProfile(profile.value());
    run_loop.Run();
  }

  void AddProfileToPersonalDataManager(const AutofillProfile& profile) {
    WaitForOnPersonalDataChanged(profile);
  }

  void SetServerCards(const std::vector<CreditCard>& server_cards) {
    test::SetServerCreditCards(personal_data_->IsSyncFeatureEnabled()
                                   ? profile_autofill_table_.get()
                                   : account_autofill_table_.get(),
                               server_cards);
  }

  std::unique_ptr<PersonalDataManager> personal_data_;
  std::unique_ptr<PersonalDataManagerCleaner> personal_data_manager_cleaner_;
};

// Tests that DedupeProfiles sets the correct profile guids to
// delete after merging similar profiles.
TEST_F(PersonalDataManagerCleanerTest, DedupeProfiles_ProfilesToDelete) {
  // Create the profile for which to find duplicates. It has the highest
  // ranking score.
  AutofillProfile* profile1 =
      new AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(profile1, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "", "742. Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "12345678910");
  profile1->set_use_count(9);

  // Create a different profile that should not be deduped (different address).
  AutofillProfile* profile2 =
      new AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(profile2, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "Fox", "1234 Other Street", "",
                       "Springfield", "IL", "91601", "US", "12345678910");
  profile2->set_use_count(7);

  // Create a profile similar to profile1 which should be deduped.
  AutofillProfile* profile3 =
      new AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(profile3, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "", "742 Evergreen Terrace", "",
                       "Springfield", "IL", "91601", "US", "12345678910");
  profile3->set_use_count(5);

  // Create another different profile that should not be deduped (different
  // name).
  AutofillProfile* profile4 =
      new AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(profile4, "Marjorie", "Jacqueline", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "12345678910");
  profile4->set_use_count(3);

  // Create another profile similar to profile1. Since that one has the lowest
  // ranking score, the result of the merge should be in this profile at the end
  // of the test.
  AutofillProfile* profile5 =
      new AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(profile5, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "US", "12345678910");
  profile5->set_use_count(1);

  // Add the profiles.
  std::vector<std::unique_ptr<AutofillProfile>> existing_profiles;
  existing_profiles.push_back(std::unique_ptr<AutofillProfile>(profile1));
  existing_profiles.push_back(std::unique_ptr<AutofillProfile>(profile2));
  existing_profiles.push_back(std::unique_ptr<AutofillProfile>(profile3));
  existing_profiles.push_back(std::unique_ptr<AutofillProfile>(profile4));
  existing_profiles.push_back(std::unique_ptr<AutofillProfile>(profile5));

  base::HistogramTester histogram_tester;
  std::unordered_map<std::string, std::string> guids_merge_map;
  std::unordered_set<std::string> profiles_to_delete;
  personal_data_manager_cleaner_->DedupeProfilesForTesting(
      &existing_profiles, &profiles_to_delete, &guids_merge_map);
  // 5 profiles were considered for dedupe.
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesConsideredForDedupe", 5, 1);
  // 2 profiles were removed (profiles 1 and 3).
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesRemovedDuringDedupe", 2, 1);

  // Profile1 should be deleted because it was sent as the profile to merge and
  // thus was merged into profile3 and then into profile5.
  EXPECT_TRUE(profiles_to_delete.count(profile1->guid()));

  // Profile3 should be deleted because profile1 was merged into it and the
  // resulting profile was then merged into profile5.
  EXPECT_TRUE(profiles_to_delete.count(profile3->guid()));

  // Only these two profiles should be deleted.
  EXPECT_EQ(2U, profiles_to_delete.size());

  // All profiles should still be present in |existing_profiles|.
  EXPECT_EQ(5U, existing_profiles.size());
}

// Tests that DedupeProfiles sets the correct merge mapping for billing address
// id references.
TEST_F(PersonalDataManagerCleanerTest, DedupeProfiles_GuidsMergeMap) {
  // Create the profile for which to find duplicates. It has the highest
  // ranking score.
  AutofillProfile* profile1 =
      new AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(profile1, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "", "742. Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "12345678910");
  profile1->set_use_count(9);

  // Create a different profile that should not be deduped (different address).
  AutofillProfile* profile2 =
      new AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(profile2, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "Fox", "1234 Other Street", "",
                       "Springfield", "IL", "91601", "US", "12345678910");
  profile2->set_use_count(7);

  // Create a profile similar to profile1 which should be deduped.
  AutofillProfile* profile3 =
      new AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(profile3, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "", "742 Evergreen Terrace", "",
                       "Springfield", "IL", "91601", "US", "12345678910");
  profile3->set_use_count(5);

  // Create another different profile that should not be deduped (different
  // name).
  AutofillProfile* profile4 =
      new AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(profile4, "Marjorie", "Jacqueline", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "12345678910");
  profile4->set_use_count(3);

  // Create another profile similar to profile1. Since that one has the lowest
  // ranking score, the result of the merge should be in this profile at the end
  // of the test.
  AutofillProfile* profile5 =
      new AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(profile5, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "US", "12345678910");
  profile5->set_use_count(1);

  // Add the profiles.
  std::vector<std::unique_ptr<AutofillProfile>> existing_profiles;
  existing_profiles.push_back(std::unique_ptr<AutofillProfile>(profile1));
  existing_profiles.push_back(std::unique_ptr<AutofillProfile>(profile2));
  existing_profiles.push_back(std::unique_ptr<AutofillProfile>(profile3));
  existing_profiles.push_back(std::unique_ptr<AutofillProfile>(profile4));
  existing_profiles.push_back(std::unique_ptr<AutofillProfile>(profile5));

  std::unordered_map<std::string, std::string> guids_merge_map;
  std::unordered_set<std::string> profiles_to_delete;

  personal_data_manager_cleaner_->DedupeProfilesForTesting(
      &existing_profiles, &profiles_to_delete, &guids_merge_map);

  // The two profile merges should be recorded in the map.
  EXPECT_EQ(2U, guids_merge_map.size());

  // Profile 1 was merged into profile 3.
  ASSERT_TRUE(guids_merge_map.count(profile1->guid()));
  EXPECT_TRUE(guids_merge_map.at(profile1->guid()) == profile3->guid());

  // Profile 3 was merged into profile 5.
  ASSERT_TRUE(guids_merge_map.count(profile3->guid()));
  EXPECT_TRUE(guids_merge_map.at(profile3->guid()) == profile5->guid());
}

// Tests that UpdateCardsBillingAddressReference sets the correct billing
// address id as specified in the map.
TEST_F(PersonalDataManagerCleanerTest, UpdateCardsBillingAddressReference) {
  /*  The merges will be as follow:

      A -> B            F (not merged)
             \
               -> E
             /
      C -> D
  */

  std::unordered_map<std::string, std::string> guids_merge_map;
  guids_merge_map.insert(std::pair<std::string, std::string>("A", "B"));
  guids_merge_map.insert(std::pair<std::string, std::string>("C", "D"));
  guids_merge_map.insert(std::pair<std::string, std::string>("B", "E"));
  guids_merge_map.insert(std::pair<std::string, std::string>("D", "E"));

  // Create a credit card without a billing address id
  CreditCard* credit_card0 =
      new CreditCard(base::GenerateGUID(), test::kEmptyOrigin);

  // Create cards that use A, D, E and F as their billing address id.
  CreditCard* credit_card1 =
      new CreditCard(base::GenerateGUID(), test::kEmptyOrigin);
  credit_card1->set_billing_address_id("A");
  CreditCard* credit_card2 =
      new CreditCard(base::GenerateGUID(), test::kEmptyOrigin);
  credit_card2->set_billing_address_id("D");
  CreditCard* credit_card3 =
      new CreditCard(base::GenerateGUID(), test::kEmptyOrigin);
  credit_card3->set_billing_address_id("E");
  CreditCard* credit_card4 =
      new CreditCard(base::GenerateGUID(), test::kEmptyOrigin);
  credit_card4->set_billing_address_id("F");

  // Add the credit cards to the database.
  personal_data_->local_credit_cards_.push_back(
      std::unique_ptr<CreditCard>(credit_card0));
  personal_data_->local_credit_cards_.push_back(
      std::unique_ptr<CreditCard>(credit_card1));
  personal_data_->server_credit_cards_.push_back(
      std::unique_ptr<CreditCard>(credit_card2));
  personal_data_->local_credit_cards_.push_back(
      std::unique_ptr<CreditCard>(credit_card3));
  personal_data_->server_credit_cards_.push_back(
      std::unique_ptr<CreditCard>(credit_card4));

  personal_data_manager_cleaner_->UpdateCardsBillingAddressReferenceForTesting(
      guids_merge_map);

  // The first card's billing address should now be E.
  EXPECT_EQ("E", credit_card1->billing_address_id());
  // The second card's billing address should now be E.
  EXPECT_EQ("E", credit_card2->billing_address_id());
  // The third card's billing address should still be E.
  EXPECT_EQ("E", credit_card3->billing_address_id());
  // The fourth card's billing address should still be F.
  EXPECT_EQ("F", credit_card4->billing_address_id());
}

// Tests that ApplyDedupingRoutine updates the credit cards' billing address id
// based on the deduped profiles.
TEST_F(PersonalDataManagerCleanerTest,
       ApplyDedupingRoutine_CardsBillingAddressIdUpdated) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeature(features::kAutofillEnableProfileDeduplication);

  // A set of 6 profiles will be created. They should merge in this way:
  //  1 -> 2 -> 3
  //  4 -> 5
  //  6
  // Set their frencency score so that profile 3 has a higher score than 5, and
  // 5 has a higher score than 6. This will ensure a deterministic order when
  // verifying results.

  // Create a set of 3 profiles to be merged together.
  // Create a profile with a higher ranking score.
  AutofillProfile profile1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "", "742. Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "");
  profile1.set_use_count(12);
  profile1.set_use_date(AutofillClock::Now() - base::Days(1));

  // Create a profile with a medium ranking score.
  AutofillProfile profile2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "", "742 Evergreen Terrace", "",
                       "Springfield", "IL", "91601", "", "12345678910");
  profile2.set_use_count(5);
  profile2.set_use_date(AutofillClock::Now() - base::Days(3));

  // Create a profile with a lower ranking score.
  AutofillProfile profile3(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "", "");
  profile3.set_use_count(3);
  profile3.set_use_date(AutofillClock::Now() - base::Days(5));

  // Create a set of two profiles to be merged together.
  // Create a profile with a higher ranking score.
  AutofillProfile profile4(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "Marge", "B", "Simpson",
                       "marge.simpson@abc.com", "", "742. Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "");
  profile4.set_use_count(11);
  profile4.set_use_date(AutofillClock::Now() - base::Days(1));

  // Create a profile with a lower ranking score.
  AutofillProfile profile5(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile5, "Marge", "B", "Simpson",
                       "marge.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "", "");
  profile5.set_use_count(5);
  profile5.set_use_date(AutofillClock::Now() - base::Days(3));

  // Create a unique profile.
  AutofillProfile profile6(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile6, "Bart", "J", "Simpson",
                       "bart.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "", "");
  profile6.set_use_count(10);
  profile6.set_use_date(AutofillClock::Now() - base::Days(1));

  // Add three credit cards. Give them a ranking score so that they are
  // suggested in order (1, 2, 3). This will ensure a deterministic order for
  // verifying results.
  CreditCard credit_card1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card1, "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  credit_card1.set_use_count(10);

  CreditCard credit_card2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card2, "John Dillinger",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  credit_card2.set_use_count(5);

  CreditCard credit_card3(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card3, "Bonnie Parker",
                          "5105105105105100" /* Mastercard */, "12", "2999",
                          "1");
  credit_card3.set_use_count(1);

  // Associate the first card with profile1.
  credit_card1.set_billing_address_id(profile1.guid());
  // Associate the second card with profile4.
  credit_card2.set_billing_address_id(profile4.guid());
  // Associate the third card with profile6.
  credit_card3.set_billing_address_id(profile6.guid());

  AddProfileToPersonalDataManager(profile1);
  AddProfileToPersonalDataManager(profile2);
  AddProfileToPersonalDataManager(profile3);
  AddProfileToPersonalDataManager(profile4);
  AddProfileToPersonalDataManager(profile5);
  AddProfileToPersonalDataManager(profile6);
  personal_data_->AddCreditCard(credit_card1);
  personal_data_->AddCreditCard(credit_card2);
  personal_data_->AddCreditCard(credit_card3);

  WaitForOnPersonalDataChanged();

  // Make sure the 6 profiles and 3 credit cards were saved.
  EXPECT_EQ(6U, personal_data_->GetProfiles().size());
  EXPECT_EQ(3U, personal_data_->GetCreditCards().size());

  EXPECT_TRUE(personal_data_manager_cleaner_->ApplyDedupingRoutineForTesting());
  WaitForOnPersonalDataChanged();

  // Get the profiles and cards sorted by their ranking score to have a
  // deterministic order.
  std::vector<AutofillProfile*> profiles =
      personal_data_->GetProfilesToSuggest();
  std::vector<CreditCard*> credit_cards =
      personal_data_->GetCreditCardsToSuggest(/*include_server_cards=*/true);

  // |profile1| should have been merged into |profile2| which should then have
  // been merged into |profile3|. |profile4| should have been merged into
  // |profile5| and |profile6| should not have merged. Therefore there should be
  // 3 profile left.
  ASSERT_EQ(3U, profiles.size());

  // Make sure the remaining profiles are the expected ones.
  EXPECT_EQ(profile3.guid(), profiles[0]->guid());
  EXPECT_EQ(profile5.guid(), profiles[1]->guid());
  EXPECT_EQ(profile6.guid(), profiles[2]->guid());

  // |credit_card1|'s billing address should now be profile 3.
  EXPECT_EQ(profile3.guid(), credit_cards[0]->billing_address_id());

  // |credit_card2|'s billing address should now be profile 5.
  EXPECT_EQ(profile5.guid(), credit_cards[1]->billing_address_id());

  // |credit_card3|'s billing address should still be profile 6.
  EXPECT_EQ(profile6.guid(), credit_cards[2]->billing_address_id());
}

// Tests that ApplyDedupingRoutine merges the profile values correctly, i.e.
// never lose information and keep the syntax of the profile with the higher
// ranking score.
TEST_F(PersonalDataManagerCleanerTest,
       ApplyDedupingRoutine_MergedProfileValues) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeature(features::kAutofillEnableProfileDeduplication);

  // Create a profile with a higher ranking score.
  AutofillProfile profile1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "", "742. Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "");
  profile1.set_use_count(10);
  profile1.set_use_date(AutofillClock::Now() - base::Days(1));

  // Create a profile with a medium ranking score.
  AutofillProfile profile2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "", "742 Evergreen Terrace", "",
                       "Springfield", "IL", "91601", "", "12345678910");
  profile2.set_use_count(5);
  profile2.set_use_date(AutofillClock::Now() - base::Days(3));

  // Create a profile with a lower ranking score.
  AutofillProfile profile3(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "", "");
  profile3.set_use_count(3);
  profile3.set_use_date(AutofillClock::Now() - base::Days(5));

  AddProfileToPersonalDataManager(profile1);
  AddProfileToPersonalDataManager(profile2);
  AddProfileToPersonalDataManager(profile3);

  // Make sure the 3 profiles were saved;
  EXPECT_EQ(3U, personal_data_->GetProfiles().size());

  base::HistogramTester histogram_tester;

  EXPECT_TRUE(personal_data_manager_cleaner_->ApplyDedupingRoutineForTesting());
  WaitForOnPersonalDataChanged();

  std::vector<AutofillProfile*> profiles = personal_data_->GetProfiles();

  // |profile1| should have been merged into |profile2| which should then have
  // been merged into |profile3|. Therefore there should only be 1 saved
  // profile.
  ASSERT_EQ(1U, profiles.size());
  // 3 profiles were considered for dedupe.
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesConsideredForDedupe", 3, 1);
  // 2 profiles were removed (profiles 1 and 2).
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesRemovedDuringDedupe", 2, 1);

  // Since profiles with higher ranking scores are merged into profiles with
  // lower ranking scores, the result of the merge should be contained in
  // profile3 since it had a lower ranking score compared to profile1.
  EXPECT_EQ(profile3.guid(), profiles[0]->guid());
  // The address syntax that results from the merge should be the one from the
  // imported profile (highest ranking).
  EXPECT_EQ(u"742. Evergreen Terrace",
            profiles[0]->GetRawInfo(ADDRESS_HOME_LINE1));
  // The middle name should be full, even if the profile with the higher
  // ranking only had an initial (no loss of information).
  EXPECT_EQ(u"Jay", profiles[0]->GetRawInfo(NAME_MIDDLE));
  // The specified phone number from profile1 should be kept (no loss of
  // information).
  EXPECT_EQ(u"12345678910", profiles[0]->GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  // The specified company name from profile2 should be kept (no loss of
  // information).
  EXPECT_EQ(u"Fox", profiles[0]->GetRawInfo(COMPANY_NAME));
  // The specified country from the imported profile should be kept (no loss of
  // information).
  EXPECT_EQ(u"US", profiles[0]->GetRawInfo(ADDRESS_HOME_COUNTRY));
  // The use count that results from the merge should be the max of all the
  // profiles use counts.
  EXPECT_EQ(10U, profiles[0]->use_count());
  // The use date that results from the merge should be the one from the
  // profile1 since it was the most recently used profile.
  EXPECT_LT(profile1.use_date() - base::Seconds(10), profiles[0]->use_date());
}

// Tests that ApplyDedupingRoutine only keeps the verified profile with its
// original data when deduping with similar profiles, even if it has a higher
// ranking score.
TEST_F(PersonalDataManagerCleanerTest,
       ApplyDedupingRoutine_VerifiedProfileFirst) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeature(features::kAutofillEnableProfileDeduplication);

  // Create a verified profile with a higher ranking score.
  AutofillProfile profile1(base::GenerateGUID(), kSettingsOrigin);
  test::SetProfileInfo(
      &profile1, "Homer", "Jay", "Simpson", "homer.simpson@abc.com", "",
      "742 Evergreen Terrace", "", "Springfield", "IL", "91601", "",
      "12345678910", /*finalize=*/true,
      /*status=*/structured_address::VerificationStatus::kUserVerified);
  profile1.set_use_count(7);
  profile1.set_use_date(kMuchLaterTime);

  // Create a similar non verified profile with a medium ranking score.
  AutofillProfile profile2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "", "742. Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "");
  profile2.set_use_count(5);
  profile2.set_use_date(kSomeLaterTime);

  // Create a similar non verified profile with a lower ranking score.
  AutofillProfile profile3(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "", "");
  profile3.set_use_count(3);
  profile3.set_use_date(kArbitraryTime);

  AddProfileToPersonalDataManager(profile1);
  AddProfileToPersonalDataManager(profile2);
  AddProfileToPersonalDataManager(profile3);

  // Make sure the 3 profiles were saved.
  EXPECT_EQ(3U, personal_data_->GetProfiles().size());

  base::HistogramTester histogram_tester;

  EXPECT_TRUE(personal_data_manager_cleaner_->ApplyDedupingRoutineForTesting());
  WaitForOnPersonalDataChanged();

  std::vector<AutofillProfile*> profiles = personal_data_->GetProfiles();

  // |profile2| should have merged with |profile3|. |profile3|
  // should then have been discarded because it is similar to the verified
  // |profile1|.
  ASSERT_EQ(1U, profiles.size());
  // 3 profiles were considered for dedupe.
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesConsideredForDedupe", 3, 1);
  // 2 profile were removed (profiles 2 and 3).
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesRemovedDuringDedupe", 2, 1);

  // Although the profile was verified, the structure of the  street address
  // still evolved with future observations. In this case, the "." was added
  // from a later observation.
  profile1.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_NAME, u"Evergreen Terrace",
      structured_address::VerificationStatus::kParsed);
  //
  // Only the verified |profile1| with its original data should have been kept.
  EXPECT_EQ(profile1.guid(), profiles[0]->guid());
  EXPECT_TRUE(profile1 == *profiles[0]);
  EXPECT_EQ(profile1.use_count(), profiles[0]->use_count());
  EXPECT_EQ(profile1.use_date(), profiles[0]->use_date());
}

// Tests that ApplyDedupingRoutine only keeps the verified profile with its
// original data when deduping with similar profiles, even if it has a lower
// ranking score.
TEST_F(PersonalDataManagerCleanerTest,
       ApplyDedupingRoutine_VerifiedProfileLast) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeature(features::kAutofillEnableProfileDeduplication);

  // Create a profile to dedupe with a higher ranking score.
  AutofillProfile profile1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "", "742. Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "");
  profile1.set_use_count(5);
  profile1.set_use_date(kMuchLaterTime);

  // Create a similar non verified profile with a medium ranking score.
  AutofillProfile profile2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "", "");
  profile2.set_use_count(5);
  profile2.set_use_date(kSomeLaterTime);

  // Create a similar verified profile with a lower ranking score.
  AutofillProfile profile3(base::GenerateGUID(), kSettingsOrigin);
  test::SetProfileInfo(
      &profile3, "Homer", "Jay", "Simpson", "homer.simpson@abc.com", "",
      "742 Evergreen Terrace", "", "Springfield", "IL", "91601", "",
      "12345678910", /*finalize=*/true,
      /*status=*/structured_address::VerificationStatus::kUserVerified);
  profile3.set_use_count(3);
  profile3.set_use_date(kArbitraryTime);

  AddProfileToPersonalDataManager(profile1);
  AddProfileToPersonalDataManager(profile2);
  AddProfileToPersonalDataManager(profile3);

  // Make sure the 3 profiles were saved.
  EXPECT_EQ(3U, personal_data_->GetProfiles().size());

  base::HistogramTester histogram_tester;

  EXPECT_TRUE(personal_data_manager_cleaner_->ApplyDedupingRoutineForTesting());
  WaitForOnPersonalDataChanged();

  std::vector<AutofillProfile*> profiles = personal_data_->GetProfiles();

  // |profile1| should have merged with |profile2|. |profile2|
  // should then have been discarded because it is similar to the verified
  // |profile3|.
  ASSERT_EQ(1U, profiles.size());
  // 3 profiles were considered for dedupe.
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesConsideredForDedupe", 3, 1);
  // 2 profile were removed (profiles 1 and 2).
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesRemovedDuringDedupe", 2, 1);

  // Only the verified |profile3| with it's original data should have been kept.
  EXPECT_EQ(profile3.guid(), profiles[0]->guid());
  EXPECT_TRUE(profile3 == *profiles[0]);
  EXPECT_EQ(profile3.use_count(), profiles[0]->use_count());
  EXPECT_EQ(profile3.use_date(), profiles[0]->use_date());
}

// Tests that ApplyDedupingRoutine does not merge unverified data into
// a verified profile. Also tests that two verified profiles don't get merged.
TEST_F(PersonalDataManagerCleanerTest,
       ApplyDedupingRoutine_MultipleVerifiedProfiles) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeature(features::kAutofillEnableProfileDeduplication);

  // Create a profile to dedupe with a higher ranking score.
  AutofillProfile profile1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "", "742. Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "");
  profile1.set_use_count(5);
  profile1.set_use_date(kMuchLaterTime);

  // Create a similar verified profile with a medium ranking score.
  AutofillProfile profile2(base::GenerateGUID(), kSettingsOrigin);
  test::SetProfileInfo(
      &profile2, "Homer", "J", "Simpson", "homer.simpson@abc.com", "Fox",
      "742 Evergreen Terrace.", "", "Springfield", "IL", "91601", "", "",
      /*finalize=*/true,
      /*status=*/structured_address::VerificationStatus::kUserVerified);

  profile2.set_use_count(5);
  profile2.set_use_date(kSomeLaterTime);

  // Create a similar verified profile with a lower ranking score.
  AutofillProfile profile3(base::GenerateGUID(), kSettingsOrigin);
  test::SetProfileInfo(
      &profile3, "Homer", "Jay", "Simpson", "homer.simpson@abc.com", "",
      "742 Evergreen Terrace", "", "Springfield", "IL", "91601", "",
      "12345678910", /*finalize=*/true,
      /*status*/ structured_address::VerificationStatus::kUserVerified);
  profile3.set_use_count(3);
  profile3.set_use_date(kArbitraryTime);

  AddProfileToPersonalDataManager(profile1);
  AddProfileToPersonalDataManager(profile2);
  AddProfileToPersonalDataManager(profile3);

  // Make sure the 3 profiles were saved.
  EXPECT_EQ(3U, personal_data_->GetProfiles().size());

  base::HistogramTester histogram_tester;

  EXPECT_TRUE(personal_data_manager_cleaner_->ApplyDedupingRoutineForTesting());
  WaitForOnPersonalDataChanged();

  // Get the profiles, sorted by ranking to have a deterministic order.
  std::vector<AutofillProfile*> profiles =
      personal_data_->GetProfilesToSuggest();

  // Although the profile was verified, the structure of the  street address
  // still evolved with future observations. In this case, the "." was removed
  // from a later observation.
  profile2.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_NAME, u"Evergreen Terrace",
      structured_address::VerificationStatus::kParsed);

  // |profile1| should have been discarded because the saved profile with the
  // highest ranking score is verified (|profile2|). Therefore, |profile1|'s
  // data should not have been merged with |profile2|'s data. Then |profile2|
  // should have been compared to |profile3| but they should not have merged
  // because both profiles are verified.
  ASSERT_EQ(2U, profiles.size());
  // 3 profiles were considered for dedupe.
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesConsideredForDedupe", 3, 1);
  // 1 profile was removed (|profile1|).
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesRemovedDuringDedupe", 1, 1);

  EXPECT_EQ(profile2.guid(), profiles[0]->guid());
  EXPECT_EQ(profile3.guid(), profiles[1]->guid());
  // The profiles should have kept their original data.
  EXPECT_TRUE(profile2 == *profiles[0]);
  EXPECT_TRUE(profile3 == *profiles[1]);
  EXPECT_EQ(profile2.use_count(), profiles[0]->use_count());
  EXPECT_EQ(profile3.use_count(), profiles[1]->use_count());
  EXPECT_EQ(profile2.use_date(), profiles[0]->use_date());
  EXPECT_EQ(profile3.use_date(), profiles[1]->use_date());
}

// Tests that ApplyDedupingRoutine works as expected in a realistic scenario.
// Tests that it merges the diffent set of similar profiles independently and
// that the resulting profiles have the right values, has no effect on the other
// profiles and that the data of verified profiles is not modified.
TEST_F(PersonalDataManagerCleanerTest, ApplyDedupingRoutine_MultipleDedupes) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeature(features::kAutofillEnableProfileDeduplication);

  // Create a Homer home profile with a higher ranking score than other Homer
  // profiles.
  AutofillProfile Homer1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&Homer1, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "", "742. Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "");
  Homer1.set_use_count(10);
  Homer1.set_use_date(AutofillClock::Now() - base::Days(1));

  // Create a Homer home profile with a medium ranking score compared to other
  // Homer profiles.
  AutofillProfile Homer2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&Homer2, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "", "742 Evergreen Terrace", "",
                       "Springfield", "IL", "91601", "", "12345678910");
  Homer2.set_use_count(5);
  Homer2.set_use_date(AutofillClock::Now() - base::Days(3));

  // Create a Homer home profile with a lower ranking score than other Homer
  // profiles.
  AutofillProfile Homer3(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&Homer3, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "", "");
  Homer3.set_use_count(3);
  Homer3.set_use_date(AutofillClock::Now() - base::Days(5));

  // Create a Homer work profile (different address).
  AutofillProfile Homer4(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&Homer4, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "Fox", "12 Nuclear Plant.", "",
                       "Springfield", "IL", "91601", "US", "9876543");
  Homer4.set_use_count(3);
  Homer4.set_use_date(AutofillClock::Now() - base::Days(5));

  // Create a Marge profile with a lower ranking score that other Marge
  // profiles.
  AutofillProfile Marge1(base::GenerateGUID(), kSettingsOrigin);
  test::SetProfileInfo(&Marge1, "Marjorie", "J", "Simpson",
                       "marge.simpson@abc.com", "", "742 Evergreen Terrace", "",
                       "Springfield", "IL", "91601", "", "12345678910");
  Marge1.set_use_count(4);
  Marge1.set_use_date(AutofillClock::Now() - base::Days(3));

  // Create a verified Marge home profile with a lower ranking score that the
  // other Marge profile.
  AutofillProfile Marge2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&Marge2, "Marjorie", "Jacqueline", "Simpson",
                       "marge.simpson@abc.com", "", "742 Evergreen Terrace", "",
                       "Springfield", "IL", "91601", "", "12345678910");
  Marge2.set_use_count(2);
  Marge2.set_use_date(AutofillClock::Now() - base::Days(3));

  // Create a Barney profile (guest user).
  AutofillProfile Barney(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&Barney, "Barney", "", "Gumble", "barney.gumble@abc.com",
                       "ABC", "123 Other Street", "", "Springfield", "IL",
                       "91601", "", "");
  Barney.set_use_count(1);
  Barney.set_use_date(AutofillClock::Now() - base::Days(180));
  Barney.FinalizeAfterImport();

  AddProfileToPersonalDataManager(Homer1);
  AddProfileToPersonalDataManager(Homer2);
  AddProfileToPersonalDataManager(Homer3);
  AddProfileToPersonalDataManager(Homer4);
  AddProfileToPersonalDataManager(Marge1);
  AddProfileToPersonalDataManager(Marge2);
  AddProfileToPersonalDataManager(Barney);

  // Make sure the 7 profiles were saved;
  EXPECT_EQ(7U, personal_data_->GetProfiles().size());

  base::HistogramTester histogram_tester;

  // |Homer1| should get merged into |Homer2| which should then be merged into
  // |Homer3|. |Marge2| should be discarded in favor of |Marge1| which is
  // verified. |Homer4| and |Barney| should not be deduped at all.
  EXPECT_TRUE(personal_data_manager_cleaner_->ApplyDedupingRoutineForTesting());
  WaitForOnPersonalDataChanged();

  // Get the profiles, sorted by ranking score to have a deterministic order.
  std::vector<AutofillProfile*> profiles =
      personal_data_->GetProfilesToSuggest();

  // The 2 duplicates Homer home profiles with the higher ranking score and the
  // unverified Marge profile should have been deduped.
  ASSERT_EQ(4U, profiles.size());
  // 7 profiles were considered for dedupe.
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesConsideredForDedupe", 7, 1);
  // 3 profile were removed (|Homer1|, |Homer2| and |Marge2|).
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesRemovedDuringDedupe", 3, 1);

  // The remaining profiles should be |Homer3|, |Marge1|, |Homer4| and |Barney|
  // in this order of ranking score.
  EXPECT_EQ(Homer3.guid(), profiles[0]->guid());
  EXPECT_EQ(Marge1.guid(), profiles[1]->guid());
  EXPECT_EQ(Homer4.guid(), profiles[2]->guid());
  EXPECT_EQ(Barney.guid(), profiles[3]->guid());

  // |Homer3|'s data:
  // The address should be saved with the syntax of |Homer1| since it has the
  // highest ranking score.
  EXPECT_EQ(u"742. Evergreen Terrace",
            profiles[0]->GetRawInfo(ADDRESS_HOME_LINE1));
  // The middle name should be the full version found in |Homer2|,
  EXPECT_EQ(u"Jay", profiles[0]->GetRawInfo(NAME_MIDDLE));
  // The phone number from |Homer2| should be kept (no loss of information).
  EXPECT_EQ(u"12345678910", profiles[0]->GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  // The company name from |Homer3| should be kept (no loss of information).
  EXPECT_EQ(u"Fox", profiles[0]->GetRawInfo(COMPANY_NAME));
  // The country from |Homer1| profile should be kept (no loss of information).
  EXPECT_EQ(u"US", profiles[0]->GetRawInfo(ADDRESS_HOME_COUNTRY));
  // The use count that results from the merge should be the max of Homer 1, 2
  // and 3's respective use counts.
  EXPECT_EQ(10U, profiles[0]->use_count());
  // The use date that results from the merge should be the one from the
  // |Homer1| since it was the most recently used profile.
  EXPECT_LT(Homer1.use_date() - base::Seconds(5), profiles[0]->use_date());
  EXPECT_GT(Homer1.use_date() + base::Seconds(5), profiles[0]->use_date());

  // The other profiles should not have been modified.
  EXPECT_TRUE(Marge1 == *profiles[1]);
  EXPECT_TRUE(Homer4 == *profiles[2]);
  EXPECT_TRUE(Barney == *profiles[3]);
}

TEST_F(PersonalDataManagerCleanerTest, ApplyDedupingRoutine_NopIfZeroProfiles) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeature(features::kAutofillEnableProfileDeduplication);
  EXPECT_TRUE(personal_data_->GetProfiles().empty());
  EXPECT_FALSE(
      personal_data_manager_cleaner_->ApplyDedupingRoutineForTesting());
}

TEST_F(PersonalDataManagerCleanerTest, ApplyDedupingRoutine_NopIfOneProfile) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeature(features::kAutofillEnableProfileDeduplication);

  // Create a profile to dedupe.
  AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "", "742. Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "");

  AddProfileToPersonalDataManager(profile);

  EXPECT_EQ(1U, personal_data_->GetProfiles().size());
  EXPECT_FALSE(
      personal_data_manager_cleaner_->ApplyDedupingRoutineForTesting());
}

// Tests that ApplyDedupingRoutine is not run a second time on the same major
// version.
TEST_F(PersonalDataManagerCleanerTest, ApplyDedupingRoutine_OncePerVersion) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeature(features::kAutofillEnableProfileDeduplication);

  // Create a profile to dedupe.
  AutofillProfile profile1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "", "742. Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "");

  // Create a similar profile.
  AutofillProfile profile2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "", "");

  AddProfileToPersonalDataManager(profile1);
  AddProfileToPersonalDataManager(profile2);

  EXPECT_EQ(2U, personal_data_->GetProfiles().size());

  // The deduping routine should be run a first time.
  EXPECT_TRUE(personal_data_manager_cleaner_->ApplyDedupingRoutineForTesting());
  WaitForOnPersonalDataChanged();

  std::vector<AutofillProfile*> profiles = personal_data_->GetProfiles();

  // The profiles should have been deduped
  EXPECT_EQ(1U, profiles.size());

  // Add another duplicate profile.
  AutofillProfile profile3(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "", "");

  AddProfileToPersonalDataManager(profile3);

  // Make sure |profile3| was saved.
  EXPECT_EQ(2U, personal_data_->GetProfiles().size());

  // The deduping routine should not be run.
  EXPECT_FALSE(
      personal_data_manager_cleaner_->ApplyDedupingRoutineForTesting());

  // The two duplicate profiles should still be present.
  EXPECT_EQ(2U, personal_data_->GetProfiles().size());
}

// Tests that settings-inaccessible profile values are removed from every stored
// profile on startup.
TEST_F(PersonalDataManagerCleanerTest,
       RemoveInaccessibleProfileValuesOnStartup) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeature(
      features::kAutofillRemoveInaccessibleProfileValuesOnStartup);

  // Add a German and a US profile.
  AutofillProfile profile0(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile0, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "DE", "12345678910");
  AutofillProfile profile1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "903 Apple Ct.", nullptr,
                       "Orlando", "FL", "32801", "US", "19482937549");
  AddProfileToPersonalDataManager(profile0);
  AddProfileToPersonalDataManager(profile1);

  personal_data_manager_cleaner_->RemoveInaccessibleProfileValuesForTesting();
  WaitForOnPersonalDataChanged();

  // profile0 should have it's state removed, while the US profile should remain
  // unchanged.
  profile0.SetRawInfo(ADDRESS_HOME_STATE, u"");
  std::vector<AutofillProfile*> expected_profiles = {&profile0, &profile1};
  EXPECT_THAT(personal_data_->GetProfiles(),
              HasSameElements(expected_profiles));
}

// Tests that DeleteDisusedAddresses only deletes the addresses that are
// supposed to be deleted.
TEST_F(PersonalDataManagerCleanerTest,
       DeleteDisusedAddresses_DeleteDesiredAddressesOnly) {
  auto now = AutofillClock::Now();

  // Create unverified/disused/not-used-by-valid-credit-card
  // address(deletable).
  AutofillProfile profile0(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile0, "Alice", "", "Delete", "", "ACME",
                       "1234 Evergreen Terrace", "Bld. 6", "Springfield", "IL",
                       "32801", "US", "15151231234");
  profile0.set_use_date(now - base::Days(400));
  AddProfileToPersonalDataManager(profile0);

  // Create unverified/disused/used-by-expired-credit-card address(deletable).
  AutofillProfile profile1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Bob", "", "Delete", "", "ACME",
                       "1234 Evergreen Terrace", "Bld. 7", "Springfield", "IL",
                       "32801", "US", "15151231234");
  profile1.set_use_date(now - base::Days(400));
  CreditCard credit_card0(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "Bob",
                          "5105105105105100" /* Mastercard */, "04", "1999",
                          "1");
  credit_card0.set_use_date(now - base::Days(400));
  credit_card0.set_billing_address_id(profile1.guid());
  AddProfileToPersonalDataManager(profile1);
  personal_data_->AddCreditCard(credit_card0);
  WaitForOnPersonalDataChanged();
  // Create verified/disused/not-used-by-valid-credit-card address(not
  // deletable).
  AutofillProfile profile2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Charlie", "", "Keep", "", "ACME",
                       "1234 Evergreen Terrace", "Bld. 8", "Springfield", "IL",
                       "32801", "US", "15151231234");
  profile2.set_origin(kSettingsOrigin);
  profile2.set_use_date(now - base::Days(400));
  AddProfileToPersonalDataManager(profile2);

  // Create unverified/recently-used/not-used-by-valid-credit-card address(not
  // deletable).
  AutofillProfile profile3(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Dave", "", "Keep", "", "ACME",
                       "1234 Evergreen Terrace", "Bld. 9", "Springfield", "IL",
                       "32801", "US", "15151231234");
  profile3.set_use_date(now - base::Days(4));
  AddProfileToPersonalDataManager(profile3);

  // Create unverified/disused/used-by-valid-credit-card address(not deletable).
  AutofillProfile profile4(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "Emma", "", "Keep", "", "ACME",
                       "1234 Evergreen Terrace", "Bld. 10", "Springfield", "IL",
                       "32801", "US", "15151231234");
  profile4.set_use_date(now - base::Days(400));
  CreditCard credit_card1(CreditCard::MASKED_SERVER_CARD, "c987");
  test::SetCreditCardInfo(&credit_card1, "Emma", "6543", "01", "2999", "1");
  credit_card1.SetNetworkForMaskedCard(kVisaCard);
  credit_card1.set_billing_address_id(profile4.guid());
  credit_card1.set_use_date(now - base::Days(1));
  AddProfileToPersonalDataManager(profile4);
  personal_data_->AddCreditCard(credit_card1);

  WaitForOnPersonalDataChanged();

  EXPECT_EQ(5U, personal_data_->GetProfiles().size());
  EXPECT_EQ(2U, personal_data_->GetCreditCards().size());

  // DeleteDisusedAddresses should return true.
  EXPECT_TRUE(
      personal_data_manager_cleaner_->DeleteDisusedAddressesForTesting());
  WaitForOnPersonalDataChanged();

  EXPECT_EQ(3U, personal_data_->GetProfiles().size());
  EXPECT_EQ(2U, personal_data_->GetCreditCards().size());
  EXPECT_EQ(u"Keep", personal_data_->GetProfiles()[0]->GetRawInfo(NAME_LAST));
  EXPECT_EQ(u"Keep", personal_data_->GetProfiles()[1]->GetRawInfo(NAME_LAST));
  EXPECT_EQ(u"Keep", personal_data_->GetProfiles()[2]->GetRawInfo(NAME_LAST));
}

// Tests that DeleteDisusedCreditCards deletes desired credit cards only.
TEST_F(PersonalDataManagerCleanerTest,
       DeleteDisusedCreditCards_OnlyDeleteExpiredDisusedLocalCards) {
  const char kHistogramName[] = "Autofill.CreditCardsDeletedForDisuse";
  auto now = AutofillClock::Now();

  // Create a recently used local card, it is expected to remain.
  CreditCard credit_card1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card1, "Alice",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  credit_card1.set_use_date(now - base::Days(4));

  // Create a local card that was expired 400 days ago, but recently used.
  // It is expected to remain.
  CreditCard credit_card2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card2, "Bob",
                          "378282246310006" /* American Express */, "04",
                          "1999", "1");
  credit_card2.set_use_date(now - base::Days(4));

  // Create a local card expired recently, and last used 400 days ago.
  // It is expected to remain.
  CreditCard credit_card3(base::GenerateGUID(), test::kEmptyOrigin);
  base::Time expiry_date = now - base::Days(32);
  base::Time::Exploded exploded;
  expiry_date.UTCExplode(&exploded);
  test::SetCreditCardInfo(&credit_card3, "Clyde", "4111111111111111" /* Visa */,
                          base::StringPrintf("%02d", exploded.month).c_str(),
                          base::StringPrintf("%04d", exploded.year).c_str(),
                          "1");
  credit_card3.set_use_date(now - base::Days(400));

  // Create a local card expired 400 days ago, and last used 400 days ago.
  // It is expected to be deleted.
  CreditCard credit_card4(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card4, "David",
                          "5105105105105100" /* Mastercard */, "04", "1999",
                          "1");
  credit_card4.set_use_date(now - base::Days(400));
  personal_data_->AddCreditCard(credit_card1);
  personal_data_->AddCreditCard(credit_card2);
  personal_data_->AddCreditCard(credit_card3);
  personal_data_->AddCreditCard(credit_card4);

  // Create a unmasked server card expired 400 days ago, and last used 400
  // days ago.
  // It is expected to remain because we do not delete server cards.
  CreditCard credit_card5(CreditCard::FULL_SERVER_CARD, "c789");
  test::SetCreditCardInfo(&credit_card5, "Emma", "4234567890123456" /* Visa */,
                          "04", "1999", "1");
  credit_card5.set_use_date(now - base::Days(400));

  // Create masked server card expired 400 days ago, and last used 400 days ago.
  // It is expected to remain because we do not delete server cards.
  CreditCard credit_card6(CreditCard::MASKED_SERVER_CARD, "c987");
  test::SetCreditCardInfo(&credit_card6, "Frank", "6543", "01", "1998", "1");
  credit_card6.set_use_date(now - base::Days(400));
  credit_card6.SetNetworkForMaskedCard(kVisaCard);

  // Save the server cards and set used_date to desired dates.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(credit_card5);
  server_cards.push_back(credit_card6);
  SetServerCards(server_cards);
  personal_data_->UpdateServerCardsMetadata({credit_card5, credit_card6});

  WaitForOnPersonalDataChanged();
  EXPECT_EQ(6U, personal_data_->GetCreditCards().size());

  // Setup histograms capturing.
  base::HistogramTester histogram_tester;

  // DeleteDisusedCreditCards should return true to indicate it was run.
  EXPECT_TRUE(
      personal_data_manager_cleaner_->DeleteDisusedCreditCardsForTesting());

  // Wait for the data to be refreshed.
  WaitForOnPersonalDataChanged();

  EXPECT_EQ(5U, personal_data_->GetCreditCards().size());
  std::unordered_set<std::u16string> expectedToRemain = {
      u"Alice", u"Bob", u"Clyde", u"Emma", u"Frank"};
  for (auto* card : personal_data_->GetCreditCards()) {
    EXPECT_NE(expectedToRemain.end(),
              expectedToRemain.find(card->GetRawInfo(CREDIT_CARD_NAME_FULL)));
  }

  // Verify histograms are logged.
  histogram_tester.ExpectTotalCount(kHistogramName, 1);
  histogram_tester.ExpectBucketCount(kHistogramName, 1, 1);
}

// Tests that all the non settings origins of autofill profiles are cleared but
// that the settings origins are untouched.
TEST_F(PersonalDataManagerCleanerTest, ClearProfileNonSettingsOrigins) {
  // Create three profile with a nonsettings, non-empty origin.
  AutofillProfile profile0(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile0, "Marion0", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile0.set_use_count(10000);
  AddProfileToPersonalDataManager(profile0);

  AutofillProfile profile1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Marion1", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile1.set_use_count(1000);
  AddProfileToPersonalDataManager(profile1);

  AutofillProfile profile2(base::GenerateGUID(), "1234");
  test::SetProfileInfo(&profile2, "Marion2", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile2.set_use_count(100);
  AddProfileToPersonalDataManager(profile2);

  // Create a profile with a settings origin.
  AutofillProfile profile3(base::GenerateGUID(), kSettingsOrigin);
  test::SetProfileInfo(&profile3, "Marion3", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile3.set_use_count(10);
  AddProfileToPersonalDataManager(profile3);

  ASSERT_EQ(4U, personal_data_->GetProfiles().size());

  base::RunLoop run_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
      .WillRepeatedly(QuitMessageLoop(&run_loop));
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .Times(2);  // The setting of profiles 0 and 2 will be cleared.

  personal_data_manager_cleaner_->ClearProfileNonSettingsOriginsForTesting();
  run_loop.Run();

  ASSERT_EQ(4U, personal_data_->GetProfiles().size());

  // The first three profiles' origin should be cleared and the fourth one still
  // be the settings origin.
  EXPECT_TRUE(personal_data_->GetProfilesToSuggest()[0]->origin().empty());
  EXPECT_TRUE(personal_data_->GetProfilesToSuggest()[1]->origin().empty());
  EXPECT_TRUE(personal_data_->GetProfilesToSuggest()[2]->origin().empty());
  EXPECT_EQ(kSettingsOrigin,
            personal_data_->GetProfilesToSuggest()[3]->origin());
}

// Tests that all the non settings origins of autofill credit cards are cleared
// but that the settings origins are untouched.
TEST_F(PersonalDataManagerCleanerTest, ClearCreditCardNonSettingsOrigins) {
  // Create three cards with a non settings origin.
  CreditCard credit_card0(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&credit_card0, "Bob0",
                          "5105105105105100" /* Mastercard */, "04", "1999",
                          "1");
  credit_card0.set_use_count(10000);
  personal_data_->AddCreditCard(credit_card0);

  CreditCard credit_card1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card1, "Bob1",
                          "5105105105105101" /* Mastercard */, "04", "1999",
                          "1");
  credit_card1.set_use_count(1000);
  personal_data_->AddCreditCard(credit_card1);

  CreditCard credit_card2(base::GenerateGUID(), "1234");
  test::SetCreditCardInfo(&credit_card2, "Bob2",
                          "5105105105105102" /* Mastercard */, "04", "1999",
                          "1");
  credit_card2.set_use_count(100);
  personal_data_->AddCreditCard(credit_card2);

  // Create a card with a settings origin.
  CreditCard credit_card3(base::GenerateGUID(), kSettingsOrigin);
  test::SetCreditCardInfo(&credit_card3, "Bob3",
                          "5105105105105103" /* Mastercard */, "04", "1999",
                          "1");
  credit_card3.set_use_count(10);
  personal_data_->AddCreditCard(credit_card3);

  WaitForOnPersonalDataChanged();
  ASSERT_EQ(4U, personal_data_->GetCreditCards().size());

  personal_data_manager_cleaner_->ClearCreditCardNonSettingsOriginsForTesting();

  WaitForOnPersonalDataChanged();
  ASSERT_EQ(4U, personal_data_->GetCreditCards().size());

  // The first three profiles' origin should be cleared and the fourth one still
  // be the settings origin.
  EXPECT_TRUE(
      personal_data_->GetCreditCardsToSuggest(false)[0]->origin().empty());
  EXPECT_TRUE(
      personal_data_->GetCreditCardsToSuggest(false)[1]->origin().empty());
  EXPECT_TRUE(
      personal_data_->GetCreditCardsToSuggest(false)[2]->origin().empty());
  EXPECT_EQ(kSettingsOrigin,
            personal_data_->GetCreditCardsToSuggest(false)[3]->origin());
}

}  // namespace autofill
