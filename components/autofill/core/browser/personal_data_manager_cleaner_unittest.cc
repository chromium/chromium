// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/personal_data_manager_cleaner.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_test_base.h"
#include "components/autofill/core/browser/test_utils/test_profiles.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Matcher;
using testing::Pointee;
using testing::Truly;
using testing::UnorderedElementsAre;

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
    ResetPersonalDataManager(
        /*use_sync_transport_mode=*/false, personal_data_.get());
    personal_data_manager_cleaner_ =
        std::make_unique<PersonalDataManagerCleaner>(personal_data_.get(),
                                                     nullptr, prefs_.get());
  }

  void TearDown() override {
    personal_data_manager_cleaner_.reset();
    if (personal_data_)
      personal_data_->Shutdown();
    personal_data_.reset();
    TearDownTest();
  }

  // Runs the deduplication routine on a set of `profiles` and returns the
  // result. For simplicity, this function skips updating the `personal_data_`
  // and just operates on the `profiles`.
  std::vector<AutofillProfile> DeduplicateProfiles(
      const std::vector<AutofillProfile>& profiles) {
    // `DedupeProfilesForTesting()` takes a vector of unique_ptrs. This
    // function's interface uses regular AutofillProfiles instead, since this
    // simplifies testing. So convert back and forth.
    std::vector<std::unique_ptr<AutofillProfile>> profile_ptrs;
    for (const AutofillProfile& profile : profiles) {
      profile_ptrs.push_back(std::make_unique<AutofillProfile>(profile));
    }
    std::unordered_set<std::string> profiles_to_delete;
    std::unordered_map<std::string, std::string> guids_merge_map;
    personal_data_manager_cleaner_->DedupeProfilesForTesting(
        &profile_ptrs, &profiles_to_delete, &guids_merge_map);
    // Convert back and remove all `profiles_to_delete`, since
    // `DedupeProfilesForTesting()` doesn't modify `profile_ptrs`.
    std::vector<AutofillProfile> deduped_profiles;
    for (const std::unique_ptr<AutofillProfile>& profile : profile_ptrs) {
      deduped_profiles.push_back(*profile);
    }
    base::EraseIf(deduped_profiles, [&](const AutofillProfile& profile) {
      return profiles_to_delete.contains(profile.guid());
    });
    return deduped_profiles;
  }

 protected:
  void AddProfileToPersonalDataManager(const AutofillProfile& profile) {
    PersonalDataProfileTaskWaiter waiter(*personal_data_);
    EXPECT_CALL(waiter.mock_observer(), OnPersonalDataChanged()).Times(1);
    personal_data_->AddProfile(profile);
    waiter.Wait();
  }

  void SetServerCards(const std::vector<CreditCard>& server_cards) {
    test::SetServerCreditCards(
        personal_data_->IsSyncFeatureEnabledForPaymentsServerMetrics()
            ? profile_autofill_table_.get()
            : account_autofill_table_.get(),
        server_cards);
  }

  std::unique_ptr<PersonalDataManager> personal_data_;
  std::unique_ptr<PersonalDataManagerCleaner> personal_data_manager_cleaner_;
};

// Tests that DedupeProfiles sets the correct profile guids to
// delete after merging similar profiles, and correct merge mappings for billing
// address id references.
TEST_F(PersonalDataManagerCleanerTest,
       DedupeProfiles_ProfilesToDelete_GuidsMergeMap) {
  // Create the profile for which to find duplicates. It has the highest
  // ranking score.
  AutofillProfile* profile1 = new AutofillProfile();
  test::SetProfileInfo(profile1, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "", "742. Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "12345678910");
  profile1->set_use_count(9);

  // Create a different profile that should not be deduped (different address).
  AutofillProfile* profile2 = new AutofillProfile();
  test::SetProfileInfo(profile2, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "Fox", "1234 Other Street", "",
                       "Springfield", "IL", "91601", "US", "12345678910");
  profile2->set_use_count(7);

  // Create a profile similar to profile1 which should be deduped.
  AutofillProfile* profile3 = new AutofillProfile();
  test::SetProfileInfo(profile3, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "", "742 Evergreen Terrace", "",
                       "Springfield", "IL", "91601", "US", "12345678910");
  profile3->set_use_count(5);

  // Create another different profile that should not be deduped (different
  // name).
  AutofillProfile* profile4 = new AutofillProfile();
  test::SetProfileInfo(profile4, "Marjorie", "Jacqueline", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "12345678910");
  profile4->set_use_count(3);

  // Create another profile similar to profile1. Since that one has the lowest
  // ranking score, the result of the merge should be in this profile at the end
  // of the test.
  AutofillProfile* profile5 = new AutofillProfile();
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

  // Profile 1 was merged into profile 3 and profile 3 into profile 5.
  std::unordered_map<std::string, std::string> expected_guids_to_merge = {
      {profile1->guid(), profile3->guid()},
      {profile3->guid(), profile5->guid()}};
  EXPECT_EQ(guids_merge_map, expected_guids_to_merge);

  // Profile1 should be deleted because it was sent as the profile to merge and
  // thus was merged into profile3 and then into profile5.
  // Profile3 should be deleted because profile1 was merged into it and the
  // resulting profile was then merged into profile5.
  EXPECT_THAT(profiles_to_delete, testing::UnorderedElementsAre(
                                      profile1->guid(), profile3->guid()));

  // All profiles should still be present in |existing_profiles|.
  EXPECT_EQ(5U, existing_profiles.size());
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
  CreditCard* credit_card0 = new CreditCard(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);

  // Create cards that use A, D, E and F as their billing address id.
  CreditCard* credit_card1 = new CreditCard(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  credit_card1->set_billing_address_id("A");
  CreditCard* credit_card2 = new CreditCard(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  credit_card2->set_billing_address_id("D");
  CreditCard* credit_card3 = new CreditCard(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  credit_card3->set_billing_address_id("E");
  CreditCard* credit_card4 = new CreditCard(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
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
  // A set of 6 profiles will be created. They should merge in this way:
  //  1 -> 2 -> 3
  //  4 -> 5
  //  6
  // Set their frencency score so that profile 3 has a higher score than 5, and
  // 5 has a higher score than 6. This will ensure a deterministic order when
  // verifying results.

  // Create a set of 3 profiles to be merged together.
  // Create a profile with a higher ranking score.
  AutofillProfile profile1;
  test::SetProfileInfo(&profile1, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "", "742. Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "");
  profile1.set_use_count(12);
  profile1.set_use_date(AutofillClock::Now() - base::Days(1));

  // Create a profile with a medium ranking score.
  AutofillProfile profile2;
  test::SetProfileInfo(&profile2, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "", "742 Evergreen Terrace", "",
                       "Springfield", "IL", "91601", "", "12345678910");
  profile2.set_use_count(5);
  profile2.set_use_date(AutofillClock::Now() - base::Days(3));

  // Create a profile with a lower ranking score.
  AutofillProfile profile3;
  test::SetProfileInfo(&profile3, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "", "");
  profile3.set_use_count(3);
  profile3.set_use_date(AutofillClock::Now() - base::Days(5));

  // Create a set of two profiles to be merged together.
  // Create a profile with a higher ranking score.
  AutofillProfile profile4;
  test::SetProfileInfo(&profile4, "Marge", "B", "Simpson",
                       "marge.simpson@abc.com", "", "742. Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "");
  profile4.set_use_count(11);
  profile4.set_use_date(AutofillClock::Now() - base::Days(1));

  // Create a profile with a lower ranking score.
  AutofillProfile profile5;
  test::SetProfileInfo(&profile5, "Marge", "B", "Simpson",
                       "marge.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "", "");
  profile5.set_use_count(5);
  profile5.set_use_date(AutofillClock::Now() - base::Days(3));

  // Create a unique profile.
  AutofillProfile profile6;
  test::SetProfileInfo(&profile6, "Bart", "J", "Simpson",
                       "bart.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "", "");
  profile6.set_use_count(10);
  profile6.set_use_date(AutofillClock::Now() - base::Days(1));

  // Add three credit cards. Give them a ranking score so that they are
  // suggested in order (1, 2, 3). This will ensure a deterministic order for
  // verifying results.
  CreditCard credit_card1(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card1, "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  credit_card1.set_use_count(10);

  CreditCard credit_card2(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card2, "John Dillinger",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  credit_card2.set_use_count(5);

  CreditCard credit_card3(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
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

  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  // Make sure the 6 profiles and 3 credit cards were saved.
  EXPECT_EQ(6U, personal_data_->GetProfiles().size());
  EXPECT_EQ(3U, personal_data_->GetCreditCards().size());

  EXPECT_TRUE(personal_data_manager_cleaner_->ApplyDedupingRoutineForTesting());
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  // Get the profiles and cards sorted by their ranking score to have a
  // deterministic order.
  std::vector<AutofillProfile*> profiles =
      personal_data_->GetProfilesToSuggest();
  std::vector<CreditCard*> credit_cards =
      personal_data_->GetCreditCardsToSuggest();

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
  // Create a profile with a higher ranking score.
  AutofillProfile profile1;
  test::SetProfileInfo(&profile1, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "", "742. Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "");
  profile1.set_use_count(10);
  profile1.set_use_date(AutofillClock::Now() - base::Days(1));

  // Create a profile with a medium ranking score.
  AutofillProfile profile2;
  test::SetProfileInfo(&profile2, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "", "742 Evergreen Terrace", "",
                       "Springfield", "IL", "91601", "", "12345678910");
  profile2.set_use_count(5);
  profile2.set_use_date(AutofillClock::Now() - base::Days(3));

  // Create a profile with a lower ranking score.
  AutofillProfile profile3;
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
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

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

// Tests that ApplyDedupingRoutine works as expected in a realistic scenario.
// Tests that it merges the different set of similar profiles independently and
// that the resulting profiles have the right values. It has no effect on the
// other profiles.
TEST_F(PersonalDataManagerCleanerTest, ApplyDedupingRoutine_MultipleDedupes) {
  // Create a Homer home profile with a higher ranking score than other Homer
  // profiles.
  AutofillProfile Homer1;
  test::SetProfileInfo(&Homer1, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "", "742. Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "");
  Homer1.set_use_count(10);
  Homer1.set_use_date(AutofillClock::Now() - base::Days(1));

  // Create a Homer home profile with a medium ranking score compared to other
  // Homer profiles.
  AutofillProfile Homer2;
  test::SetProfileInfo(&Homer2, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "", "742 Evergreen Terrace", "",
                       "Springfield", "IL", "91601", "", "12345678910");
  Homer2.set_use_count(5);
  Homer2.set_use_date(AutofillClock::Now() - base::Days(3));

  // Create a Homer home profile with a lower ranking score than other Homer
  // profiles.
  AutofillProfile Homer3;
  test::SetProfileInfo(&Homer3, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "", "");
  Homer3.set_use_count(3);
  Homer3.set_use_date(AutofillClock::Now() - base::Days(5));

  // Create a Homer work profile (different address).
  AutofillProfile Homer4;
  test::SetProfileInfo(&Homer4, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "Fox", "12 Nuclear Plant.", "",
                       "Springfield", "IL", "91601", "US", "9876543");
  Homer4.set_use_count(3);
  Homer4.set_use_date(AutofillClock::Now() - base::Days(5));

  // Create a Barney profile (guest user).
  AutofillProfile Barney;
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
  AddProfileToPersonalDataManager(Barney);

  // Make sure the 5 profiles were saved;
  EXPECT_EQ(5U, personal_data_->GetProfiles().size());

  base::HistogramTester histogram_tester;

  // |Homer1| should get merged into |Homer2| which should then be merged into
  // |Homer3|. |Homer4| and |Barney| should not be deduped at all.
  EXPECT_TRUE(personal_data_manager_cleaner_->ApplyDedupingRoutineForTesting());
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  // Get the profiles, sorted by ranking score to have a deterministic order.
  std::vector<AutofillProfile*> profiles =
      personal_data_->GetProfilesToSuggest();

  // The 2 duplicates Homer home profiles with the higher ranking score  should
  // have been deduped.
  ASSERT_EQ(3U, profiles.size());
  // 5 profiles were considered for dedupe.
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesConsideredForDedupe", 5, 1);
  // 2 profile were removed (|Homer1|, |Homer2|).
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfProfilesRemovedDuringDedupe", 2, 1);

  // The remaining profiles should be |Homer3|, |Homer4| and |Barney| in this
  // order of ranking score.
  EXPECT_EQ(Homer3.guid(), profiles[0]->guid());
  EXPECT_EQ(Homer4.guid(), profiles[1]->guid());
  EXPECT_EQ(Barney.guid(), profiles[2]->guid());

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
  EXPECT_TRUE(Homer4 == *profiles[1]);
  EXPECT_TRUE(Barney == *profiles[2]);
}

TEST_F(PersonalDataManagerCleanerTest, ApplyDedupingRoutine_NopIfZeroProfiles) {
  EXPECT_TRUE(personal_data_->GetProfiles().empty());
  EXPECT_FALSE(
      personal_data_manager_cleaner_->ApplyDedupingRoutineForTesting());
}

TEST_F(PersonalDataManagerCleanerTest, ApplyDedupingRoutine_NopIfOneProfile) {
  // Create a profile to dedupe.
  AutofillProfile profile;
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
  // Create a profile to dedupe.
  AutofillProfile profile1;
  test::SetProfileInfo(&profile1, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "", "742. Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "");

  // Create a similar profile.
  AutofillProfile profile2;
  test::SetProfileInfo(&profile2, "Homer", "J", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace.",
                       "", "Springfield", "IL", "91601", "", "");

  AddProfileToPersonalDataManager(profile1);
  AddProfileToPersonalDataManager(profile2);

  EXPECT_EQ(2U, personal_data_->GetProfiles().size());

  // The deduping routine should be run a first time.
  EXPECT_TRUE(personal_data_manager_cleaner_->ApplyDedupingRoutineForTesting());
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  std::vector<AutofillProfile*> profiles = personal_data_->GetProfiles();

  // The profiles should have been deduped
  EXPECT_EQ(1U, profiles.size());

  // Add another duplicate profile.
  AutofillProfile profile3;
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

// Tests that `kAccount` profiles are not deduplicated against each other.
TEST_F(PersonalDataManagerCleanerTest, Deduplicate_kAccountPairs) {
  base::test::ScopedFeatureList features(
      features::kAutofillAccountProfileStorage);
  AutofillProfile account_profile1 = test::StandardProfile();
  account_profile1.set_source_for_testing(AutofillProfile::Source::kAccount);
  AutofillProfile account_profile2 = test::StandardProfile();
  account_profile2.set_source_for_testing(AutofillProfile::Source::kAccount);
  EXPECT_THAT(
      DeduplicateProfiles({account_profile1, account_profile2}),
      testing::UnorderedElementsAre(account_profile1, account_profile2));
}

// Tests that `kLocalOrSyncable` profiles which are a subset of a `kAccount`
// profile are deduplicated. The result is a Chrome account profile.
TEST_F(PersonalDataManagerCleanerTest, Deduplicate_kAccountSuperset) {
  base::test::ScopedFeatureList features(
      features::kAutofillAccountProfileStorage);
  // Create a non-Chrome account profile and a local profile.
  AutofillProfile account_profile = test::StandardProfile();
  const int non_chrome_service =
      AutofillProfile::kInitialCreatorOrModifierChrome + 1;
  account_profile.set_initial_creator_id(non_chrome_service);
  account_profile.set_last_modifier_id(non_chrome_service);
  account_profile.set_source_for_testing(AutofillProfile::Source::kAccount);
  AutofillProfile local_profile = test::SubsetOfStandardProfile();

  // Expect that only the account profile remains and that it became a Chrome-
  // originating profile.
  std::vector<AutofillProfile> deduped_profiles =
      DeduplicateProfiles({account_profile, local_profile});
  EXPECT_THAT(deduped_profiles, testing::UnorderedElementsAre(account_profile));
  EXPECT_EQ(deduped_profiles[0].initial_creator_id(),
            AutofillProfile::kInitialCreatorOrModifierChrome);
  EXPECT_EQ(deduped_profiles[0].last_modifier_id(),
            AutofillProfile::kInitialCreatorOrModifierChrome);
}

// Tests that `kAccount` profiles which are a subset of a `kLocalOrSyncable`
// profile are not deduplicated.
TEST_F(PersonalDataManagerCleanerTest, Deduplicate_kAccountSubset) {
  base::test::ScopedFeatureList features(
      features::kAutofillAccountProfileStorage);
  AutofillProfile account_profile = test::SubsetOfStandardProfile();
  account_profile.set_source_for_testing(AutofillProfile::Source::kAccount);
  AutofillProfile local_profile = test::StandardProfile();
  EXPECT_THAT(DeduplicateProfiles({account_profile, local_profile}),
              testing::UnorderedElementsAre(account_profile, local_profile));
}

// Tests that settings-inaccessible profile values are removed from every stored
// profile on startup.
TEST_F(PersonalDataManagerCleanerTest,
       RemoveInaccessibleProfileValuesOnStartup) {
  base::test::ScopedFeatureList features(
      features::kAutofillRemoveInaccessibleProfileValuesOnStartup);

  // Add a German and a US profile.
  AutofillProfile profile0;
  test::SetProfileInfo(&profile0, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "DE", "12345678910");
  AutofillProfile profile1;
  test::SetProfileInfo(&profile1, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "903 Apple Ct.", nullptr,
                       "Orlando", "FL", "32801", "US", "19482937549");
  AddProfileToPersonalDataManager(profile0);
  AddProfileToPersonalDataManager(profile1);

  personal_data_manager_cleaner_->RemoveInaccessibleProfileValuesForTesting();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

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

  // Create a disused, not-used-by-valid-credit-card address (deletable).
  AutofillProfile profile0 = test::GetFullProfile();
  profile0.set_use_date(now - base::Days(400));
  AddProfileToPersonalDataManager(profile0);

  // Create a disused, used-by-expired-credit-card address (deletable).
  AutofillProfile profile1 = test::GetFullProfile2();
  profile1.set_use_date(now - base::Days(400));
  CreditCard credit_card0 = test::GetCreditCard();
  credit_card0.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2001");
  credit_card0.set_use_date(now - base::Days(400));
  credit_card0.set_billing_address_id(profile1.guid());
  AddProfileToPersonalDataManager(profile1);
  personal_data_->AddCreditCard(credit_card0);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  // Create a recently-used, not-used-by-valid-credit-card address (not
  // deletable).
  AutofillProfile profile2 = test::GetFullCanadianProfile();
  profile2.set_use_date(now - base::Days(4));
  AddProfileToPersonalDataManager(profile2);

  // Create a disused, used-by-valid-credit-card address (not deletable).
  // (The fact that it's incomplete doesn't matter. It only needs to be
  // different from the other profiles).
  AutofillProfile profile3 = test::GetIncompleteProfile1();
  profile3.set_use_date(now - base::Days(400));
  CreditCard credit_card1 = test::GetCreditCard2();
  credit_card1.set_billing_address_id(profile3.guid());
  AddProfileToPersonalDataManager(profile3);
  personal_data_->AddCreditCard(credit_card1);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  EXPECT_TRUE(
      personal_data_manager_cleaner_->DeleteDisusedAddressesForTesting());
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  EXPECT_THAT(personal_data_->GetProfiles(),
              UnorderedElementsAre(Pointee(profile2), Pointee(profile3)));
}

// Tests that DeleteDisusedCreditCards deletes desired credit cards only.
TEST_F(PersonalDataManagerCleanerTest,
       DeleteDisusedCreditCards_OnlyDeleteExpiredDisusedLocalCards) {
  const char kHistogramName[] = "Autofill.CreditCardsDeletedForDisuse";
  auto now = AutofillClock::Now();

  // Create a recently used local card, it is expected to remain.
  CreditCard credit_card1(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card1, "Alice",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  credit_card1.set_use_date(now - base::Days(4));

  // Create a local card that was expired 400 days ago, but recently used.
  // It is expected to remain.
  CreditCard credit_card2(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card2, "Bob",
                          "378282246310006" /* American Express */, "04",
                          "1999", "1");
  credit_card2.set_use_date(now - base::Days(4));

  // Create a local card expired recently, and last used 400 days ago.
  // It is expected to remain.
  CreditCard credit_card3(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
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
  CreditCard credit_card4(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
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

  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_EQ(6U, personal_data_->GetCreditCards().size());

  // Setup histograms capturing.
  base::HistogramTester histogram_tester;

  // DeleteDisusedCreditCards should return true to indicate it was run.
  EXPECT_TRUE(
      personal_data_manager_cleaner_->DeleteDisusedCreditCardsForTesting());

  // Wait for the data to be refreshed.
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

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

// Tests that all the non settings origins of autofill credit cards are cleared
// but that the settings origins are untouched.
TEST_F(PersonalDataManagerCleanerTest, ClearCreditCardNonSettingsOrigins) {
  // Create three cards with a non settings origin.
  CreditCard credit_card0(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          "https://www.example.com");
  test::SetCreditCardInfo(&credit_card0, "Bob0",
                          "5105105105105100" /* Mastercard */, "04", "1999",
                          "1");
  credit_card0.set_use_count(10000);
  personal_data_->AddCreditCard(credit_card0);

  CreditCard credit_card1(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card1, "Bob1",
                          "5105105105105101" /* Mastercard */, "04", "1999",
                          "1");
  credit_card1.set_use_count(1000);
  personal_data_->AddCreditCard(credit_card1);

  CreditCard credit_card2(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          "1234");
  test::SetCreditCardInfo(&credit_card2, "Bob2",
                          "5105105105105102" /* Mastercard */, "04", "1999",
                          "1");
  credit_card2.set_use_count(100);
  personal_data_->AddCreditCard(credit_card2);

  // Create a card with a settings origin.
  CreditCard credit_card3(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          kSettingsOrigin);
  test::SetCreditCardInfo(&credit_card3, "Bob3",
                          "5105105105105103" /* Mastercard */, "04", "1999",
                          "1");
  credit_card3.set_use_count(10);
  personal_data_->AddCreditCard(credit_card3);

  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  ASSERT_EQ(4U, personal_data_->GetCreditCards().size());

  personal_data_manager_cleaner_->ClearCreditCardNonSettingsOriginsForTesting();

  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  ASSERT_EQ(4U, personal_data_->GetCreditCards().size());

  // The first three profiles' origin should be cleared and the fourth one still
  // be the settings origin.
  EXPECT_TRUE(personal_data_->GetCreditCardsToSuggest()[0]->origin().empty());
  EXPECT_TRUE(personal_data_->GetCreditCardsToSuggest()[1]->origin().empty());
  EXPECT_TRUE(personal_data_->GetCreditCardsToSuggest()[2]->origin().empty());
  EXPECT_EQ(kSettingsOrigin,
            personal_data_->GetCreditCardsToSuggest()[3]->origin());
}

}  // namespace autofill
