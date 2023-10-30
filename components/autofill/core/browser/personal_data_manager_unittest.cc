// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/personal_data_manager.h"

#include <stddef.h>

#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/data_model/credit_card_art_image.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/mandatory_reauth_metrics.h"
#include "components/autofill/core/browser/personal_data_manager_test_base.h"
#include "components/autofill/core/browser/profile_token_quality_test_api.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/ui/label_formatter_utils.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/test_sync_service.h"
#include "components/version_info/version_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace autofill {

namespace {

using testing::ElementsAre;
using testing::Pointee;
using testing::UnorderedElementsAre;

constexpr char kGuid[] = "a21f010a-eac1-41fc-aee9-c06bbedfb292";
constexpr char kPrimaryAccountEmail[] = "syncuser@example.com";

const base::Time kArbitraryTime = base::Time::FromSecondsSinceUnixEpoch(25);
const base::Time kSomeLaterTime = base::Time::FromSecondsSinceUnixEpoch(1000);
const base::Time kMuchLaterTime = base::Time::FromSecondsSinceUnixEpoch(5000);

class PersonalDataManagerMock : public PersonalDataManager {
 public:
  explicit PersonalDataManagerMock(const std::string& app_locale,
                                   const std::string& variations_country_code)
      : PersonalDataManager(app_locale, variations_country_code) {}
  ~PersonalDataManagerMock() override = default;

  MOCK_METHOD(void,
              FetchImagesForURLs,
              ((base::span<const GURL> updated_urls)),
              (const override));
};

template <typename T>
bool CompareElements(T* a, T* b) {
  return a->Compare(*b) < 0;
}

template <typename T>
bool ElementsEqual(T* a, T* b) {
  return a->Compare(*b) == 0;
}

// Verifies that two vectors have the same elements (according to T::Compare)
// while ignoring order. This is useful because multiple profiles or credit
// cards that are added to the SQLite DB within the same second will be returned
// in GUID (aka random) order.
template <typename T>
void ExpectSameElements(const std::vector<T*>& expectations,
                        const std::vector<T*>& results) {
  ASSERT_EQ(expectations.size(), results.size());

  std::vector<T*> expectations_copy = expectations;
  std::sort(expectations_copy.begin(), expectations_copy.end(),
            CompareElements<T>);
  std::vector<T*> results_copy = results;
  std::sort(results_copy.begin(), results_copy.end(), CompareElements<T>);

  EXPECT_EQ(
      base::ranges::mismatch(results_copy, expectations_copy, ElementsEqual<T>)
          .first,
      results_copy.end());
}

}  // anonymous namespace

class PersonalDataManagerHelper : public PersonalDataManagerTestBase {
 protected:
  PersonalDataManagerHelper() = default;

  virtual ~PersonalDataManagerHelper() {
    if (personal_data_)
      personal_data_->Shutdown();
    personal_data_.reset();
  }

  void ResetPersonalDataManager(bool use_sync_transport_mode = false) {
    if (personal_data_)
      personal_data_->Shutdown();
    personal_data_ = std::make_unique<PersonalDataManager>("EN", "US");
    PersonalDataManagerTestBase::ResetPersonalDataManager(
        use_sync_transport_mode, personal_data_.get());
  }

  bool TurnOnSyncFeature() {
    return PersonalDataManagerTestBase::TurnOnSyncFeature(personal_data_.get());
  }

  void SetUpReferenceProfile(const AutofillProfile& profile) {
    ASSERT_EQ(0U, personal_data_->GetProfiles().size());

    AddProfileToPersonalDataManager(profile);

    ASSERT_EQ(1U, personal_data_->GetProfiles().size());
  }

  AutofillProfile GetDefaultProfile() {
    AutofillProfile profile;
    test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                         "johnwayne@me.xyz", "Fox", "123 Zoo St", "unit 5",
                         "Hollywood", "CA", "91601", "US", "12345678910",
                         false);

    return profile;
  }

  // Adds three local cards to the |personal_data_|. The three cards are
  // different: two are from different companies and the third doesn't have a
  // number. All three have different owners and credit card number. This allows
  // to test the suggestions based on name as well as on credit card number.
  void SetUpReferenceLocalCreditCards() {
    ASSERT_EQ(0U, personal_data_->GetCreditCards().size());

    CreditCard credit_card0("287151C8-6AB1-487C-9095-28E80BE5DA15",
                            test::kEmptyOrigin);
    test::SetCreditCardInfo(&credit_card0, "Clyde Barrow",
                            "378282246310005" /* American Express */, "04",
                            "2999", "1");
    credit_card0.set_use_count(3);
    credit_card0.set_use_date(AutofillClock::Now() - base::Days(1));
    personal_data_->AddCreditCard(credit_card0);

    CreditCard credit_card1("1141084B-72D7-4B73-90CF-3D6AC154673B",
                            test::kEmptyOrigin);
    credit_card1.set_use_count(300);
    credit_card1.set_use_date(AutofillClock::Now() - base::Days(10));
    test::SetCreditCardInfo(&credit_card1, "John Dillinger",
                            "4234567890123456" /* Visa */, "01", "2999", "1");
    personal_data_->AddCreditCard(credit_card1);

    CreditCard credit_card2("002149C1-EE28-4213-A3B9-DA243FFF021B",
                            test::kEmptyOrigin);
    credit_card2.set_use_count(1);
    credit_card2.set_use_date(AutofillClock::Now() - base::Days(1));
    test::SetCreditCardInfo(&credit_card2, "Bonnie Parker",
                            "5105105105105100" /* Mastercard */, "12", "2999",
                            "1");
    PersonalDataProfileTaskWaiter waiter(*personal_data_);
    EXPECT_CALL(waiter.mock_observer(), OnPersonalDataChanged());
    personal_data_->AddCreditCard(credit_card2);
    std::move(waiter).Wait();
    ASSERT_EQ(3U, personal_data_->GetCreditCards().size());
  }

  // Add 3 credit cards. One local, one masked, one full.
  void SetUpThreeCardTypes() {
    EXPECT_EQ(0U, personal_data_->GetCreditCards().size());
    CreditCard masked_server_card;
    test::SetCreditCardInfo(&masked_server_card, "Elvis Presley",
                            "4234567890123456",  // Visa
                            "04", "2999", "1");
    masked_server_card.set_guid("00000000-0000-0000-0000-000000000007");
    masked_server_card.set_record_type(CreditCard::RecordType::kFullServerCard);
    masked_server_card.set_server_id("masked_id");
    masked_server_card.set_use_count(15);
    {
      PersonalDataProfileTaskWaiter waiter(*personal_data_);
      EXPECT_CALL(waiter.mock_observer(), OnPersonalDataChanged());
      personal_data_->AddFullServerCreditCard(masked_server_card);
      std::move(waiter).Wait();
    }
    ASSERT_EQ(1U, personal_data_->GetCreditCards().size());

    personal_data_->ResetFullServerCard(
        personal_data_->GetCreditCards()[0]->guid());

    CreditCard full_server_card;
    test::SetCreditCardInfo(&full_server_card, "Buddy Holly",
                            "5187654321098765",  // Mastercard
                            "10", "2998", "1");
    full_server_card.set_guid("00000000-0000-0000-0000-000000000008");
    full_server_card.set_record_type(CreditCard::RecordType::kFullServerCard);
    full_server_card.set_server_id("full_id");
    full_server_card.set_use_count(10);
    personal_data_->AddFullServerCreditCard(full_server_card);

    CreditCard local_card;
    test::SetCreditCardInfo(&local_card, "Freddy Mercury",
                            "4234567890123463",  // Visa
                            "08", "2999", "1");
    local_card.set_guid("00000000-0000-0000-0000-000000000009");
    local_card.set_record_type(CreditCard::RecordType::kLocalCard);
    local_card.set_use_count(5);
    {
      PersonalDataProfileTaskWaiter waiter(*personal_data_);
      EXPECT_CALL(waiter.mock_observer(), OnPersonalDataChanged());
      personal_data_->AddCreditCard(local_card);
      std::move(waiter).Wait();
    }
    EXPECT_EQ(3U, personal_data_->GetCreditCards().size());
  }

  // Helper method to create a local card that was expired 400 days ago,
  // and has not been used in last 400 days. This card is supposed to be
  // deleted during a major version upgrade.
  void CreateDeletableExpiredAndDisusedCreditCard() {
    CreditCard credit_card1(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                            test::kEmptyOrigin);
    test::SetCreditCardInfo(&credit_card1, "Clyde Barrow",
                            "378282246310005" /* American Express */, "04",
                            "1999", "1");
    credit_card1.set_use_date(AutofillClock::Now() - base::Days(400));

    personal_data_->AddCreditCard(credit_card1);

    PersonalDataProfileTaskWaiter(*personal_data_).Wait();
    EXPECT_EQ(1U, personal_data_->GetCreditCards().size());
  }

  // Helper method to create a profile that was last used 400 days ago.
  // This profile is supposed to be deleted during a major version upgrade.
  void CreateDeletableDisusedProfile() {
    AutofillProfile profile0(test::GetFullProfile());
    profile0.set_use_date(AutofillClock::Now() - base::Days(400));
    AddProfileToPersonalDataManager(profile0);

    EXPECT_EQ(1U, personal_data_->GetProfiles().size());
  }

  AutofillTable* GetServerDataTable() {
    return personal_data_->IsSyncFeatureEnabledForPaymentsServerMetrics()
               ? profile_autofill_table_.get()
               : account_autofill_table_.get();
  }

  void AddProfileToPersonalDataManager(const AutofillProfile& profile) {
    PersonalDataProfileTaskWaiter waiter(*personal_data_);
    EXPECT_CALL(waiter.mock_observer(), OnPersonalDataChanged());
    personal_data_->AddProfile(profile);
    std::move(waiter).Wait();
  }

  void UpdateProfileOnPersonalDataManager(const AutofillProfile& profile) {
    PersonalDataProfileTaskWaiter waiter(*personal_data_);
    EXPECT_CALL(waiter.mock_observer(), OnPersonalDataChanged());
    personal_data_->UpdateProfile(profile);
    std::move(waiter).Wait();
  }

  void RemoveByGUIDFromPersonalDataManager(const std::string& guid) {
    PersonalDataProfileTaskWaiter waiter(*personal_data_);
    EXPECT_CALL(waiter.mock_observer(), OnPersonalDataChanged());
    personal_data_->RemoveByGUID(guid);
    std::move(waiter).Wait();
  }

  void SetServerCards(const std::vector<CreditCard>& server_cards) {
    test::SetServerCreditCards(GetServerDataTable(), server_cards);
  }

  void AddOfferDataForTest(AutofillOfferData offer_data) {
    personal_data_->AddOfferDataForTest(
        std::make_unique<AutofillOfferData>(offer_data));
  }

  void AddLocalIban(Iban& iban) {
    iban.set_identifier(Iban::Guid(personal_data_->AddIban(iban)));
    PersonalDataProfileTaskWaiter(*personal_data_).Wait();
    iban.set_record_type(Iban::kLocalIban);
  }

  std::unique_ptr<PersonalDataManager> personal_data_;
};

class PersonalDataManagerTest : public PersonalDataManagerHelper,
                                public testing::Test {
 protected:
  void SetUp() override {
    SetUpTest();
    ResetPersonalDataManager();
  }
  void TearDown() override { TearDownTest(); }
};

class PersonalDataManagerSyncTransportModeTest
    : public PersonalDataManagerHelper,
      public testing::Test {
 protected:
  void SetUp() override {
    SetUpTest();
    ResetPersonalDataManager(
        /*use_sync_transport_mode=*/true);
  }
  void TearDown() override { TearDownTest(); }
};

class PersonalDataManagerMockTest : public PersonalDataManagerTestBase,
                                    public testing::Test {
 protected:
  void SetUp() override {
    SetUpTest();
    ResetPersonalDataManager();
  }

  void TearDown() override {
    if (personal_data_) {
      personal_data_->Shutdown();
    }
    personal_data_.reset();
    TearDownTest();
  }

  void ResetPersonalDataManager() {
    if (personal_data_) {
      personal_data_->Shutdown();
    }
    personal_data_ =
        std::make_unique<PersonalDataManagerMock>("en", std::string());
    PersonalDataManagerTestBase::ResetPersonalDataManager(
        /*use_sync_transport_mode=*/true, personal_data_.get());
  }

  // Verifies the credit card art image fetching should begin.
  void WaitForFetchImagesForUrls() {
    base::RunLoop run_loop;
    EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
        .Times(testing::AnyNumber());
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
        .Times(testing::AnyNumber());
    EXPECT_CALL(*personal_data_, FetchImagesForURLs(testing::_))
        .Times(1)
        .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
    run_loop.Run();
  }

  std::unique_ptr<PersonalDataManagerMock> personal_data_;
};

TEST_F(PersonalDataManagerTest, AddProfile) {
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
  std::vector<AutofillProfile*> profiles;
  profiles.push_back(&profile0);
  profiles.push_back(&profile1);
  ExpectSameElements(profiles, personal_data_->GetProfiles());
}

TEST_F(PersonalDataManagerTest, UpdateProfile_ModificationDate) {
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
TEST_F(PersonalDataManagerTest, GetProfiles) {
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
TEST_F(PersonalDataManagerTest, GetProfiles_Order) {
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

// Tests that `GetProfilesForSettings()` orders by descending modification
// dates.
// TODO(crbug.com/1420547): The modification date is set in AutofillTable.
// Setting it on the test profiles directly doesn't suffice.
TEST_F(PersonalDataManagerTest, GetProfilesForSettings) {
  TestAutofillClock test_clock;

  AutofillProfile kAccountProfile = test::GetFullProfile();
  kAccountProfile.set_source_for_testing(AutofillProfile::Source::kAccount);
  AddProfileToPersonalDataManager(kAccountProfile);

  AutofillProfile kLocalOrSyncableProfile = test::GetFullProfile2();
  kLocalOrSyncableProfile.set_source_for_testing(
      AutofillProfile::Source::kLocalOrSyncable);
  test_clock.Advance(base::Minutes(123));
  AddProfileToPersonalDataManager(kLocalOrSyncableProfile);

  EXPECT_THAT(
      personal_data_->GetProfilesForSettings(),
      ElementsAre(Pointee(kLocalOrSyncableProfile), Pointee(kAccountProfile)));
}

// Adding, updating, removing operations without waiting in between.
TEST_F(PersonalDataManagerTest, AddRemoveUpdateProfileSequence) {
  AutofillProfile profile(test::GetFullProfile());

  personal_data_->AddProfile(profile);
  personal_data_->RemoveByGUID(profile.guid());
  personal_data_->UpdateProfile(profile);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  auto profiles = personal_data_->GetProfiles();
  ASSERT_EQ(0U, profiles.size());

  personal_data_->AddProfile(profile);
  personal_data_->RemoveByGUID(profile.guid());
  personal_data_->RemoveByGUID(profile.guid());
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  profiles = personal_data_->GetProfiles();
  ASSERT_EQ(0U, profiles.size());

  personal_data_->AddProfile(profile);
  profile.SetRawInfo(EMAIL_ADDRESS, u"new@email.com");
  personal_data_->UpdateProfile(profile);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  profiles = personal_data_->GetProfiles();
  ASSERT_EQ(1U, profiles.size());
  EXPECT_EQ(profiles[0]->GetRawInfo(EMAIL_ADDRESS), u"new@email.com");

  profile.SetRawInfo(EMAIL_ADDRESS, u"newer@email.com");
  personal_data_->UpdateProfile(profile);
  profile.SetRawInfo(EMAIL_ADDRESS, u"newest@email.com");
  personal_data_->UpdateProfile(profile);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  profiles = personal_data_->GetProfiles();
  ASSERT_EQ(1U, profiles.size());
  EXPECT_EQ(profiles[0]->GetRawInfo(EMAIL_ADDRESS), u"newest@email.com");
}

// Test that a new profile has its basic information set.
TEST_F(PersonalDataManagerTest, AddProfile_BasicInformation) {
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
TEST_F(PersonalDataManagerTest, AddProfile_CrazyCharacters) {
  std::vector<AutofillProfile> profiles;
  AutofillProfile profile1;
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

  AutofillProfile profile2;
  profile2.SetRawInfo(NAME_FIRST,
                      u"\u4e0a\u6d77\u5e02\u91d1\u5c71\u533a "
                      u"\u677e\u9690\u9547\u4ead\u67ab\u516c"
                      u"\u8def1915\u53f7");
  profile2.SetRawInfo(NAME_LAST, u"aguantó");
  profile2.SetRawInfo(ADDRESS_HOME_ZIP, u"HOME 94043");
  profile2.FinalizeAfterImport();
  profiles.push_back(profile2);

  AutofillProfile profile3;
  profile3.SetRawInfo(EMAIL_ADDRESS, u"sue@example.com");
  profile3.SetRawInfo(COMPANY_NAME, u"Company X");
  profile3.FinalizeAfterImport();
  profiles.push_back(profile3);

  AutofillProfile profile4;
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

  AutofillProfile profile5;
  profile5.SetRawInfo(NAME_FIRST, u"Larry");
  profile5.SetRawInfo(NAME_LAST,
                      u"\u0938\u094d\u091f\u093e\u0902\u092a "
                      u"\u0921\u094d\u092f\u0942\u091f\u0940");
  profile5.SetRawInfo(ADDRESS_HOME_ZIP, u"111111111111110000GOOGLE");
  profile5.SetRawInfo(EMAIL_ADDRESS, u"page@000000.com");
  profile5.SetRawInfo(COMPANY_NAME, u"Google");
  profile5.FinalizeAfterImport();
  profiles.push_back(profile5);

  AutofillProfile profile6;
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

  AutofillProfile profile7;
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
TEST_F(PersonalDataManagerTest, AddProfile_Invalid) {
  // First try profiles with invalid ZIP input.
  AutofillProfile without_invalid;
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

TEST_F(PersonalDataManagerTest, AddUpdateRemoveProfiles) {
  AutofillProfile profile0;
  test::SetProfileInfo(&profile0, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");

  AutofillProfile profile1;
  test::SetProfileInfo(&profile1, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "903 Apple Ct.", nullptr,
                       "Orlando", "FL", "32801", "US", "19482937549");

  AutofillProfile profile2;
  test::SetProfileInfo(&profile2, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5",
                       "Orlando", "FL", "32801", "US", "19482937549");

  // Add two test profiles to the database.
  AddProfileToPersonalDataManager(profile0);
  AddProfileToPersonalDataManager(profile1);

  std::vector<AutofillProfile*> profiles;
  profiles.push_back(&profile0);
  profiles.push_back(&profile1);
  ExpectSameElements(profiles, personal_data_->GetProfiles());

  // Update, remove, and add.
  profile0.SetRawInfo(NAME_FIRST, u"John");
  UpdateProfileOnPersonalDataManager(profile0);
  RemoveByGUIDFromPersonalDataManager(profile1.guid());
  AddProfileToPersonalDataManager(profile2);

  profiles.clear();
  profiles.push_back(&profile0);
  profiles.push_back(&profile2);
  ExpectSameElements(profiles, personal_data_->GetProfiles());

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the profiles from the web
  // database.
  ResetPersonalDataManager();

  // Verify that we've loaded the profiles from the web database.
  ExpectSameElements(profiles, personal_data_->GetProfiles());
}

// Tests that `UpdateProfile()` takes changes in the `ProfileTokenQuality`
// observations into considerations.
TEST_F(PersonalDataManagerTest, UpdateProfile_NewObservations) {
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
TEST_F(PersonalDataManagerTest, UpdateProfile_ResetObservations) {
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

TEST_F(PersonalDataManagerTest, MigrateProfileToAccount) {
  const AutofillProfile kLocalProfile = test::GetFullProfile();
  ASSERT_EQ(kLocalProfile.source(), AutofillProfile::Source::kLocalOrSyncable);
  AddProfileToPersonalDataManager(kLocalProfile);

  personal_data_->MigrateProfileToAccount(kLocalProfile);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
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

#if BUILDFLAG(IS_ANDROID)
TEST_F(PersonalDataManagerTest,
       AutofillPaymentMethodsMandatoryReauthAlwaysEnabledOnAutomotive) {
  if (!base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should only run on automotive.";
  }

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillEnablePaymentsMandatoryReauth);

  EXPECT_TRUE(personal_data_->IsPaymentMethodsMandatoryReauthEnabled());

  EXPECT_CHECK_DEATH_WITH(
      { personal_data_->SetPaymentMethodsMandatoryReauthEnabled(false); },
      "This feature should not be able to be turned off on automotive "
      "devices.");

  EXPECT_TRUE(personal_data_->IsPaymentMethodsMandatoryReauthEnabled());
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
// Test that setting the `kAutofillEnablePaymentsMandatoryReauth` pref works
// correctly.
TEST_F(PersonalDataManagerTest, AutofillPaymentMethodsMandatoryReauthEnabled) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillEnablePaymentsMandatoryReauth);
  EXPECT_FALSE(personal_data_->IsPaymentMethodsMandatoryReauthEnabled());

  personal_data_->SetPaymentMethodsMandatoryReauthEnabled(true);

  EXPECT_TRUE(personal_data_->IsPaymentMethodsMandatoryReauthEnabled());

  personal_data_->SetPaymentMethodsMandatoryReauthEnabled(false);

  EXPECT_FALSE(personal_data_->IsPaymentMethodsMandatoryReauthEnabled());
}

// Test that setting the `kAutofillEnablePaymentsMandatoryReauth` does not
// enable the feature when the flag is off.
TEST_F(PersonalDataManagerTest,
       AutofillPaymentMethodsMandatoryReauthEnabled_FlagOff) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAutofillEnablePaymentsMandatoryReauth);
  EXPECT_FALSE(personal_data_->IsPaymentMethodsMandatoryReauthEnabled());

  personal_data_->SetPaymentMethodsMandatoryReauthEnabled(true);

  EXPECT_FALSE(personal_data_->IsPaymentMethodsMandatoryReauthEnabled());
}

// Test that
// `PersonalDataManager::ShouldShowPaymentMethodsMandatoryReauthPromo()`
// only returns that we should show the promo when we are below the max counter
// limit for showing the promo.
TEST_F(
    PersonalDataManagerTest,
    ShouldShowPaymentMethodsMandatoryReauthPromo_MaxValueForPromoShownCounterReached) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillEnablePaymentsMandatoryReauth);
  base::HistogramTester histogram_tester;
  for (int i = 0; i < prefs::kMaxValueForMandatoryReauthPromoShownCounter;
       i++) {
    // This also verifies that ShouldShowPaymentMethodsMandatoryReauthPromo()
    // works as expected when below the max cap.
    EXPECT_TRUE(personal_data_->ShouldShowPaymentMethodsMandatoryReauthPromo());
    personal_data_->IncrementPaymentMethodsMandatoryReauthPromoShownCounter();
  }

  EXPECT_FALSE(personal_data_->ShouldShowPaymentMethodsMandatoryReauthPromo());
  histogram_tester.ExpectUniqueSample(
      "Autofill.PaymentMethods.MandatoryReauth.CheckoutFlow."
      "ReauthOfferOptInDecision2",
      autofill_metrics::MandatoryReauthOfferOptInDecision::
          kBlockedByStrikeDatabase,
      1);
}

// Test that
// `PersonalDataManager::ShouldShowPaymentMethodsMandatoryReauthPromo()`
// returns that we should not show the promo if the user already opted in.
TEST_F(PersonalDataManagerTest,
       ShouldShowPaymentMethodsMandatoryReauthPromo_UserOptedInAlready) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillEnablePaymentsMandatoryReauth);
  base::HistogramTester histogram_tester;
  // Simulate user is already opted in.
  personal_data_->SetPaymentMethodsMandatoryReauthEnabled(true);

  EXPECT_FALSE(personal_data_->ShouldShowPaymentMethodsMandatoryReauthPromo());
  histogram_tester.ExpectUniqueSample(
      "Autofill.PaymentMethods.MandatoryReauth.CheckoutFlow."
      "ReauthOfferOptInDecision2",
      autofill_metrics::MandatoryReauthOfferOptInDecision::kAlreadyOptedIn, 1);
}

// Test that
// `PersonalDataManager::ShouldShowPaymentMethodsMandatoryReauthPromo()`
// returns that we should not show the promo if the user has already opted out.
TEST_F(PersonalDataManagerTest,
       ShouldShowPaymentMethodsMandatoryReauthPromo_UserOptedOut) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillEnablePaymentsMandatoryReauth);
  base::HistogramTester histogram_tester;
  // Simulate user is already opted out.
  personal_data_->SetPaymentMethodsMandatoryReauthEnabled(false);

  EXPECT_FALSE(personal_data_->ShouldShowPaymentMethodsMandatoryReauthPromo());
  histogram_tester.ExpectUniqueSample(
      "Autofill.PaymentMethods.MandatoryReauth.CheckoutFlow."
      "ReauthOfferOptInDecision2",
      autofill_metrics::MandatoryReauthOfferOptInDecision::kAlreadyOptedOut, 1);
}

// Test that
// `PersonalDataManager::ShouldShowPaymentMethodsMandatoryReauthPromo()`
// returns that we should not show the promo if the flag is off.
TEST_F(PersonalDataManagerTest,
       ShouldShowPaymentMethodsMandatoryReauthPromo_FlagOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAutofillEnablePaymentsMandatoryReauth);
  EXPECT_FALSE(personal_data_->ShouldShowPaymentMethodsMandatoryReauthPromo());
}
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)

// Test that server IBANs can be added and automatically loaded/cached.
TEST_F(PersonalDataManagerTest, AddAndReloadServerIbans) {
  Iban server_iban1 = test::GetServerIban();
  Iban server_iban2 = test::GetServerIban2();

  GetServerDataTable()->SetServerIbans({server_iban1, server_iban2});
  std::vector<const Iban*> expected_ibans = {&server_iban1, &server_iban2};
  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  ExpectSameElements(expected_ibans, personal_data_->GetServerIbans());

  // Reset the PersonalDataManager. This tests that the personal data was saved
  // to the web database, and that we can load the IBANs from the web database.
  ResetPersonalDataManager();

  // Verify that we've reloaded the IBANs from the web database.
  ExpectSameElements(expected_ibans, personal_data_->GetServerIbans());
}

// Test that all (local and server) IBANs can be returned.
TEST_F(PersonalDataManagerTest, GetIbans) {
  personal_data_->SetSyncingForTest(true);

  Iban local_iban1;
  local_iban1.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  Iban local_iban2;
  local_iban2.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue_1)));
  Iban server_iban1 = test::GetServerIban();
  Iban server_iban2 = test::GetServerIban2();

  AddLocalIban(local_iban1);
  AddLocalIban(local_iban2);

  GetServerDataTable()->SetServerIbans({server_iban1, server_iban2});
  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  std::vector<const Iban*> all_ibans = {&local_iban1, &local_iban2,
                                        &server_iban1, &server_iban2};
  ExpectSameElements(all_ibans, personal_data_->GetIbans());
}

// Test that deduplication works correctly when a local IBAN has a matching
// prefix and suffix (either equal or starting with) and the same length as the
// server IBANs.
TEST_F(PersonalDataManagerTest, GetIbansToSuggest) {
  personal_data_->SetSyncingForTest(true);

  // Create two IBANs, and two server IBANs.
  // `local_iban1` and `server_iban1` have the same prefix, suffix and length.
  Iban local_iban1;
  local_iban1.set_value(u"FR76 3000 6000 0112 3456 7890 189");
  Iban local_iban2;
  local_iban2.set_value(u"CH56 0483 5012 3456 7800 9");
  Iban server_iban1(Iban::InstrumentId("1234567"));
  server_iban1.set_prefix(u"FR76");
  server_iban1.set_suffix(u"0189");
  server_iban1.set_length(27);
  Iban server_iban2 = test::GetServerIban2();
  server_iban2.set_length(34);

  AddLocalIban(local_iban1);
  AddLocalIban(local_iban2);

  GetServerDataTable()->SetServerIbans({server_iban1, server_iban2});
  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  std::vector<const Iban*> ibans_to_suggest = {&server_iban1, &server_iban2,
                                               &local_iban2};
  ExpectSameElements(ibans_to_suggest, personal_data_->GetIbansToSuggest());
}

TEST_F(PersonalDataManagerTest, NoIbansAddedIfDisabled) {
  prefs::SetAutofillPaymentMethodsEnabled(prefs_.get(), false);

  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  Iban iban1;
  iban1.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue_1)));

  personal_data_->AddIban(iban);
  personal_data_->AddIban(iban1);

  EXPECT_EQ(0U, personal_data_->GetLocalIbans().size());
}

TEST_F(PersonalDataManagerTest, AddingIbanUpdatesPref) {
  // The pref should always start disabled.
  ASSERT_FALSE(personal_data_->IsAutofillHasSeenIbanPrefEnabled());
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));

  personal_data_->AddIban(iban);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  // Adding an IBAN permanently enables the pref.
  EXPECT_TRUE(personal_data_->IsAutofillHasSeenIbanPrefEnabled());
}

TEST_F(PersonalDataManagerTest, AddLocalIbans) {
  Iban iban1;
  iban1.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  iban1.set_nickname(u"Nickname for Iban");

  Iban iban2;
  iban2.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue_1)));
  iban2.set_nickname(u"Original nickname");

  Iban iban2_with_different_nickname = iban2;
  iban2_with_different_nickname.set_nickname(u"Different nickname");

  // Attempt to add all three IBANs to the database. The first two should add
  // successfully, but the third should get skipped because its value is
  // identical to `iban2`.
  AddLocalIban(iban1);
  AddLocalIban(iban2);
  // Do not add `PersonalDataProfileTaskWaiter(*personal_data_).Wait()` for this
  // `AddIban` operation, as it will be terminated prematurely for
  // `iban2_with_different_nickname` due to the presence of an IBAN with the
  // same value.
  personal_data_->AddIban(iban2_with_different_nickname);

  std::vector<Iban*> ibans = {&iban1, &iban2};
  ExpectSameElements(ibans, personal_data_->GetLocalIbans());
}

TEST_F(PersonalDataManagerTest, UpdateLocalIbans) {
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  iban.set_nickname(u"Nickname for Iban");
  AddLocalIban(iban);

  // Verify the `iban` has been added successfully.
  std::vector<Iban*> ibans = {&iban};
  ExpectSameElements(ibans, personal_data_->GetLocalIbans());

  // Update the `iban` with new value.
  iban.SetRawInfo(IBAN_VALUE, u"GB98 MIDL 0700 9312 3456 78");
  personal_data_->UpdateIban(iban);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  ibans = {&iban};
  ExpectSameElements(ibans, personal_data_->GetLocalIbans());

  // Update the `iban` with new nickname.
  iban.set_nickname(u"Another nickname");
  personal_data_->UpdateIban(iban);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  ibans = {&iban};
  ExpectSameElements(ibans, personal_data_->GetLocalIbans());
}

TEST_F(PersonalDataManagerTest, RemoveLocalIbans) {
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  iban.set_nickname(u"Nickname for Iban");
  AddLocalIban(iban);

  // Verify the `iban` has been added successfully.
  std::vector<Iban*> ibans = {&iban};
  ExpectSameElements(ibans, personal_data_->GetLocalIbans());

  RemoveByGUIDFromPersonalDataManager(iban.guid());
  EXPECT_TRUE(personal_data_->GetLocalIbans().empty());

  // Verify that removal of a GUID that doesn't exist won't crash.
  RemoveByGUIDFromPersonalDataManager(iban.guid());
}

// Ensure that new IBANs can be updated and saved via
// `OnAcceptedLocalIbanSave()`.
TEST_F(PersonalDataManagerTest, OnAcceptedLocalIbanSave) {
  // Start with a new IBAN.
  Iban iban0;
  iban0.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  // Add the IBAN to the database.
  std::string guid = personal_data_->OnAcceptedLocalIbanSave(iban0);
  iban0.set_identifier(Iban::Guid(guid));
  iban0.set_record_type(Iban::kLocalIban);

  // Make sure everything is set up correctly.
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_EQ(1U, personal_data_->GetLocalIbans().size());

  // Creates a new IBAN and call `OnAcceptedLocalIbanSave()` and verify that
  // the new IBAN is saved.
  Iban iban1;
  iban1.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue_1)));
  guid = personal_data_->OnAcceptedLocalIbanSave(iban1);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  iban1.set_identifier(Iban::Guid(guid));
  iban1.set_record_type(Iban::kLocalIban);

  // Expect that the new IBAN is added.
  ASSERT_EQ(2U, personal_data_->GetLocalIbans().size());

  std::vector<Iban*> ibans;
  ibans.push_back(&iban0);
  ibans.push_back(&iban1);
  // Verify that we've loaded the IBAN from the web database.
  ExpectSameElements(ibans, personal_data_->GetLocalIbans());

  // Creates a new `iban2` which has the same value as `iban0` but with
  // different nickname and call `OnAcceptedLocalIbanSave()`.
  Iban iban2 = iban0;
  iban2.set_nickname(u"Nickname 2");
  personal_data_->OnAcceptedLocalIbanSave(iban2);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  // Updates the nickname for `iban1` and call `OnAcceptedLocalIbanSave()`.
  iban1.set_nickname(u"Nickname 1 updated");
  personal_data_->OnAcceptedLocalIbanSave(iban1);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  ibans.clear();
  ibans.push_back(&iban1);
  ibans.push_back(&iban2);
  // Expect that the existing IBANs are updated.
  ASSERT_EQ(2U, personal_data_->GetLocalIbans().size());

  // Verify that we've loaded the IBANs from the web database.
  ExpectSameElements(ibans, personal_data_->GetLocalIbans());

  // Call `OnAcceptedLocalIbanSave()` with the same iban1, verify that nothing
  // changes.
  personal_data_->OnAcceptedLocalIbanSave(iban1);
  ExpectSameElements(ibans, personal_data_->GetLocalIbans());

  // Reset the PersonalDataManager. This tests that the IBANs are persisted
  // in the local web database even if the browser is re-loaded, ensuring that
  // the user can load the IBANs from the local web database on browser startup.
  ResetPersonalDataManager();
  ExpectSameElements(ibans, personal_data_->GetLocalIbans());
}

TEST_F(PersonalDataManagerTest, AddUpdateRemoveCreditCards) {
  CreditCard credit_card0(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "John Dillinger",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  credit_card0.SetNickname(u"card zero");

  CreditCard credit_card1(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card1, "Bonnie Parker",
                          "5105105105105100" /* Mastercard */, "12", "2999",
                          "1");

  CreditCard credit_card2(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card2, "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  credit_card2.SetNickname(u"card two");

  // Add two test credit cards to the database.
  personal_data_->AddCreditCard(credit_card0);
  personal_data_->AddCreditCard(credit_card1);

  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  std::vector<CreditCard*> cards;
  cards.push_back(&credit_card0);
  cards.push_back(&credit_card1);
  ExpectSameElements(cards, personal_data_->GetCreditCards());

  // Update, remove, and add.
  credit_card0.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Joe");
  credit_card0.SetNickname(u"new card zero");
  personal_data_->UpdateCreditCard(credit_card0);
  RemoveByGUIDFromPersonalDataManager(credit_card1.guid());
  personal_data_->AddCreditCard(credit_card2);

  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  cards.clear();
  cards.push_back(&credit_card0);
  cards.push_back(&credit_card2);
  ExpectSameElements(cards, personal_data_->GetCreditCards());

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the credit cards from the web
  // database.
  ResetPersonalDataManager();

  // Verify that we've loaded the credit cards from the web database.
  cards.clear();
  cards.push_back(&credit_card0);
  cards.push_back(&credit_card2);
  ExpectSameElements(cards, personal_data_->GetCreditCards());

  // Add a full server card.
  CreditCard credit_card3(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card3, "Jane Doe",
                          "4111111111111111" /* Visa */, "04", "2999", "1");
  credit_card3.set_record_type(CreditCard::RecordType::kFullServerCard);
  credit_card3.set_server_id("server_id");

  personal_data_->AddFullServerCreditCard(credit_card3);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  cards.push_back(&credit_card3);
  ExpectSameElements(cards, personal_data_->GetCreditCards());

  // Must not add a duplicate server card with same GUID.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(0);

  personal_data_->AddFullServerCreditCard(credit_card3);

  ExpectSameElements(cards, personal_data_->GetCreditCards());

  // Must not add a duplicate card with same contents as another server card.
  CreditCard duplicate_server_card(credit_card3);
  duplicate_server_card.set_guid(
      base::Uuid::GenerateRandomV4().AsLowercaseString());

  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(0);

  personal_data_->AddFullServerCreditCard(duplicate_server_card);

  ExpectSameElements(cards, personal_data_->GetCreditCards());
}

// Test that UpdateLocalCvc function working as expected.
TEST_F(PersonalDataManagerTest, UpdateLocalCvc) {
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);
  CreditCard credit_card = test::GetCreditCard();
  const std::u16string kCvc = u"111";
  credit_card.set_cvc(kCvc);
  PersonalDataProfileTaskWaiter add_waiter(*personal_data_);
  personal_data_->AddCreditCard(credit_card);
  std::move(add_waiter).Wait();
  ASSERT_EQ(personal_data_->GetLocalCreditCards().size(), 1U);
  EXPECT_EQ(personal_data_->GetLocalCreditCards()[0]->cvc(), kCvc);

  const std::u16string kNewCvc = u"222";
  PersonalDataProfileTaskWaiter update_waiter(*personal_data_);
  personal_data_->UpdateLocalCvc(credit_card.guid(), kNewCvc);
  std::move(update_waiter).Wait();
  ASSERT_EQ(personal_data_->GetLocalCreditCards().size(), 1U);
  EXPECT_EQ(personal_data_->GetLocalCreditCards()[0]->cvc(), kNewCvc);
}

// Test that verify add, update, remove server cvc function working as expected.
TEST_F(PersonalDataManagerTest, ServerCvc) {
  const std::u16string kCvc = u"111";
  CreditCard credit_card = test::GetMaskedServerCard();
  SetServerCards({credit_card});

  // Add an empty cvc will fail a CHECK().
  EXPECT_DEATH_IF_SUPPORTED(personal_data_->AddServerCvc(1, u""), "");

  personal_data_->AddServerCvc(credit_card.instrument_id(), kCvc);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  ASSERT_EQ(personal_data_->GetCreditCards().size(), 1U);
  EXPECT_EQ(personal_data_->GetCreditCards()[0]->cvc(), kCvc);

  // Update an empty cvc will fail a CHECK().
  EXPECT_DEATH_IF_SUPPORTED(
      personal_data_->UpdateServerCvc(credit_card.instrument_id(), u""), "");
  // Update an non-exist card cvc will fail a CHECK().
  EXPECT_DEATH_IF_SUPPORTED(personal_data_->UpdateServerCvc(99999, u""), "");

  const std::u16string kNewCvc = u"222";
  personal_data_->UpdateServerCvc(credit_card.instrument_id(), kNewCvc);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_EQ(personal_data_->GetCreditCards()[0]->cvc(), kNewCvc);

  personal_data_->RemoveServerCvc(credit_card.instrument_id());
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  ASSERT_EQ(personal_data_->GetCreditCards().size(), 1U);
  EXPECT_TRUE(personal_data_->GetCreditCards()[0]->cvc().empty());
}

// Test that verify clear server cvc function working as expected.
TEST_F(PersonalDataManagerTest, ClearServerCvc) {
  // Add a server card cvc.
  const std::u16string kCvc = u"111";
  CreditCard credit_card = test::GetMaskedServerCard();
  SetServerCards({credit_card});
  personal_data_->AddServerCvc(credit_card.instrument_id(), kCvc);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  ASSERT_EQ(personal_data_->GetCreditCards().size(), 1U);
  EXPECT_EQ(personal_data_->GetCreditCards()[0]->cvc(), kCvc);

  // After we clear server cvcs we should expect empty cvc.
  personal_data_->ClearServerCvcs();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_TRUE(personal_data_->GetCreditCards()[0]->cvc().empty());
}

// Test that a new credit card has its basic information set.
TEST_F(PersonalDataManagerTest, AddCreditCard_BasicInformation) {
  // Create the test clock and set the time to a specific value.
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);

  // Add a credit card to the database.
  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card, "John Dillinger",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  personal_data_->AddCreditCard(credit_card);

  // Reload the database.
  ResetPersonalDataManager();

  // Verify the addition.
  const std::vector<CreditCard*>& results = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, credit_card.Compare(*results[0]));

  // Make sure the use count and use date were set.
  EXPECT_EQ(1U, results[0]->use_count());
  EXPECT_EQ(kArbitraryTime, results[0]->use_date());
  EXPECT_EQ(kArbitraryTime, results[0]->modification_date());
}

// Test filling credit cards with unicode strings and crazy characters.
TEST_F(PersonalDataManagerTest, AddCreditCard_CrazyCharacters) {
  std::vector<CreditCard> cards;
  CreditCard card1;
  card1.SetRawInfo(CREDIT_CARD_NAME_FULL,
                   u"\u751f\u6d3b\u5f88\u6709\u89c4\u5f8b "
                   u"\u4ee5\u73a9\u4e3a\u4e3b");
  card1.SetRawInfo(CREDIT_CARD_NUMBER, u"6011111111111117");
  card1.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"12");
  card1.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2011");
  cards.push_back(card1);

  CreditCard card2;
  card2.SetRawInfo(CREDIT_CARD_NAME_FULL, u"John Williams");
  card2.SetRawInfo(CREDIT_CARD_NUMBER, u"WokoAwesome12345");
  card2.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"10");
  card2.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2015");
  cards.push_back(card2);

  CreditCard card3;
  card3.SetRawInfo(CREDIT_CARD_NAME_FULL,
                   u"\u0623\u062d\u0645\u062f\u064a "
                   u"\u0646\u062c\u0627\u062f "
                   u"\u0644\u0645\u062d\u0627\u0648\u0644\u0647 "
                   u"\u0627\u063a\u062a\u064a\u0627\u0644 "
                   u"\u0641\u064a \u0645\u062f\u064a\u0646\u0629 "
                   u"\u0647\u0645\u062f\u0627\u0646 ");
  card3.SetRawInfo(CREDIT_CARD_NUMBER,
                   u"\u092a\u0941\u0928\u0930\u094d\u091c\u0940"
                   u"\u0935\u093f\u0924 \u0939\u094b\u0917\u093e "
                   u"\u0928\u093e\u0932\u0902\u0926\u093e");
  card3.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"10");
  card3.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2015");
  cards.push_back(card3);

  CreditCard card4;
  card4.SetRawInfo(CREDIT_CARD_NAME_FULL,
                   u"\u039d\u03ad\u03b5\u03c2 "
                   u"\u03c3\u03c5\u03b3\u03c7\u03c9\u03bd\u03b5"
                   u"\u03cd\u03c3\u03b5\u03b9\u03c2 "
                   u"\u03ba\u03b1\u03b9 "
                   u"\u03ba\u03b1\u03c4\u03b1\u03c1\u03b3\u03ae"
                   u"\u03c3\u03b5\u03b9\u03c2");
  card4.SetRawInfo(CREDIT_CARD_NUMBER, u"00000000000000000000000");
  card4.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"01");
  card4.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2016");
  cards.push_back(card4);

  personal_data_->SetCreditCards(&cards);

  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  ASSERT_EQ(cards.size(), personal_data_->GetCreditCards().size());
  for (size_t i = 0; i < cards.size(); ++i) {
    EXPECT_TRUE(base::Contains(cards, *personal_data_->GetCreditCards()[i]));
  }
}

// Test invalid credit card numbers typed in settings UI should be saved as-is.
TEST_F(PersonalDataManagerTest, AddCreditCard_Invalid) {
  CreditCard card;
  card.SetRawInfo(CREDIT_CARD_NUMBER, u"Not_0123-5Checked");

  std::vector<CreditCard> cards;
  cards.push_back(card);
  personal_data_->SetCreditCards(&cards);

  ASSERT_EQ(1u, personal_data_->GetCreditCards().size());
  ASSERT_EQ(card, *personal_data_->GetCreditCards()[0]);
}

TEST_F(PersonalDataManagerTest, GetCreditCardByServerId) {
  CreditCard card = test::GetFullServerCard();
  card.set_server_id("server id");
  personal_data_->AddFullServerCreditCard(card);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  ASSERT_EQ(1u, personal_data_->GetCreditCards().size());
  EXPECT_TRUE(personal_data_->GetCreditCardByServerId("server id"));
  EXPECT_FALSE(personal_data_->GetCreditCardByServerId("non-existing id"));
}

#if !BUILDFLAG(IS_IOS)
TEST_F(PersonalDataManagerTest, AddAndGetCreditCardArtImage) {
  gfx::Image expected_image = gfx::test::CreateImage(40, 24);
  std::unique_ptr<CreditCardArtImage> credit_card_art_image =
      std::make_unique<CreditCardArtImage>(GURL("https://www.example.com"),
                                           expected_image);
  std::vector<std::unique_ptr<CreditCardArtImage>> images;
  images.push_back(std::move(credit_card_art_image));

  personal_data_->OnCardArtImagesFetched(std::move(images));

  gfx::Image* actual_image = personal_data_->GetCreditCardArtImageForUrl(
      GURL("https://www.example.com"));
  ASSERT_TRUE(actual_image);
  EXPECT_TRUE(gfx::test::AreImagesEqual(expected_image, *actual_image));

  // TODO(crbug.com/1284788): Look into integrating with PersonalDataManagerMock
  // and checking that PersonalDataManager::FetchImagesForUrls() does not get
  // triggered when PersonalDataManager::GetCachedCardArtImageForUrl() is
  // called.
  gfx::Image* cached_image = personal_data_->GetCachedCardArtImageForUrl(
      GURL("https://www.example.com"));
  ASSERT_TRUE(cached_image);
  EXPECT_TRUE(gfx::test::AreImagesEqual(expected_image, *cached_image));
}

TEST_F(PersonalDataManagerTest,
       TestNoImageFetchingAttemptForCardsWithInvalidCardArtUrls) {
  base::HistogramTester histogram_tester;

  gfx::Image* actual_image =
      personal_data_->GetCreditCardArtImageForUrl(GURL());
  EXPECT_FALSE(actual_image);
  EXPECT_EQ(0, histogram_tester.GetTotalSum("Autofill.ImageFetcher.Result"));
}

TEST_F(PersonalDataManagerMockTest, ProcessCardArtUrlChanges) {
  CreditCard card = test::GetFullServerCard();
  card.set_server_id("card_server_id");
  personal_data_->AddFullServerCreditCard(card);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  card.set_server_id("card_server_id");
  card.set_card_art_url(GURL("https://www.example.com/card1"));
  std::vector<GURL> updated_urls;
  updated_urls.emplace_back("https://www.example.com/card1");

  personal_data_->AddFullServerCreditCard(card);
  WaitForFetchImagesForUrls();

  card.set_card_art_url(GURL("https://www.example.com/card2"));
  updated_urls.clear();
  updated_urls.emplace_back("https://www.example.com/card2");

  personal_data_->AddFullServerCreditCard(card);
  WaitForFetchImagesForUrls();
}
#endif

TEST_F(PersonalDataManagerTest, UpdateUnverifiedCreditCards) {
  // Start with unverified data.
  CreditCard credit_card = test::GetCreditCard();
  EXPECT_FALSE(credit_card.IsVerified());

  // Add the data to the database.
  personal_data_->AddCreditCard(credit_card);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  EXPECT_THAT(personal_data_->GetCreditCards(),
              testing::UnorderedElementsAre(Pointee(credit_card)));

  // Try to update with just the origin changed.
  CreditCard original_credit_card(credit_card);
  credit_card.set_origin(kSettingsOrigin);
  EXPECT_TRUE(credit_card.IsVerified());
  personal_data_->UpdateCreditCard(credit_card);

  // Credit Card origin should not be overwritten.
  EXPECT_THAT(personal_data_->GetCreditCards(),
              testing::UnorderedElementsAre(Pointee(original_credit_card)));

  // Try to update with data changed as well.
  credit_card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Joe");
  personal_data_->UpdateCreditCard(credit_card);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  EXPECT_THAT(personal_data_->GetCreditCards(),
              testing::UnorderedElementsAre(Pointee(credit_card)));
}

// Test that ensure local data is not lost on sign-in.
// Clearing/changing the primary account is not supported on CrOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PersonalDataManagerTest, KeepExistingLocalDataOnSignIn) {
  // Sign out.
  identity_test_env_.ClearPrimaryAccount();
  sync_service_.SetAccountInfo(CoreAccountInfo());
  EXPECT_TRUE(sync_service_.GetAccountInfo().IsEmpty());
  EXPECT_EQ(0U, personal_data_->GetCreditCards().size());

  // Add local card.
  CreditCard local_card;
  test::SetCreditCardInfo(&local_card, "Freddy Mercury",
                          "4234567890123463",  // Visa
                          "08", "2999", "1");
  local_card.set_guid("00000000-0000-0000-0000-000000000009");
  local_card.set_record_type(CreditCard::RecordType::kLocalCard);
  local_card.set_use_count(5);
  personal_data_->AddCreditCard(local_card);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  // Sign in.
  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSync);
  sync_service_.SetAccountInfo(
      identity_test_env_.identity_manager()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSync));
  sync_service_.SetHasSyncConsent(true);
  EXPECT_TRUE(
      sync_service_.IsSyncFeatureEnabled() &&
      sync_service_.GetActiveDataTypes().Has(syncer::AUTOFILL_WALLET_DATA));
  ASSERT_TRUE(TurnOnSyncFeature());

  // Check saved local card should be not lost.
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());
  EXPECT_EQ(0, local_card.Compare(*personal_data_->GetCreditCards()[0]));
}
#endif

TEST_F(PersonalDataManagerTest, AddProfilesAndCreditCards) {
  AutofillProfile profile0;
  test::SetProfileInfo(&profile0, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");

  AutofillProfile profile1;
  test::SetProfileInfo(&profile1, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "903 Apple Ct.", nullptr,
                       "Orlando", "FL", "32801", "US", "19482937549");

  CreditCard credit_card0(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "John Dillinger",
                          "4234567890123456" /* Visa */, "01", "2999", "1");

  CreditCard credit_card1(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card1, "Bonnie Parker",
                          "5105105105105100" /* Mastercard */, "12", "2999",
                          "1");

  // Add two test profiles to the database.
  AddProfileToPersonalDataManager(profile0);
  AddProfileToPersonalDataManager(profile1);

  std::vector<AutofillProfile*> profiles;
  profiles.push_back(&profile0);
  profiles.push_back(&profile1);
  ExpectSameElements(profiles, personal_data_->GetProfiles());

  // Add two test credit cards to the database.
  personal_data_->AddCreditCard(credit_card0);
  personal_data_->AddCreditCard(credit_card1);

  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  std::vector<CreditCard*> cards;
  cards.push_back(&credit_card0);
  cards.push_back(&credit_card1);
  ExpectSameElements(cards, personal_data_->GetCreditCards());

  // Determine uniqueness by inserting all of the GUIDs into a set and verifying
  // the size of the set matches the number of GUIDs.
  std::set<std::string> guids;
  guids.insert(profile0.guid());
  guids.insert(profile1.guid());
  guids.insert(credit_card0.guid());
  guids.insert(credit_card1.guid());
  EXPECT_EQ(4U, guids.size());
}

// Test for http://crbug.com/50047. Makes sure that guids are populated
// correctly on load.
TEST_F(PersonalDataManagerTest, PopulateUniqueIDsOnLoad) {
  AutofillProfile profile0;
  test::SetProfileInfo(&profile0, "y", "", "", "", "", "", "", "", "", "", "",
                       "");

  // Add the profile0 to the db.
  AddProfileToPersonalDataManager(profile0);

  // Verify that we've loaded the profiles from the web database.
  const std::vector<AutofillProfile*>& results2 = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, profile0.Compare(*results2[0]));

  // Add a new profile.
  AutofillProfile profile1;
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

TEST_F(PersonalDataManagerTest, SetUniqueCreditCardLabels) {
  CreditCard credit_card0(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  credit_card0.SetRawInfo(CREDIT_CARD_NAME_FULL, u"John");
  CreditCard credit_card1(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  credit_card1.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Paul");
  CreditCard credit_card2(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  credit_card2.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Ringo");
  CreditCard credit_card3(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  credit_card3.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Other");
  CreditCard credit_card4(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  credit_card4.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Ozzy");
  CreditCard credit_card5(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  credit_card5.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Dio");

  // Add the test credit cards to the database.
  personal_data_->AddCreditCard(credit_card0);
  personal_data_->AddCreditCard(credit_card1);
  personal_data_->AddCreditCard(credit_card2);
  personal_data_->AddCreditCard(credit_card3);
  personal_data_->AddCreditCard(credit_card4);
  personal_data_->AddCreditCard(credit_card5);

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the credit cards from the web
  // database.
  ResetPersonalDataManager();

  std::vector<CreditCard*> cards;
  cards.push_back(&credit_card0);
  cards.push_back(&credit_card1);
  cards.push_back(&credit_card2);
  cards.push_back(&credit_card3);
  cards.push_back(&credit_card4);
  cards.push_back(&credit_card5);
  ExpectSameElements(cards, personal_data_->GetCreditCards());
}

TEST_F(PersonalDataManagerTest, SetEmptyProfile) {
  AutofillProfile profile0;
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

TEST_F(PersonalDataManagerTest, SetEmptyCreditCard) {
  CreditCard credit_card0(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "", "", "", "", "");

  // Add the empty credit card to the database.
  personal_data_->AddCreditCard(credit_card0);

  // Note: no refresh here.

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the credit cards from the web
  // database.
  ResetPersonalDataManager();

  // Verify that we've loaded the credit cards from the web database.
  ASSERT_EQ(0U, personal_data_->GetCreditCards().size());
}

TEST_F(PersonalDataManagerTest, Refresh) {
  AutofillProfile profile0;
  test::SetProfileInfo(&profile0, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");

  AutofillProfile profile1;
  test::SetProfileInfo(&profile1, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "903 Apple Ct.", nullptr,
                       "Orlando", "FL", "32801", "US", "19482937549");

  // Add the test profiles to the database.
  AddProfileToPersonalDataManager(profile0);
  AddProfileToPersonalDataManager(profile1);

  std::vector<AutofillProfile*> profiles;
  profiles.push_back(&profile0);
  profiles.push_back(&profile1);
  ExpectSameElements(profiles, personal_data_->GetProfiles());

  AutofillProfile profile2;
  test::SetProfileInfo(&profile2, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5",
                       "Orlando", "FL", "32801", "US", "19482937549");

  profile_database_service_->AddAutofillProfile(profile2);

  personal_data_->Refresh();

  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  profiles.clear();
  profiles.push_back(&profile0);
  profiles.push_back(&profile1);
  profiles.push_back(&profile2);
  ExpectSameElements(profiles, personal_data_->GetProfiles());

  profile_database_service_->RemoveAutofillProfile(
      profile1.guid(), AutofillProfile::Source::kLocalOrSyncable);
  profile_database_service_->RemoveAutofillProfile(
      profile2.guid(), AutofillProfile::Source::kLocalOrSyncable);

  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  auto results = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(profile0, *results[0]);

  profile0.SetRawInfo(NAME_FIRST, u"Mar");
  profile_database_service_->UpdateAutofillProfile(profile0);

  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  results = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(profile0, *results[0]);
}

// Ensure that verified credit cards can be saved via
// OnAcceptedLocalCreditCardSave.
TEST_F(PersonalDataManagerTest, OnAcceptedLocalCreditCardSaveWithVerifiedData) {
  // Start with a verified credit card.
  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         kSettingsOrigin);
  test::SetCreditCardInfo(&credit_card, "Biggie Smalls",
                          "4111 1111 1111 1111" /* Visa */, "01", "2999", "");
  EXPECT_TRUE(credit_card.IsVerified());

  // Add the credit card to the database.
  personal_data_->AddCreditCard(credit_card);

  // Make sure everything is set up correctly.
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  CreditCard new_verified_card = credit_card;
  new_verified_card.set_guid(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  new_verified_card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"B. Small");
  EXPECT_TRUE(new_verified_card.IsVerified());

  personal_data_->OnAcceptedLocalCreditCardSave(new_verified_card);

  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  // Expect that the saved credit card is updated.
  const std::vector<CreditCard*>& results = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(u"B. Small", results[0]->GetRawInfo(CREDIT_CARD_NAME_FULL));
}

TEST_F(PersonalDataManagerTest, GetNonEmptyTypes) {
  // Check that there are no available types with no profiles stored.
  ServerFieldTypeSet non_empty_types;
  personal_data_->GetNonEmptyTypes(&non_empty_types);
  EXPECT_EQ(0U, non_empty_types.size());

  // Test with one profile stored.
  AutofillProfile profile0;
  test::SetProfileInfo(&profile0, "Marion", nullptr, "Morrison",
                       "johnwayne@me.xyz", nullptr, "123 Zoo St.", nullptr,
                       "Hollywood", "CA", "91601", "US", "14155678910");

  AddProfileToPersonalDataManager(profile0);

  // Make sure everything is set up correctly.
  EXPECT_EQ(1U, personal_data_->GetProfiles().size());

  std::vector<ServerFieldType> expected_types{NAME_FIRST,
                                              NAME_LAST,
                                              NAME_FULL,
                                              EMAIL_ADDRESS,
                                              ADDRESS_HOME_ADDRESS,
                                              ADDRESS_HOME_STREET_LOCATION,
                                              ADDRESS_HOME_LINE1,
                                              ADDRESS_HOME_STREET_ADDRESS,
                                              ADDRESS_HOME_CITY,
                                              ADDRESS_HOME_STATE,
                                              ADDRESS_HOME_ZIP,
                                              ADDRESS_HOME_COUNTRY,
                                              PHONE_HOME_NUMBER,
                                              PHONE_HOME_NUMBER_PREFIX,
                                              PHONE_HOME_NUMBER_SUFFIX,
                                              PHONE_HOME_COUNTRY_CODE,
                                              PHONE_HOME_CITY_CODE,
                                              PHONE_HOME_CITY_AND_NUMBER,
                                              PHONE_HOME_WHOLE_NUMBER};
  // For structured names and addresses, there are more non-empty types.
  expected_types.push_back(NAME_LAST_SECOND);
  expected_types.insert(expected_types.end(),
                        {ADDRESS_HOME_STREET_NAME, ADDRESS_HOME_HOUSE_NUMBER});

  personal_data_->GetNonEmptyTypes(&non_empty_types);
  EXPECT_THAT(non_empty_types,
              testing::UnorderedElementsAreArray(expected_types));

  // Test with multiple profiles stored.
  AutofillProfile profile1;
  test::SetProfileInfo(&profile1, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "903 Apple Ct.", nullptr,
                       "Orlando", "FL", "32801", "US", "16502937549");

  AutofillProfile profile2;
  test::SetProfileInfo(&profile2, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5",
                       "Orlando", "FL", "32801", "US", "16502937549");

  AddProfileToPersonalDataManager(profile1);
  AddProfileToPersonalDataManager(profile2);

  EXPECT_EQ(3U, personal_data_->GetProfiles().size());

  expected_types.insert(
      expected_types.end(),
      {NAME_MIDDLE, NAME_MIDDLE_INITIAL, ADDRESS_HOME_LINE2, COMPANY_NAME});

  personal_data_->GetNonEmptyTypes(&non_empty_types);
  EXPECT_THAT(non_empty_types,
              testing::UnorderedElementsAreArray(expected_types));

  // Test with credit card information also stored.
  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card, "John Dillinger",
                          "4234567890123456" /* Visa */, "01", "2999", "");
  personal_data_->AddCreditCard(credit_card);

  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  expected_types.insert(
      expected_types.end(),
      {CREDIT_CARD_NAME_FULL, CREDIT_CARD_NAME_FIRST, CREDIT_CARD_NAME_LAST,
       CREDIT_CARD_NUMBER, CREDIT_CARD_TYPE, CREDIT_CARD_EXP_MONTH,
       CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_EXP_4_DIGIT_YEAR,
       CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR});

  personal_data_->GetNonEmptyTypes(&non_empty_types);
  EXPECT_THAT(non_empty_types,
              testing::UnorderedElementsAreArray(expected_types));
}

// Tests that GetAutofillOffers returns all available offers.
TEST_F(PersonalDataManagerTest, GetAutofillOffers) {
  // Add two card-linked offers and one promo code offer.
  AddOfferDataForTest(test::GetCardLinkedOfferData1());
  AddOfferDataForTest(test::GetCardLinkedOfferData2());
  AddOfferDataForTest(test::GetPromoCodeOfferData());

  // Should return all three.
  EXPECT_EQ(3U, personal_data_->GetAutofillOffers().size());
}

// Tests that GetAutofillOffers does not return any offers if
// |IsAutofillWalletImportEnabled()| returns |false|.
TEST_F(PersonalDataManagerTest, GetAutofillOffers_WalletImportDisabled) {
  syncer::TestSyncService sync_service;
  personal_data_->SetSyncServiceForTest(&sync_service);

  // Add a card-linked offer and a promo code offer.
  AddOfferDataForTest(test::GetCardLinkedOfferData1());
  AddOfferDataForTest(test::GetPromoCodeOfferData());

  ASSERT_EQ(2U, personal_data_->GetAutofillOffers().size());

  sync_service.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, syncer::UserSelectableTypeSet());

  // Should return neither of them as the wallet import pref is disabled.
  EXPECT_EQ(0U, personal_data_->GetAutofillOffers().size());

  // Unregister the Sync observer.
  personal_data_->OnSyncShutdown(&sync_service);
}

// Tests that GetAutofillOffers does not return any offers if
// |IsAutofillCreditCardEnabled()| returns |false|.
TEST_F(PersonalDataManagerTest, GetAutofillOffers_AutofillCreditCardDisabled) {
  // Add a card-linked offer and a promo code offer.
  AddOfferDataForTest(test::GetCardLinkedOfferData1());
  AddOfferDataForTest(test::GetPromoCodeOfferData());

  prefs::SetAutofillPaymentMethodsEnabled(prefs_.get(), false);

  // Should return neither of the offers as the autofill credit card import pref
  // is disabled.
  EXPECT_EQ(0U, personal_data_->GetAutofillOffers().size());
}

// Tests that GetActiveAutofillPromoCodeOffersForOrigin returns only active and
// site-relevant promo code offers.
TEST_F(PersonalDataManagerTest, GetActiveAutofillPromoCodeOffersForOrigin) {
  // Card-linked offers should not be returned.
  AddOfferDataForTest(test::GetCardLinkedOfferData1());
  // Expired promo code offers should not be returned.
  AddOfferDataForTest(test::GetPromoCodeOfferData(
      /*origin=*/GURL("http://www.example.com"), /*is_expired=*/true));
  // Active promo code offers should be returned.
  AddOfferDataForTest(test::GetPromoCodeOfferData(
      /*origin=*/GURL("http://www.example.com"), /*is_expired=*/false));
  // Active promo code offers for a different site should not be returned.
  AddOfferDataForTest(test::GetPromoCodeOfferData(
      /*origin=*/GURL("http://www.some-other-merchant.com"),
      /*is_expired=*/false));

  // Only the active offer for example.com should be returned.
  EXPECT_EQ(1U, personal_data_
                    ->GetActiveAutofillPromoCodeOffersForOrigin(
                        GURL("http://www.example.com"))
                    .size());
}

// Tests that GetActiveAutofillPromoCodeOffersForOrigin does not return any
// promo code offers if |IsAutofillWalletImportEnabled()| returns |false|.
TEST_F(PersonalDataManagerTest,
       GetActiveAutofillPromoCodeOffersForOrigin_WalletImportDisabled) {
  syncer::TestSyncService sync_service;
  personal_data_->SetSyncServiceForTest(&sync_service);

  // Add an active promo code offer.
  AddOfferDataForTest(test::GetPromoCodeOfferData(
      /*origin=*/GURL("http://www.example.com")));

  ASSERT_EQ(1U, personal_data_
                    ->GetActiveAutofillPromoCodeOffersForOrigin(
                        GURL("http://www.example.com"))
                    .size());

  sync_service.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, syncer::UserSelectableTypeSet());

  // Should not return the offer as the wallet import pref is disabled.
  EXPECT_EQ(0U, personal_data_
                    ->GetActiveAutofillPromoCodeOffersForOrigin(
                        GURL("http://www.example.com"))
                    .size());

  // Unregister the Sync observer.
  personal_data_->OnSyncShutdown(&sync_service);
}

// Tests that GetActiveAutofillPromoCodeOffersForOrigin does not return any
// promo code offers if |IsAutofillCreditCardEnabled()| returns |false|.
TEST_F(PersonalDataManagerTest,
       GetActiveAutofillPromoCodeOffersForOrigin_AutofillCreditCardDisabled) {
  // Add an active promo code offer.
  AddOfferDataForTest(test::GetPromoCodeOfferData(
      /*origin=*/GURL("http://www.example.com")));

  prefs::SetAutofillPaymentMethodsEnabled(prefs_.get(), false);

  // Should not return the offer as the autofill credit card pref is disabled.
  EXPECT_EQ(0U, personal_data_
                    ->GetActiveAutofillPromoCodeOffersForOrigin(
                        GURL("http://www.example.com"))
                    .size());
}

TEST_F(PersonalDataManagerTest, DefaultCountryCodeIsCached) {
  // The return value should always be some country code, no matter what.
  std::string default_country =
      personal_data_->GetDefaultCountryCodeForNewAddress();
  EXPECT_EQ(2U, default_country.size());

  AutofillProfile profile = test::GetFullProfile();
  AddProfileToPersonalDataManager(profile);

  // The value is cached and doesn't change even after adding an address.
  EXPECT_EQ(default_country,
            personal_data_->GetDefaultCountryCodeForNewAddress());

  // Disabling Autofill blows away this cache and shouldn't account for Autofill
  // profiles.
  prefs::SetAutofillProfileEnabled(prefs_.get(), false);
  prefs::SetAutofillPaymentMethodsEnabled(prefs_.get(), false);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_EQ(default_country,
            personal_data_->GetDefaultCountryCodeForNewAddress());

  // Enabling Autofill blows away the cached value and should reflect the new
  // value (accounting for profiles).
  prefs::SetAutofillProfileEnabled(prefs_.get(), true);
  EXPECT_EQ(base::UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_COUNTRY)),
            personal_data_->GetDefaultCountryCodeForNewAddress());
}

TEST_F(PersonalDataManagerTest, DefaultCountryCodeComesFromProfiles) {
  AutofillProfile canadian_profile = test::GetFullCanadianProfile();
  ASSERT_EQ(canadian_profile.GetRawInfo(ADDRESS_HOME_COUNTRY), u"CA");
  AddProfileToPersonalDataManager(canadian_profile);
  ResetPersonalDataManager();
  EXPECT_EQ("CA", personal_data_->GetDefaultCountryCodeForNewAddress());

  // Multiple profiles cast votes.
  AutofillProfile us_profile1 = test::GetFullProfile();
  AutofillProfile us_profile2 = test::GetFullProfile2();
  ASSERT_EQ(us_profile1.GetRawInfo(ADDRESS_HOME_COUNTRY), u"US");
  ASSERT_EQ(us_profile2.GetRawInfo(ADDRESS_HOME_COUNTRY), u"US");
  AddProfileToPersonalDataManager(us_profile1);
  AddProfileToPersonalDataManager(us_profile2);
  ResetPersonalDataManager();
  EXPECT_EQ("US", personal_data_->GetDefaultCountryCodeForNewAddress());
}

TEST_F(PersonalDataManagerTest, DefaultCountryCodeComesFromVariations) {
  const std::string expected_country_code = "DE";
  const std::string unexpected_country_code = "FR";

  // Normally, the variation country code is passed to the constructor.
  personal_data_->set_variations_country_code_for_testing(
      expected_country_code);

  // Since there are no profiles set, the country code supplied buy variations
  // should be used get get a default country code.
  ASSERT_EQ(0u, personal_data_->GetProfiles().size());
  std::string actual_country_code =
      personal_data_->GetDefaultCountryCodeForNewAddress();
  EXPECT_EQ(expected_country_code, actual_country_code);

  // Set a new country code.
  // The default country code retrieved before should have been cached.
  personal_data_->set_variations_country_code_for_testing(
      unexpected_country_code);
  actual_country_code = personal_data_->GetDefaultCountryCodeForNewAddress();
  EXPECT_EQ(expected_country_code, actual_country_code);

  // Now a profile is set and the correct caching of the country code is
  // verified once more.
  AddProfileToPersonalDataManager(test::GetFullProfile());
  actual_country_code = personal_data_->GetDefaultCountryCodeForNewAddress();
  EXPECT_EQ(actual_country_code, expected_country_code);
}

TEST_F(PersonalDataManagerTest, UpdateLanguageCodeInProfile) {
  AutofillProfile profile;
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

// Test that profiles are not shown if |kAutofillProfileEnabled| is set to
// |false|.
TEST_F(PersonalDataManagerTest, GetProfilesToSuggest_ProfileAutofillDisabled) {
  const std::string kServerAddressId("server_address1");
  ASSERT_TRUE(TurnOnSyncFeature());

  // Add two different profiles, a local and a server one.
  AutofillProfile local_profile;
  test::SetProfileInfo(&local_profile, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5",
                       "Orlando", "FL", "32801", "US", "19482937549");
  AddProfileToPersonalDataManager(local_profile);

  // Disable Profile autofill.
  prefs::SetAutofillProfileEnabled(personal_data_->pref_service_, false);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  // Check that profiles were saved.
  const size_t expected_profiles = 1;
  EXPECT_EQ(expected_profiles, personal_data_->GetProfiles().size());
  // Expect no autofilled values or suggestions.
  EXPECT_EQ(0U, personal_data_->GetProfilesToSuggest().size());
}

// Test that local and server profiles are not loaded into memory on start-up if
// |kAutofillProfileEnabled| is set to |false|.
TEST_F(PersonalDataManagerTest,
       GetProfilesToSuggest_NoProfilesLoadedIfDisabled) {
  const std::string kServerAddressId("server_address1");
  ASSERT_TRUE(TurnOnSyncFeature());

  // Add two different profiles, a local and a server one.
  AutofillProfile local_profile;
  test::SetProfileInfo(&local_profile, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5",
                       "Orlando", "FL", "32801", "US", "19482937549");
  AddProfileToPersonalDataManager(local_profile);

  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  // Expect that all profiles are suggested.
  const size_t expected_profiles = 1;
  EXPECT_EQ(expected_profiles, personal_data_->GetProfiles().size());
  EXPECT_EQ(expected_profiles, personal_data_->GetProfilesToSuggest().size());

  // Disable Profile autofill.
  prefs::SetAutofillProfileEnabled(personal_data_->pref_service_, false);
  // Reload the database.
  ResetPersonalDataManager();

  // Expect no profile values or suggestions were loaded.
  EXPECT_EQ(0U, personal_data_->GetProfilesToSuggest().size());
}

// Test that local profiles are not added if |kAutofillProfileEnabled| is set to
// |false|.
TEST_F(PersonalDataManagerTest,
       GetProfilesToSuggest_NoProfilesAddedIfDisabled) {
  // Disable Profile autofill.
  prefs::SetAutofillProfileEnabled(personal_data_->pref_service_, false);

  // Add a local profile.
  AutofillProfile local_profile;
  test::SetProfileInfo(&local_profile, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5",
                       "Orlando", "FL", "32801", "US", "19482937549");
  AddProfileToPersonalDataManager(local_profile);

  // Expect no profile values or suggestions were added.
  EXPECT_EQ(0U, personal_data_->GetProfiles().size());
}

TEST_F(PersonalDataManagerTest, IsKnownCard_MatchesMaskedServerCard) {
  // Add a masked server card.
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::RecordType::kMaskedServerCard, "b459");
  test::SetCreditCardInfo(&server_cards.back(), "Emmet Dalton",
                          "2110" /* last 4 digits */, "12", "2999", "1");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  CreditCard cardToCompare;
  cardToCompare.SetNumber(u"4234 5678 9012 2110" /* Visa */);
  ASSERT_TRUE(personal_data_->IsKnownCard(cardToCompare));
}

TEST_F(PersonalDataManagerTest, IsKnownCard_MatchesFullServerCard) {
  // Add a full server card.
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::RecordType::kFullServerCard, "b459");
  test::SetCreditCardInfo(&server_cards.back(), "Emmet Dalton",
                          "4234567890122110" /* Visa */, "12", "2999", "1");

  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  CreditCard cardToCompare;
  cardToCompare.SetNumber(u"4234 5678 9012 2110" /* Visa */);
  ASSERT_TRUE(personal_data_->IsKnownCard(cardToCompare));
}

TEST_F(PersonalDataManagerTest, IsKnownCard_MatchesLocalCard) {
  // Add a local card.
  CreditCard credit_card0("287151C8-6AB1-487C-9095-28E80BE5DA15",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "Clyde Barrow",
                          "4234 5678 9012 2110" /* Visa */, "04", "2999", "1");
  personal_data_->AddCreditCard(credit_card0);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  CreditCard cardToCompare;
  cardToCompare.SetNumber(u"4234567890122110" /* Visa */);
  ASSERT_TRUE(personal_data_->IsKnownCard(cardToCompare));
}

TEST_F(PersonalDataManagerTest, IsKnownCard_TypeDoesNotMatch) {
  // Add a local card.
  CreditCard credit_card0("287151C8-6AB1-487C-9095-28E80BE5DA15",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "Clyde Barrow",
                          "4234 5678 9012 2110" /* Visa */, "04", "2999", "1");
  personal_data_->AddCreditCard(credit_card0);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  CreditCard cardToCompare;
  cardToCompare.SetNumber(u"5105 1051 0510 2110" /* American Express */);
  ASSERT_FALSE(personal_data_->IsKnownCard(cardToCompare));
}

TEST_F(PersonalDataManagerTest, IsKnownCard_LastFourDoesNotMatch) {
  // Add a local card.
  CreditCard credit_card0("287151C8-6AB1-487C-9095-28E80BE5DA15",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "Clyde Barrow",
                          "4234 5678 9012 2110" /* Visa */, "04", "2999", "1");
  personal_data_->AddCreditCard(credit_card0);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  CreditCard cardToCompare;
  cardToCompare.SetNumber(u"4234 5678 9012 0000" /* Visa */);
  ASSERT_FALSE(personal_data_->IsKnownCard(cardToCompare));
}

TEST_F(PersonalDataManagerTest, IsServerCard_DuplicateOfFullServerCard) {
  // Add a full server card.
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::RecordType::kFullServerCard, "b459");
  test::SetCreditCardInfo(&server_cards.back(), "Emmet Dalton",
                          "4234567890122110" /* Visa */, "12", "2999", "1");

  SetServerCards(server_cards);

  // Add a dupe local card of a full server card.
  CreditCard local_card("287151C8-6AB1-487C-9095-28E80BE5DA15",
                        test::kEmptyOrigin);
  test::SetCreditCardInfo(&local_card, "Emmet Dalton",
                          "4234 5678 9012 2110" /* Visa */, "12", "2999", "1");
  personal_data_->AddCreditCard(local_card);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_EQ(2U, personal_data_->GetCreditCards().size());

  CreditCard cardToCompare;
  cardToCompare.SetNumber(u"4234 5678 9012 2110" /* Visa */);
  ASSERT_TRUE(personal_data_->IsServerCard(&cardToCompare));
  ASSERT_TRUE(personal_data_->IsServerCard(&local_card));
}

TEST_F(PersonalDataManagerTest, IsServerCard_DuplicateOfMaskedServerCard) {
  // Add a masked server card.
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::RecordType::kMaskedServerCard, "b459");
  test::SetCreditCardInfo(&server_cards.back(), "Emmet Dalton",
                          "2110" /* last 4 digits */, "12", "2999", "1");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  SetServerCards(server_cards);

  // Add a dupe local card of a full server card.
  CreditCard local_card("287151C8-6AB1-487C-9095-28E80BE5DA15",
                        test::kEmptyOrigin);
  test::SetCreditCardInfo(&local_card, "Emmet Dalton",
                          "4234 5678 9012 2110" /* Visa */, "12", "2999", "1");
  personal_data_->AddCreditCard(local_card);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_EQ(2U, personal_data_->GetCreditCards().size());

  CreditCard cardToCompare;
  cardToCompare.SetNumber(u"4234 5678 9012 2110" /* Visa */);
  ASSERT_TRUE(personal_data_->IsServerCard(&cardToCompare));
  ASSERT_TRUE(personal_data_->IsServerCard(&local_card));
}

TEST_F(PersonalDataManagerTest, IsServerCard_AlreadyServerCard) {
  std::vector<CreditCard> server_cards;
  // Create a full server card.
  CreditCard full_server_card(CreditCard::RecordType::kFullServerCard, "c789");
  test::SetCreditCardInfo(&full_server_card, "Homer Simpson",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  server_cards.push_back(full_server_card);
  // Create a masked server card.
  CreditCard masked_card(CreditCard::RecordType::kMaskedServerCard, "a123");
  test::SetCreditCardInfo(&masked_card, "Homer Simpson", "2110" /* Visa */,
                          "01", "2999", "1");
  masked_card.SetNetworkForMaskedCard(kVisaCard);
  server_cards.push_back(masked_card);

  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_EQ(2U, personal_data_->GetCreditCards().size());

  ASSERT_TRUE(personal_data_->IsServerCard(&full_server_card));
  ASSERT_TRUE(personal_data_->IsServerCard(&masked_card));
}

TEST_F(PersonalDataManagerTest, IsServerCard_UniqueLocalCard) {
  // Add a unique local card.
  CreditCard local_card("1141084B-72D7-4B73-90CF-3D6AC154673B",
                        test::kEmptyOrigin);
  test::SetCreditCardInfo(&local_card, "Homer Simpson",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  personal_data_->AddCreditCard(local_card);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  ASSERT_FALSE(personal_data_->IsServerCard(&local_card));
}

// Test that local credit cards are ordered as expected.
TEST_F(PersonalDataManagerTest, GetCreditCardsToSuggest_LocalCardsRanking) {
  SetUpReferenceLocalCreditCards();

  // Sublabel is card number when filling name (exact format depends on
  // the platform, but the last 4 digits should appear).
  std::vector<CreditCard*> card_to_suggest =
      personal_data_->GetCreditCardsToSuggest();
  ASSERT_EQ(3U, card_to_suggest.size());

  // Ordered as expected.
  EXPECT_EQ(u"John Dillinger",
            card_to_suggest[0]->GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"Clyde Barrow",
            card_to_suggest[1]->GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"Bonnie Parker",
            card_to_suggest[2]->GetRawInfo(CREDIT_CARD_NAME_FULL));
}

// Test that local and server cards are ordered as expected.
TEST_F(PersonalDataManagerTest,
       GetCreditCardsToSuggest_LocalAndServerCardsRanking) {
  SetUpReferenceLocalCreditCards();

  // Add some server cards.
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::RecordType::kMaskedServerCard, "b459");
  test::SetCreditCardInfo(&server_cards.back(), "Emmet Dalton", "2110", "12",
                          "2999", "1");
  server_cards.back().set_use_count(2);
  server_cards.back().set_use_date(AutofillClock::Now() - base::Days(1));
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  server_cards.emplace_back(CreditCard::RecordType::kFullServerCard, "b460");
  test::SetCreditCardInfo(&server_cards.back(), "Jesse James", "2109", "12",
                          "2999", "1");
  server_cards.back().set_use_count(6);
  server_cards.back().set_use_date(AutofillClock::Now() - base::Days(1));

  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_EQ(5U, personal_data_->GetCreditCards().size());

  std::vector<CreditCard*> card_to_suggest =
      personal_data_->GetCreditCardsToSuggest();
  ASSERT_EQ(5U, card_to_suggest.size());

  // All cards should be ordered as expected.
  EXPECT_EQ(u"Jesse James",
            card_to_suggest[0]->GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"John Dillinger",
            card_to_suggest[1]->GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"Clyde Barrow",
            card_to_suggest[2]->GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"Emmet Dalton",
            card_to_suggest[3]->GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"Bonnie Parker",
            card_to_suggest[4]->GetRawInfo(CREDIT_CARD_NAME_FULL));
}

// Test that local and server cards are not shown if
// |kAutofillCreditCardEnabled| is set to |false|.
TEST_F(PersonalDataManagerTest,
       GetCreditCardsToSuggest_CreditCardAutofillDisabled) {
  SetUpReferenceLocalCreditCards();

  // Add some server cards.
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::RecordType::kMaskedServerCard, "b459");
  test::SetCreditCardInfo(&server_cards.back(), "Emmet Dalton", "2110", "12",
                          "2999", "1");
  server_cards.back().set_use_count(2);
  server_cards.back().set_use_date(AutofillClock::Now() - base::Days(1));
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  server_cards.emplace_back(CreditCard::RecordType::kFullServerCard, "b460");
  test::SetCreditCardInfo(&server_cards.back(), "Jesse James", "2109", "12",
                          "2999", "1");
  server_cards.back().set_use_count(6);
  server_cards.back().set_use_date(AutofillClock::Now() - base::Days(1));

  SetServerCards(server_cards);
  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  // Disable Credit card autofill.
  prefs::SetAutofillPaymentMethodsEnabled(personal_data_->pref_service_, false);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  // Check that profiles were saved.
  EXPECT_EQ(5U, personal_data_->GetCreditCards().size());
  // Expect no autofilled values or suggestions.
  EXPECT_EQ(0U, personal_data_->GetCreditCardsToSuggest().size());

  std::vector<CreditCard*> card_to_suggest =
      personal_data_->GetCreditCardsToSuggest();
  ASSERT_EQ(0U, card_to_suggest.size());
}

// Test that local and server cards are not loaded into memory on start-up if
// |kAutofillCreditCardEnabled| is set to |false|.
TEST_F(PersonalDataManagerTest,
       GetCreditCardsToSuggest_NoCardsLoadedIfDisabled) {
  SetUpReferenceLocalCreditCards();

  // Add some server cards.
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::RecordType::kMaskedServerCard, "b459");
  test::SetCreditCardInfo(&server_cards.back(), "Emmet Dalton", "2110", "12",
                          "2999", "1");
  server_cards.back().set_use_count(2);
  server_cards.back().set_use_date(AutofillClock::Now() - base::Days(1));
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  server_cards.emplace_back(CreditCard::RecordType::kFullServerCard, "b460");
  test::SetCreditCardInfo(&server_cards.back(), "Jesse James", "2109", "12",
                          "2999", "1");
  server_cards.back().set_use_count(6);
  server_cards.back().set_use_date(AutofillClock::Now() - base::Days(1));

  SetServerCards(server_cards);

  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  // Expect 5 autofilled values or suggestions.
  EXPECT_EQ(5U, personal_data_->GetCreditCards().size());

  // Disable Credit card autofill.
  prefs::SetAutofillPaymentMethodsEnabled(personal_data_->pref_service_, false);
  // Reload the database.
  ResetPersonalDataManager();

  // Expect no credit card values or suggestions were loaded.
  EXPECT_EQ(0U, personal_data_->GetCreditCardsToSuggest().size());

  std::vector<CreditCard*> card_to_suggest =
      personal_data_->GetCreditCardsToSuggest();
  ASSERT_EQ(0U, card_to_suggest.size());
}

// Test that local credit cards are not added if |kAutofillCreditCardEnabled| is
// set to |false|.
TEST_F(PersonalDataManagerTest,
       GetCreditCardsToSuggest_NoCreditCardsAddedIfDisabled) {
  // Disable Profile autofill.
  prefs::SetAutofillPaymentMethodsEnabled(personal_data_->pref_service_, false);

  // Add a local credit card.
  CreditCard credit_card("002149C1-EE28-4213-A3B9-DA243FFF021B",
                         "https://www.example.com");
  test::SetCreditCardInfo(&credit_card, "Bonnie Parker",
                          "5105105105105100" /* Mastercard */, "04", "2999",
                          "1");
  personal_data_->AddCreditCard(credit_card);

  // Expect no credit card values or suggestions were added.
  EXPECT_EQ(0U, personal_data_->GetCreditCards().size());
}

// Tests the suggestions of duplicate local and server credit cards.
TEST_F(PersonalDataManagerTest, GetCreditCardsToSuggest_ServerDuplicates) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillSuggestServerCardInsteadOfLocalCard);
  SetUpReferenceLocalCreditCards();

  // Add some server cards. If there are local dupes, the locals should be
  // hidden.
  std::vector<CreditCard> server_cards;
  // This server card matches a local card, except the local card is missing the
  // number. This should count as a dupe and thus not be shown in the
  // suggestions since the locally saved card takes precedence.
  server_cards.emplace_back(CreditCard::RecordType::kMaskedServerCard, "a123");
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "3456" /* Visa */, "01", "2999", "1");
  server_cards.back().set_use_count(2);
  server_cards.back().set_use_date(AutofillClock::Now() - base::Days(15));
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  // This unmasked server card is an exact dupe of a local card. Therefore only
  // this card should appear in the suggestions as full server cards have
  // precedence over local cards.
  server_cards.emplace_back(CreditCard::RecordType::kFullServerCard, "c789");
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  server_cards.back().set_use_count(1);
  server_cards.back().set_use_date(AutofillClock::Now() - base::Days(15));

  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_EQ(5U, personal_data_->GetCreditCards().size());

  std::vector<CreditCard*> card_to_suggest =
      personal_data_->GetCreditCardsToSuggest();
  ASSERT_EQ(3U, card_to_suggest.size());
  EXPECT_EQ(u"John Dillinger",
            card_to_suggest[0]->GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"Clyde Barrow",
            card_to_suggest[1]->GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"Bonnie Parker",
            card_to_suggest[2]->GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(CreditCard::RecordType::kLocalCard,
            card_to_suggest[0]->record_type());
  EXPECT_EQ(CreditCard::RecordType::kFullServerCard,
            card_to_suggest[1]->record_type());
  EXPECT_EQ(CreditCard::RecordType::kLocalCard,
            card_to_suggest[2]->record_type());
}

// Tests that a full server card can be a dupe of more than one local card.
TEST_F(PersonalDataManagerTest,
       GetCreditCardsToSuggest_ServerCardDuplicateOfMultipleLocalCards) {
  SetUpReferenceLocalCreditCards();

  // Add a duplicate server card.
  std::vector<CreditCard> server_cards;
  // This unmasked server card is an exact dupe of a local card. Therefore only
  // the local card should appear in the suggestions.
  server_cards.emplace_back(CreditCard::RecordType::kFullServerCard, "c789");
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");

  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_EQ(4U, personal_data_->GetCreditCards().size());

  std::vector<CreditCard*> card_to_suggest =
      personal_data_->GetCreditCardsToSuggest();
  ASSERT_EQ(3U, card_to_suggest.size());

  // Add a second dupe local card to make sure a full server card can be a dupe
  // of more than one local card.
  CreditCard credit_card3("4141084B-72D7-4B73-90CF-3D6AC154673B",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card3, "Clyde Barrow", "", "04", "", "");
  personal_data_->AddCreditCard(credit_card3);

  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  card_to_suggest = personal_data_->GetCreditCardsToSuggest();
  ASSERT_EQ(3U, card_to_suggest.size());
}

// Tests that only the full server card is kept when deduping with a local
// duplicate of it.
TEST_F(PersonalDataManagerTest,
       DedupeCreditCardToSuggest_FullServerShadowsLocal) {
  std::list<CreditCard*> credit_cards;

  // Create 3 different local credit cards.
  CreditCard local_card("287151C8-6AB1-487C-9095-28E80BE5DA15",
                        test::kEmptyOrigin);
  test::SetCreditCardInfo(&local_card, "Homer Simpson",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  local_card.set_use_count(3);
  local_card.set_use_date(AutofillClock::Now() - base::Days(1));
  credit_cards.push_back(&local_card);

  // Create a full server card that is a duplicate of one of the local cards.
  CreditCard full_server_card(CreditCard::RecordType::kFullServerCard, "c789");
  test::SetCreditCardInfo(&full_server_card, "Homer Simpson",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  full_server_card.set_use_count(1);
  full_server_card.set_use_date(AutofillClock::Now() - base::Days(15));
  credit_cards.push_back(&full_server_card);

  PersonalDataManager::DedupeCreditCardToSuggest(&credit_cards);
  ASSERT_EQ(1U, credit_cards.size());

  const CreditCard* deduped_card = credit_cards.front();
  EXPECT_TRUE(*deduped_card == full_server_card);
}

// Tests that only the local card is kept when deduping with a masked server
// duplicate of it or vice-versa. This is checked based on the value assigned
// during the for loop.
TEST_F(PersonalDataManagerTest,
       DedupeCreditCardToSuggest_BothLocalAndServerShadowsMaskedInTurns) {
  for (bool is_dedupe_experiment_active : {true, false}) {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatureState(
        features::kAutofillSuggestServerCardInsteadOfLocalCard,
        is_dedupe_experiment_active);
    std::list<CreditCard*> credit_cards;

    CreditCard local_card("1141084B-72D7-4B73-90CF-3D6AC154673B",
                          test::kEmptyOrigin);
    test::SetCreditCardInfo(&local_card, "Homer Simpson",
                            "4234567890123456" /* Visa */, "01", "2999", "1");
    credit_cards.push_back(&local_card);

    // Create a masked server card that is a duplicate of a local card.
    CreditCard masked_card(CreditCard::RecordType::kMaskedServerCard, "a123");
    test::SetCreditCardInfo(&masked_card, "Homer Simpson", "3456" /* Visa */,
                            "01", "2999", "1");
    masked_card.SetNetworkForMaskedCard(kVisaCard);
    credit_cards.push_back(&masked_card);

    PersonalDataManager::DedupeCreditCardToSuggest(&credit_cards);
    ASSERT_EQ(1U, credit_cards.size());

    const CreditCard* deduped_card = credit_cards.front();
    if (is_dedupe_experiment_active) {
      EXPECT_EQ(*deduped_card, masked_card);
    } else {
      EXPECT_EQ(*deduped_card, local_card);
    }
  }
}

// Tests that identical full server and masked credit cards are not deduped.
TEST_F(PersonalDataManagerTest, DedupeCreditCardToSuggest_FullServerAndMasked) {
  std::list<CreditCard*> credit_cards;

  // Create a full server card that is a duplicate of one of the local cards.
  CreditCard full_server_card(CreditCard::RecordType::kFullServerCard, "c789");
  test::SetCreditCardInfo(&full_server_card, "Homer Simpson",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  full_server_card.set_use_count(1);
  full_server_card.set_use_date(AutofillClock::Now() - base::Days(15));
  credit_cards.push_back(&full_server_card);

  // Create a masked server card that is a duplicate of a local card.
  CreditCard masked_card(CreditCard::RecordType::kMaskedServerCard, "a123");
  test::SetCreditCardInfo(&masked_card, "Homer Simpson", "3456" /* Visa */,
                          "01", "2999", "1");
  masked_card.set_use_count(2);
  masked_card.set_use_date(AutofillClock::Now() - base::Days(15));
  masked_card.SetNetworkForMaskedCard(kVisaCard);
  credit_cards.push_back(&masked_card);

  PersonalDataManager::DedupeCreditCardToSuggest(&credit_cards);
  EXPECT_EQ(2U, credit_cards.size());
}

// Tests that different local, masked, and full server credit cards are not
// deduped.
TEST_F(PersonalDataManagerTest, DedupeCreditCardToSuggest_DifferentCards) {
  std::list<CreditCard*> credit_cards;

  CreditCard local_card("002149C1-EE28-4213-A3B9-DA243FFF021B",
                        test::kEmptyOrigin);
  local_card.set_use_count(1);
  local_card.set_use_date(AutofillClock::Now() - base::Days(1));
  test::SetCreditCardInfo(&local_card, "Homer Simpson",
                          "5105105105105100" /* Mastercard */, "", "", "");
  credit_cards.push_back(&local_card);

  // Create a masked server card that is different from the local card.
  CreditCard masked_card(CreditCard::RecordType::kMaskedServerCard, "b456");
  test::SetCreditCardInfo(&masked_card, "Homer Simpson", "0005", "12", "2999",
                          "1");
  masked_card.set_use_count(3);
  masked_card.set_use_date(AutofillClock::Now() - base::Days(15));
  // credit_card4.SetNetworkForMaskedCard(kVisaCard);
  credit_cards.push_back(&masked_card);

  // Create a full server card that is slightly different of the two other
  // cards.
  CreditCard full_server_card(CreditCard::RecordType::kFullServerCard, "c789");
  test::SetCreditCardInfo(&full_server_card, "Homer Simpson",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  full_server_card.set_use_count(1);
  full_server_card.set_use_date(AutofillClock::Now() - base::Days(15));
  credit_cards.push_back(&full_server_card);

  PersonalDataManager::DedupeCreditCardToSuggest(&credit_cards);
  EXPECT_EQ(3U, credit_cards.size());
}

TEST_F(PersonalDataManagerTest, RecordUseOf) {
  // Create the test clock and set the time to a specific value.
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);

  auto Check = [](const AutofillDataModel& data_model, size_t use_count,
                  base::Time use_date, base::Time modification_date) {
    EXPECT_EQ(use_count, data_model.use_count());
    EXPECT_EQ(use_date, data_model.use_date());
    EXPECT_EQ(modification_date, data_model.modification_date());
  };

  AutofillProfile profile(test::GetFullProfile());
  Check(profile, 1u, kArbitraryTime, kArbitraryTime);
  AddProfileToPersonalDataManager(profile);

  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card, "John Dillinger",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  Check(credit_card, 1u, kArbitraryTime, kArbitraryTime);
  personal_data_->AddCreditCard(credit_card);

  // Make sure everything is set up correctly.
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  // Set the current time to another value.
  test_clock.SetNow(kSomeLaterTime);

  // Notify the PDM that the profile and credit card were used.
  AutofillProfile* added_profile =
      personal_data_->GetProfileByGUID(profile.guid());
  ASSERT_TRUE(added_profile);
  EXPECT_EQ(*added_profile, profile);
  Check(*added_profile, 1u, kArbitraryTime, kArbitraryTime);

  CreditCard* added_card =
      personal_data_->GetCreditCardByGUID(credit_card.guid());
  ASSERT_TRUE(added_card);
  EXPECT_EQ(*added_card, credit_card);
  Check(*added_card, 1u, kArbitraryTime, kArbitraryTime);

  // Use |profile|, then verify usage stats.
  {
    PersonalDataProfileTaskWaiter waiter(*personal_data_);
    EXPECT_CALL(waiter.mock_observer(), OnPersonalDataChanged());
    personal_data_->RecordUseOf(&profile);
    std::move(waiter).Wait();
  }

  added_profile = personal_data_->GetProfileByGUID(profile.guid());
  added_card = personal_data_->GetCreditCardByGUID(credit_card.guid());
  ASSERT_TRUE(added_profile);
  ASSERT_TRUE(added_card);
  Check(*added_profile, 2u, kSomeLaterTime, kArbitraryTime);
  Check(*added_card, 1u, kArbitraryTime, kArbitraryTime);

  // Use |credit_card|, then verify usage stats.
  {
    PersonalDataProfileTaskWaiter waiter(*personal_data_);
    EXPECT_CALL(waiter.mock_observer(), OnPersonalDataChanged());
    personal_data_->RecordUseOf(&credit_card);
    std::move(waiter).Wait();
  }

  added_profile = personal_data_->GetProfileByGUID(profile.guid());
  added_card = personal_data_->GetCreditCardByGUID(credit_card.guid());
  ASSERT_TRUE(added_profile);
  ASSERT_TRUE(added_card);
  Check(*added_profile, 2u, kSomeLaterTime, kArbitraryTime);
  Check(*added_card, 2u, kSomeLaterTime, kArbitraryTime);
}

TEST_F(PersonalDataManagerTest, ClearAllLocalData) {
  // Add some local data.
  AddProfileToPersonalDataManager(test::GetFullProfile());
  personal_data_->AddCreditCard(test::GetCreditCard());
  personal_data_->Refresh();

  // The card and profile should be there.
  ResetPersonalDataManager();
  EXPECT_FALSE(personal_data_->GetCreditCards().empty());
  EXPECT_FALSE(personal_data_->GetProfiles().empty());

  personal_data_->ClearAllLocalData();

  // Reload the database, everything should be gone.
  ResetPersonalDataManager();
  EXPECT_TRUE(personal_data_->GetCreditCards().empty());
  EXPECT_TRUE(personal_data_->GetProfiles().empty());
}

TEST_F(PersonalDataManagerTest, DeleteLocalCreditCards) {
  CreditCard credit_card1(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card1, "Alice",
                          "378282246310005" /* American Express */, "04",
                          "2020", "1");
  CreditCard credit_card2(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card2, "Ben",
                          "378282246310006" /* American Express */, "04",
                          "2021", "1");
  CreditCard credit_card3(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card3, "Clyde",
                          "5105105105105100" /* Mastercard */, "04", "2022",
                          "1");
  std::vector<CreditCard> cards;
  cards.push_back(credit_card1);
  cards.push_back(credit_card2);

  personal_data_->AddCreditCard(credit_card1);
  personal_data_->AddCreditCard(credit_card2);
  personal_data_->AddCreditCard(credit_card3);

  personal_data_->DeleteLocalCreditCards(cards);

  // Wait for the data to be refreshed.
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  std::unordered_set<std::u16string> expectedToRemain = {u"Clyde"};
  for (auto* card : personal_data_->GetCreditCards()) {
    EXPECT_NE(expectedToRemain.end(),
              expectedToRemain.find(card->GetRawInfo(CREDIT_CARD_NAME_FULL)));
  }
}

TEST_F(PersonalDataManagerTest, DeleteAllLocalCreditCards) {
  SetUpReferenceLocalCreditCards();

  // Expect 3 local credit cards.
  EXPECT_EQ(3U, personal_data_->GetLocalCreditCards().size());

  personal_data_->DeleteAllLocalCreditCards();

  // Wait for the data to be refreshed.
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  // Expect the local credit cards to have been deleted.
  EXPECT_EQ(0U, personal_data_->GetLocalCreditCards().size());
}

TEST_F(PersonalDataManagerTest, LogStoredCreditCardMetrics) {
  ASSERT_EQ(0U, personal_data_->GetCreditCards().size());

  // Helper timestamps for setting up the test data.
  base::Time now = AutofillClock::Now();
  base::Time one_month_ago = now - base::Days(30);
  base::Time::Exploded one_month_ago_exploded;
  one_month_ago.LocalExplode(&one_month_ago_exploded);

  std::vector<CreditCard> server_cards;
  server_cards.reserve(10);

  // Create in-use and in-disuse cards of each record type.
  const std::vector<CreditCard::RecordType> record_types{
      CreditCard::RecordType::kLocalCard,
      CreditCard::RecordType::kMaskedServerCard,
      CreditCard::RecordType::kFullServerCard};
  for (auto record_type : record_types) {
    // Create a card that's still in active use.
    CreditCard card_in_use = test::GetRandomCreditCard(record_type);
    card_in_use.set_use_date(now - base::Days(30));
    card_in_use.set_use_count(10);

    // Create a card that's not in active use.
    CreditCard card_in_disuse = test::GetRandomCreditCard(record_type);
    card_in_disuse.SetExpirationYear(one_month_ago_exploded.year);
    card_in_disuse.SetExpirationMonth(one_month_ago_exploded.month);
    card_in_disuse.set_use_date(now - base::Days(200));
    card_in_disuse.set_use_count(10);

    // Add the cards to the personal data manager in the appropriate way.
    if (record_type == CreditCard::RecordType::kLocalCard) {
      personal_data_->AddCreditCard(card_in_use);
      personal_data_->AddCreditCard(card_in_disuse);
    } else {
      server_cards.push_back(std::move(card_in_use));
      server_cards.push_back(std::move(card_in_disuse));
    }
  }

  // Sets the virtual card enrollment state for the first three server cards.
  server_cards[0].set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  server_cards[0].set_card_art_url(GURL("https://www.example.com/image1"));
  server_cards[1].set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  server_cards[1].set_card_art_url(GURL("https://www.example.com/image1"));
  server_cards[2].set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  server_cards[2].set_card_art_url(GURL("https://www.example.com/image2"));

  SetServerCards(server_cards);

  // SetServerCards modifies the metadata (use_count and use_date)
  // of unmasked cards. Reset the server card metadata to match the data set
  // up above.
  for (const auto& card : server_cards)
    account_autofill_table_->UpdateServerCardMetadata(card);

  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  ASSERT_EQ(6U, personal_data_->GetCreditCards().size());

  // Reload the database, which will log the stored profile counts.
  base::HistogramTester histogram_tester;
  ResetPersonalDataManager();

  EXPECT_EQ(personal_data_->GetServerCardWithArtImageCount(), 3U);

  ASSERT_EQ(6U, personal_data_->GetCreditCards().size());

  // Validate the basic count metrics for both local and server cards. Deep
  // validation of the metrics is done in:
  //    AutofillMetricsTest::LogStoredCreditCardMetrics
  histogram_tester.ExpectTotalCount("Autofill.StoredCreditCardCount", 1);
  histogram_tester.ExpectTotalCount("Autofill.StoredCreditCardCount.Local", 1);
  histogram_tester.ExpectTotalCount("Autofill.StoredCreditCardCount.Server", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredCreditCardCount.Server.Masked", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredCreditCardCount.Server.Unmasked", 1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardCount", 6, 1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardCount.Local", 2,
                                     1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardCount.Server", 4,
                                     1);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardCount.Server.Masked", 2, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardCount.Server.Unmasked", 2, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredCreditCardCount.Server.WithVirtualCardMetadata", 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardCount.Server.WithCardArtImage", 3, 1);
}

// Test that setting a null sync service returns only local credit cards.
TEST_F(PersonalDataManagerTest, GetCreditCards_NoSyncService) {
  base::HistogramTester histogram_tester;
  SetUpThreeCardTypes();

  // Set no sync service.
  personal_data_->SetSyncServiceForTest(nullptr);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  // No sync service is the same as payments integration being disabled, i.e.
  // IsAutofillWalletImportEnabled() returning false. Only local credit
  // cards are shown.
  EXPECT_EQ(0U, personal_data_->GetServerCreditCards().size());
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());
}

// Test that setting a sync service in auth error returns only local credit
// cards.
TEST_F(PersonalDataManagerTest, GetCreditCards_NotActiveSyncService) {
  base::HistogramTester histogram_tester;
  SetUpThreeCardTypes();

  // Set a sync service in auth error.
  syncer::TestSyncService sync_service;
  sync_service.SetPersistentAuthError();
  personal_data_->SetSyncServiceForTest(&sync_service);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  // Remove the auth error to be able to get the server cards.
  sync_service.ClearAuthError();

  // Check that cards were masked and other were untouched.
  EXPECT_EQ(3U, personal_data_->GetCreditCards().size());
  std::vector<CreditCard*> server_cards =
      personal_data_->GetServerCreditCards();
  EXPECT_EQ(2U, server_cards.size());
  for (CreditCard* card : server_cards)
    EXPECT_TRUE(card->record_type() ==
                CreditCard::RecordType::kMaskedServerCard);

  // Call OnSyncShutdown to ensure removing observer added by
  // SetSyncServiceForTest.
  personal_data_->OnSyncShutdown(&sync_service);
}

// Sync Transport mode is only for Win, Mac, and Linux.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
TEST_F(PersonalDataManagerSyncTransportModeTest,
       ServerCardsShowInTransportMode) {
  SetUpThreeCardTypes();

  CoreAccountInfo active_info =
      identity_test_env_.identity_manager()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);

  // Opt-in to seeing server card in sync transport mode.
  ::autofill::prefs::SetUserOptedInWalletSyncTransport(
      prefs_.get(), active_info.account_id, true);

  // Check that the server cards are available for suggestion.
  EXPECT_EQ(3U, personal_data_->GetCreditCards().size());
  EXPECT_EQ(3U, personal_data_->GetCreditCardsToSuggest().size());
  EXPECT_EQ(1U, personal_data_->GetLocalCreditCards().size());
  EXPECT_EQ(2U, personal_data_->GetServerCreditCards().size());

  // Stop Wallet sync.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged());
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet());

  // Check that server cards are unavailable.
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());
  EXPECT_EQ(1U, personal_data_->GetCreditCardsToSuggest().size());
  EXPECT_EQ(1U, personal_data_->GetLocalCreditCards().size());
  EXPECT_EQ(0U, personal_data_->GetServerCreditCards().size());
}

// Make sure that the opt in is necessary to show server cards if the
// appropriate feature is disabled.
TEST_F(PersonalDataManagerSyncTransportModeTest,
       ServerCardsShowInTransportMode_NeedOptIn) {
  SetUpThreeCardTypes();

  CoreAccountInfo active_info =
      identity_test_env_.identity_manager()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);

  // The server cards should not be available at first. The user needs to
  // accept the opt-in offer.
  EXPECT_EQ(3U, personal_data_->GetCreditCards().size());
  EXPECT_EQ(1U, personal_data_->GetCreditCardsToSuggest().size());
  EXPECT_EQ(1U, personal_data_->GetLocalCreditCards().size());
  EXPECT_EQ(2U, personal_data_->GetServerCreditCards().size());

  // Opt-in to seeing server card in sync transport mode.
  ::autofill::prefs::SetUserOptedInWalletSyncTransport(
      prefs_.get(), active_info.account_id, true);

  // Check that the server cards are available for suggestion.
  EXPECT_EQ(3U, personal_data_->GetCreditCards().size());
  EXPECT_EQ(3U, personal_data_->GetCreditCardsToSuggest().size());
  EXPECT_EQ(1U, personal_data_->GetLocalCreditCards().size());
  EXPECT_EQ(2U, personal_data_->GetServerCreditCards().size());
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

// Tests that all the non settings origins of autofill credit cards are cleared
// even if sync is disabled.
TEST_F(
    PersonalDataManagerTest,
    SyncServiceInitializedWithAutofillDisabled_ClearCreditCardNonSettingsOrigins) {
  // Create a card with a non-settings, non-empty origin.
  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         "https://www.example.com");
  test::SetCreditCardInfo(&credit_card, "Bob0",
                          "5105105105105100" /* Mastercard */, "04", "1999",
                          "1");
  personal_data_->AddCreditCard(credit_card);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  // Turn off payments sync.
  syncer::UserSelectableTypeSet user_selectable_type_set =
      sync_service_.GetUserSettings()->GetSelectedTypes();
  user_selectable_type_set.Remove(syncer::UserSelectableType::kPayments);
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/user_selectable_type_set);

  // The credit card should still exist.
  ASSERT_EQ(1U, personal_data_->GetCreditCards().size());

  // Reload the personal data manager.
  ResetPersonalDataManager();

  // The credit card should still exist.
  ASSERT_EQ(1U, personal_data_->GetCreditCards().size());

  // The card's origin should be cleared
  EXPECT_TRUE(personal_data_->GetCreditCards()[0]->origin().empty());
}

// Sanity check that the mode where we use the regular, persistent storage for
// cards still works.
TEST_F(PersonalDataManagerTest, UsePersistentServerStorage) {
  ASSERT_TRUE(identity_test_env_.identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSync));
  ASSERT_TRUE(sync_service_.HasSyncConsent());
  SetUpThreeCardTypes();

  EXPECT_EQ(3U, personal_data_->GetCreditCards().size());
  EXPECT_EQ(3U, personal_data_->GetCreditCardsToSuggest().size());
  EXPECT_EQ(1U, personal_data_->GetLocalCreditCards().size());
  EXPECT_EQ(2U, personal_data_->GetServerCreditCards().size());
}

// Verify that PDM can switch at runtime between the different storages.
TEST_F(PersonalDataManagerSyncTransportModeTest, SwitchServerStorages) {
  // Start with account storage.
  SetUpThreeCardTypes();

  // Check that we do have 2 server cards, as expected.
  ASSERT_EQ(2U, personal_data_->GetServerCreditCards().size());

  // Switch to persistent storage.
  sync_service_.SetHasSyncConsent(true);
  personal_data_->OnStateChanged(&sync_service_);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  EXPECT_EQ(0U, personal_data_->GetServerCreditCards().size());

  CreditCard server_card;
  test::SetCreditCardInfo(&server_card, "Server Card",
                          "4234567890123456",  // Visa
                          "04", "2999", "1");
  server_card.set_guid("00000000-0000-0000-0000-000000000007");
  server_card.set_record_type(CreditCard::RecordType::kFullServerCard);
  server_card.set_server_id("server_id");
  personal_data_->AddFullServerCreditCard(server_card);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  EXPECT_EQ(1U, personal_data_->GetServerCreditCards().size());

  // Switch back to the account storage.
  sync_service_.SetHasSyncConsent(false);
  personal_data_->OnStateChanged(&sync_service_);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  EXPECT_EQ(2U, personal_data_->GetServerCreditCards().size());
}

// Sanity check that the mode where we use the regular, persistent storage for
// cards still works.
TEST_F(PersonalDataManagerSyncTransportModeTest,
       UseCorrectStorageForDifferentCards) {
  // Add a server card.
  CreditCard server_card;
  test::SetCreditCardInfo(&server_card, "Server Card",
                          "4234567890123456",  // Visa
                          "04", "2999", "1");
  server_card.set_guid("00000000-0000-0000-0000-000000000007");
  server_card.set_record_type(CreditCard::RecordType::kFullServerCard);
  server_card.set_server_id("server_id");
  personal_data_->AddFullServerCreditCard(server_card);

  // Set server card metadata.
  server_card.set_use_count(15);
  personal_data_->UpdateServerCardsMetadata({server_card});

  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  // Expect that the server card is stored in the account autofill table.
  std::vector<std::unique_ptr<CreditCard>> cards;
  account_autofill_table_->GetServerCreditCards(&cards);
  EXPECT_EQ(1U, cards.size());
  EXPECT_EQ(server_card.LastFourDigits(), cards[0]->LastFourDigits());

  // Add a local card.
  CreditCard local_card;
  test::SetCreditCardInfo(&local_card, "Freddy Mercury",
                          "4234567890123463",  // Visa
                          "08", "2999", "1");
  local_card.set_guid("00000000-0000-0000-0000-000000000009");
  local_card.set_record_type(CreditCard::RecordType::kLocalCard);
  local_card.set_use_date(AutofillClock::Now() - base::Days(5));
  personal_data_->AddCreditCard(local_card);

  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  // Expect that the local card is stored in the profile autofill table.
  profile_autofill_table_->GetCreditCards(&cards);
  EXPECT_EQ(1U, cards.size());
  EXPECT_EQ(local_card.LastFourDigits(), cards[0]->LastFourDigits());

  // Add a local profile
  AutofillProfile profile;
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  AddProfileToPersonalDataManager(profile);

  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  // Expect that a profile is stored in the profile autofill table.
  profile_autofill_table_->GetAutofillProfiles(
      AutofillProfile::Source::kLocalOrSyncable, &profiles);
  EXPECT_EQ(1U, profiles.size());
  EXPECT_EQ(profile, *profiles[0]);
}

// Tests that the least recently used profile of two existing profiles is
// deleted, when an update of one of the profiles makes it a duplicate of the
// other, already existing profile. Here, the less recently used profile is
// edited to become a duplicate of the more recently used profile.
TEST_F(PersonalDataManagerTest, CreateDuplicateWithAnUpdate) {
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
TEST_F(PersonalDataManagerTest,
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
  PersonalDataProfileTaskWaiter update_waiter(*personal_data_);
  // Expect an update and a deletion.
  EXPECT_CALL(update_waiter.mock_observer(), OnPersonalDataChanged()).Times(2);
  personal_data_->UpdateProfile(updated_more_recently_used_profile);
  std::move(update_waiter).Wait();

  // Verify that less recently used profile was removed.
  ASSERT_EQ(personal_data_->GetProfiles().size(), 1U);

  EXPECT_EQ(*personal_data_->GetProfiles()[0],
            updated_more_recently_used_profile);
  EXPECT_EQ(personal_data_->GetProfiles()[0]->use_date(), newer_use_data);
}

TEST_F(PersonalDataManagerTest, GetAccountInfoForPaymentsServer) {
  // Make the IdentityManager return a non-empty AccountInfo when
  // GetPrimaryAccountInfo() is called.
  std::string sync_account_email =
      identity_test_env_.identity_manager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
          .email;
  ASSERT_FALSE(sync_account_email.empty());

  // Make the sync service returns consistent AccountInfo when GetAccountInfo()
  // is called.
  ASSERT_EQ(sync_service_.GetAccountInfo().email, sync_account_email);

  // The Active Sync AccountInfo should be returned.
  EXPECT_EQ(sync_account_email,
            personal_data_->GetAccountInfoForPaymentsServer().email);
}

TEST_F(PersonalDataManagerTest, OnAccountsCookieDeletedByUserAction) {
  // Set up some sync transport opt-ins in the prefs.
  ::autofill::prefs::SetUserOptedInWalletSyncTransport(
      prefs_.get(), CoreAccountId::FromGaiaId("account1"), true);
  EXPECT_FALSE(prefs_->GetDict(prefs::kAutofillSyncTransportOptIn).empty());

  // Simulate that the cookies get cleared by the user.
  personal_data_->OnAccountsCookieDeletedByUserAction();

  // Make sure the pref is now empty.
  EXPECT_TRUE(prefs_->GetDict(prefs::kAutofillSyncTransportOptIn).empty());
}

TEST_F(PersonalDataManagerTest, SaveProfileMigrationStrikes) {
  EXPECT_FALSE(personal_data_->IsProfileMigrationBlocked(kGuid));

  personal_data_->AddStrikeToBlockProfileMigration(kGuid);
  EXPECT_FALSE(personal_data_->IsProfileMigrationBlocked(kGuid));

  personal_data_->AddStrikeToBlockProfileMigration(kGuid);
  EXPECT_FALSE(personal_data_->IsProfileMigrationBlocked(kGuid));

  // After the third strike, the guid should be blocked.
  personal_data_->AddStrikeToBlockProfileMigration(kGuid);
  EXPECT_TRUE(personal_data_->IsProfileMigrationBlocked(kGuid));

  // Until the strikes are removed again.
  personal_data_->RemoveStrikesToBlockProfileMigration(kGuid);
  EXPECT_FALSE(personal_data_->IsProfileMigrationBlocked(kGuid));

  // `AddMaxStrikesToBlockProfileMigration()` should add sufficiently many
  // strikes.
  personal_data_->AddMaxStrikesToBlockProfileMigration(kGuid);
  EXPECT_TRUE(personal_data_->IsProfileMigrationBlocked(kGuid));
}

TEST_F(PersonalDataManagerTest, SaveProfileUpdateStrikes) {
  EXPECT_FALSE(personal_data_->IsProfileUpdateBlocked(kGuid));

  personal_data_->AddStrikeToBlockProfileUpdate(kGuid);
  EXPECT_FALSE(personal_data_->IsProfileUpdateBlocked(kGuid));

  personal_data_->AddStrikeToBlockProfileUpdate(kGuid);
  EXPECT_FALSE(personal_data_->IsProfileUpdateBlocked(kGuid));

  // After the third strike, the guid should be blocked.
  personal_data_->AddStrikeToBlockProfileUpdate(kGuid);
  EXPECT_TRUE(personal_data_->IsProfileUpdateBlocked(kGuid));

  // Until the strikes are removed again.
  personal_data_->RemoveStrikesToBlockProfileUpdate(kGuid);
  EXPECT_FALSE(personal_data_->IsProfileUpdateBlocked(kGuid));
}

TEST_F(PersonalDataManagerTest, SaveProfileSaveStrikes) {
  GURL domain("https://www.block.me/index.html");

  EXPECT_FALSE(personal_data_->IsNewProfileImportBlockedForDomain(domain));

  personal_data_->AddStrikeToBlockNewProfileImportForDomain(domain);
  EXPECT_FALSE(personal_data_->IsNewProfileImportBlockedForDomain(domain));

  personal_data_->AddStrikeToBlockNewProfileImportForDomain(domain);
  EXPECT_FALSE(personal_data_->IsNewProfileImportBlockedForDomain(domain));

  // After the third strike, the domain should be blocked.
  personal_data_->AddStrikeToBlockNewProfileImportForDomain(domain);
  EXPECT_TRUE(personal_data_->IsNewProfileImportBlockedForDomain(domain));

  // Until the strikes are removed again.
  personal_data_->RemoveStrikesToBlockNewProfileImportForDomain(domain);
  EXPECT_FALSE(personal_data_->IsNewProfileImportBlockedForDomain(domain));
}

TEST_F(PersonalDataManagerTest, ClearFullBrowsingHistory) {
  GURL domain("https://www.block.me/index.html");

  personal_data_->AddStrikeToBlockNewProfileImportForDomain(domain);
  personal_data_->AddStrikeToBlockNewProfileImportForDomain(domain);
  personal_data_->AddStrikeToBlockNewProfileImportForDomain(domain);
  EXPECT_TRUE(personal_data_->IsNewProfileImportBlockedForDomain(domain));

  history::DeletionInfo deletion_info = history::DeletionInfo::ForAllHistory();

  personal_data_->OnURLsDeleted(/*history_service=*/nullptr, deletion_info);

  EXPECT_FALSE(personal_data_->IsNewProfileImportBlockedForDomain(domain));
}

TEST_F(PersonalDataManagerTest, ClearUrlsFromBrowsingHistory) {
  GURL first_url("https://www.block.me/index.html");
  GURL second_url("https://www.block.too/index.html");

  // Add strikes to block both domains.
  personal_data_->AddStrikeToBlockNewProfileImportForDomain(first_url);
  personal_data_->AddStrikeToBlockNewProfileImportForDomain(first_url);
  personal_data_->AddStrikeToBlockNewProfileImportForDomain(first_url);
  EXPECT_TRUE(personal_data_->IsNewProfileImportBlockedForDomain(first_url));

  personal_data_->AddStrikeToBlockNewProfileImportForDomain(second_url);
  personal_data_->AddStrikeToBlockNewProfileImportForDomain(second_url);
  personal_data_->AddStrikeToBlockNewProfileImportForDomain(second_url);
  EXPECT_TRUE(personal_data_->IsNewProfileImportBlockedForDomain(second_url));

  history::URLRows deleted_urls = {history::URLRow(first_url)};

  history::DeletionInfo deletion_info =
      history::DeletionInfo::ForUrls(deleted_urls, {});

  personal_data_->OnURLsDeleted(/*history_service=*/nullptr, deletion_info);

  // The strikes for `domain` should be deleted, but the strikes for
  // `another_domain` should not.
  EXPECT_FALSE(personal_data_->IsNewProfileImportBlockedForDomain(first_url));
  EXPECT_TRUE(personal_data_->IsNewProfileImportBlockedForDomain(second_url));
}

TEST_F(PersonalDataManagerTest, ClearUrlsFromBrowsingHistoryInTimeRange) {
  GURL first_url("https://www.block.me/index.html");
  GURL second_url("https://www.block.too/index.html");

  TestAutofillClock test_clock;

  // Add strikes to block both domains.
  personal_data_->AddStrikeToBlockNewProfileImportForDomain(first_url);
  personal_data_->AddStrikeToBlockNewProfileImportForDomain(first_url);
  personal_data_->AddStrikeToBlockNewProfileImportForDomain(first_url);
  personal_data_->AddStrikeToBlockNewProfileImportForDomain(second_url);
  personal_data_->AddStrikeToBlockNewProfileImportForDomain(second_url);
  EXPECT_TRUE(personal_data_->IsNewProfileImportBlockedForDomain(first_url));

  test_clock.Advance(base::Hours(1));
  base::Time end_of_deletion = AutofillClock::Now();
  test_clock.Advance(base::Hours(1));

  personal_data_->AddStrikeToBlockNewProfileImportForDomain(second_url);
  EXPECT_TRUE(personal_data_->IsNewProfileImportBlockedForDomain(second_url));

  history::URLRows deleted_urls = {history::URLRow(first_url),
                                   history::URLRow(second_url)};

  history::DeletionInfo deletion_info(
      history::DeletionTimeRange(base::Time::Min(), end_of_deletion), false,
      deleted_urls, {},
      absl::make_optional<std::set<GURL>>({first_url, second_url}));

  personal_data_->OnURLsDeleted(/*history_service=*/nullptr, deletion_info);

  // The strikes for `first_url` should be deleted because the strikes have been
  // added within the deletion time range.
  EXPECT_FALSE(personal_data_->IsNewProfileImportBlockedForDomain(first_url));
  // The last strike for 'second_url' was collected after the deletion time
  // range and therefore, the blocking should prevail.
  EXPECT_TRUE(personal_data_->IsNewProfileImportBlockedForDomain(second_url));
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PersonalDataManagerSyncTransportModeTest,
       ShouldShowCardsFromAccountOption) {
  // The method should return false if one of these is not respected:
  //   * The sync_service is not null
  //   * The sync feature is not enabled
  //   * The user has server cards
  //   * The user has not opted-in to seeing their account cards
  // Start by setting everything up, then making each of these conditions false
  // independently, one by one.

  // Set everything up so that the proposition should be shown.

  // Set a server credit card.
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::RecordType::kFullServerCard, "c789");
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  SetServerCards(server_cards);
  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  // Make sure the function returns true.
  EXPECT_TRUE(personal_data_->ShouldShowCardsFromAccountOption());

  // Set that the user already opted-in. Check that the function now returns
  // false.
  CoreAccountId account_id =
      identity_test_env_.identity_manager()->GetPrimaryAccountId(
          signin::ConsentLevel::kSignin);
  ::autofill::prefs::SetUserOptedInWalletSyncTransport(prefs_.get(), account_id,
                                                       true);
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());

  // Re-opt the user out. Check that the function now returns true.
  ::autofill::prefs::SetUserOptedInWalletSyncTransport(prefs_.get(), account_id,
                                                       false);
  EXPECT_TRUE(personal_data_->ShouldShowCardsFromAccountOption());

  // Set that the user has no server cards. Check that the function now returns
  // false.
  SetServerCards({});
  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());

  // Re-set some server cards. Check that the function now returns true.
  SetServerCards(server_cards);
  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_TRUE(personal_data_->ShouldShowCardsFromAccountOption());

  // Set that the user enabled the sync feature. Check that the function now
  // returns false.
  sync_service_.SetHasSyncConsent(true);
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());

  // Re-disable the sync feature. Check that the function now returns true.
  sync_service_.SetHasSyncConsent(false);
  EXPECT_TRUE(personal_data_->ShouldShowCardsFromAccountOption());

  // Set a null sync service. Check that the function now returns false.
  personal_data_->SetSyncServiceForTest(nullptr);
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());
}
#else   // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS) &&
        // !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PersonalDataManagerSyncTransportModeTest,
       ShouldShowCardsFromAccountOption) {
  // The method should return false if one of these is not respected:
  //   * The sync_service is not null
  //   * The sync feature is not enabled
  //   * The user has server cards
  //   * The user has not opted-in to seeing their account cards
  // Start by setting everything up, then making each of these conditions false
  // independently, one by one.

  // Set everything up so that the proposition should be shown on Desktop.

  // Set a server credit card.
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::RecordType::kFullServerCard, "c789");
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  SetServerCards(server_cards);
  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  // Make sure the function returns false.
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());

  // Set that the user already opted-in. Check that the function still returns
  // false.
  CoreAccountId account_id =
      identity_test_env_.identity_manager()->GetPrimaryAccountId(
          signin::ConsentLevel::kSignin);
  ::autofill::prefs::SetUserOptedInWalletSyncTransport(prefs_.get(), account_id,
                                                       true);
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());

  // Re-opt the user out. Check that the function now returns true.
  ::autofill::prefs::SetUserOptedInWalletSyncTransport(prefs_.get(), account_id,
                                                       false);
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());

  // Set that the user has no server cards. Check that the function still
  // returns false.
  SetServerCards({});
  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());

  // Re-set some server cards. Check that the function still returns false.
  SetServerCards(server_cards);
  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());

  // Set that the user enabled the sync feature. Check that the function still
  // returns false.
  sync_service_.SetHasSyncConsent(true);
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());

  // Re-disable the sync feature. Check that the function still returns false.
  sync_service_.SetHasSyncConsent(false);
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());

  // Set a null sync service. Check that the function still returns false.
  personal_data_->SetSyncServiceForTest(nullptr);
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS) &&
        // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(PersonalDataManagerSyncTransportModeTest,
       GetPaymentsSigninStateForMetrics) {
  // Make sure a non-sync-consented account is available for the first tests.
  ASSERT_TRUE(identity_test_env_.identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  ASSERT_FALSE(sync_service_.HasSyncConsent());
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kAutofill,
                 syncer::UserSelectableType::kPayments});

  EXPECT_EQ(AutofillMetrics::PaymentsSigninState::
                kSignedInAndWalletSyncTransportEnabled,
            personal_data_->GetPaymentsSigninStateForMetrics());

  // Check that the sync state is |SignedIn| if the sync service does not have
  // wallet data active.
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet(
          {syncer::UserSelectableType::kAutofill}));
  EXPECT_EQ(AutofillMetrics::PaymentsSigninState::kSignedIn,
            personal_data_->GetPaymentsSigninStateForMetrics());

  // Nothing should change if |kAutofill| is also removed.
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet());
  EXPECT_EQ(AutofillMetrics::PaymentsSigninState::kSignedIn,
            personal_data_->GetPaymentsSigninStateForMetrics());

// ClearPrimaryAccount is not supported on CrOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Check that the sync state is |SignedOut| when the account info is empty.
  {
    identity_test_env_.ClearPrimaryAccount();
    sync_service_.SetAccountInfo(CoreAccountInfo());
    sync_service_.SetHasSyncConsent(false);
    EXPECT_EQ(AutofillMetrics::PaymentsSigninState::kSignedOut,
              personal_data_->GetPaymentsSigninStateForMetrics());
  }
#endif

  // Simulate that the user has enabled the sync feature.
  AccountInfo primary_account_info;
  primary_account_info.email = kPrimaryAccountEmail;
  sync_service_.SetAccountInfo(primary_account_info);
  sync_service_.SetHasSyncConsent(true);
// MakePrimaryAccountAvailable is not supported on CrOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  identity_test_env_.MakePrimaryAccountAvailable(primary_account_info.email,
                                                 signin::ConsentLevel::kSync);
#endif

  // Check that the sync state is |SignedInAndSyncFeature| if the sync feature
  // is enabled.
  EXPECT_EQ(
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled,
      personal_data_->GetPaymentsSigninStateForMetrics());
}

// On mobile, no dedicated opt-in is required for WalletSyncTransport - the
// user is always considered opted-in and thus this test doesn't make sense.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(PersonalDataManagerSyncTransportModeTest, OnUserAcceptedUpstreamOffer) {
  ///////////////////////////////////////////////////////////
  // kSignedInAndWalletSyncTransportEnabled
  ///////////////////////////////////////////////////////////
  // Make sure a primary account with no sync consent is available so
  // AUTOFILL_WALLET_DATA can run in sync-transport mode.
  ASSERT_TRUE(identity_test_env_.identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  ASSERT_FALSE(identity_test_env_.identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSync));
  CoreAccountInfo active_info =
      identity_test_env_.identity_manager()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);
  sync_service_.SetAccountInfo(active_info);
  sync_service_.SetHasSyncConsent(false);

  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kAutofill,
                 syncer::UserSelectableType::kPayments});
  // Make sure there are no opt-ins recorded yet.
  ASSERT_FALSE(prefs::IsUserOptedInWalletSyncTransport(prefs_.get(),
                                                       active_info.account_id));

  // Account wallet storage only makes sense together with support for
  // unconsented primary accounts, i.e. on Win/Mac/Linux.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_TRUE(
      !sync_service_.IsSyncFeatureEnabled() &&
      sync_service_.GetActiveDataTypes().Has(syncer::AUTOFILL_WALLET_DATA));

  // Make sure an opt-in gets recorded if the user accepted an Upstream offer.
  personal_data_->OnUserAcceptedUpstreamOffer();
  EXPECT_TRUE(prefs::IsUserOptedInWalletSyncTransport(prefs_.get(),
                                                      active_info.account_id));

  // Clear the prefs.
  prefs::ClearSyncTransportOptIns(prefs_.get());
  ASSERT_FALSE(prefs::IsUserOptedInWalletSyncTransport(prefs_.get(),
                                                       active_info.account_id));

  ///////////////////////////////////////////////////////////
  // kSignedIn
  ///////////////////////////////////////////////////////////
  // Disable the wallet data type. kSignedInAndWalletSyncTransportEnabled
  // shouldn't be available.
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet());
  EXPECT_TRUE(!sync_service_.GetAccountInfo().IsEmpty());

  // Make sure an opt-in does not get recorded even if the user accepted an
  // Upstream offer.
  personal_data_->OnUserAcceptedUpstreamOffer();
  EXPECT_FALSE(prefs::IsUserOptedInWalletSyncTransport(prefs_.get(),
                                                       active_info.account_id));

  // Clear the prefs.
  prefs::ClearSyncTransportOptIns(prefs_.get());
  ASSERT_FALSE(prefs::IsUserOptedInWalletSyncTransport(prefs_.get(),
                                                       active_info.account_id));

  ///////////////////////////////////////////////////////////
  // kSignedOut
  ///////////////////////////////////////////////////////////
  identity_test_env_.ClearPrimaryAccount();
  sync_service_.SetAccountInfo(CoreAccountInfo());
  sync_service_.SetHasSyncConsent(false);
  {
    EXPECT_TRUE(sync_service_.GetAccountInfo().IsEmpty());

    // Make sure an opt-in does not get recorded even if the user accepted an
    // Upstream offer.
    personal_data_->OnUserAcceptedUpstreamOffer();
    EXPECT_FALSE(prefs::IsUserOptedInWalletSyncTransport(
        prefs_.get(), active_info.account_id));
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  ///////////////////////////////////////////////////////////
  // kSignedInAndSyncFeature
  ///////////////////////////////////////////////////////////
  identity_test_env_.MakePrimaryAccountAvailable(active_info.email,
                                                 signin::ConsentLevel::kSync);
  sync_service_.SetAccountInfo(active_info);
  sync_service_.SetHasSyncConsent(true);
  {
    EXPECT_TRUE(sync_service_.IsSyncFeatureEnabled());

    // Make sure an opt-in does not get recorded even if the user accepted an
    // Upstream offer.
    personal_data_->OnUserAcceptedUpstreamOffer();
    EXPECT_FALSE(prefs::IsUserOptedInWalletSyncTransport(
        prefs_.get(), active_info.account_id));
  }
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

namespace {

class OneTimeObserver : public PersonalDataManagerObserver {
 public:
  explicit OneTimeObserver(PersonalDataManager* manager) : manager_(manager) {}

  ~OneTimeObserver() override {
    if (manager_)
      manager_->RemoveObserver(this);
  }

  void OnPersonalDataChanged() override {
    ASSERT_TRUE(manager_) << "Callback called after RemoveObserver()";
    manager_->RemoveObserver(this);
    manager_ = nullptr;
  }

  void OnPersonalDataFinishedProfileTasks() override {
    EXPECT_TRUE(manager_) << "Callback called after RemoveObserver()";
  }

  bool IsConnected() { return manager_; }

 private:
  raw_ptr<PersonalDataManager> manager_;
};

}  // namespace

TEST_F(PersonalDataManagerTest, RemoveObserverInOnPersonalDataChanged) {
  OneTimeObserver observer(personal_data_.get());

  personal_data_->AddObserver(&observer);

  // Do something to trigger a data change
  personal_data_->AddProfile(test::GetFullProfile());

  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  EXPECT_FALSE(observer.IsConnected()) << "Observer not called";
}

TEST_F(PersonalDataManagerTest, IsEligibleForAddressAccountStorage) {
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{base::test::FeatureRefAndParams(
           features::kAutofillAccountProfileStorage,
           {{features::kAutofillAccountProfileStorageFromUnsupportedIPs.name,
             "false"}})}},
      /*disabled_features=*/{});

  // No Sync, no account storage.
  personal_data_->SetSyncServiceForTest(nullptr);
  EXPECT_FALSE(personal_data_->IsEligibleForAddressAccountStorage());

  // Fake the Sync service. All data types are running by default.
  syncer::TestSyncService sync_service;
  personal_data_->SetSyncServiceForTest(&sync_service);
  EXPECT_TRUE(personal_data_->IsEligibleForAddressAccountStorage());

  // Being located in an unsupported country makes the user ineligible.
  personal_data_->set_variations_country_code_for_testing("CU");
  EXPECT_FALSE(personal_data_->IsEligibleForAddressAccountStorage());

  // Unregister the Sync observer.
  personal_data_->OnSyncShutdown(&sync_service);
}

TEST_F(PersonalDataManagerTest, IsCountryEligibleForAccountStorage) {
  EXPECT_TRUE(personal_data_->IsCountryEligibleForAccountStorage("AT"));
  EXPECT_FALSE(personal_data_->IsCountryEligibleForAccountStorage("IR"));
}

}  // namespace autofill
