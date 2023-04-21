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
#include "components/autofill/core/browser/personal_data_manager_test_base.h"
#include "components/autofill/core/browser/sync_utils.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/ui/label_formatter_utils.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_selection.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/test_sync_service.h"
#include "components/version_info/version_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace autofill {

namespace {

using testing::ElementsAre;
using testing::Pointee;
using testing::UnorderedElementsAre;

constexpr char kGuid[] = "a21f010a-eac1-41fc-aee9-c06bbedfb292";
constexpr char kPrimaryAccountEmail[] = "syncuser@example.com";
constexpr char16_t kPrimaryAccountEmail16[] = u"syncuser@example.com";
constexpr char kAddressEntryIcon[] = "accountIcon";

enum UserMode { USER_MODE_NORMAL, USER_MODE_INCOGNITO };

const base::Time kArbitraryTime = base::Time::FromDoubleT(25);
const base::Time kSomeLaterTime = base::Time::FromDoubleT(1000);
const base::Time kMuchLaterTime = base::Time::FromDoubleT(5000);

ACTION_P(QuitMessageLoop, loop) {
  loop->Quit();
}

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

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
std::vector<std::vector<Suggestion::Text>> ConstructLabelLineMatrix(
    const std::vector<std::u16string>& parts) {
  return {{Suggestion::Text(ConstructLabelLine(parts))}};
}
#endif

}  // anonymous namespace

class PersonalDataManagerHelper : public PersonalDataManagerTestBase {
 protected:
  PersonalDataManagerHelper() = default;

  virtual ~PersonalDataManagerHelper() {
    if (personal_data_)
      personal_data_->Shutdown();
    personal_data_.reset();
  }

  void ResetPersonalDataManager(UserMode user_mode,
                                bool use_sync_transport_mode = false) {
    if (personal_data_)
      personal_data_->Shutdown();
    personal_data_ = std::make_unique<PersonalDataManager>("EN", "US");
    PersonalDataManagerTestBase::ResetPersonalDataManager(
        user_mode == USER_MODE_INCOGNITO, use_sync_transport_mode,
        personal_data_.get());
  }

  void ResetProfiles() {
    std::vector<AutofillProfile> empty_profiles;
    personal_data_->SetProfilesForAllSources(&empty_profiles);
    WaitForOnPersonalDataChanged();
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
    AutofillProfile profile(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                            test::kEmptyOrigin);
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
    personal_data_->AddCreditCard(credit_card2);

    WaitOnceForOnPersonalDataChanged();
    ASSERT_EQ(3U, personal_data_->GetCreditCards().size());
  }

  // Add 3 credit cards. One local, one masked, one full. Creates two masked
  // cards on Linux, since full server cards are not supported.
  void SetUpThreeCardTypes() {
    EXPECT_EQ(0U, personal_data_->GetCreditCards().size());
    CreditCard masked_server_card;
    test::SetCreditCardInfo(&masked_server_card, "Elvis Presley",
                            "4234567890123456",  // Visa
                            "04", "2999", "1");
    masked_server_card.set_guid("00000000-0000-0000-0000-000000000007");
    masked_server_card.set_record_type(CreditCard::FULL_SERVER_CARD);
    masked_server_card.set_server_id("masked_id");
    masked_server_card.set_use_count(15);
    personal_data_->AddFullServerCreditCard(masked_server_card);
    WaitOnceForOnPersonalDataChanged();
    ASSERT_EQ(1U, personal_data_->GetCreditCards().size());

// Cards are automatically remasked on Linux since full server cards are not
// supported.
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if !(BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
    personal_data_->ResetFullServerCard(
        personal_data_->GetCreditCards()[0]->guid());
#endif

    CreditCard full_server_card;
    test::SetCreditCardInfo(&full_server_card, "Buddy Holly",
                            "5187654321098765",  // Mastercard
                            "10", "2998", "1");
    full_server_card.set_guid("00000000-0000-0000-0000-000000000008");
    full_server_card.set_record_type(CreditCard::FULL_SERVER_CARD);
    full_server_card.set_server_id("full_id");
    full_server_card.set_use_count(10);
    personal_data_->AddFullServerCreditCard(full_server_card);

    CreditCard local_card;
    test::SetCreditCardInfo(&local_card, "Freddy Mercury",
                            "4234567890123463",  // Visa
                            "08", "2999", "1");
    local_card.set_guid("00000000-0000-0000-0000-000000000009");
    local_card.set_record_type(CreditCard::LOCAL_CARD);
    local_card.set_use_count(5);
    personal_data_->AddCreditCard(local_card);

    WaitOnceForOnPersonalDataChanged();
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

    WaitForOnPersonalDataChanged();
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
    return personal_data_->IsSyncFeatureEnabled()
               ? profile_autofill_table_.get()
               : account_autofill_table_.get();
  }

  void AddProfileToPersonalDataManager(const AutofillProfile& profile) {
    base::RunLoop run_loop;
    EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
        .WillOnce(QuitMessageLoop(&run_loop));
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
        .Times(testing::AnyNumber());
    personal_data_->AddProfile(profile);
    run_loop.Run();
  }

  void UpdateProfileOnPersonalDataManager(const AutofillProfile& profile) {
    base::RunLoop run_loop;
    EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
        .WillOnce(QuitMessageLoop(&run_loop));
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
        .Times(testing::AnyNumber());

    personal_data_->UpdateProfile(profile);
    run_loop.Run();
  }

  void RemoveByGUIDFromPersonalDataManager(const std::string& guid) {
    PersonalDataManagerTestBase::RemoveByGUIDFromPersonalDataManager(
        guid, personal_data_.get());
  }

  void SetServerCards(const std::vector<CreditCard>& server_cards) {
    test::SetServerCreditCards(GetServerDataTable(), server_cards);
  }

  void SetServerProfiles(const std::vector<AutofillProfile>& server_profiles) {
    GetServerDataTable()->SetServerProfiles(server_profiles);
  }

  void SaveImportedProfileToPersonalDataManager(
      const AutofillProfile& profile) {
    base::RunLoop run_loop;
    EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
        .WillOnce(QuitMessageLoop(&run_loop));
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
        .Times(testing::AnyNumber());
    personal_data_->SaveImportedProfile(profile);
    run_loop.Run();
  }

  void ConvertWalletAddressesAndUpdateWalletCards() {
    // Simulate new data is coming from sync which triggers a conversion of
    // wallet addresses which in turn triggers a refresh.
    personal_data_->AutofillMultipleChangedBySync();
    WaitForOnPersonalDataChanged();
  }

  void AddOfferDataForTest(AutofillOfferData offer_data) {
    personal_data_->AddOfferDataForTest(
        std::make_unique<AutofillOfferData>(offer_data));
  }

  std::unique_ptr<PersonalDataManager> personal_data_;
};

class PersonalDataManagerTest : public PersonalDataManagerHelper,
                                public testing::Test {
 protected:
  void SetUp() override {
    SetUpTest();
    ResetPersonalDataManager(USER_MODE_NORMAL);
  }
  void TearDown() override { TearDownTest(); }
};

class PersonalDataManagerSyncTransportModeTest
    : public PersonalDataManagerHelper,
      public testing::Test {
 protected:
  void SetUp() override {
    SetUpTest();
    ResetPersonalDataManager(USER_MODE_NORMAL,
                             /*use_sync_transport_mode=*/true);
  }
  void TearDown() override { TearDownTest(); }
};

class PersonalDataManagerMockTest : public PersonalDataManagerTestBase,
                                    public testing::Test {
 protected:
  void SetUp() override {
    SetUpTest();
    ResetPersonalDataManager(USER_MODE_NORMAL);
    personal_data_->pref_service_->SetInteger(
        prefs::kAutofillLastVersionDeduped, 0);
  }

  void TearDown() override {
    if (personal_data_)
      personal_data_->Shutdown();
    personal_data_.reset();
    TearDownTest();
  }

  void ResetPersonalDataManager(UserMode user_mode) {
    if (personal_data_)
      personal_data_->Shutdown();
    personal_data_ =
        std::make_unique<PersonalDataManagerMock>("en", std::string());
    PersonalDataManagerTestBase::ResetPersonalDataManager(
        user_mode == USER_MODE_INCOGNITO, /*use_sync_transport_mode=*/true,
        personal_data_.get());
  }

  bool TurnOnSyncFeature() {
    return PersonalDataManagerTestBase::TurnOnSyncFeature(personal_data_.get());
  }

  void StopTheDedupeProcess() {
    personal_data_->pref_service_->SetInteger(
        prefs::kAutofillLastVersionDeduped,
        atoi(version_info::GetVersionNumber().c_str()));
  }

  void AddProfileToPersonalDataManager(const AutofillProfile& profile) {
    base::RunLoop run_loop;

    EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
        .WillOnce(QuitMessageLoop(&run_loop));
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
        .Times(testing::AnyNumber());

    personal_data_->AddProfile(profile);

    run_loop.Run();
  }

  void UpdateProfileOnPersonalDataManager(const AutofillProfile& profile) {
    base::RunLoop run_loop;

    EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
        .WillOnce(QuitMessageLoop(&run_loop));
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
        .Times(testing::AnyNumber());

    personal_data_->UpdateProfile(profile);

    run_loop.Run();
  }

  void RemoveByGUIDFromPersonalDataManager(const std::string& guid) {
    PersonalDataManagerTestBase::RemoveByGUIDFromPersonalDataManager(
        guid, personal_data_.get());
  }

  void AddOfferDataForTest(AutofillOfferData offer_data) {
    personal_data_->AddOfferDataForTest(
        std::make_unique<AutofillOfferData>(offer_data));
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
        .WillOnce(QuitMessageLoop(&run_loop));
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
  ResetPersonalDataManager(USER_MODE_NORMAL);
  // Verify the addition.
  const std::vector<AutofillProfile*>& results1 = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(0, profile0.Compare(*results1[0]));

  // Add profile with identical values.  Duplicates should not get saved.
  AutofillProfile profile0a = profile0;
  profile0a.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());

  AddProfileToPersonalDataManager(profile0a);

  // Reload the database.
  ResetPersonalDataManager(USER_MODE_NORMAL);

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
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Verify the addition.
  std::vector<AutofillProfile*> profiles;
  profiles.push_back(&profile0);
  profiles.push_back(&profile1);
  ExpectSameElements(profiles, personal_data_->GetProfiles());
}

// Tests that profiles with source `kAccount` and `kLocalOrSyncable` are loaded,
// and accessible via `GetProfiles()` and `GetProfilesFromSource()`.
// If duplicates exist across sources, they should be considered distinct.
TEST_F(PersonalDataManagerTest, GetProfiles) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeature(features::kAutofillAccountProfilesUnionView);

  AutofillProfile kAccountProfile = test::GetFullProfile();
  kAccountProfile.set_source_for_testing(AutofillProfile::Source::kAccount);
  AutofillProfile kAccountProfile2 = test::GetFullProfile2();
  kAccountProfile2.set_source_for_testing(AutofillProfile::Source::kAccount);
  AutofillProfile kLocalProfile = test::GetFullProfile();

  AddProfileToPersonalDataManager(kAccountProfile);
  AddProfileToPersonalDataManager(kAccountProfile2);
  AddProfileToPersonalDataManager(kLocalProfile);
  ResetPersonalDataManager(USER_MODE_NORMAL);

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
  ResetPersonalDataManager(USER_MODE_NORMAL);

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
  // Enable UnionView to test the ordering of profiles from different sources.
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeature(features::kAutofillAccountProfilesUnionView);

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

// Tests that `SetProfilesForAllSources()` overwrites profiles with the correct
// source.
TEST_F(PersonalDataManagerTest, SetProfiles) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeature(features::kAutofillAccountProfilesUnionView);

  AutofillProfile kAccountProfile = test::GetFullProfile();
  kAccountProfile.set_source_for_testing(AutofillProfile::Source::kAccount);
  AutofillProfile kLocalProfile = test::GetFullProfile();

  // Set `kAccount` profiles only.
  std::vector<AutofillProfile> profiles = {kAccountProfile};
  personal_data_->SetProfilesForAllSources(&profiles);
  WaitForOnPersonalDataChanged();
  EXPECT_THAT(
      personal_data_->GetProfilesFromSource(AutofillProfile::Source::kAccount),
      ElementsAre(Pointee(kAccountProfile)));
  EXPECT_TRUE(
      personal_data_
          ->GetProfilesFromSource(AutofillProfile::Source::kLocalOrSyncable)
          .empty());

  // Set `kLocalOrSyncable` profiles only. This clear the existing `kAccount`
  // profiles
  profiles = {kLocalProfile};
  personal_data_->SetProfilesForAllSources(&profiles);
  WaitForOnPersonalDataChanged();
  EXPECT_TRUE(
      personal_data_->GetProfilesFromSource(AutofillProfile::Source::kAccount)
          .empty());
  EXPECT_THAT(personal_data_->GetProfilesFromSource(
                  AutofillProfile::Source::kLocalOrSyncable),
              ElementsAre(Pointee(kLocalProfile)));

  // Set profiles of both sources.
  profiles = {kAccountProfile, kLocalProfile};
  personal_data_->SetProfilesForAllSources(&profiles);
  WaitForOnPersonalDataChanged();
  EXPECT_THAT(
      personal_data_->GetProfilesFromSource(AutofillProfile::Source::kAccount),
      ElementsAre(Pointee(kAccountProfile)));
  EXPECT_THAT(personal_data_->GetProfilesFromSource(
                  AutofillProfile::Source::kLocalOrSyncable),
              ElementsAre(Pointee(kLocalProfile)));
}

// Adding, updating, removing operations without waiting in between.
TEST_F(PersonalDataManagerTest, AddRemoveUpdateProfileSequence) {
  AutofillProfile profile(test::GetFullProfile());

  personal_data_->AddProfile(profile);
  personal_data_->RemoveByGUID(profile.guid());
  personal_data_->UpdateProfile(profile);
  WaitForOnPersonalDataChanged();

  auto profiles = personal_data_->GetProfiles();
  ASSERT_EQ(0U, profiles.size());

  personal_data_->AddProfile(profile);
  personal_data_->RemoveByGUID(profile.guid());
  personal_data_->RemoveByGUID(profile.guid());
  WaitForOnPersonalDataChanged();

  profiles = personal_data_->GetProfiles();
  ASSERT_EQ(0U, profiles.size());

  personal_data_->AddProfile(profile);
  profile.SetRawInfo(EMAIL_ADDRESS, u"new@email.com");
  personal_data_->UpdateProfile(profile);
  WaitForOnPersonalDataChanged();

  profiles = personal_data_->GetProfiles();
  ASSERT_EQ(1U, profiles.size());
  EXPECT_EQ(profiles[0]->GetRawInfo(EMAIL_ADDRESS), u"new@email.com");

  profile.SetRawInfo(EMAIL_ADDRESS, u"newer@email.com");
  personal_data_->UpdateProfile(profile);
  profile.SetRawInfo(EMAIL_ADDRESS, u"newest@email.com");
  personal_data_->UpdateProfile(profile);
  WaitForOnPersonalDataChanged();

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
  ResetPersonalDataManager(USER_MODE_NORMAL);

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

  personal_data_->SetProfilesForAllSources(&profiles);

  WaitForOnPersonalDataChanged();

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

  std::vector<AutofillProfile> profiles;
  profiles.push_back(with_invalid);
  personal_data_->SetProfilesForAllSources(&profiles);
  WaitForOnPersonalDataChanged();
  ASSERT_EQ(1u, personal_data_->GetProfiles().size());
  AutofillProfile profile = *personal_data_->GetProfiles()[0];
  ASSERT_NE(without_invalid.GetRawInfo(PHONE_HOME_WHOLE_NUMBER),
            profile.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
}

// Tests that SaveImportedProfile sets the modification date on new profiles.
TEST_F(PersonalDataManagerTest, SaveImportedProfileSetModificationDate) {
  AutofillProfile profile(test::GetFullProfile());
  EXPECT_NE(base::Time(), profile.modification_date());

  SaveImportedProfileToPersonalDataManager(profile);
  const std::vector<AutofillProfile*>& profiles = personal_data_->GetProfiles();
  ASSERT_EQ(1U, profiles.size());
  EXPECT_GT(base::Milliseconds(2000),
            AutofillClock::Now() - profiles[0]->modification_date());
}

TEST_F(PersonalDataManagerTest, AddUpdateRemoveProfiles) {
  AutofillProfile profile0(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetProfileInfo(&profile0, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");

  AutofillProfile profile1(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "903 Apple Ct.", nullptr,
                       "Orlando", "FL", "32801", "US", "19482937549");

  AutofillProfile profile2(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
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
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Verify that we've loaded the profiles from the web database.
  ExpectSameElements(profiles, personal_data_->GetProfiles());
}

TEST_F(PersonalDataManagerTest, NoIBANsAddedIfDisabled) {
  prefs::SetAutofillIBANEnabled(prefs_.get(), false);
  personal_data_->AddIBAN(autofill::test::GetIBAN());
  personal_data_->AddIBAN(autofill::test::GetIBAN2());

  EXPECT_EQ(0U, personal_data_->GetLocalIBANs().size());
}

TEST_F(PersonalDataManagerTest, AddingIbanUpdatesPref) {
  prefs::SetAutofillIBANEnabled(prefs_.get(), true);
  // The pref should always start disabled.
  ASSERT_FALSE(personal_data_->IsAutofillHasSeenIbanPrefEnabled());
  IBAN iban = test::GetIBAN();

  personal_data_->AddIBAN(iban);
  WaitForOnPersonalDataChanged();
  // Adding an IBAN permanently enables the pref.
  EXPECT_TRUE(personal_data_->IsAutofillHasSeenIbanPrefEnabled());
}

TEST_F(PersonalDataManagerTest, AddUpdateRemoveIBANs) {
  prefs::SetAutofillIBANEnabled(prefs_.get(), true);
  IBAN iban0 = autofill::test::GetIBAN();

  IBAN iban1 = autofill::test::GetIBAN2();
  iban1.set_nickname(u"Nickname 1");

  IBAN iban1_1 = iban1;
  iban1_1.set_nickname(u"Nickname 1_1");

  IBAN iban2 = autofill::test::GetIBANWithoutNickname();
  iban2.set_nickname(u"Nickname 2");

  // Add two test IBANs to the database. `iban1_1` has the same value
  // as `iban1` but with a different nickname. Verify that only `iban1` is
  // added.
  personal_data_->AddIBAN(iban0);
  WaitForOnPersonalDataChanged();

  personal_data_->AddIBAN(iban1);
  WaitForOnPersonalDataChanged();

  personal_data_->AddIBAN(iban1_1);

  std::vector<IBAN*> ibans;
  ibans.push_back(&iban0);
  ibans.push_back(&iban1);
  ExpectSameElements(ibans, personal_data_->GetLocalIBANs());

  // `iban1_2` has the same fields as `iban1_1`, verify that `iban1_2` is
  // not added.
  IBAN iban1_2 = iban1_1;
  personal_data_->AddIBAN(iban1_1);
  ExpectSameElements(ibans, personal_data_->GetLocalIBANs());

  // Update IBAN0, remove IBAN1, and add IBAN2.
  iban0.set_nickname(u"Nickname new 0");
  iban0.SetRawInfo(IBAN_VALUE, u"GB98 MIDL 0700 9312 3456 78");
  personal_data_->UpdateIBAN(iban0);
  RemoveByGUIDFromPersonalDataManager(iban1.guid());
  personal_data_->AddIBAN(iban2);

  WaitForOnPersonalDataChanged();

  ibans.clear();
  ibans.push_back(&iban0);
  ibans.push_back(&iban2);
  ExpectSameElements(ibans, personal_data_->GetLocalIBANs());

  // Verify that a duplicate IBAN should not be added.
  IBAN iban0_dup = iban0;
  personal_data_->AddIBAN(iban0_dup);
  ibans.clear();
  ibans.push_back(&iban0);
  ibans.push_back(&iban2);
  ExpectSameElements(ibans, personal_data_->GetLocalIBANs());

  // Reset the PersonalDataManager. This tests that the personal data was saved
  // to the web database, and that we can load the IBANs from the web database.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Verify that we've reloaded the IBANs from the web database.
  ibans.clear();
  ibans.push_back(&iban0);
  ibans.push_back(&iban2);
  ExpectSameElements(ibans, personal_data_->GetLocalIBANs());
}

// Ensure that new IBANs can be updated and saved via
// `OnAcceptedLocalIBANSave()`.
TEST_F(PersonalDataManagerTest, OnAcceptedLocalIBANSave) {
  prefs::SetAutofillIBANEnabled(prefs_.get(), true);

  // Start with a new IBAN.
  IBAN iban0 = autofill::test::GetIBAN();
  // Add the IBAN to the database.
  personal_data_->OnAcceptedLocalIBANSave(iban0);

  // Make sure everything is set up correctly.
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_->GetLocalIBANs().size());

  // Creates a new IBAN and call `OnAcceptedLocalIBANSave()` and verify that
  // the new IBAN is saved.
  IBAN iban1 = autofill::test::GetIBAN2();
  personal_data_->OnAcceptedLocalIBANSave(iban1);
  WaitForOnPersonalDataChanged();

  // Expect that the new IBAN is added.
  ASSERT_EQ(2U, personal_data_->GetLocalIBANs().size());

  std::vector<IBAN*> ibans;
  ibans.push_back(&iban0);
  ibans.push_back(&iban1);
  // Verify that we've loaded the IBAN from the web database.
  ExpectSameElements(ibans, personal_data_->GetLocalIBANs());

  // Creates a new `iban2` which has the same value as `iban0` but with
  // different nickname and call `OnAcceptedLocalIBANSave()`.
  IBAN iban2 = iban0;
  iban2.set_nickname(u"Nickname 2");
  personal_data_->OnAcceptedLocalIBANSave(iban2);
  WaitForOnPersonalDataChanged();
  // Updates the nickname for `iban1` and call `OnAcceptedLocalIBANSave()`.
  iban1.set_nickname(u"Nickname 1 updated");
  personal_data_->OnAcceptedLocalIBANSave(iban1);
  WaitForOnPersonalDataChanged();

  ibans.clear();
  ibans.push_back(&iban1);
  ibans.push_back(&iban2);
  // Expect that the existing IBANs are updated.
  ASSERT_EQ(2U, personal_data_->GetLocalIBANs().size());

  // Verify that we've loaded the IBANs from the web database.
  ExpectSameElements(ibans, personal_data_->GetLocalIBANs());

  // Call `OnAcceptedLocalIBANSave()` with the same iban1, verify that nothing
  // changes.
  personal_data_->OnAcceptedLocalIBANSave(iban1);
  ExpectSameElements(ibans, personal_data_->GetLocalIBANs());

  // Reset the PersonalDataManager. This tests that the IBANs are persisted
  // in the local web database even if the browser is re-loaded, ensuring that
  // the user can load the IBANs from the local web database on browser startup.
  ResetPersonalDataManager(USER_MODE_NORMAL);
  ExpectSameElements(ibans, personal_data_->GetLocalIBANs());
}

// Ensure that new IBAN cannot be updated nor saved via
// `OnAcceptedLocalIBANSave()` if `is_off_the_record_` is true.
TEST_F(PersonalDataManagerTest, OnAcceptedLocalIBANSave_IsOffTheRecordTrue) {
  personal_data_->set_is_off_the_record_for_testing(true);
  prefs::SetAutofillIBANEnabled(prefs_.get(), true);

  // Start with a new IBAN and add the IBAN to the database.
  personal_data_->AddIBAN(autofill::test::GetIBAN());

  // Verify the new IBAN is not saved.
  EXPECT_TRUE(personal_data_->GetLocalIBANs().empty());

  // Creates a new IBAN and call `OnAcceptedLocalIBANSave()` and verify that
  // the new IBAN is not saved.
  IBAN iban1 = autofill::test::GetIBAN2();
  personal_data_->OnAcceptedLocalIBANSave(iban1);

  EXPECT_TRUE(personal_data_->GetLocalIBANs().empty());

  // Updates the nickname for `iban1` and call `OnAcceptedLocalIBANSave()`,
  // verify that nothing happens.
  iban1.set_nickname(u"Nickname 0 updated");
  personal_data_->OnAcceptedLocalIBANSave(iban1);

  EXPECT_TRUE(personal_data_->GetLocalIBANs().empty());
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

  WaitForOnPersonalDataChanged();

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

  WaitForOnPersonalDataChanged();

  cards.clear();
  cards.push_back(&credit_card0);
  cards.push_back(&credit_card2);
  ExpectSameElements(cards, personal_data_->GetCreditCards());

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the credit cards from the web
  // database.
  ResetPersonalDataManager(USER_MODE_NORMAL);

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
  credit_card3.set_record_type(CreditCard::FULL_SERVER_CARD);
  credit_card3.set_server_id("server_id");

  personal_data_->AddFullServerCreditCard(credit_card3);
  WaitForOnPersonalDataChanged();

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
  ResetPersonalDataManager(USER_MODE_NORMAL);

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

  WaitForOnPersonalDataChanged();

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
  WaitForOnPersonalDataChanged();

  ASSERT_EQ(1u, personal_data_->GetCreditCards().size());
  EXPECT_TRUE(personal_data_->GetCreditCardByServerId("server id"));
  EXPECT_FALSE(personal_data_->GetCreditCardByServerId("non-existing id"));
}

#if !BUILDFLAG(IS_IOS)
TEST_F(PersonalDataManagerTest, AddAndGetCreditCardArtImage) {
  gfx::Image expected_image = gfx::test::CreateImage(32, 20);
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

TEST_F(PersonalDataManagerMockTest, ProcessCardArtUrlChanges) {
  CreditCard card = test::GetFullServerCard();
  card.set_server_id("card_server_id");
  personal_data_->AddFullServerCreditCard(card);
  WaitForOnPersonalDataChanged();

  card.set_server_id("card_server_id");
  card.set_card_art_url(GURL("https://www.example.com/card1"));
  std::vector<GURL> updated_urls;
  updated_urls.emplace_back(GURL("https://www.example.com/card1"));

  personal_data_->AddFullServerCreditCard(card);
  WaitForFetchImagesForUrls();

  card.set_card_art_url(GURL("https://www.example.com/card2"));
  updated_urls.clear();
  updated_urls.emplace_back(GURL("https://www.example.com/card2"));

  personal_data_->AddFullServerCreditCard(card);
  WaitForFetchImagesForUrls();
}
#endif

TEST_F(PersonalDataManagerTest, UpdateUnverifiedProfilesAndCreditCards) {
  // Start with unverified data.
  AutofillProfile profile(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          "https://www.example.com/");
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  EXPECT_FALSE(profile.IsVerified());

  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         "https://www.example.com/");
  test::SetCreditCardInfo(&credit_card, "John Dillinger",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  EXPECT_FALSE(credit_card.IsVerified());

  // Add the data to the database.
  AddProfileToPersonalDataManager(profile);
  personal_data_->AddCreditCard(credit_card);

  WaitForOnPersonalDataChanged();

  const std::vector<AutofillProfile*>& profiles1 =
      personal_data_->GetProfiles();
  const std::vector<CreditCard*>& cards1 = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, profiles1.size());
  ASSERT_EQ(1U, cards1.size());
  EXPECT_EQ(0, profile.Compare(*profiles1[0]));
  EXPECT_EQ(0, credit_card.Compare(*cards1[0]));

  // Try to update with just the origin changed.
  AutofillProfile original_profile(profile);
  ASSERT_FALSE(original_profile.IsVerified());
  CreditCard original_credit_card(credit_card);
  profile.set_origin(kSettingsOrigin);
  credit_card.set_origin(kSettingsOrigin);

  EXPECT_TRUE(profile.IsVerified());
  EXPECT_TRUE(credit_card.IsVerified());
  UpdateProfileOnPersonalDataManager(profile);
  personal_data_->UpdateCreditCard(credit_card);

  // Credit Card origin should not be overwritten.
  const std::vector<AutofillProfile*>& profiles2 =
      personal_data_->GetProfiles();
  const std::vector<CreditCard*>& cards2 = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, profiles2.size());
  ASSERT_EQ(1U, cards2.size());
  EXPECT_EQ(profile.origin(), profiles2[0]->origin());
  EXPECT_NE(credit_card.origin(), cards2[0]->origin());
  EXPECT_NE(original_profile.origin(), profiles2[0]->origin());
  EXPECT_EQ(original_credit_card.origin(), cards2[0]->origin());

  // Try to update with data changed as well.
  profile.SetRawInfo(NAME_FIRST, u"John");
  credit_card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Joe");

  UpdateProfileOnPersonalDataManager(profile);
  personal_data_->UpdateCreditCard(credit_card);

  WaitForOnPersonalDataChanged();

  const std::vector<AutofillProfile*>& profiles3 =
      personal_data_->GetProfiles();
  const std::vector<CreditCard*>& cards3 = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, profiles3.size());
  ASSERT_EQ(1U, cards3.size());
  EXPECT_EQ(0, profile.Compare(*profiles3[0]));
  EXPECT_EQ(0, credit_card.Compare(*cards3[0]));
  EXPECT_EQ(profile.origin(), profiles3[0]->origin());
  EXPECT_EQ(credit_card.origin(), cards3[0]->origin());
}

// Test that updating a verified profile with another profile whose only
// difference is the origin, would not change the old profile, and thus it would
// remain verified.
TEST_F(PersonalDataManagerTest, UpdateVerifiedProfilesOrigin) {
  // Start with verified data.
  AutofillProfile profile(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          kSettingsOrigin);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  ASSERT_TRUE(profile.IsVerified());
  AddProfileToPersonalDataManager(profile);

  const std::vector<AutofillProfile*>& profiles1 =
      personal_data_->GetProfiles();
  ASSERT_EQ(1U, profiles1.size());
  EXPECT_EQ(0, profile.Compare(*profiles1[0]));

  // Try to update with just the origin changed to a non-setting origin.
  AutofillProfile new_profile(profile);
  new_profile.set_origin("");
  ASSERT_FALSE(new_profile.IsVerified());

  UpdateProfileOnPersonalDataManager(profile);

  // Verified profile origin should not be overwritten.
  const std::vector<AutofillProfile*>& profiles2 =
      personal_data_->GetProfiles();
  ASSERT_EQ(1U, profiles2.size());
  EXPECT_EQ(profile.origin(), profiles2[0]->origin());
  EXPECT_NE(new_profile.origin(), profiles2[0]->origin());
  EXPECT_TRUE(profiles2[0]->IsVerified());
}

// Test that ensure local data is not lost on sign-in.
// Clearing/changing the primary account is not supported on CrOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PersonalDataManagerTest, KeepExistingLocalDataOnSignIn) {
  // Sign out.
  identity_test_env_.ClearPrimaryAccount();
  sync_service_.SetAccountInfo(CoreAccountInfo());
  EXPECT_EQ(AutofillSyncSigninState::kSignedOut,
            personal_data_->GetSyncSigninState());
  EXPECT_EQ(0U, personal_data_->GetCreditCards().size());

  // Add local card.
  CreditCard local_card;
  test::SetCreditCardInfo(&local_card, "Freddy Mercury",
                          "4234567890123463",  // Visa
                          "08", "2999", "1");
  local_card.set_guid("00000000-0000-0000-0000-000000000009");
  local_card.set_record_type(CreditCard::LOCAL_CARD);
  local_card.set_use_count(5);
  personal_data_->AddCreditCard(local_card);
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  // Sign in.
  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSync);
  sync_service_.SetAccountInfo(
      identity_test_env_.identity_manager()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSync));
  sync_service_.SetHasSyncConsent(true);
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet(
          syncer::UserSelectableType::kAutofill));
  EXPECT_EQ(AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled,
            personal_data_->GetSyncSigninState());
  ASSERT_TRUE(TurnOnSyncFeature());

  // Check saved local card should be not lost.
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());
  EXPECT_EQ(0, local_card.Compare(*personal_data_->GetCreditCards()[0]));
}
#endif

TEST_F(PersonalDataManagerTest, AddProfilesAndCreditCards) {
  AutofillProfile profile0(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetProfileInfo(&profile0, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");

  AutofillProfile profile1(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
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

  WaitForOnPersonalDataChanged();

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
  AutofillProfile profile0(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetProfileInfo(&profile0, "y", "", "", "", "", "", "", "", "", "", "",
                       "");

  // Add the profile0 to the db.
  AddProfileToPersonalDataManager(profile0);

  // Verify that we've loaded the profiles from the web database.
  const std::vector<AutofillProfile*>& results2 = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, profile0.Compare(*results2[0]));

  // Add a new profile.
  AutofillProfile profile1(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
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
  ResetPersonalDataManager(USER_MODE_NORMAL);

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
  AutofillProfile profile0(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetProfileInfo(&profile0, "", "", "", "", "", "", "", "", "", "", "",
                       "");

  // Add the empty profile to the database.
  AddProfileToPersonalDataManager(profile0);

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the profiles from the web
  // database.
  ResetPersonalDataManager(USER_MODE_NORMAL);

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
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Verify that we've loaded the credit cards from the web database.
  ASSERT_EQ(0U, personal_data_->GetCreditCards().size());
}

TEST_F(PersonalDataManagerTest, Refresh) {
  AutofillProfile profile0(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetProfileInfo(&profile0, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");

  AutofillProfile profile1(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
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

  AutofillProfile profile2(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5",
                       "Orlando", "FL", "32801", "US", "19482937549");

  profile_database_service_->AddAutofillProfile(profile2);

  personal_data_->Refresh();

  WaitForOnPersonalDataChanged();

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
  WaitForOnPersonalDataChanged();

  auto results = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(profile0, *results[0]);

  profile0.SetRawInfo(NAME_FIRST, u"Mar");
  profile_database_service_->UpdateAutofillProfile(profile0);

  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();

  results = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(profile0, *results[0]);
}

// Ensure that verified profiles can be saved via SaveImportedProfile,
// overwriting existing unverified profiles.
TEST_F(PersonalDataManagerTest, SaveImportedProfileWithVerifiedData) {
  // Start with an unverified profile.
  AutofillProfile profile(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  EXPECT_FALSE(profile.IsVerified());

  AddProfileToPersonalDataManager(profile);

  // Make sure everything is set up correctly.
  EXPECT_EQ(1U, personal_data_->GetProfiles().size());

  AutofillProfile new_verified_profile = profile;
  new_verified_profile.set_guid(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  new_verified_profile.set_origin(kSettingsOrigin);
  new_verified_profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"1 234 567-8910");
  EXPECT_TRUE(new_verified_profile.IsVerified());

  SaveImportedProfileToPersonalDataManager(new_verified_profile);

  // The new profile should be merged into the existing one.
  const std::vector<AutofillProfile*>& results = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  AutofillProfile expected(new_verified_profile);
  // The full name was missing in |profile| and was formatted from its
  // components.
  expected.SetRawInfoWithVerificationStatus(
      NAME_FULL, u"Marion Mitchell Morrison", VerificationStatus::kFormatted);
  expected.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"+1 234-567-8910");
  EXPECT_EQ(0, expected.Compare(*results[0]))
      << "result = {" << *results[0] << "} | expected = {" << expected << "}";
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
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  CreditCard new_verified_card = credit_card;
  new_verified_card.set_guid(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  new_verified_card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"B. Small");
  EXPECT_TRUE(new_verified_card.IsVerified());

  personal_data_->OnAcceptedLocalCreditCardSave(new_verified_card);

  WaitForOnPersonalDataChanged();

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
  AutofillProfile profile0(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
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
  AutofillProfile profile1(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "903 Apple Ct.", nullptr,
                       "Orlando", "FL", "32801", "US", "16502937549");

  AutofillProfile profile2(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
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

  WaitForOnPersonalDataChanged();
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

TEST_F(PersonalDataManagerTest, IncognitoReadOnly) {
  ASSERT_TRUE(personal_data_->GetProfiles().empty());
  ASSERT_TRUE(personal_data_->GetCreditCards().empty());

  AutofillProfile steve_jobs(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                             test::kEmptyOrigin);
  test::SetProfileInfo(&steve_jobs, "Steven", "Paul", "Jobs", "sjobs@apple.com",
                       "Apple Computer, Inc.", "1 Infinite Loop", "",
                       "Cupertino", "CA", "95014", "US", "(800) 275-2273");
  AddProfileToPersonalDataManager(steve_jobs);

  CreditCard bill_gates(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                        test::kEmptyOrigin);
  test::SetCreditCardInfo(&bill_gates, "William H. Gates", "5555555555554444",
                          "1", "2020", "1");
  personal_data_->AddCreditCard(bill_gates);

  // The personal data manager should be able to read existing profiles in an
  // off-the-record context.
  ResetPersonalDataManager(USER_MODE_INCOGNITO);
  ASSERT_EQ(1U, personal_data_->GetProfiles().size());
  ASSERT_EQ(1U, personal_data_->GetCreditCards().size());

  // No adds, saves, or updates should take effect.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(0);

  // Add profiles or credit card shouldn't work.
  personal_data_->AddProfile(test::GetFullProfile());

  CreditCard larry_page(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                        test::kEmptyOrigin);
  test::SetCreditCardInfo(&larry_page, "Lawrence Page", "4111111111111111",
                          "10", "2025", "1");
  personal_data_->AddCreditCard(larry_page);

  ResetPersonalDataManager(USER_MODE_INCOGNITO);
  EXPECT_EQ(1U, personal_data_->GetProfiles().size());
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  // Saving or creating profiles from imported profiles shouldn't work.
  steve_jobs.SetRawInfo(NAME_FIRST, u"Steve");
  personal_data_->SaveImportedProfile(steve_jobs);

  bill_gates.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Bill Gates");
  personal_data_->OnAcceptedLocalCreditCardSave(bill_gates);

  ResetPersonalDataManager(USER_MODE_INCOGNITO);
  EXPECT_EQ(u"Steven",
            personal_data_->GetProfiles()[0]->GetRawInfo(NAME_FIRST));
  EXPECT_EQ(
      u"William H. Gates",
      personal_data_->GetCreditCards()[0]->GetRawInfo(CREDIT_CARD_NAME_FULL));

  // Updating existing profiles shouldn't work.
  steve_jobs.SetRawInfo(NAME_FIRST, u"Steve");
  personal_data_->UpdateProfile(steve_jobs);

  bill_gates.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Bill Gates");
  personal_data_->UpdateCreditCard(bill_gates);

  ResetPersonalDataManager(USER_MODE_INCOGNITO);
  EXPECT_EQ(u"Steven",
            personal_data_->GetProfiles()[0]->GetRawInfo(NAME_FIRST));
  EXPECT_EQ(
      u"William H. Gates",
      personal_data_->GetCreditCards()[0]->GetRawInfo(CREDIT_CARD_NAME_FULL));

  // Removing shouldn't work.
  personal_data_->RemoveByGUID(steve_jobs.guid());
  personal_data_->RemoveByGUID(bill_gates.guid());

  ResetPersonalDataManager(USER_MODE_INCOGNITO);
  EXPECT_EQ(1U, personal_data_->GetProfiles().size());
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());
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
TEST_F(PersonalDataManagerMockTest, GetAutofillOffers_WalletImportDisabled) {
  // Add a card-linked offer and a promo code offer.
  AddOfferDataForTest(test::GetCardLinkedOfferData1());
  AddOfferDataForTest(test::GetPromoCodeOfferData());

  base::RunLoop run_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
      .WillOnce(QuitMessageLoop(&run_loop));
  prefs::SetPaymentsIntegrationEnabled(prefs_.get(), false);

  // Should return neither of them as the wallet import pref is disabled.
  EXPECT_EQ(0U, personal_data_->GetAutofillOffers().size());
}

// Tests that GetAutofillOffers does not return any offers if
// |IsAutofillCreditCardEnabled()| returns |false|.
TEST_F(PersonalDataManagerMockTest,
       GetAutofillOffers_AutofillCreditCardDisabled) {
  // Add a card-linked offer and a promo code offer.
  AddOfferDataForTest(test::GetCardLinkedOfferData1());
  AddOfferDataForTest(test::GetPromoCodeOfferData());

  prefs::SetAutofillCreditCardEnabled(prefs_.get(), false);

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
TEST_F(PersonalDataManagerMockTest,
       GetActiveAutofillPromoCodeOffersForOrigin_WalletImportDisabled) {
  // Add an active promo code offer.
  AddOfferDataForTest(test::GetPromoCodeOfferData(
      /*origin=*/GURL("http://www.example.com")));

  base::RunLoop run_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
      .WillOnce(QuitMessageLoop(&run_loop));
  prefs::SetPaymentsIntegrationEnabled(prefs_.get(), false);

  // Should not return the offer as the wallet import pref is disabled.
  EXPECT_EQ(0U, personal_data_
                    ->GetActiveAutofillPromoCodeOffersForOrigin(
                        GURL("http://www.example.com"))
                    .size());
}

// Tests that GetActiveAutofillPromoCodeOffersForOrigin does not return any
// promo code offers if |IsAutofillCreditCardEnabled()| returns |false|.
TEST_F(PersonalDataManagerMockTest,
       GetActiveAutofillPromoCodeOffersForOrigin_AutofillCreditCardDisabled) {
  // Add an active promo code offer.
  AddOfferDataForTest(test::GetPromoCodeOfferData(
      /*origin=*/GURL("http://www.example.com")));

  prefs::SetAutofillCreditCardEnabled(prefs_.get(), false);

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

  AutofillProfile moose(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                        kSettingsOrigin);
  test::SetProfileInfo(&moose, "Moose", "P", "McMahon", "mpm@example.com", "",
                       "1 Taiga TKTR", "", "Calgary", "AB", "T2B 2K2", "CA",
                       "(800) 555-9000");
  AddProfileToPersonalDataManager(moose);

  // Make sure everything is set up correctly.
  EXPECT_EQ(1U, personal_data_->GetProfiles().size());

  // The value is cached and doesn't change even after adding an address.
  EXPECT_EQ(default_country,
            personal_data_->GetDefaultCountryCodeForNewAddress());

  // Disabling Autofill blows away this cache and shouldn't account for Autofill
  // profiles.
  prefs::SetAutofillProfileEnabled(prefs_.get(), false);
  prefs::SetAutofillCreditCardEnabled(prefs_.get(), false);
  prefs::SetAutofillIBANEnabled(prefs_.get(), false);
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(default_country,
            personal_data_->GetDefaultCountryCodeForNewAddress());

  // Enabling Autofill blows away the cached value and should reflect the new
  // value (accounting for profiles).
  prefs::SetAutofillProfileEnabled(prefs_.get(), true);
  EXPECT_EQ(base::UTF16ToUTF8(moose.GetRawInfo(ADDRESS_HOME_COUNTRY)),
            personal_data_->GetDefaultCountryCodeForNewAddress());
}

TEST_F(PersonalDataManagerTest, DefaultCountryCodeComesFromProfiles) {
  AutofillProfile moose(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                        kSettingsOrigin);
  test::SetProfileInfo(&moose, "Moose", "P", "McMahon", "mpm@example.com", "",
                       "1 Taiga TKTR", "", "Calgary", "AB", "T2B 2K2", "CA",
                       "(800) 555-9000");
  AddProfileToPersonalDataManager(moose);
  ResetPersonalDataManager(USER_MODE_NORMAL);
  EXPECT_EQ("CA", personal_data_->GetDefaultCountryCodeForNewAddress());

  // Multiple profiles cast votes.
  AutofillProfile armadillo(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                            kSettingsOrigin);
  test::SetProfileInfo(&armadillo, "Armin", "Dill", "Oh", "ado@example.com", "",
                       "1 Speed Bump", "", "Lubbock", "TX", "77500", "MX",
                       "(800) 555-9000");
  AutofillProfile armadillo2(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                             kSettingsOrigin);
  test::SetProfileInfo(&armadillo2, "Armin", "Dill", "Oh", "ado@example.com",
                       "", "2 Speed Bump", "", "Lubbock", "TX", "77500", "MX",
                       "(800) 555-9000");
  AddProfileToPersonalDataManager(armadillo);
  AddProfileToPersonalDataManager(armadillo2);
  ResetPersonalDataManager(USER_MODE_NORMAL);
  EXPECT_EQ("MX", personal_data_->GetDefaultCountryCodeForNewAddress());

  RemoveByGUIDFromPersonalDataManager(armadillo.guid());
  RemoveByGUIDFromPersonalDataManager(armadillo2.guid());
  ResetPersonalDataManager(USER_MODE_NORMAL);
  // Verified profiles count more.
  armadillo.set_origin("http://randomwebsite.com");
  armadillo2.set_origin("http://randomwebsite.com");
  AddProfileToPersonalDataManager(armadillo);
  AddProfileToPersonalDataManager(armadillo2);
  ResetPersonalDataManager(USER_MODE_NORMAL);
  EXPECT_EQ("CA", personal_data_->GetDefaultCountryCodeForNewAddress());

  RemoveByGUIDFromPersonalDataManager(armadillo.guid());
  ResetPersonalDataManager(USER_MODE_NORMAL);
  // But unverified profiles can be a tie breaker.
  armadillo.set_origin(kSettingsOrigin);
  AddProfileToPersonalDataManager(armadillo);
  ResetPersonalDataManager(USER_MODE_NORMAL);
  EXPECT_EQ("MX", personal_data_->GetDefaultCountryCodeForNewAddress());
}

TEST_F(PersonalDataManagerTest, DefaultCountryCodeComesFromVariations) {
  const std::string expected_country_code = "DE";
  const std::string unepected_country_code = "FR";

  // Normally, the variation country code is passed to the constructor.
  personal_data_->set_variations_country_code_for_testing(
      expected_country_code);

  // Verify that there are no profiles set.
  EXPECT_EQ(0u, personal_data_->GetProfiles().size());

  // Since there are no profiles set, the country code supplied buy variations
  // should be used get get a default country code.
  std::string actual_country_code =
      personal_data_->GetDefaultCountryCodeForNewAddress();

  // Verify the the correct country code was retrieved.
  EXPECT_EQ(expected_country_code, actual_country_code);

  // Set a new country code.
  personal_data_->set_variations_country_code_for_testing(
      unepected_country_code);

  // The default country code retrieved before should have been cached.
  actual_country_code = personal_data_->GetDefaultCountryCodeForNewAddress();

  // Verify the expectations newly set country code is actually different.
  EXPECT_NE(expected_country_code, unepected_country_code);

  // Verify that it was actually set.
  EXPECT_EQ(unepected_country_code,
            personal_data_->variations_country_code_for_testing());

  // Verify that the retrieved country code is the initial one.
  EXPECT_EQ(expected_country_code, actual_country_code);

  // Now a profile is set and the correctcaching of the country code is verified
  // once more.
  AutofillProfile profile(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  AddProfileToPersonalDataManager(profile);

  // Once more, retrieve the defaultcountry code.
  actual_country_code = personal_data_->GetDefaultCountryCodeForNewAddress();

  // Verify that the profile was actually set.
  EXPECT_EQ(1U, personal_data_->GetProfiles().size());

  // Verify that the country code is still the initial one.
  EXPECT_EQ(actual_country_code, expected_country_code);
}

TEST_F(PersonalDataManagerTest, UpdateLanguageCodeInProfile) {
  AutofillProfile profile(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
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

TEST_F(PersonalDataManagerTest, GetProfileSuggestions) {
  AutofillProfile profile(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  AddProfileToPersonalDataManager(profile);
  ResetPersonalDataManager(USER_MODE_NORMAL);

  std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
      AutofillType(ADDRESS_HOME_STREET_ADDRESS), u"123", false,
      std::vector<ServerFieldType>());
  ASSERT_FALSE(suggestions.empty());
  EXPECT_EQ(u"123 Zoo St., Second Line, Third line, unit 5",
            suggestions[0].main_text.value);
}

TEST_F(PersonalDataManagerTest,
       GetProfileSuggestions_PhoneSubstring_NoImprovedDisambiguation) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndDisableFeature(
      features::kAutofillUseImprovedLabelDisambiguation);

  AutofillProfile profile(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  AddProfileToPersonalDataManager(profile);
  ResetPersonalDataManager(USER_MODE_NORMAL);

  std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
      AutofillType(PHONE_HOME_WHOLE_NUMBER), u"234", false,
      std::vector<ServerFieldType>());
  ASSERT_FALSE(suggestions.empty());
  EXPECT_EQ(u"12345678910", suggestions[0].main_text.value);
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(PersonalDataManagerTest,
       GetProfileSuggestions_PhoneSubstring_ImprovedDisambiguation) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(
      features::kAutofillUseImprovedLabelDisambiguation);

  AutofillProfile profile(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  AddProfileToPersonalDataManager(profile);
  ResetPersonalDataManager(USER_MODE_NORMAL);

  std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
      AutofillType(PHONE_HOME_WHOLE_NUMBER), u"234", false,
      std::vector<ServerFieldType>());
  ASSERT_FALSE(suggestions.empty());
  EXPECT_EQ(u"(234) 567-8910", suggestions[0].main_text.value);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

TEST_F(PersonalDataManagerTest, GetProfileSuggestions_HideSubsets) {
  AutofillProfile profile(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");

  // Dupe profile, except different in email address (irrelevant for this form).
  AutofillProfile profile1 = profile;
  profile1.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  profile1.SetRawInfo(EMAIL_ADDRESS, u"spam_me@example.com");

  // Dupe profile, except different in address state.
  AutofillProfile profile2 = profile;
  profile2.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  profile2.SetRawInfo(ADDRESS_HOME_STATE, u"TX");

  // Subset profile.
  AutofillProfile profile3 = profile;
  profile3.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  profile3.SetRawInfo(ADDRESS_HOME_STATE, std::u16string());

  // For easier results verification, make sure |profile| is suggested first.
  profile.set_use_count(5);
  AddProfileToPersonalDataManager(profile);
  AddProfileToPersonalDataManager(profile1);
  AddProfileToPersonalDataManager(profile2);
  AddProfileToPersonalDataManager(profile3);
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Simulate a form with street address, city and state.
  std::vector<ServerFieldType> types;
  types.push_back(ADDRESS_HOME_CITY);
  types.push_back(ADDRESS_HOME_STATE);
  std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
      AutofillType(ADDRESS_HOME_STREET_ADDRESS), u"123", false, types);
  ASSERT_EQ(2U, suggestions.size());
  ASSERT_EQ(1U, suggestions[0].labels.size());
  ASSERT_EQ(1U, suggestions[0].labels[0].size());
  EXPECT_EQ(u"Hollywood, CA", suggestions[0].labels[0][0].value);
  ASSERT_EQ(1U, suggestions[1].labels.size());
  ASSERT_EQ(1U, suggestions[1].labels.size());
  EXPECT_EQ(u"Hollywood, TX", suggestions[1].labels[0][0].value);
}

TEST_F(PersonalDataManagerTest, GetProfileSuggestions_SuggestionsLimit) {
  // Drawing takes noticeable time when there are more than 10 profiles.
  // Therefore, we keep only the 10 first suggested profiles.
  std::vector<AutofillProfile> profiles;
  for (size_t i = 0; i < 2 * suggestion_selection::kMaxUniqueSuggestionsCount;
       i++) {
    AutofillProfile profile(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                            test::kEmptyOrigin);
    test::SetProfileInfo(&profile, base::StringPrintf("Marion%zu", i).c_str(),
                         "Mitchell", "Morrison", "johnwayne@me.xyz", "Fox",
                         "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                         "Hollywood", "CA", "91601", "US", "12345678910");
    AddProfileToPersonalDataManager(profile);
    profiles.push_back(profile);
  }
  ResetPersonalDataManager(USER_MODE_NORMAL);

  std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
      AutofillType(NAME_FIRST), u"Ma", false, std::vector<ServerFieldType>());

  ASSERT_EQ(2 * suggestion_selection::kMaxUniqueSuggestionsCount,
            personal_data_->GetProfiles().size());
  ASSERT_EQ(suggestion_selection::kMaxUniqueSuggestionsCount,
            suggestions.size());
}

TEST_F(PersonalDataManagerTest, GetProfileSuggestions_ProfilesLimit) {
  // Deduping takes noticeable time when there are more than 50 profiles.
  // Therefore, keep only the 50 first pre-dedupe matching profiles.
  std::vector<AutofillProfile> profiles;
  for (size_t i = 0; i < suggestion_selection::kMaxSuggestedProfilesCount;
       i++) {
    AutofillProfile profile(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                            test::kEmptyOrigin);

    test::SetProfileInfo(
        &profile, "Marion", "Mitchell", "Morrison", "johnwayne@me.xyz", "Fox",
        base::StringPrintf("%zu123 Zoo St.\nSecond Line\nThird line", i)
            .c_str(),
        "unit 5", "Hollywood", "CA", "91601", "US", "12345678910");

    // Set ranking score such that they appear before the "last" profile (added
    // next).
    profile.set_use_count(12);
    profile.set_use_date(AutofillClock::Now() - base::Days(1));

    AddProfileToPersonalDataManager(profile);
    profiles.push_back(profile);
  }

  // Add another profile that matches, but that will get stripped out.
  AutofillProfile profile(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Marie", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "000 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile.set_use_count(1);
  profile.set_use_date(AutofillClock::Now() - base::Days(7));
  AddProfileToPersonalDataManager(profile);

  ResetPersonalDataManager(USER_MODE_NORMAL);

  std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
      AutofillType(NAME_FIRST), u"Ma", false, std::vector<ServerFieldType>());

  ASSERT_EQ(suggestion_selection::kMaxSuggestedProfilesCount + 1,
            personal_data_->GetProfiles().size());
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ(u"Marion", suggestions[0].main_text.value);
}

// Tests that GetProfileSuggestions orders its suggestions based on the ranking
// formula.
TEST_F(PersonalDataManagerTest, GetProfileSuggestions_Ranking) {
  // Set up the profiles. They are named with number suffixes X so the X is the
  // order in which they should be ordered by the ranking formula.
  AutofillProfile profile3(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Marion3", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile3.set_use_date(AutofillClock::Now() - base::Days(1));
  profile3.set_use_count(5);
  AddProfileToPersonalDataManager(profile3);

  AutofillProfile profile1(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Marion1", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile1.set_use_date(AutofillClock::Now() - base::Days(1));
  profile1.set_use_count(10);
  AddProfileToPersonalDataManager(profile1);

  AutofillProfile profile2(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Marion2", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile2.set_use_date(AutofillClock::Now() - base::Days(15));
  profile2.set_use_count(300);
  AddProfileToPersonalDataManager(profile2);

  ResetPersonalDataManager(USER_MODE_NORMAL);
  std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
      AutofillType(NAME_FIRST), u"Ma", false, std::vector<ServerFieldType>());
  ASSERT_EQ(3U, suggestions.size());
  EXPECT_EQ(suggestions[0].main_text.value, u"Marion1");
  EXPECT_EQ(suggestions[1].main_text.value, u"Marion2");
  EXPECT_EQ(suggestions[2].main_text.value, u"Marion3");
}

// Tests that GetProfileSuggestions returns all profiles suggestions.
TEST_F(PersonalDataManagerTest, GetProfileSuggestions_NumberOfSuggestions) {
  // Set up 3 different profiles.
  AutofillProfile profile1(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Marion1", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  AddProfileToPersonalDataManager(profile1);

  AutofillProfile profile2(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Marion2", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  AddProfileToPersonalDataManager(profile2);

  AutofillProfile profile3(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Marion3", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  AddProfileToPersonalDataManager(profile3);

  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Verify that all the profiles are suggested.
  std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
      AutofillType(NAME_FIRST), std::u16string(), false,
      std::vector<ServerFieldType>());
  EXPECT_EQ(3U, suggestions.size());
}

// Tests that disused profiles are suppressed when supression is enabled and
// the input field is empty.
TEST_F(PersonalDataManagerTest,
       GetProfileSuggestions_SuppressDisusedProfilesOnEmptyField) {
  // Set up 2 different profiles.
  AutofillProfile profile1(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Marion1", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile1.set_use_date(AutofillClock::Now() - base::Days(200));
  AddProfileToPersonalDataManager(profile1);

  AutofillProfile profile2(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Marion2", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "456 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile2.set_use_date(AutofillClock::Now() - base::Days(20));
  AddProfileToPersonalDataManager(profile2);

  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Query with empty string only returns profile2.
  {
    std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
        AutofillType(ADDRESS_HOME_STREET_ADDRESS), std::u16string(), false,
        std::vector<ServerFieldType>());
    EXPECT_EQ(1U, suggestions.size());
  }

  // Query with non-alpha-numeric string only returns profile2.
  {
    std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
        AutofillType(ADDRESS_HOME_STREET_ADDRESS), u"--", false,
        std::vector<ServerFieldType>());
    EXPECT_EQ(1U, suggestions.size());
  }

  // Query with prefix for profile1 returns profile1.
  {
    std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
        AutofillType(ADDRESS_HOME_STREET_ADDRESS), u"123", false,
        std::vector<ServerFieldType>());
    ASSERT_EQ(1U, suggestions.size());
    EXPECT_EQ(u"123 Zoo St., Second Line, Third line, unit 5",
              suggestions[0].main_text.value);
  }

  // Query with prefix for profile2 returns profile2.
  {
    std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
        AutofillType(ADDRESS_HOME_STREET_ADDRESS), u"456", false,
        std::vector<ServerFieldType>());
    EXPECT_EQ(1U, suggestions.size());
    EXPECT_EQ(u"456 Zoo St., Second Line, Third line, unit 5",
              suggestions[0].main_text.value);
  }
}

// Test that local and server profiles are not shown if
// |kAutofillProfileEnabled| is set to |false|.
TEST_F(PersonalDataManagerTest, GetProfileSuggestions_ProfileAutofillDisabled) {
  ///////////////////////////////////////////////////////////////////////
  // Setup.
  ///////////////////////////////////////////////////////////////////////
  const std::string kServerAddressId("server_address1");

  ASSERT_TRUE(TurnOnSyncFeature());

  // Add two different profiles, a local and a server one.
  AutofillProfile local_profile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&local_profile, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5",
                       "Orlando", "FL", "32801", "US", "19482937549");
  AddProfileToPersonalDataManager(local_profile);

  // Add a different server profile.
  std::vector<AutofillProfile> server_profiles;
  server_profiles.emplace_back(AutofillProfile::SERVER_PROFILE,
                               kServerAddressId);
  test::SetProfileInfo(&server_profiles.back(), "John", "", "Doe", "",
                       "ACME Corp", "500 Oak View", "Apt 8", "Houston", "TX",
                       "77401", "US", "");
  // Wallet only provides a full name, so the above first and last names
  // will be ignored when the profile is written to the DB.
  server_profiles.back().SetRawInfo(NAME_FULL, u"John Doe");
  SetServerProfiles(server_profiles);

  // Disable Profile autofill.
  prefs::SetAutofillProfileEnabled(personal_data_->pref_service_, false);
  WaitForOnPersonalDataChanged();
  ConvertWalletAddressesAndUpdateWalletCards();

  // Check that profiles were saved.
  EXPECT_EQ(2U, personal_data_->GetProfiles().size());
  // Expect no autofilled values or suggestions.
  EXPECT_EQ(0U, personal_data_->GetProfilesToSuggest().size());

  std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
      AutofillType(ADDRESS_HOME_STREET_ADDRESS), u"123", false,
      std::vector<ServerFieldType>());
  ASSERT_EQ(0U, suggestions.size());
}

// Test that local and server profiles are not loaded into memory on start-up if
// |kAutofillProfileEnabled| is set to |false|.
TEST_F(PersonalDataManagerTest,
       GetProfileSuggestions_NoProfilesLoadedIfDisabled) {
  ///////////////////////////////////////////////////////////////////////
  // Setup.
  ///////////////////////////////////////////////////////////////////////
  const std::string kServerAddressId("server_address1");

  ASSERT_TRUE(TurnOnSyncFeature());

  // Add two different profiles, a local and a server one.
  AutofillProfile local_profile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&local_profile, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5",
                       "Orlando", "FL", "32801", "US", "19482937549");
  AddProfileToPersonalDataManager(local_profile);

  // Add a different server profile.
  std::vector<AutofillProfile> server_profiles;
  server_profiles.emplace_back(AutofillProfile::SERVER_PROFILE,
                               kServerAddressId);
  test::SetProfileInfo(&server_profiles.back(), "John", "", "Doe", "",
                       "ACME Corp", "500 Oak View", "Apt 8", "Houston", "TX",
                       "77401", "US", "");
  // Wallet only provides a full name, so the above first and last names
  // will be ignored when the profile is written to the DB.
  server_profiles.back().SetRawInfo(NAME_FULL, u"John Doe");
  SetServerProfiles(server_profiles);

  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  ConvertWalletAddressesAndUpdateWalletCards();

  // Expect 2 autofilled values or suggestions.
  EXPECT_EQ(2U, personal_data_->GetProfiles().size());
  EXPECT_EQ(2U, personal_data_->GetProfilesToSuggest().size());

  // Disable Profile autofill.
  prefs::SetAutofillProfileEnabled(personal_data_->pref_service_, false);
  // Reload the database.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Expect no profile values or suggestions were loaded.
  EXPECT_EQ(0U, personal_data_->GetProfilesToSuggest().size());

  std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
      AutofillType(ADDRESS_HOME_STREET_ADDRESS), u"123", false,
      std::vector<ServerFieldType>());
  ASSERT_EQ(0U, suggestions.size());
}

// Test that local profiles are not added if |kAutofillProfileEnabled| is set to
// |false|.
TEST_F(PersonalDataManagerTest,
       GetProfileSuggestions_NoProfilesAddedIfDisabled) {
  // Disable Profile autofill.
  prefs::SetAutofillProfileEnabled(personal_data_->pref_service_, false);

  // Add a local profile.
  AutofillProfile local_profile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&local_profile, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5",
                       "Orlando", "FL", "32801", "US", "19482937549");
  AddProfileToPersonalDataManager(local_profile);

  // Expect no profile values or suggestions were added.
  EXPECT_EQ(0U, personal_data_->GetProfiles().size());
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(PersonalDataManagerTest,
       GetProfileSuggestions_LogProfileSuggestionsMadeWithFormatter) {
  AutofillProfile profile(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Hoa", "", "Pham", "hoa.pham@comcast.net", "",
                       "401 Merrimack St", "", "Lowell", "MA", "01852", "US",
                       "19786744120");
  AddProfileToPersonalDataManager(profile);

  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(
      features::kAutofillUseImprovedLabelDisambiguation);

  base::HistogramTester histogram_tester;
  EXPECT_THAT(
      personal_data_->GetProfileSuggestions(
          AutofillType(NAME_FIRST), std::u16string(), false,
          std::vector<ServerFieldType>{NAME_FIRST, NAME_LAST, EMAIL_ADDRESS,
                                       PHONE_HOME_WHOLE_NUMBER}),
      ElementsAre(testing::Field(
          &Suggestion::main_text,
          Suggestion::Text(u"Hoa", Suggestion::Text::IsPrimary(true)))));
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileSuggestionsMadeWithFormatter", true, 1);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(PersonalDataManagerTest, GetProfileSuggestions_ForContactForm) {
  AutofillProfile profile(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Hoa", "", "Pham", "hoa.pham@comcast.net", "",
                       "401 Merrimack St", "", "Lowell", "MA", "01852", "US",
                       "19786744120");
  AddProfileToPersonalDataManager(profile);

  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(
      features::kAutofillUseImprovedLabelDisambiguation);

  EXPECT_THAT(
      personal_data_->GetProfileSuggestions(
          AutofillType(NAME_FIRST), std::u16string(), false,
          std::vector<ServerFieldType>{NAME_FIRST, NAME_LAST, EMAIL_ADDRESS,
                                       PHONE_HOME_WHOLE_NUMBER}),
      ElementsAre(AllOf(
          testing::Field(&Suggestion::labels,
                         ConstructLabelLineMatrix(
                             {u"(978) 674-4120", u"hoa.pham@comcast.net"})),
          testing::Field(&Suggestion::icon, kAddressEntryIcon))));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(PersonalDataManagerTest, GetProfileSuggestions_AddressForm) {
  AutofillProfile profile(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Hoa", "", "Pham", "hoa.pham@comcast.net", "",
                       "401 Merrimack St", "", "Lowell", "MA", "01852", "US",
                       "19786744120");
  AddProfileToPersonalDataManager(profile);

  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(
      features::kAutofillUseImprovedLabelDisambiguation);

  EXPECT_THAT(personal_data_->GetProfileSuggestions(
                  AutofillType(NAME_FULL), std::u16string(), false,
                  std::vector<ServerFieldType>{
                      NAME_FULL, ADDRESS_HOME_STREET_ADDRESS, ADDRESS_HOME_CITY,
                      ADDRESS_HOME_STATE, ADDRESS_HOME_ZIP}),
              ElementsAre(AllOf(
                  testing::Field(&Suggestion::labels,
                                 ConstructLabelLineMatrix(
                                     {u"401 Merrimack St, Lowell, MA 01852"})),
                  testing::Field(&Suggestion::icon, kAddressEntryIcon))));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(PersonalDataManagerTest, GetProfileSuggestions_AddressPhoneForm) {
  AutofillProfile profile(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Hoa", "", "Pham", "hoa.pham@comcast.net", "",
                       "401 Merrimack St", "", "Lowell", "MA", "01852", "US",
                       "19786744120");
  AddProfileToPersonalDataManager(profile);

  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(
      features::kAutofillUseImprovedLabelDisambiguation);

  EXPECT_THAT(
      personal_data_->GetProfileSuggestions(
          AutofillType(NAME_FULL), std::u16string(), false,
          std::vector<ServerFieldType>{NAME_FULL, ADDRESS_HOME_STREET_ADDRESS,
                                       PHONE_HOME_WHOLE_NUMBER}),
      ElementsAre(
          AllOf(testing::Field(&Suggestion::labels,
                               ConstructLabelLineMatrix(
                                   {u"(978) 674-4120", u"401 Merrimack St"})),
                testing::Field(&Suggestion::icon, kAddressEntryIcon))));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(PersonalDataManagerTest, GetProfileSuggestions_AddressEmailForm) {
  AutofillProfile profile(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Hoa", "", "Pham", "hoa.pham@comcast.net", "",
                       "401 Merrimack St", "", "Lowell", "MA", "01852", "US",
                       "19786744120");
  AddProfileToPersonalDataManager(profile);

  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(
      features::kAutofillUseImprovedLabelDisambiguation);

  EXPECT_THAT(
      personal_data_->GetProfileSuggestions(
          AutofillType(NAME_FULL), std::u16string(), false,
          std::vector<ServerFieldType>{NAME_FULL, ADDRESS_HOME_STREET_ADDRESS,
                                       EMAIL_ADDRESS}),
      ElementsAre(AllOf(
          testing::Field(&Suggestion::labels,
                         ConstructLabelLineMatrix(
                             {u"401 Merrimack St", u"hoa.pham@comcast.net"})),
          testing::Field(&Suggestion::icon, kAddressEntryIcon))));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(PersonalDataManagerTest, GetProfileSuggestions_FormWithOneProfile) {
  AutofillProfile profile(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Hoa", "", "Pham", "hoa.pham@comcast.net", "",
                       "401 Merrimack St", "", "Lowell", "MA", "01852", "US",
                       "19786744120");
  AddProfileToPersonalDataManager(profile);

  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(
      features::kAutofillUseImprovedLabelDisambiguation);

  EXPECT_THAT(
      personal_data_->GetProfileSuggestions(
          AutofillType(NAME_FULL), std::u16string(), false,
          std::vector<ServerFieldType>{NAME_FULL, ADDRESS_HOME_STREET_ADDRESS,
                                       EMAIL_ADDRESS, PHONE_HOME_WHOLE_NUMBER}),
      ElementsAre(
          AllOf(testing::Field(&Suggestion::labels,
                               ConstructLabelLineMatrix({u"401 Merrimack St"})),
                testing::Field(&Suggestion::icon, kAddressEntryIcon))));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(PersonalDataManagerTest,
       GetProfileSuggestions_AddressContactFormWithProfiles) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitWithFeatures(
      /*enabled_features=*/{features::
                                kAutofillEnableRankingFormulaAddressProfiles,
                            features::kAutofillUseImprovedLabelDisambiguation},
      /*disabled_features=*/{});

  AutofillProfile profile1(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Hoa", "", "Pham", "hoa.pham@comcast.net", "",
                       "401 Merrimack St", "", "Lowell", "MA", "01852", "US",
                       "19786744120");

  AutofillProfile profile2(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Hoa", "", "Pham", "hp@aol.com", "",
                       "216 Broadway St", "", "Lowell", "MA", "01854", "US",
                       "19784523366");

  // The profiles' use dates and counts are set make this test deterministic.
  // The suggestion created with data from profile1 should be ranked higher
  // than profile2's associated suggestion. This ensures that profile1's
  // suggestion is the first element in the collection returned by
  // GetProfileSuggestions.
  profile1.set_use_date(AutofillClock::Now());
  profile1.set_use_count(10);
  profile2.set_use_date(AutofillClock::Now() - base::Days(10));
  profile2.set_use_count(1);

  EXPECT_TRUE(profile1.HasGreaterRankingThan(&profile2, AutofillClock::Now()));

  AddProfileToPersonalDataManager(profile1);
  AddProfileToPersonalDataManager(profile2);

  EXPECT_THAT(
      personal_data_->GetProfileSuggestions(
          AutofillType(NAME_FULL), std::u16string(), false,
          std::vector<ServerFieldType>{NAME_FULL, ADDRESS_HOME_STREET_ADDRESS,
                                       EMAIL_ADDRESS, PHONE_HOME_WHOLE_NUMBER}),
      ElementsAre(
          AllOf(testing::Field(&Suggestion::labels,
                               ConstructLabelLineMatrix(
                                   {u"401 Merrimack St", u"(978) 674-4120",
                                    u"hoa.pham@comcast.net"})),
                testing::Field(&Suggestion::icon, kAddressEntryIcon)),
          AllOf(testing::Field(&Suggestion::labels,
                               ConstructLabelLineMatrix({u"216 Broadway St",
                                                         u"(978) 452-3366",
                                                         u"hp@aol.com"})),
                testing::Field(&Suggestion::icon, kAddressEntryIcon))));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
TEST_F(PersonalDataManagerTest, GetProfileSuggestions_MobileShowOne) {
  std::map<std::string, std::string> parameters;
  parameters[features::kAutofillUseMobileLabelDisambiguationParameterName] =
      features::kAutofillUseMobileLabelDisambiguationParameterShowOne;
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeatureWithParameters(
      features::kAutofillUseMobileLabelDisambiguation, parameters);

  AutofillProfile profile1(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Hoa", "", "Pham", "hoa.pham@comcast.net", "",
                       "401 Merrimack St", "", "Lowell", "MA", "01852", "US",
                       "19786744120");
  AutofillProfile profile2(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "María", "", "Lòpez", "maria@aol.com", "",
                       "11 Elkins St", "", "Boston", "MA", "02127", "US",
                       "6172686862");

  // The profiles' use dates and counts are set make this test deterministic.
  // The suggestion created with data from profile1 should be ranked higher
  // than profile2's associated suggestion. This ensures that profile1's
  // suggestion is the first element in the collection returned by
  // GetProfileSuggestions.
  profile1.set_use_date(AutofillClock::Now());
  profile1.set_use_count(10);
  profile2.set_use_date(AutofillClock::Now() - base::Days(10));
  profile2.set_use_count(1);

  AddProfileToPersonalDataManager(profile1);
  AddProfileToPersonalDataManager(profile2);

  // Tests a form with name, email address, and phone number fields.
  EXPECT_THAT(
      personal_data_->GetProfileSuggestions(
          AutofillType(EMAIL_ADDRESS), std::u16string(), false,
          std::vector<ServerFieldType>{NAME_FIRST, NAME_LAST, EMAIL_ADDRESS,
                                       PHONE_HOME_WHOLE_NUMBER}),
      ElementsAre(
          AllOf(testing::Field(&Suggestion::labels,
                               std::vector<std::vector<Suggestion::Text>>{
                                   {Suggestion::Text(u"(978) 674-4120")}}),
                testing::Field(&Suggestion::icon, kAddressEntryIcon)),
          AllOf(testing::Field(&Suggestion::labels,
                               std::vector<std::vector<Suggestion::Text>>{
                                   {Suggestion::Text(u"(617) 268-6862")}}),
                testing::Field(&Suggestion::icon, kAddressEntryIcon))));

  // Tests a form with name, address, phone number, and email address fields.
  EXPECT_THAT(
      personal_data_->GetProfileSuggestions(
          AutofillType(EMAIL_ADDRESS), std::u16string(), false,
          std::vector<ServerFieldType>{NAME_FULL, ADDRESS_HOME_STREET_ADDRESS,
                                       ADDRESS_HOME_CITY, EMAIL_ADDRESS,
                                       PHONE_HOME_WHOLE_NUMBER}),
      ElementsAre(
          AllOf(testing::Field(&Suggestion::labels,
                               std::vector<std::vector<Suggestion::Text>>{
                                   {Suggestion::Text(u"401 Merrimack St")}}),
                testing::Field(&Suggestion::icon, kAddressEntryIcon)),
          AllOf(testing::Field(&Suggestion::labels,
                               std::vector<std::vector<Suggestion::Text>>{
                                   {Suggestion::Text(u"11 Elkins St")}}),
                testing::Field(&Suggestion::icon, kAddressEntryIcon))));
}
#endif  // if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
TEST_F(PersonalDataManagerTest, GetProfileSuggestions_MobileShowAll) {
  std::map<std::string, std::string> parameters;
  parameters[features::kAutofillUseMobileLabelDisambiguationParameterName] =
      features::kAutofillUseMobileLabelDisambiguationParameterShowAll;
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeatureWithParameters(
      features::kAutofillUseMobileLabelDisambiguation, parameters);

  AutofillProfile profile1(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Hoa", "", "Pham", "hoa.pham@comcast.net", "",
                       "401 Merrimack St", "", "Lowell", "MA", "01852", "US",
                       "19786744120");
  AutofillProfile profile2(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "María", "", "Lòpez", "maria@aol.com", "",
                       "11 Elkins St", "", "Boston", "MA", "02127", "US",
                       "6172686862");

  // The profiles' use dates and counts are set make this test deterministic.
  // The suggestion created with data from profile1 should be ranked higher
  // than profile2's associated suggestion. This ensures that profile1's
  // suggestion is the first element in the collection returned by
  // GetProfileSuggestions.
  profile1.set_use_date(AutofillClock::Now());
  profile1.set_use_count(10);
  profile2.set_use_date(AutofillClock::Now() - base::Days(10));
  profile2.set_use_count(1);

  AddProfileToPersonalDataManager(profile1);
  AddProfileToPersonalDataManager(profile2);

  // Tests a form with name, email address, and phone number fields.
  EXPECT_THAT(
      personal_data_->GetProfileSuggestions(
          AutofillType(EMAIL_ADDRESS), std::u16string(), false,
          std::vector<ServerFieldType>{NAME_FIRST, NAME_LAST, EMAIL_ADDRESS,
                                       PHONE_HOME_WHOLE_NUMBER}),
      ElementsAre(
          AllOf(testing::Field(&Suggestion::labels,
                               std::vector<std::vector<Suggestion::Text>>{
                                   {Suggestion::Text(ConstructMobileLabelLine(
                                       {u"Hoa", u"(978) 674-4120"}))}}),
                testing::Field(&Suggestion::icon, kAddressEntryIcon)),
          AllOf(testing::Field(&Suggestion::labels,
                               std::vector<std::vector<Suggestion::Text>>{
                                   {Suggestion::Text(ConstructMobileLabelLine(
                                       {u"María", u"(617) 268-6862"}))}}),
                testing::Field(&Suggestion::icon, kAddressEntryIcon))));

  // Tests a form with name, address, phone number, and email address fields.
  EXPECT_THAT(
      personal_data_->GetProfileSuggestions(
          AutofillType(EMAIL_ADDRESS), std::u16string(), false,
          std::vector<ServerFieldType>{NAME_FULL, ADDRESS_HOME_STREET_ADDRESS,
                                       ADDRESS_HOME_CITY, EMAIL_ADDRESS,
                                       PHONE_HOME_WHOLE_NUMBER}),
      ElementsAre(
          AllOf(
              testing::Field(
                  &Suggestion::labels,
                  std::vector<std::vector<Suggestion::Text>>{
                      {Suggestion::Text(ConstructMobileLabelLine(
                          {u"Hoa", u"401 Merrimack St", u"(978) 674-4120"}))}}),
              testing::Field(&Suggestion::icon, kAddressEntryIcon)),
          AllOf(testing::Field(
                    &Suggestion::labels,
                    std::vector<std::vector<Suggestion::Text>>{
                        {Suggestion::Text(ConstructMobileLabelLine(
                            {u"María", u"11 Elkins St", u"(617) 268-6862"}))}}),
                testing::Field(&Suggestion::icon, kAddressEntryIcon))));
}
#endif  // if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

TEST_F(PersonalDataManagerTest, IsKnownCard_MatchesMaskedServerCard) {
  // Add a masked server card.
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::MASKED_SERVER_CARD, "b459");
  test::SetCreditCardInfo(&server_cards.back(), "Emmet Dalton",
                          "2110" /* last 4 digits */, "12", "2999", "1");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  CreditCard cardToCompare;
  cardToCompare.SetNumber(u"4234 5678 9012 2110" /* Visa */);
  ASSERT_TRUE(personal_data_->IsKnownCard(cardToCompare));
}

TEST_F(PersonalDataManagerTest, IsKnownCard_MatchesFullServerCard) {
  // Add a full server card.
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::FULL_SERVER_CARD, "b459");
  test::SetCreditCardInfo(&server_cards.back(), "Emmet Dalton",
                          "4234567890122110" /* Visa */, "12", "2999", "1");

  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
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
  WaitForOnPersonalDataChanged();
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
  WaitForOnPersonalDataChanged();
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
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  CreditCard cardToCompare;
  cardToCompare.SetNumber(u"4234 5678 9012 0000" /* Visa */);
  ASSERT_FALSE(personal_data_->IsKnownCard(cardToCompare));
}

TEST_F(PersonalDataManagerTest, IsServerCard_DuplicateOfFullServerCard) {
  // Add a full server card.
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::FULL_SERVER_CARD, "b459");
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
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(2U, personal_data_->GetCreditCards().size());

  CreditCard cardToCompare;
  cardToCompare.SetNumber(u"4234 5678 9012 2110" /* Visa */);
  ASSERT_TRUE(personal_data_->IsServerCard(&cardToCompare));
  ASSERT_TRUE(personal_data_->IsServerCard(&local_card));
}

TEST_F(PersonalDataManagerTest, IsServerCard_DuplicateOfMaskedServerCard) {
  // Add a masked server card.
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::MASKED_SERVER_CARD, "b459");
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
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(2U, personal_data_->GetCreditCards().size());

  CreditCard cardToCompare;
  cardToCompare.SetNumber(u"4234 5678 9012 2110" /* Visa */);
  ASSERT_TRUE(personal_data_->IsServerCard(&cardToCompare));
  ASSERT_TRUE(personal_data_->IsServerCard(&local_card));
}

TEST_F(PersonalDataManagerTest, IsServerCard_AlreadyServerCard) {
  std::vector<CreditCard> server_cards;
  // Create a full server card.
  CreditCard full_server_card(CreditCard::FULL_SERVER_CARD, "c789");
  test::SetCreditCardInfo(&full_server_card, "Homer Simpson",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  server_cards.push_back(full_server_card);
  // Create a masked server card.
  CreditCard masked_card(CreditCard::MASKED_SERVER_CARD, "a123");
  test::SetCreditCardInfo(&masked_card, "Homer Simpson", "2110" /* Visa */,
                          "01", "2999", "1");
  masked_card.SetNetworkForMaskedCard(kVisaCard);
  server_cards.push_back(masked_card);

  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
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
  WaitForOnPersonalDataChanged();
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
  server_cards.emplace_back(CreditCard::MASKED_SERVER_CARD, "b459");
  test::SetCreditCardInfo(&server_cards.back(), "Emmet Dalton", "2110", "12",
                          "2999", "1");
  server_cards.back().set_use_count(2);
  server_cards.back().set_use_date(AutofillClock::Now() - base::Days(1));
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  server_cards.emplace_back(CreditCard::FULL_SERVER_CARD, "b460");
  test::SetCreditCardInfo(&server_cards.back(), "Jesse James", "2109", "12",
                          "2999", "1");
  server_cards.back().set_use_count(6);
  server_cards.back().set_use_date(AutofillClock::Now() - base::Days(1));

  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
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
  server_cards.emplace_back(CreditCard::MASKED_SERVER_CARD, "b459");
  test::SetCreditCardInfo(&server_cards.back(), "Emmet Dalton", "2110", "12",
                          "2999", "1");
  server_cards.back().set_use_count(2);
  server_cards.back().set_use_date(AutofillClock::Now() - base::Days(1));
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  server_cards.emplace_back(CreditCard::FULL_SERVER_CARD, "b460");
  test::SetCreditCardInfo(&server_cards.back(), "Jesse James", "2109", "12",
                          "2999", "1");
  server_cards.back().set_use_count(6);
  server_cards.back().set_use_date(AutofillClock::Now() - base::Days(1));

  SetServerCards(server_cards);
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();

  // Disable Credit card autofill.
  prefs::SetAutofillCreditCardEnabled(personal_data_->pref_service_, false);
  WaitForOnPersonalDataChanged();

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
  server_cards.emplace_back(CreditCard::MASKED_SERVER_CARD, "b459");
  test::SetCreditCardInfo(&server_cards.back(), "Emmet Dalton", "2110", "12",
                          "2999", "1");
  server_cards.back().set_use_count(2);
  server_cards.back().set_use_date(AutofillClock::Now() - base::Days(1));
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  server_cards.emplace_back(CreditCard::FULL_SERVER_CARD, "b460");
  test::SetCreditCardInfo(&server_cards.back(), "Jesse James", "2109", "12",
                          "2999", "1");
  server_cards.back().set_use_count(6);
  server_cards.back().set_use_date(AutofillClock::Now() - base::Days(1));

  SetServerCards(server_cards);

  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();

  // Expect 5 autofilled values or suggestions.
  EXPECT_EQ(5U, personal_data_->GetCreditCards().size());

  // Disable Credit card autofill.
  prefs::SetAutofillCreditCardEnabled(personal_data_->pref_service_, false);
  // Reload the database.
  ResetPersonalDataManager(USER_MODE_NORMAL);

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
  prefs::SetAutofillCreditCardEnabled(personal_data_->pref_service_, false);

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
  SetUpReferenceLocalCreditCards();

  // Add some server cards. If there are local dupes, the locals should be
  // hidden.
  std::vector<CreditCard> server_cards;
  // This server card matches a local card, except the local card is missing the
  // number. This should count as a dupe and thus not be shown in the
  // suggestions since the locally saved card takes precedence.
  server_cards.emplace_back(CreditCard::MASKED_SERVER_CARD, "a123");
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "3456" /* Visa */, "01", "2999", "1");
  server_cards.back().set_use_count(2);
  server_cards.back().set_use_date(AutofillClock::Now() - base::Days(15));
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  // This unmasked server card is an exact dupe of a local card. Therefore only
  // this card should appear in the suggestions as full server cards have
  // precedence over local cards.
  server_cards.emplace_back(CreditCard::FULL_SERVER_CARD, "c789");
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  server_cards.back().set_use_count(1);
  server_cards.back().set_use_date(AutofillClock::Now() - base::Days(15));

  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
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
  EXPECT_EQ(CreditCard::LOCAL_CARD, card_to_suggest[0]->record_type());
  EXPECT_EQ(CreditCard::FULL_SERVER_CARD, card_to_suggest[1]->record_type());
  EXPECT_EQ(CreditCard::LOCAL_CARD, card_to_suggest[2]->record_type());
}

// Tests that a full server card can be a dupe of more than one local card.
TEST_F(PersonalDataManagerTest,
       GetCreditCardsToSuggest_ServerCardDuplicateOfMultipleLocalCards) {
  SetUpReferenceLocalCreditCards();

  // Add a duplicate server card.
  std::vector<CreditCard> server_cards;
  // This unmasked server card is an exact dupe of a local card. Therefore only
  // the local card should appear in the suggestions.
  server_cards.emplace_back(CreditCard::FULL_SERVER_CARD, "c789");
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");

  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
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

  WaitForOnPersonalDataChanged();

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
  CreditCard full_server_card(CreditCard::FULL_SERVER_CARD, "c789");
  test::SetCreditCardInfo(&full_server_card, "Homer Simpson",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  full_server_card.set_use_count(1);
  full_server_card.set_use_date(AutofillClock::Now() - base::Days(15));
  credit_cards.push_back(&full_server_card);

  PersonalDataManager::DedupeCreditCardToSuggest(&credit_cards);
  ASSERT_EQ(1U, credit_cards.size());

  const CreditCard* deduped_card(credit_cards.front());
  EXPECT_TRUE(*deduped_card == full_server_card);
}

// Tests that only the local card is kept when deduping with a masked server
// duplicate of it.
TEST_F(PersonalDataManagerTest, DedupeCreditCardToSuggest_LocalShadowsMasked) {
  std::list<CreditCard*> credit_cards;

  CreditCard local_card("1141084B-72D7-4B73-90CF-3D6AC154673B",
                        test::kEmptyOrigin);
  local_card.set_use_count(300);
  local_card.set_use_date(AutofillClock::Now() - base::Days(10));
  test::SetCreditCardInfo(&local_card, "Homer Simpson",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  credit_cards.push_back(&local_card);

  // Create a masked server card that is a duplicate of a local card.
  CreditCard masked_card(CreditCard::MASKED_SERVER_CARD, "a123");
  test::SetCreditCardInfo(&masked_card, "Homer Simpson", "3456" /* Visa */,
                          "01", "2999", "1");
  masked_card.set_use_count(2);
  masked_card.set_use_date(AutofillClock::Now() - base::Days(15));
  masked_card.SetNetworkForMaskedCard(kVisaCard);
  credit_cards.push_back(&masked_card);

  PersonalDataManager::DedupeCreditCardToSuggest(&credit_cards);
  ASSERT_EQ(1U, credit_cards.size());

  const CreditCard* deduped_card(credit_cards.front());
  EXPECT_TRUE(*deduped_card == local_card);
}

// Tests that identical full server and masked credit cards are not deduped.
TEST_F(PersonalDataManagerTest, DedupeCreditCardToSuggest_FullServerAndMasked) {
  std::list<CreditCard*> credit_cards;

  // Create a full server card that is a duplicate of one of the local cards.
  CreditCard full_server_card(CreditCard::FULL_SERVER_CARD, "c789");
  test::SetCreditCardInfo(&full_server_card, "Homer Simpson",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  full_server_card.set_use_count(1);
  full_server_card.set_use_date(AutofillClock::Now() - base::Days(15));
  credit_cards.push_back(&full_server_card);

  // Create a masked server card that is a duplicate of a local card.
  CreditCard masked_card(CreditCard::MASKED_SERVER_CARD, "a123");
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
  CreditCard masked_card(CreditCard::MASKED_SERVER_CARD, "b456");
  test::SetCreditCardInfo(&masked_card, "Homer Simpson", "0005", "12", "2999",
                          "1");
  masked_card.set_use_count(3);
  masked_card.set_use_date(AutofillClock::Now() - base::Days(15));
  // credit_card4.SetNetworkForMaskedCard(kVisaCard);
  credit_cards.push_back(&masked_card);

  // Create a full server card that is slightly different of the two other
  // cards.
  CreditCard full_server_card(CreditCard::FULL_SERVER_CARD, "c789");
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
  WaitForOnPersonalDataChanged();
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
  base::RunLoop profile_run_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
      .WillOnce(QuitMessageLoop(&profile_run_loop));
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(1);
  personal_data_->RecordUseOf(&profile);
  profile_run_loop.Run();

  added_profile = personal_data_->GetProfileByGUID(profile.guid());
  added_card = personal_data_->GetCreditCardByGUID(credit_card.guid());
  ASSERT_TRUE(added_profile);
  ASSERT_TRUE(added_card);
  Check(*added_profile, 2u, kSomeLaterTime, kArbitraryTime);
  Check(*added_card, 1u, kArbitraryTime, kArbitraryTime);

  // Use |credit_card|, then verify usage stats.
  base::RunLoop credit_card_run_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
      .WillOnce(QuitMessageLoop(&credit_card_run_loop));
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(1);
  personal_data_->RecordUseOf(&credit_card);
  credit_card_run_loop.Run();

  added_profile = personal_data_->GetProfileByGUID(profile.guid());
  added_card = personal_data_->GetCreditCardByGUID(credit_card.guid());
  ASSERT_TRUE(added_profile);
  ASSERT_TRUE(added_card);
  Check(*added_profile, 2u, kSomeLaterTime, kArbitraryTime);
  Check(*added_card, 2u, kSomeLaterTime, kArbitraryTime);
}

TEST_F(PersonalDataManagerTest, ClearAllServerData) {
  // Add a server card.
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::MASKED_SERVER_CARD, "a123");
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "3456" /* Visa */, "01", "2999", "1");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);
  SetServerCards(server_cards);
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();

  // The card and profile should be there.
  ResetPersonalDataManager(USER_MODE_NORMAL);
  EXPECT_FALSE(personal_data_->GetCreditCards().empty());

  personal_data_->ClearAllServerData();

  // Reload the database, everything should be gone.
  ResetPersonalDataManager(USER_MODE_NORMAL);
  EXPECT_TRUE(personal_data_->GetCreditCards().empty());
}

TEST_F(PersonalDataManagerTest, ClearAllLocalData) {
  // Add some local data.
  AddProfileToPersonalDataManager(test::GetFullProfile());
  personal_data_->AddCreditCard(test::GetCreditCard());
  personal_data_->Refresh();

  // The card and profile should be there.
  ResetPersonalDataManager(USER_MODE_NORMAL);
  EXPECT_FALSE(personal_data_->GetCreditCards().empty());
  EXPECT_FALSE(personal_data_->GetProfiles().empty());

  personal_data_->ClearAllLocalData();

  // Reload the database, everything should be gone.
  ResetPersonalDataManager(USER_MODE_NORMAL);
  EXPECT_TRUE(personal_data_->GetCreditCards().empty());
  EXPECT_TRUE(personal_data_->GetProfiles().empty());
}

// Tests the SaveImportedProfile method with different profiles to make sure the
// merge logic works correctly.
typedef struct {
  autofill::ServerFieldType field_type;
  std::u16string field_value;
} ProfileField;

typedef std::vector<ProfileField> ProfileFields;

typedef struct {
  // Each test starts with a default pre-existing profile and applies these
  // changes to it.
  ProfileFields changes_to_original;
  // Each test saves a second profile. Applies these changes to the default
  // values before saving.
  ProfileFields changes_to_new;
  // For tests with profile merging, makes sure that these fields' values are
  // the ones we expect (depending on the test).
  ProfileFields changed_field_values;
} SaveImportedProfileTestCase;

class SaveImportedProfileTest
    : public PersonalDataManagerHelper,
      public testing::TestWithParam<SaveImportedProfileTestCase> {
 public:
  SaveImportedProfileTest() = default;
  ~SaveImportedProfileTest() override = default;

  void SetUp() override {
    SetUpTest();
    ResetPersonalDataManager(USER_MODE_NORMAL);
  }

  void TearDown() override { TearDownTest(); }
};

TEST_P(SaveImportedProfileTest, SaveImportedProfile) {
  // Create the test clock.
  TestAutofillClock test_clock;
  auto test_case = GetParam();
  // Set the time to a specific value.
  test_clock.SetNow(kArbitraryTime);

  AutofillProfile original_profile = GetDefaultProfile();

  // Apply changes to the original profile (if applicable).
  for (ProfileField change : test_case.changes_to_original) {
    original_profile.SetRawInfoWithVerificationStatus(
        change.field_type, change.field_value, VerificationStatus::kObserved);
  }

  // Initialize PersonalDataManager with the original profile.
  original_profile.FinalizeAfterImport();
  SetUpReferenceProfile(original_profile);

  // Set the time to a bigger value.
  test_clock.SetNow(kSomeLaterTime);

  AutofillProfile profile2(GetDefaultProfile());

  // Apply changes to the second profile (if applicable).
  for (ProfileField change : test_case.changes_to_new) {
    profile2.SetRawInfoWithVerificationStatus(
        change.field_type, change.field_value, VerificationStatus::kObserved);
  }

  profile2.FinalizeAfterImport();
  SaveImportedProfileToPersonalDataManager(profile2);

  const std::vector<AutofillProfile*>& saved_profiles =
      personal_data_->GetProfiles();

  // Get the set of profiles persisted in the db.
  std::vector<std::unique_ptr<AutofillProfile>> db_profiles;
  profile_autofill_table_->GetAutofillProfiles(
      &db_profiles, AutofillProfile::Source::kLocalOrSyncable);

  // Expect the profiles held in-memory by PersonalDataManager and the db
  // profiles to be the same.
  EXPECT_EQ(db_profiles.size(), saved_profiles.size());
  for (const auto& it : db_profiles) {
    AutofillProfile* inmemory_profile =
        personal_data_->GetProfileByGUID(it->guid());
    ASSERT_TRUE(inmemory_profile != nullptr);
    EXPECT_TRUE(it->EqualsIncludingUsageStatsForTesting(*inmemory_profile));
  }

  // If there are no merge changes to verify, make sure that two profiles were
  // saved.
  if (test_case.changed_field_values.empty()) {
    EXPECT_EQ(2U, saved_profiles.size());
  } else {
    EXPECT_EQ(1U, saved_profiles.size());

    // Make sure the new information was merged correctly.
    for (ProfileField changed_field : test_case.changed_field_values) {
      EXPECT_EQ(changed_field.field_value,
                saved_profiles.front()->GetRawInfo(changed_field.field_type));
    }
    // Verify that the merged profile's use count, use date and modification
    // date were properly updated.
    EXPECT_EQ(1U, saved_profiles.front()->use_count());
    EXPECT_EQ(kSomeLaterTime, saved_profiles.front()->use_date());

    // The modification date is only updated when the profile actually changes.
    EXPECT_EQ(*saved_profiles.front() == original_profile ? kArbitraryTime
                                                          : kSomeLaterTime,
              saved_profiles.front()->modification_date());
  }

  // Erase the profiles for the next test.
  ResetProfiles();
}

INSTANTIATE_TEST_SUITE_P(
    PersonalDataManagerTest,
    SaveImportedProfileTest,
    testing::Values(
        // Test that saving an identical profile except for the name results
        // in two profiles being saved.
        SaveImportedProfileTestCase{ProfileFields(),
                                    {{NAME_FIRST, u"Marionette"}}},

        // Test that saving an identical profile except with the middle name
        // initial instead of the full middle name results in the profiles
        // getting merged and the full middle name being kept.
        SaveImportedProfileTestCase{ProfileFields(),
                                    {{NAME_MIDDLE, u"M"}},
                                    {{NAME_MIDDLE, u"Mitchell"},
                                     {NAME_FULL, u"Marion Mitchell Morrison"}}},

        // Test that saving an identical profile except with the full middle
        // name instead of the middle name initial results in the profiles
        // getting merged and the full middle name replacing the initial.
        SaveImportedProfileTestCase{{{NAME_MIDDLE, u"M"}},
                                    {{NAME_MIDDLE, u"Mitchell"}},
                                    {{NAME_MIDDLE, u"Mitchell"}}},

        // Test that saving an identical profile except with no middle name
        // results in the profiles getting merged and the full middle name
        // being kept.
        SaveImportedProfileTestCase{ProfileFields(),
                                    {{NAME_MIDDLE, u""}},
                                    {{NAME_MIDDLE, u"Mitchell"}}},

        // Test that saving an identical profile except with a middle name
        // initial results in the profiles getting merged and the middle
        // name initial being saved.
        SaveImportedProfileTestCase{{{NAME_MIDDLE, u""}},
                                    {{NAME_MIDDLE, u"M"}},
                                    {{NAME_MIDDLE, u"M"}}},

        // Test that saving an identical profile except with a middle name
        // results in the profiles getting merged and the full middle name
        // being saved.
        SaveImportedProfileTestCase{{{NAME_MIDDLE, u""}},
                                    {{NAME_MIDDLE, u"Mitchell"}},
                                    {{NAME_MIDDLE, u"Mitchell"}}},

        // Test that saving an identical profile except with the full name
        // set instead of the name parts results in the two profiles being
        // merged and all the name parts kept and the full name being added.
        SaveImportedProfileTestCase{
            {
                {NAME_FIRST, u"Marion"},
                {NAME_MIDDLE, u"Mitchell"},
                {NAME_LAST, u"Morrison"},
                {NAME_FULL, u""},
            },
            {
                {NAME_FIRST, u""},
                {NAME_MIDDLE, u""},
                {NAME_LAST, u""},
                {NAME_FULL, u"Marion Mitchell Morrison"},
            },
            {
                {NAME_FIRST, u"Marion"},
                {NAME_MIDDLE, u"Mitchell"},
                {NAME_LAST, u"Morrison"},
                {NAME_FULL, u"Marion Mitchell Morrison"},
            },
        },

        // Test that saving a identical profile except with the name parts
        // set instead of the full name results in the two profiles being
        // merged and the full name being kept and all the name parts being
        // added.
        SaveImportedProfileTestCase{
            {
                {NAME_FIRST, u""},
                {NAME_MIDDLE, u""},
                {NAME_LAST, u""},
                {NAME_FULL, u"Marion Mitchell Morrison"},
            },
            {
                {NAME_FIRST, u"Marion"},
                {NAME_MIDDLE, u"Mitchell"},
                {NAME_LAST, u"Morrison"},
                {NAME_FULL, u""},
            },
            {
                {NAME_FIRST, u"Marion"},
                {NAME_MIDDLE, u"Mitchell"},
                {NAME_LAST, u"Morrison"},
                {NAME_FULL, u"Marion Mitchell Morrison"},
            },
        },

        // Test that saving a profile that has only a full name set does not
        // get merged with a profile with only the name parts set if the
        // names are different.
        SaveImportedProfileTestCase{
            {
                {NAME_FIRST, u"Marion"},
                {NAME_MIDDLE, u"Mitchell"},
                {NAME_LAST, u"Morrison"},
                {NAME_FULL, u""},
            },
            {
                {NAME_FIRST, u""},
                {NAME_MIDDLE, u""},
                {NAME_LAST, u""},
                {NAME_FULL, u"John Thompson Smith"},
            },
        },

        // Test that saving a profile that has only the name parts set does
        // not get merged with a profile with only the full name set if the
        // names are different.
        SaveImportedProfileTestCase{
            {
                {NAME_FIRST, u""},
                {NAME_MIDDLE, u""},
                {NAME_LAST, u""},
                {NAME_FULL, u"John Thompson Smith"},
            },
            {
                {NAME_FIRST, u"Marion"},
                {NAME_MIDDLE, u"Mitchell"},
                {NAME_LAST, u"Morrison"},
                {NAME_FULL, u""},
            },
        },

        // Test that saving an identical profile except for the first
        // address line results in two profiles being saved.
        SaveImportedProfileTestCase{
            ProfileFields(),
            {{ADDRESS_HOME_LINE1, u"123 Aquarium St."}}},

        // Test that saving an identical profile except for the second
        // address line results in two profiles being saved.
        SaveImportedProfileTestCase{ProfileFields(),
                                    {{ADDRESS_HOME_LINE2, u"unit 7"}}},

        // Tests that saving an identical profile that has a new piece of
        // information (company name) results in a merge and that the
        // original empty value gets overwritten by the new information.
        SaveImportedProfileTestCase{{{COMPANY_NAME, u""}},
                                    ProfileFields(),
                                    {{COMPANY_NAME, u"Fox"}}},

        // Tests that saving an identical profile except a loss of
        // information results in a merge but the original value is not
        // overwritten (no information loss).
        SaveImportedProfileTestCase{ProfileFields(),
                                    {{COMPANY_NAME, u""}},
                                    {{COMPANY_NAME, u"Fox"}}},

        // Tests that saving an identical profile except a slightly
        // different postal code results in a merge with the new value kept.
        SaveImportedProfileTestCase{{{ADDRESS_HOME_ZIP, u"R2C 0A1"}},
                                    {{ADDRESS_HOME_ZIP, u"R2C0A1"}},
                                    {{ADDRESS_HOME_ZIP, u"R2C0A1"}}},
        SaveImportedProfileTestCase{{{ADDRESS_HOME_ZIP, u"R2C0A1"}},
                                    {{ADDRESS_HOME_ZIP, u"R2C 0A1"}},
                                    {{ADDRESS_HOME_ZIP, u"R2C 0A1"}}},
        SaveImportedProfileTestCase{{{ADDRESS_HOME_ZIP, u"r2c 0a1"}},
                                    {{ADDRESS_HOME_ZIP, u"R2C0A1"}},
                                    {{ADDRESS_HOME_ZIP, u"R2C0A1"}}},

        // Tests that saving an identical profile plus a new piece of
        // information on the address line 2 results in a merge and that the
        // original empty value gets overwritten by the new information.
        SaveImportedProfileTestCase{{{ADDRESS_HOME_LINE2, u""}},
                                    ProfileFields(),
                                    {{ADDRESS_HOME_LINE2, u"unit 5"}}},

        // Tests that saving an identical profile except a loss of
        // information on the address line 2 results in a merge but that the
        // original value gets not overwritten (no information loss).
        SaveImportedProfileTestCase{ProfileFields(),
                                    {{ADDRESS_HOME_LINE2, u""}},
                                    {{ADDRESS_HOME_LINE2, u"unit 5"}}},

        // Tests that saving an identical except with more punctuation in
        // the fist address line, while the second is empty, results in a
        // merge and that the original address gets overwritten.
        SaveImportedProfileTestCase{
            {{ADDRESS_HOME_LINE2, u""}},
            {{ADDRESS_HOME_LINE2, u""}, {ADDRESS_HOME_LINE1, u"123, Zoo St."}},
            {{ADDRESS_HOME_LINE1, u"123, Zoo St."}}},

        // Tests that saving an identical profile except with less
        // punctuation in the fist address line, while the second is empty,
        // results in a merge and that the longer address is retained.
        SaveImportedProfileTestCase{
            {{ADDRESS_HOME_LINE2, u""}, {ADDRESS_HOME_LINE1, u"123, Zoo St."}},
            {{ADDRESS_HOME_LINE2, u""}},
            {{ADDRESS_HOME_LINE1, u"123 Zoo St"}}},

        // Tests that saving an identical profile except additional
        // punctuation in the two address lines results in a merge and that
        // the newer address is retained.
        SaveImportedProfileTestCase{ProfileFields(),
                                    {{ADDRESS_HOME_LINE1, u"123, Zoo St."},
                                     {ADDRESS_HOME_LINE2, u"unit. 5"}},
                                    {{ADDRESS_HOME_LINE1, u"123, Zoo St."},
                                     {ADDRESS_HOME_LINE2, u"unit. 5"}}},

        // Tests that saving an identical profile except less punctuation in
        // the two address lines results in a merge and that the newer
        // address is retained.
        SaveImportedProfileTestCase{{{ADDRESS_HOME_LINE1, u"123, Zoo St."},
                                     {ADDRESS_HOME_LINE2, u"unit. 5"}},
                                    ProfileFields(),
                                    {{ADDRESS_HOME_LINE1, u"123 Zoo St"},
                                     {ADDRESS_HOME_LINE2, u"unit 5"}}},

        // Tests that saving an identical profile with accented characters
        // in the two address lines results in a merge and that the newer
        // address is retained.
        SaveImportedProfileTestCase{ProfileFields(),
                                    {{ADDRESS_HOME_LINE1, u"123 Zôö St"},
                                     {ADDRESS_HOME_LINE2, u"üñìt 5"}},
                                    {{ADDRESS_HOME_LINE1, u"123 Zôö St"},
                                     {ADDRESS_HOME_LINE2, u"üñìt 5"}}},

        // Tests that saving an identical profile without accented
        // characters in the two address lines results in a merge and that
        // the newer address is retained.
        SaveImportedProfileTestCase{{{ADDRESS_HOME_LINE1, u"123 Zôö St"},
                                     {ADDRESS_HOME_LINE2, u"üñìt 5"}},
                                    ProfileFields(),
                                    {{ADDRESS_HOME_LINE1, u"123 Zoo St"},
                                     {ADDRESS_HOME_LINE2, u"unit 5"}}},

        // Tests that saving an identical profile except that the address
        // line 1 is in the address line 2 results in a merge and that the
        // multi-lne address is retained.
        SaveImportedProfileTestCase{
            ProfileFields(),
            {{ADDRESS_HOME_LINE1, u"123 Zoo St, unit 5"},
             {ADDRESS_HOME_LINE2, u""}},
            {{ADDRESS_HOME_LINE1, u"123 Zoo St"},
             {ADDRESS_HOME_LINE2, u"unit 5"}}},

        // Tests that saving an identical profile except that the address
        // line 2 contains part of the old address line 1 results in a merge
        // and that the original address lines of the reference profile get
        // overwritten.
        SaveImportedProfileTestCase{
            {{ADDRESS_HOME_LINE1, u"123 Zoo St, unit 5"},
             {ADDRESS_HOME_LINE2, u""}},
            ProfileFields(),
            {{ADDRESS_HOME_LINE1, u"123 Zoo St"},
             {ADDRESS_HOME_LINE2, u"unit 5"}}},

        // Tests that saving an identical profile except that the state is
        // the abbreviation instead of the full form results in a merge and
        // that the original state gets overwritten.
        SaveImportedProfileTestCase{{{ADDRESS_HOME_STATE, u"California"}},
                                    ProfileFields(),
                                    {{ADDRESS_HOME_STATE, u"CA"}}},

        // Tests that saving an identical profile except that the state is
        // the full form instead of the abbreviation results in a merge and
        // that the abbreviated state is retained.
        SaveImportedProfileTestCase{ProfileFields(),
                                    {{ADDRESS_HOME_STATE, u"California"}},
                                    {{ADDRESS_HOME_STATE, u"CA"}}},

        // Tests that saving and identical profile except that the company
        // name has different punctuation and case results in a merge and
        // that the syntax of the new profile replaces the old one.
        SaveImportedProfileTestCase{{{COMPANY_NAME, u"Stark inc"}},
                                    {{COMPANY_NAME, u"Stark Inc."}},
                                    {{COMPANY_NAME, u"Stark Inc."}}}));

// Tests that MergeProfile tries to merge the imported profile into the
// existing profile in decreasing order of ranking score.
TEST_F(PersonalDataManagerTest, MergeProfile_Ranking) {
  // Create two very similar profiles except with different company names.
  std::unique_ptr<AutofillProfile> profile1 = std::make_unique<AutofillProfile>(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(profile1.get(), "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "SNP", "742 Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "12345678910");
  AutofillProfile* profile2 = new AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(profile2, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "Fox", "742 Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "12345678910");

  // Give the "Fox" profile a larger ranking score.
  profile2->set_use_count(15);

  // Create the |existing_profiles| vector.
  std::vector<std::unique_ptr<AutofillProfile>> existing_profiles;
  existing_profiles.push_back(std::move(profile1));
  existing_profiles.push_back(std::unique_ptr<AutofillProfile>(profile2));

  // Create a new imported profile with no company name.
  AutofillProfile imported_profile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&imported_profile, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "", "742 Evergreen Terrace", "",
                       "Springfield", "IL", "91601", "US", "12345678910");

  // Merge the imported profile into the existing profiles.
  std::vector<AutofillProfile> profiles;
  std::string guid = AutofillProfileComparator::MergeProfile(
      imported_profile, existing_profiles, "US-EN", &profiles);

  // The new profile should be merged into the "fox" profile.
  EXPECT_EQ(profile2->guid(), guid);
}

// Tests that MergeProfile produces a merged profile with the expected usage
// statistics.
// Flaky on TSan, see crbug.com/686226.
#if defined(THREAD_SANITIZER)
#define MAYBE_MergeProfile_UsageStats DISABLED_MergeProfile_UsageStats
#else
#define MAYBE_MergeProfile_UsageStats MergeProfile_UsageStats
#endif
TEST_F(PersonalDataManagerTest, MAYBE_MergeProfile_UsageStats) {
  // Create the test clock and set the time to a specific value.
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);

  // Create an initial profile with a use count of 10, an old use date and an
  // old modification date of 4 days ago.
  AutofillProfile* profile = new AutofillProfile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(profile, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "SNP", "742 Evergreen Terrace",
                       "", "Springfield", "IL", "91601", "US", "12345678910");
  profile->set_use_count(4U);
  EXPECT_EQ(kArbitraryTime, profile->use_date());
  EXPECT_EQ(kArbitraryTime, profile->modification_date());

  // Create the |existing_profiles| vector.
  std::vector<std::unique_ptr<AutofillProfile>> existing_profiles;
  existing_profiles.push_back(std::unique_ptr<AutofillProfile>(profile));

  // Change the current date.
  test_clock.SetNow(kSomeLaterTime);

  // Create a new imported profile that will get merged with the existing one.
  AutofillProfile imported_profile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&imported_profile, "Homer", "Jay", "Simpson",
                       "homer.simpson@abc.com", "", "742 Evergreen Terrace", "",
                       "Springfield", "IL", "91601", "US", "12345678910");

  // Change the current date.
  test_clock.SetNow(kMuchLaterTime);

  // Merge the imported profile into the existing profiles.
  std::vector<AutofillProfile> profiles;
  std::string guid = AutofillProfileComparator::MergeProfile(
      imported_profile, existing_profiles, "US-EN", &profiles);

  // The new profile should be merged into the existing profile.
  EXPECT_EQ(profile->guid(), guid);
  EXPECT_EQ(1U, profiles.size());
  // The use count should have be max(4, 1) => 4.
  EXPECT_EQ(4U, profiles[0].use_count());
  // The use date should be the one of the most recent profile, which is
  // kSecondArbitraryTime.
  EXPECT_EQ(kSomeLaterTime, profiles[0].use_date());
  // Since the merge is considered a modification, the modification_date should
  // be set to kMuchLaterTime.
  EXPECT_EQ(kMuchLaterTime, profiles[0].modification_date());
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
  WaitForOnPersonalDataChanged();

  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  std::unordered_set<std::u16string> expectedToRemain = {u"Clyde"};
  for (auto* card : personal_data_->GetCreditCards()) {
    EXPECT_NE(expectedToRemain.end(),
              expectedToRemain.find(card->GetRawInfo(CREDIT_CARD_NAME_FULL)));
  }
}

// Tests that a new local profile is created if no existing one is a duplicate
// of the server address. Also tests that the billing address relationship was
// transferred to the converted address.
TEST_F(PersonalDataManagerTest,
       ConvertWalletAddressesAndUpdateWalletCards_NewProfile) {
  ///////////////////////////////////////////////////////////////////////
  // Setup.
  ///////////////////////////////////////////////////////////////////////
  ASSERT_TRUE(TurnOnSyncFeature());

  base::HistogramTester histogram_tester;
  const std::string kServerAddressId("server_address1");

  // Add two different profiles, a local and a server one. Set the use stats so
  // the server profile has a higher ranking, to have a predictable ordering to
  // validate results.
  AutofillProfile local_profile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&local_profile, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5",
                       "Orlando", "FL", "32801", "US", "19482937549");
  local_profile.set_use_count(1);
  AddProfileToPersonalDataManager(local_profile);

  // Add a different server profile.
  std::vector<AutofillProfile> server_profiles;
  server_profiles.push_back(
      AutofillProfile(AutofillProfile::SERVER_PROFILE, kServerAddressId));
  test::SetProfileInfo(&server_profiles.back(), "John", "", "Doe", "",
                       "ACME Corp", "500 Oak View", "Apt 8", "Houston", "TX",
                       "77401", "US", "");
  EXPECT_EQ(server_profiles.back().GetRawInfo(NAME_FULL), u"John Doe");
  server_profiles.back().set_use_count(100);
  SetServerProfiles(server_profiles);

  // Add a server and a local card that have the server address as billing
  // address.
  CreditCard local_card("287151C8-6AB1-487C-9095-28E80BE5DA15",
                        test::kEmptyOrigin);
  test::SetCreditCardInfo(&local_card, "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  local_card.set_billing_address_id(kServerAddressId);
  personal_data_->AddCreditCard(local_card);

  std::vector<CreditCard> server_cards;
  server_cards.push_back(
      CreditCard(CreditCard::MASKED_SERVER_CARD, "server_card1"));
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "1111" /* Visa */, "01", "2999", "1");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);
  server_cards.back().set_billing_address_id(kServerAddressId);
  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  ASSERT_EQ(1U, personal_data_->GetProfiles().size());
  ASSERT_EQ(1U, personal_data_->GetServerProfiles().size());
  ASSERT_EQ(2U, personal_data_->GetCreditCards().size());

  ConvertWalletAddressesAndUpdateWalletCards();

  // The Wallet address should have been added as a new local profile.
  EXPECT_EQ(2U, personal_data_->GetProfiles().size());
  EXPECT_EQ(1U, personal_data_->GetServerProfiles().size());
  histogram_tester.ExpectUniqueSample("Autofill.WalletAddressConversionType",
                                      AutofillMetrics::CONVERTED_ADDRESS_ADDED,
                                      1);

  // The conversion should be recorded in the Wallet address.
  EXPECT_TRUE(personal_data_->GetServerProfiles().back()->has_converted());

  // Get the profiles, sorted by ranking to have a deterministic order.
  std::vector<AutofillProfile*> profiles =
      personal_data_->GetProfilesToSuggest();

  // Make sure that the two profiles have not merged.
  ASSERT_EQ(2U, profiles.size());
  EXPECT_EQ(u"John", profiles[0]->GetRawInfo(NAME_FIRST));
  EXPECT_EQ(local_profile, *profiles[1]);

  // Make sure that the billing address id of the two cards now point to the
  // converted profile.
  EXPECT_EQ(profiles[0]->guid(),
            personal_data_->GetCreditCards()[0]->billing_address_id());
  EXPECT_EQ(profiles[0]->guid(),
            personal_data_->GetCreditCards()[1]->billing_address_id());

  // Make sure that the added address has the email address of the currently
  // signed-in user.
  EXPECT_EQ(kPrimaryAccountEmail16, profiles[0]->GetRawInfo(EMAIL_ADDRESS));
}

// Tests that the converted wallet address is merged into an existing local
// profile if they are considered equivalent. Also tests that the billing
// address relationship was transferred to the converted address.
TEST_F(PersonalDataManagerTest,
       ConvertWalletAddressesAndUpdateWalletCards_MergedProfile) {
  ///////////////////////////////////////////////////////////////////////
  // Setup.
  ///////////////////////////////////////////////////////////////////////
  ASSERT_TRUE(TurnOnSyncFeature());

  base::HistogramTester histogram_tester;
  const std::string kServerAddressId("server_address1");

  // Add two similar profile, a local and a server one. Set the use stats so
  // the server card has a higher ranking, to have a predictable ordering to
  // validate results.
  // Add a local profile.
  AutofillProfile local_profile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&local_profile, "John", "", "Doe", "john@doe.com", "",
                       "1212 Center.", "Bld. 5", "Orlando", "FL", "32801", "US",
                       "19482937549");
  local_profile.set_use_count(1);
  AddProfileToPersonalDataManager(local_profile);

  // Add a different server profile.
  std::vector<AutofillProfile> server_profiles;
  server_profiles.emplace_back(AutofillProfile::SERVER_PROFILE,
                               kServerAddressId);
  test::SetProfileInfo(&server_profiles.back(), "John", "", "Doe", "", "Fox",
                       "1212 Center", "Bld. 5", "Orlando", "FL", "", "US", "");
  // Wallet only provides a full name, so the above first and last names
  // will be ignored when the profile is written to the DB.
  server_profiles.back().SetRawInfo(NAME_FULL, u"John Doe");
  server_profiles.back().set_use_count(100);
  SetServerProfiles(server_profiles);

  // Add a server and a local card that have the server address as billing
  // address.
  CreditCard local_card("287151C8-6AB1-487C-9095-28E80BE5DA15",
                        test::kEmptyOrigin);
  test::SetCreditCardInfo(&local_card, "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  local_card.set_billing_address_id(kServerAddressId);
  personal_data_->AddCreditCard(local_card);

  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::MASKED_SERVER_CARD, "server_card1");
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "1111" /* Visa */, "01", "2999", "1");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);
  server_cards.back().set_billing_address_id(kServerAddressId);
  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_->GetProfiles().size());
  EXPECT_EQ(1U, personal_data_->GetServerProfiles().size());
  EXPECT_EQ(2U, personal_data_->GetCreditCards().size());

  ConvertWalletAddressesAndUpdateWalletCards();

  // The Wallet address should have been merged with the existing local profile.
  EXPECT_EQ(1U, personal_data_->GetProfiles().size());
  EXPECT_EQ(1U, personal_data_->GetServerProfiles().size());
  histogram_tester.ExpectUniqueSample("Autofill.WalletAddressConversionType",
                                      AutofillMetrics::CONVERTED_ADDRESS_MERGED,
                                      1);

  // The conversion should be recorded in the Wallet address.
  EXPECT_TRUE(personal_data_->GetServerProfiles().back()->has_converted());

  // Get the profiles, sorted by frequency to have a deterministic order.
  std::vector<AutofillProfile*> profiles =
      personal_data_->GetProfilesToSuggest();

  // Make sure that the two profiles have merged.
  ASSERT_EQ(1U, profiles.size());

  // Check that the values were merged.
  EXPECT_EQ(u"john@doe.com", profiles[0]->GetRawInfo(EMAIL_ADDRESS));
  EXPECT_EQ(u"Fox", profiles[0]->GetRawInfo(COMPANY_NAME));
  EXPECT_EQ(u"32801", profiles[0]->GetRawInfo(ADDRESS_HOME_ZIP));

  // Make sure that the billing address id of the two cards now point to the
  // converted profile.
  EXPECT_EQ(profiles[0]->guid(),
            personal_data_->GetCreditCards()[0]->billing_address_id());
  EXPECT_EQ(profiles[0]->guid(),
            personal_data_->GetCreditCards()[1]->billing_address_id());
}

// Tests that a Wallet address that has already converted does not get converted
// a second time.
TEST_F(PersonalDataManagerTest,
       ConvertWalletAddressesAndUpdateWalletCards_AlreadyConverted) {
  ///////////////////////////////////////////////////////////////////////
  // Setup.
  ///////////////////////////////////////////////////////////////////////
  ASSERT_TRUE(TurnOnSyncFeature());

  base::HistogramTester histogram_tester;
  const std::string kServerAddressId("server_address1");

  // Add a server profile that has already been converted.
  std::vector<AutofillProfile> server_profiles;
  server_profiles.push_back(
      AutofillProfile(AutofillProfile::SERVER_PROFILE, kServerAddressId));
  test::SetProfileInfo(&server_profiles.back(), "John", "Ray", "Doe",
                       "john@doe.com", "Fox", "1212 Center", "Bld. 5",
                       "Orlando", "FL", "32801", "US", "");
  server_profiles.back().set_has_converted(true);
  // Wallet only provides a full name, so the above first and last names
  // will be ignored when the profile is written to the DB.
  SetServerProfiles(server_profiles);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(0U, personal_data_->GetProfiles().size());
  EXPECT_EQ(1U, personal_data_->GetServerProfiles().size());

  ///////////////////////////////////////////////////////////////////////
  // Tested method.
  ///////////////////////////////////////////////////////////////////////
  ConvertWalletAddressesAndUpdateWalletCards();

  // There should be no local profiles added.
  EXPECT_EQ(0U, personal_data_->GetProfiles().size());
  EXPECT_EQ(1U, personal_data_->GetServerProfiles().size());
}

// Tests that when the user has multiple similar Wallet addresses, they get
// merged into a single local profile, and that the billing address relationship
// is merged too.
TEST_F(
    PersonalDataManagerTest,
    ConvertWalletAddressesAndUpdateWalletCards_MultipleSimilarWalletAddresses) {
  ///////////////////////////////////////////////////////////////////////
  // Setup.
  ///////////////////////////////////////////////////////////////////////
  ASSERT_TRUE(TurnOnSyncFeature());

  base::HistogramTester histogram_tester;
  const std::string kServerAddressId("server_address1");
  const std::string kServerAddressId2("server_address2");

  // Add a unique local profile and two similar server profiles. Set the use
  // stats to have a predictable ordering to validate results.
  // Add a local profile.
  AutofillProfile local_profile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&local_profile, "Bob", "", "Doe", "", "Fox",
                       "1212 Center.", "Bld. 5", "Orlando", "FL", "32801", "US",
                       "19482937549");
  local_profile.set_use_count(1);
  AddProfileToPersonalDataManager(local_profile);

  // Add a server profile.
  std::vector<AutofillProfile> server_profiles;
  server_profiles.emplace_back(AutofillProfile::SERVER_PROFILE,
                               kServerAddressId);
  test::SetProfileInfo(&server_profiles.back(), "John", "", "Doe", "", "",
                       "1212 Center", "Bld. 5", "Orlando", "FL", "32801", "US",
                       "");
  EXPECT_EQ(server_profiles.back().GetRawInfo(NAME_FULL), u"John Doe");
  server_profiles.back().set_use_count(100);

  // Add a similar server profile.
  server_profiles.emplace_back(AutofillProfile::SERVER_PROFILE,
                               kServerAddressId2);
  test::SetProfileInfo(&server_profiles.back(), "John", "", "Doe",
                       "john@doe.com", "Fox", "1212 Center", "Bld. 5",
                       "Orlando", "FL", "", "US", "");
  // Wallet only provides a full name, so the above first and last names
  // will be ignored when the profile is written to the DB.
  server_profiles.back().SetRawInfo(NAME_FULL, u"John Doe");
  server_profiles.back().set_use_count(200);
  SetServerProfiles(server_profiles);

  // Add a server and a local card that have the first and second Wallet address
  // as a billing address.
  CreditCard local_card("287151C8-6AB1-487C-9095-28E80BE5DA15",
                        test::kEmptyOrigin);
  test::SetCreditCardInfo(&local_card, "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  local_card.set_billing_address_id(kServerAddressId);
  personal_data_->AddCreditCard(local_card);
  WaitForOnPersonalDataChanged();

  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::MASKED_SERVER_CARD, "server_card1");
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "1111" /* Visa */, "01", "2999", "1");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);
  server_cards.back().set_billing_address_id(kServerAddressId2);
  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_->GetProfiles().size());
  EXPECT_EQ(2U, personal_data_->GetServerProfiles().size());
  EXPECT_EQ(2U, personal_data_->GetCreditCards().size());

  ConvertWalletAddressesAndUpdateWalletCards();

  // The first Wallet address should have been added as a new local profile and
  // the second one should have merged with the first.
  EXPECT_EQ(2U, personal_data_->GetProfiles().size());
  EXPECT_EQ(2U, personal_data_->GetServerProfiles().size());
  histogram_tester.ExpectBucketCount("Autofill.WalletAddressConversionType",
                                     AutofillMetrics::CONVERTED_ADDRESS_ADDED,
                                     1);
  histogram_tester.ExpectBucketCount("Autofill.WalletAddressConversionType",
                                     AutofillMetrics::CONVERTED_ADDRESS_MERGED,
                                     1);

  // The conversion should be recorded in the Wallet addresses.
  EXPECT_TRUE(personal_data_->GetServerProfiles()[0]->has_converted());
  EXPECT_TRUE(personal_data_->GetServerProfiles()[1]->has_converted());

  // Get the profiles, sorted by ranking to have a deterministic order.
  std::vector<AutofillProfile*> profiles =
      personal_data_->GetProfilesToSuggest();

  // Make sure that the two Wallet addresses merged together and were added as
  // a new local profile.
  ASSERT_EQ(2U, profiles.size());
  EXPECT_EQ(u"John", profiles[0]->GetRawInfo(NAME_FIRST));
  EXPECT_EQ(local_profile, *profiles[1]);

  // Check that the values were merged.
  EXPECT_EQ(u"Fox", profiles[0]->GetRawInfo(COMPANY_NAME));
  EXPECT_EQ(u"32801", profiles[0]->GetRawInfo(ADDRESS_HOME_ZIP));

  // Make sure that the billing address id of the two cards now point to the
  // converted profile.
  EXPECT_EQ(profiles[0]->guid(),
            personal_data_->GetCreditCards()[0]->billing_address_id());
  EXPECT_EQ(profiles[0]->guid(),
            personal_data_->GetCreditCards()[1]->billing_address_id());
}

// Tests a new server card's billing address is updated propely even if the
// address was already converted in the past.
TEST_F(
    PersonalDataManagerTest,
    ConvertWalletAddressesAndUpdateWalletCards_NewCrd_AddressAlreadyConverted) {
  ///////////////////////////////////////////////////////////////////////
  // Setup.
  ///////////////////////////////////////////////////////////////////////
  // Go through the conversion process for a server address and card. Then add
  // a new server card that refers to the already converted server address as
  // its billing address.
  ASSERT_TRUE(TurnOnSyncFeature());

  base::HistogramTester histogram_tester;
  const std::string kServerAddressId("server_address1");

  // Add a server profile.
  std::vector<AutofillProfile> server_profiles;
  server_profiles.emplace_back(AutofillProfile::SERVER_PROFILE,
                               kServerAddressId);
  test::SetProfileInfo(&server_profiles.back(), "John", "", "Doe", "", "Fox",
                       "1212 Center", "Bld. 5", "Orlando", "FL", "", "US", "");
  // Wallet only provides a full name, so the above first and last names
  // will be ignored when the profile is written to the DB.
  server_profiles.back().SetRawInfo(NAME_FULL, u"John Doe");
  server_profiles.back().set_use_count(100);
  SetServerProfiles(server_profiles);

  // Add a server card that have the server address as billing address.
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::MASKED_SERVER_CARD, "server_card1");
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "1111" /* Visa */, "01", "2999", "1");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);
  server_cards.back().set_billing_address_id(kServerAddressId);
  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_->GetServerProfiles().size());
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  ConvertWalletAddressesAndUpdateWalletCards();

  // The Wallet address should have been converted to a new local profile.
  EXPECT_EQ(1U, personal_data_->GetProfiles().size());

  // The conversion should be recorded in the Wallet address.
  EXPECT_TRUE(personal_data_->GetServerProfiles().back()->has_converted());

  // Make sure that the billing address id of the card now point to the
  // converted profile.
  std::vector<AutofillProfile*> profiles =
      personal_data_->GetProfilesToSuggest();
  ASSERT_EQ(1U, profiles.size());
  EXPECT_EQ(profiles[0]->guid(),
            personal_data_->GetCreditCards()[0]->billing_address_id());

  // Add a new server card that has the same billing address as the old one.
  server_cards.emplace_back(CreditCard::MASKED_SERVER_CARD, "server_card2");
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "1112" /* Visa */, "01", "2888", "1");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);
  server_cards.back().set_billing_address_id(kServerAddressId);
  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_->GetProfiles().size());
  EXPECT_EQ(2U, personal_data_->GetCreditCards().size());

  ConvertWalletAddressesAndUpdateWalletCards();

  // The conversion should still be recorded in the Wallet address.
  EXPECT_TRUE(personal_data_->GetServerProfiles().back()->has_converted());

  // Get the profiles, sorted by ranking score to have a deterministic order.
  profiles = personal_data_->GetProfilesToSuggest();

  // Make sure that there is still only one profile.
  ASSERT_EQ(1U, profiles.size());

  // Make sure that the billing address id of the first server card still refers
  // to the converted address.
  EXPECT_EQ(profiles[0]->guid(),
            personal_data_->GetCreditCards()[0]->billing_address_id());
  // Make sure that the billing address id of the new server card still refers
  // to the converted address.
  EXPECT_EQ(profiles[0]->guid(),
            personal_data_->GetCreditCards()[1]->billing_address_id());
}

// Tests that Wallet addresses do NOT get converted if they're stored in
// ephemeral storage.
TEST_F(PersonalDataManagerSyncTransportModeTest,
       DoNotConvertWalletAddressesInEphemeralStorage) {
  ///////////////////////////////////////////////////////////////////////
  // Setup.
  ///////////////////////////////////////////////////////////////////////
  ASSERT_FALSE(personal_data_->IsSyncFeatureEnabled());

  // Add a local profile.
  AutofillProfile local_profile(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetProfileInfo(&local_profile, "Josephine", "Alicia", "Saenz", "",
                       "Fox", "1212 Center.", "Bld. 5", "", "", "", "", "");
  AddProfileToPersonalDataManager(local_profile);

  // Add two server profiles: The first is unique, the second is similar to the
  // local one but has some additional info.
  std::vector<AutofillProfile> server_profiles;
  server_profiles.emplace_back(AutofillProfile::SERVER_PROFILE,
                               "server_address1");
  test::SetProfileInfo(&server_profiles.back(), "John", "", "Doe", "", "",
                       "1212 Center", "Bld. 5", "Orlando", "FL", "32801", "US",
                       "");
  server_profiles.back().SetRawInfo(NAME_FULL, u"John Doe");

  server_profiles.emplace_back(AutofillProfile::SERVER_PROFILE,
                               "server_address2");
  test::SetProfileInfo(&server_profiles.back(), "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5",
                       "Orlando", "FL", "32801", "US", "19482937549");
  server_profiles.back().SetRawInfo(NAME_FULL, u"Josephine Alicia Saenz");
  SetServerProfiles(server_profiles);

  ASSERT_TRUE(AutofillProfileComparator(personal_data_->app_locale())
                  .AreMergeable(local_profile, server_profiles.back()));

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
  ASSERT_EQ(1U, personal_data_->GetProfiles().size());
  ASSERT_EQ(2U, personal_data_->GetServerProfiles().size());

  ///////////////////////////////////////////////////////////////////////
  // Tested method.
  ///////////////////////////////////////////////////////////////////////
  // Since the wallet addresses are in ephemeral storage, they should *not* get
  // converted to local addresses.
  ConvertWalletAddressesAndUpdateWalletCards();

  ///////////////////////////////////////////////////////////////////////
  // Validation.
  ///////////////////////////////////////////////////////////////////////
  // There should be no changes to the local profiles: No new one added, and no
  // changes to the existing one (even though the second server profile contains
  // additional information and is mergeable in principle).
  EXPECT_EQ(1U, personal_data_->GetProfiles().size());
  EXPECT_EQ(local_profile, *personal_data_->GetProfiles()[0]);
}

TEST_F(PersonalDataManagerTest, RemoveByGUID_ResetsBillingAddress) {
  ///////////////////////////////////////////////////////////////////////
  // Setup.
  ///////////////////////////////////////////////////////////////////////
  std::vector<CreditCard> server_cards;

  // Add two different profiles
  AutofillProfile profile0(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetProfileInfo(&profile0, "Bob", "", "Doe", "", "Fox", "1212 Center.",
                       "Bld. 5", "Orlando", "FL", "32801", "US", "19482937549");
  AutofillProfile profile1(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Seb", "", "Doe", "", "ACME",
                       "1234 Evergreen Terrace", "Bld. 5", "Springfield", "IL",
                       "32801", "US", "15151231234");

  // Add a local and a server card that have profile0 as their billing address.
  CreditCard local_card0(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         test::kEmptyOrigin);
  test::SetCreditCardInfo(&local_card0, "John Dillinger",
                          "4111111111111111" /* Visa */, "01", "2999",
                          profile0.guid());
  CreditCard server_card0(CreditCard::FULL_SERVER_CARD, "c789");
  test::SetCreditCardInfo(&server_card0, "John Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", profile0.guid());
  server_cards.push_back(server_card0);

  // Do the same but for profile1.
  CreditCard local_card1(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         test::kEmptyOrigin);
  test::SetCreditCardInfo(&local_card1, "Seb Dillinger",
                          "4111111111111111" /* Visa */, "01", "2999",
                          profile1.guid());
  CreditCard server_card1(CreditCard::FULL_SERVER_CARD, "c789");
  test::SetCreditCardInfo(&server_card1, "John Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", profile1.guid());
  server_cards.push_back(server_card1);

  // Add the data to the database.
  AddProfileToPersonalDataManager(profile0);
  AddProfileToPersonalDataManager(profile1);
  personal_data_->AddCreditCard(local_card0);
  personal_data_->AddCreditCard(local_card1);
  SetServerCards(server_cards);

  // Verify that the web database has been updated and the notification sent.
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();

  // Make sure everything was saved properly.
  EXPECT_EQ(2U, personal_data_->GetProfiles().size());
  EXPECT_EQ(4U, personal_data_->GetCreditCards().size());

  ///////////////////////////////////////////////////////////////////////
  // Tested method.
  ///////////////////////////////////////////////////////////////////////
  RemoveByGUIDFromPersonalDataManager(profile0.guid());

  ///////////////////////////////////////////////////////////////////////
  // Validation.
  ///////////////////////////////////////////////////////////////////////

  // Wait for the data to be refreshed.
  // WaitForOnPersonalDataChanged();

  // Make sure only profile0 was deleted.
  ASSERT_EQ(1U, personal_data_->GetProfiles().size());
  EXPECT_EQ(profile1.guid(), personal_data_->GetProfiles()[0]->guid());
  EXPECT_EQ(4U, personal_data_->GetCreditCards().size());

  for (CreditCard* card : personal_data_->GetCreditCards()) {
    if (card->guid() == local_card0.guid() ||
        card->guid() == server_card0.guid()) {
      // The billing address id of local_card0 and server_card0 should have been
      // reset.
      EXPECT_EQ("", card->billing_address_id());
    } else {
      // The billing address of local_card1 and server_card1 should still refer
      // to profile1.
      EXPECT_EQ(profile1.guid(), card->billing_address_id());
    }
  }
}

TEST_F(PersonalDataManagerTest, LogStoredCreditCardMetrics) {
  ASSERT_EQ(0U, personal_data_->GetCreditCards().size());

  // Helper timestamps for setting up the test data.
  base::Time now = AutofillClock::Now();
  base::Time one_month_ago = now - base::Days(30);
  base::Time::Exploded now_exploded;
  base::Time::Exploded one_month_ago_exploded;
  now.LocalExplode(&now_exploded);
  one_month_ago.LocalExplode(&one_month_ago_exploded);

  std::vector<CreditCard> server_cards;
  server_cards.reserve(10);

  // Create in-use and in-disuse cards of each record type.
  const std::vector<CreditCard::RecordType> record_types{
      CreditCard::LOCAL_CARD, CreditCard::MASKED_SERVER_CARD,
      CreditCard::FULL_SERVER_CARD};
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
    if (record_type == CreditCard::LOCAL_CARD) {
      personal_data_->AddCreditCard(card_in_use);
      personal_data_->AddCreditCard(card_in_disuse);
    } else {
      server_cards.push_back(std::move(card_in_use));
      server_cards.push_back(std::move(card_in_disuse));
    }
  }

  // Sets the virtual card enrollment state for the first three server cards.
  server_cards[0].set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::ENROLLED);
  server_cards[0].set_card_art_url(GURL("https://www.example.com/image1"));
  server_cards[1].set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::ENROLLED);
  server_cards[1].set_card_art_url(GURL("https://www.example.com/image1"));
  server_cards[2].set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::ENROLLED);
  server_cards[2].set_card_art_url(GURL("https://www.example.com/image2"));

  SetServerCards(server_cards);

  // SetServerCards modifies the metadata (use_count and use_date)
  // of unmasked cards. Reset the server card metadata to match the data set
  // up above.
  for (const auto& card : server_cards)
    account_autofill_table_->UpdateServerCardMetadata(card);

  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();

  ASSERT_EQ(6U, personal_data_->GetCreditCards().size());

  // Reload the database, which will log the stored profile counts.
  base::HistogramTester histogram_tester;
  ResetPersonalDataManager(USER_MODE_NORMAL);

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

// These tests are not applicable on Linux since it does not support full server
// cards.
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if !(BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
// Test that setting a null sync service remasks full server cards.
TEST_F(PersonalDataManagerTest, OnSyncServiceInitialized_NoSyncService) {
  base::HistogramTester histogram_tester;
  SetUpThreeCardTypes();

  // Set no sync service.
  personal_data_->SetSyncServiceForTest(nullptr);
  WaitForOnPersonalDataChanged();

  // Check that cards were masked and other were untouched.
  EXPECT_EQ(3U, personal_data_->GetCreditCards().size());
  std::vector<CreditCard*> server_cards =
      personal_data_->GetServerCreditCards();
  EXPECT_EQ(2U, server_cards.size());
  for (CreditCard* card : server_cards)
    EXPECT_TRUE(card->record_type() == CreditCard::MASKED_SERVER_CARD);
}

// Test that setting a sync service in auth error remasks full server cards.
TEST_F(PersonalDataManagerTest, OnSyncServiceInitialized_NotActiveSyncService) {
  base::HistogramTester histogram_tester;
  SetUpThreeCardTypes();

  // Set a sync service in auth error.
  syncer::TestSyncService sync_service;
  sync_service.SetPersistentAuthError();
  personal_data_->SetSyncServiceForTest(&sync_service);
  WaitForOnPersonalDataChanged();

  // Remove the auth error to be able to get the server cards.
  sync_service.ClearAuthError();

  // Check that cards were masked and other were untouched.
  EXPECT_EQ(3U, personal_data_->GetCreditCards().size());
  std::vector<CreditCard*> server_cards =
      personal_data_->GetServerCreditCards();
  EXPECT_EQ(2U, server_cards.size());
  for (CreditCard* card : server_cards)
    EXPECT_TRUE(card->record_type() == CreditCard::MASKED_SERVER_CARD);

  // Call OnSyncShutdown to ensure removing observer added by
  // SetSyncServiceForTest.
  personal_data_->OnSyncShutdown(&sync_service);
}
#endif  // !(BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))

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
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet());

  // Check that server cards are unavailable.
  EXPECT_EQ(3U, personal_data_->GetCreditCards().size());
  EXPECT_EQ(1U, personal_data_->GetCreditCardsToSuggest().size());
  EXPECT_EQ(1U, personal_data_->GetLocalCreditCards().size());
  EXPECT_EQ(2U, personal_data_->GetServerCreditCards().size());
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

// Tests that all the non settings origins of autofill profiles are cleared even
// if sync is disabled.
TEST_F(
    PersonalDataManagerTest,
    SyncServiceInitializedWithAutofillDisabled_ClearProfileNonSettingsOrigins) {
  // Create a profile with a non-settings, non-empty origin.
  AutofillProfile profile(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          "https://www.example.com");
  test::SetProfileInfo(&profile, "Marion0", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  AddProfileToPersonalDataManager(profile);

  // Turn off autofill profile sync.
  syncer::UserSelectableTypeSet user_selectable_type_set =
      sync_service_.GetUserSettings()->GetSelectedTypes();
  user_selectable_type_set.Remove(syncer::UserSelectableType::kAutofill);
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/user_selectable_type_set);

  // The data should still exist.
  ASSERT_EQ(1U, personal_data_->GetProfiles().size());

  // Reload the personal data manager.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // The profile should still exist.
  ASSERT_EQ(1U, personal_data_->GetProfiles().size());

  // The profile's origin should be cleared
  EXPECT_TRUE(personal_data_->GetProfiles()[0]->origin().empty());
}

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
  WaitForOnPersonalDataChanged();

  // Turn off autofill profile sync.
  syncer::UserSelectableTypeSet user_selectable_type_set =
      sync_service_.GetUserSettings()->GetSelectedTypes();
  user_selectable_type_set.Remove(syncer::UserSelectableType::kAutofill);
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/user_selectable_type_set);

  // The credit card should still exist.
  ASSERT_EQ(1U, personal_data_->GetCreditCards().size());

  // Reload the personal data manager.
  ResetPersonalDataManager(USER_MODE_NORMAL);

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
  WaitForOnPersonalDataChanged();

  EXPECT_EQ(0U, personal_data_->GetServerCreditCards().size());

  CreditCard server_card;
  test::SetCreditCardInfo(&server_card, "Server Card",
                          "4234567890123456",  // Visa
                          "04", "2999", "1");
  server_card.set_guid("00000000-0000-0000-0000-000000000007");
  server_card.set_record_type(CreditCard::FULL_SERVER_CARD);
  server_card.set_server_id("server_id");
  personal_data_->AddFullServerCreditCard(server_card);
  WaitForOnPersonalDataChanged();

  EXPECT_EQ(1U, personal_data_->GetServerCreditCards().size());

  // Switch back to the account storage.
  sync_service_.SetHasSyncConsent(false);
  personal_data_->OnStateChanged(&sync_service_);
  WaitForOnPersonalDataChanged();

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
  server_card.set_record_type(CreditCard::FULL_SERVER_CARD);
  server_card.set_server_id("server_id");
  personal_data_->AddFullServerCreditCard(server_card);

  // Set server card metadata.
  server_card.set_use_count(15);
  personal_data_->UpdateServerCardsMetadata({server_card});

  WaitForOnPersonalDataChanged();

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
  local_card.set_record_type(CreditCard::LOCAL_CARD);
  local_card.set_use_date(AutofillClock::Now() - base::Days(5));
  personal_data_->AddCreditCard(local_card);

  WaitForOnPersonalDataChanged();

  // Expect that the local card is stored in the profile autofill table.
  profile_autofill_table_->GetCreditCards(&cards);
  EXPECT_EQ(1U, cards.size());
  EXPECT_EQ(local_card.LastFourDigits(), cards[0]->LastFourDigits());

  // Add a local profile
  AutofillProfile profile(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          "https://www.example.com");
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  AddProfileToPersonalDataManager(profile);

  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  // Expect that a profile is stored in the profile autofill table.
  profile_autofill_table_->GetAutofillProfiles(
      &profiles, AutofillProfile::Source::kLocalOrSyncable);
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
  UpdateProfileOnPersonalDataManager(updated_more_recently_used_profile);

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
  server_cards.emplace_back(CreditCard::FULL_SERVER_CARD, "c789");
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  SetServerCards(server_cards);
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();

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
  WaitForOnPersonalDataChanged();
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());

  // Re-set some server cards. Check that the function now returns true.
  SetServerCards(server_cards);
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
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
  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  SetServerCards(server_cards);
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();

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
  WaitForOnPersonalDataChanged();
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());

  // Re-set some server cards. Check that the function still returns false.
  SetServerCards(server_cards);
  personal_data_->Refresh();
  WaitForOnPersonalDataChanged();
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

TEST_F(PersonalDataManagerSyncTransportModeTest, GetSyncSigninState) {
  // Make sure a non-sync-consented account is available for the first tests.
  ASSERT_TRUE(identity_test_env_.identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  ASSERT_FALSE(sync_service_.HasSyncConsent());
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet(
          syncer::UserSelectableType::kAutofill));

  EXPECT_EQ(AutofillSyncSigninState::kSignedInAndWalletSyncTransportEnabled,
            personal_data_->GetSyncSigninState());

  // Check that the sync state is |SignedIn| if the sync service does not have
  // wallet data active.
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet());

  EXPECT_EQ(AutofillSyncSigninState::kSignedIn,
            personal_data_->GetSyncSigninState());

// ClearPrimaryAccount is not supported on CrOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Check that the sync state is |SignedOut| when the account info is empty.
  {
    identity_test_env_.ClearPrimaryAccount();
    sync_service_.SetAccountInfo(CoreAccountInfo());
    sync_service_.SetHasSyncConsent(false);
    EXPECT_EQ(AutofillSyncSigninState::kSignedOut,
              personal_data_->GetSyncSigninState());
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
  EXPECT_EQ(AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled,
            personal_data_->GetSyncSigninState());
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
      /*types=*/syncer::UserSelectableTypeSet(
          syncer::UserSelectableType::kAutofill));
  // Make sure there are no opt-ins recorded yet.
  ASSERT_FALSE(prefs::IsUserOptedInWalletSyncTransport(prefs_.get(),
                                                       active_info.account_id));

  // Account wallet storage only makes sense together with support for
  // unconsented primary accounts, i.e. on Win/Mac/Linux.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(AutofillSyncSigninState::kSignedInAndWalletSyncTransportEnabled,
            personal_data_->GetSyncSigninState());

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

  EXPECT_EQ(AutofillSyncSigninState::kSignedIn,
            personal_data_->GetSyncSigninState());

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
    EXPECT_EQ(AutofillSyncSigninState::kSignedOut,
              personal_data_->GetSyncSigninState());

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
    EXPECT_EQ(AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled,
              personal_data_->GetSyncSigninState());

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

  WaitForOnPersonalDataChanged();

  EXPECT_FALSE(observer.IsConnected()) << "Observer not called";
}

TEST_F(PersonalDataManagerTest, AddAndGetUpiId) {
  constexpr char upi_id[] = "vpa@indianbank";
  personal_data_->AddUpiId(upi_id);
  WaitOnceForOnPersonalDataChanged();
  std::vector<std::string> all_upi_ids = personal_data_->GetUpiIds();
  EXPECT_THAT(all_upi_ids, testing::ElementsAre(upi_id));
}

TEST_F(PersonalDataManagerTest, IsEligibleForAddressAccountStorage) {
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{base::test::FeatureRefAndParams(
           features::kAutofillAccountProfilesUnionView, {})},
       {base::test::FeatureRefAndParams(
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
