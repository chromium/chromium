// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_suggestion_generator.h"

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_granular_filling_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_test_api.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/browser/test_address_data_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_test_helpers.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

using testing::Field;
using testing::IsEmpty;
using testing::Matcher;

#if !BUILDFLAG(IS_IOS)
constexpr AutofillSuggestionTriggerSource kDefaultTriggerSource =
    AutofillSuggestionTriggerSource::kFormControlElementClicked;
#endif  // !BUILDFLAG(IS_IOS)

constexpr char kAddressesSuppressedHistogramName[] =
    "Autofill.AddressesSuppressedForDisuse";

Matcher<Suggestion> EqualLabels(
    const std::vector<std::vector<Suggestion::Text>>& suggestion_objects) {
  return Field(&Suggestion::labels, suggestion_objects);
}

Matcher<Suggestion> EqualLabels(
    const std::vector<std::vector<std::u16string>>& labels) {
  std::vector<std::vector<Suggestion::Text>> suggestion_objects;
  for (const auto& row : labels) {
    suggestion_objects.emplace_back();
    for (const auto& col : row) {
      suggestion_objects.back().emplace_back(col);
    }
  }
  return EqualLabels(suggestion_objects);
}

Matcher<Suggestion> EqualsFieldByFieldFillingSuggestion(
    SuggestionType id,
    const std::u16string& main_text,
    FieldType field_by_field_filling_type_used,
    const Suggestion::Payload& payload,
    const std::vector<std::vector<Suggestion::Text>>& labels = {}) {
  return AllOf(
      Field(&Suggestion::type, id),
      Field(&Suggestion::main_text,
            Suggestion::Text(main_text, Suggestion::Text::IsPrimary(true))),
      Field(&Suggestion::payload, payload),
      Field(&Suggestion::icon, Suggestion::Icon::kNoIcon),
      Field(&Suggestion::field_by_field_filling_type_used,
            std::optional(field_by_field_filling_type_used)),
      EqualLabels(labels));
}

#if !BUILDFLAG(IS_IOS)
Matcher<Suggestion> EqualsUndoAutofillSuggestion() {
  return EqualsSuggestion(SuggestionType::kUndoOrClear,
#if BUILDFLAG(IS_ANDROID)
                          base::i18n::ToUpper(l10n_util::GetStringUTF16(
                              IDS_AUTOFILL_UNDO_MENU_ITEM)),
#else
                          l10n_util::GetStringUTF16(
                              IDS_AUTOFILL_UNDO_MENU_ITEM),
#endif
                          Suggestion::Icon::kUndo);
}
#endif

Matcher<Suggestion> EqualsManageAddressesSuggestion() {
  return EqualsSuggestion(
      SuggestionType::kManageAddress,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_ADDRESSES),
      Suggestion::Icon::kSettings);
}

// Checks that `arg` contains necessary address footer suggestions. `arg`
// has to be of type std::vector<Suggestion>.
MATCHER(ContainsAddressFooterSuggestions, "") {
  EXPECT_GT(arg.size(), 2ul);
  EXPECT_THAT(arg[arg.size() - 2],
              EqualsSuggestion(SuggestionType::kSeparator));
  EXPECT_THAT(arg.back(), EqualsManageAddressesSuggestion());
  return true;
}

class AddressSuggestionGeneratorTest : public testing::Test {
 public:
  void SetUp() override {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    address_data().SetPrefService(autofill_client_.GetPrefs());
    address_data().SetSyncServiceForTest(&sync_service_);
  }

  TestAddressDataManager& address_data() {
    return autofill_client_.GetPersonalDataManager()
        ->test_address_data_manager();
  }

  const std::string& app_locale() { return address_data().app_locale(); }

  TestAutofillClient* autofill_client() { return &autofill_client_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::SYSTEM_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClient autofill_client_;
  syncer::TestSyncService sync_service_;
};

// Tests that special characters will be used while prefix matching the user's
// field input with the available emails to suggest.
TEST_F(AddressSuggestionGeneratorTest,
       GetProfilesToSuggest_UseSpecialCharactersInEmail) {
  AutofillProfile profile_1(i18n_model_definition::kLegacyHierarchyCountryCode);
  AutofillProfile profile_2(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile_1.SetRawInfo(EMAIL_ADDRESS, u"test@email.xyz");
  profile_2.SetRawInfo(EMAIL_ADDRESS, u"test1@email.xyz");
  address_data().AddProfile(profile_1);
  address_data().AddProfile(profile_2);
  ASSERT_EQ(address_data().GetProfilesToSuggest().size(), 2u);

  std::vector<AutofillProfile> profiles = GetProfilesToSuggestForTest(
      address_data(), EMAIL_ADDRESS, u"Test@", false, {});

  ASSERT_EQ(profiles.size(), 1u);
  EXPECT_EQ(profiles[0], profile_1);
}

TEST_F(AddressSuggestionGeneratorTest, GetProfilesToSuggest_HideSubsets) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
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
  address_data().AddProfile(profile);
  address_data().AddProfile(profile1);
  address_data().AddProfile(profile2);
  address_data().AddProfile(profile3);

  // Simulate a form with street address, city and state.
  FieldTypeSet types = {ADDRESS_HOME_CITY, ADDRESS_HOME_STATE};
  std::vector<AutofillProfile> profiles = GetProfilesToSuggestForTest(
      address_data(), ADDRESS_HOME_STREET_ADDRESS, u"123", false, types);
  ASSERT_EQ(2U, profiles.size());
  EXPECT_EQ(profiles[0].GetRawInfo(ADDRESS_HOME_STATE), u"CA");
  EXPECT_EQ(profiles[1].GetRawInfo(ADDRESS_HOME_STATE), u"TX");
}

// Drawing takes noticeable time when there are more than 10 profiles.
// Therefore, we keep only the 10 first suggested profiles.
TEST_F(AddressSuggestionGeneratorTest, GetProfilesToSuggest_SuggestionsLimit) {
  std::vector<AutofillProfile> profiles;
  for (size_t i = 0; i < 2 * kMaxDeduplicatedProfilesForSuggestion; i++) {
    AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
    test::SetProfileInfo(&profile, base::StringPrintf("Marion%zu", i).c_str(),
                         "Mitchell", "Morrison", "johnwayne@me.xyz", "Fox",
                         "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                         "Hollywood", "CA", "91601", "US", "12345678910");
    address_data().AddProfile(profile);
    profiles.push_back(profile);
  }

  std::vector<AutofillProfile> suggested_profiles =
      GetProfilesToSuggestForTest(address_data(), NAME_FIRST, u"Ma", false, {});

  ASSERT_EQ(2 * kMaxDeduplicatedProfilesForSuggestion,
            address_data().GetProfiles().size());
  ASSERT_EQ(kMaxDeduplicatedProfilesForSuggestion, suggested_profiles.size());
}

// Deduping takes noticeable time when there are more than 50 profiles.
// Therefore, keep only the 50 first pre-dedupe matching profiles.
TEST_F(AddressSuggestionGeneratorTest, GetProfilesToSuggest_ProfilesLimit) {
  std::vector<AutofillProfile> profiles;
  for (size_t i = 0; i < kMaxPrefixMatchedProfilesForSuggestion; i++) {
    AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);

    test::SetProfileInfo(
        &profile, "Marion", "Mitchell", "Morrison", "johnwayne@me.xyz", "Fox",
        base::StringPrintf("%zu123 Zoo St.\nSecond Line\nThird line", i)
            .c_str(),
        "unit 5", "Hollywood", "CA", "91601", "US", "12345678910");

    // Set ranking score such that they appear before the "last" profile (added
    // next).
    profile.set_use_count(12);
    profile.set_use_date(AutofillClock::Now() - base::Days(1));

    address_data().AddProfile(profile);
    profiles.push_back(profile);
  }

  // Add another profile that matches, but that will get stripped out.
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile, "Marie", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "000 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile.set_use_count(1);
  profile.set_use_date(AutofillClock::Now() - base::Days(7));
  address_data().AddProfile(profile);

  std::vector<AutofillProfile> suggested_profiles =
      GetProfilesToSuggestForTest(address_data(), NAME_FIRST, u"Ma", false, {});

  ASSERT_EQ(kMaxPrefixMatchedProfilesForSuggestion + 1,
            address_data().GetProfiles().size());
  ASSERT_EQ(1U, suggested_profiles.size());
  EXPECT_EQ(suggested_profiles.front().GetRawInfo(NAME_FIRST),
            profiles.front().GetRawInfo(NAME_FIRST));
}

// Tests that GetProfilesToSuggest orders its suggestions based on the
// ranking formula.
TEST_F(AddressSuggestionGeneratorTest, GetProfilesToSuggest_Ranking) {
  // Set up the profiles. They are named with number suffixes X so the X is the
  // order in which they should be ordered by the ranking formula.
  AutofillProfile profile3(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile3, "Marion3", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile3.set_use_date(AutofillClock::Now() - base::Days(1));
  profile3.set_use_count(5);
  address_data().AddProfile(profile3);

  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile1, "Marion1", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile1.set_use_date(AutofillClock::Now() - base::Days(1));
  profile1.set_use_count(10);
  address_data().AddProfile(profile1);

  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile2, "Marion2", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile2.set_use_date(AutofillClock::Now() - base::Days(15));
  profile2.set_use_count(300);
  address_data().AddProfile(profile2);

  std::vector<AutofillProfile> suggested_profiles =
      GetProfilesToSuggestForTest(address_data(), NAME_FIRST, u"Ma", false, {});
  ASSERT_EQ(3U, suggested_profiles.size());
  EXPECT_EQ(suggested_profiles[0].GetRawInfo(NAME_FIRST), u"Marion1");
  EXPECT_EQ(suggested_profiles[1].GetRawInfo(NAME_FIRST), u"Marion2");
  EXPECT_EQ(suggested_profiles[2].GetRawInfo(NAME_FIRST), u"Marion3");
}

// Tests that GetProfilesToSuggest returns all profiles suggestions.
TEST_F(AddressSuggestionGeneratorTest,
       GetProfilesToSuggest_NumberOfSuggestions) {
  // Set up 3 different profiles.
  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile1, "Marion1", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  address_data().AddProfile(profile1);

  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile2, "Marion2", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  address_data().AddProfile(profile2);

  AutofillProfile profile3(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile3, "Marion3", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  address_data().AddProfile(profile3);

  // Verify that all the profiles are suggested.
  std::vector<AutofillProfile> suggested_profiles = GetProfilesToSuggestForTest(
      address_data(), NAME_FIRST, std::u16string(), false, {});
  EXPECT_EQ(3U, suggested_profiles.size());
}

// Tests that phone number types are correctly deduplicated for suggestions.
TEST_F(AddressSuggestionGeneratorTest,
       GetProfilesToSuggest_PhoneNumberDeduplication) {
  // Set up 2 different profiles.
  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile1.SetRawInfo(NAME_FULL, u"First Middle Last");
  profile1.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"+491601234567");
  address_data().AddProfile(profile1);

  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile2.SetRawInfo(NAME_FULL, u"First Middle Last");
  profile2.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"+491607654321");
  address_data().AddProfile(profile2);

  {
    std::vector<AutofillProfile> suggested_profiles =
        GetProfilesToSuggestForTest(address_data(), NAME_FULL, std::u16string(),
                                    false,
                                    {NAME_FULL, PHONE_HOME_WHOLE_NUMBER});
    EXPECT_EQ(2U, suggested_profiles.size());
  }
  {
    std::vector<AutofillProfile> suggested_profiles =
        GetProfilesToSuggestForTest(
            address_data(), NAME_FULL, std::u16string(), false,
            {NAME_FULL, PHONE_HOME_COUNTRY_CODE, PHONE_HOME_CITY_AND_NUMBER});
    EXPECT_EQ(2U, suggested_profiles.size());
  }
  {
    std::vector<AutofillProfile> suggested_profiles =
        GetProfilesToSuggestForTest(address_data(), NAME_FULL, std::u16string(),
                                    false,
                                    {NAME_FULL, PHONE_HOME_COUNTRY_CODE,
                                     PHONE_HOME_CITY_CODE, PHONE_HOME_NUMBER});
    EXPECT_EQ(2U, suggested_profiles.size());
  }
  {
    std::vector<AutofillProfile> suggested_profiles =
        GetProfilesToSuggestForTest(
            address_data(), NAME_FULL, std::u16string(), false,
            {NAME_FULL, PHONE_HOME_COUNTRY_CODE, PHONE_HOME_CITY_CODE});
    EXPECT_EQ(1U, suggested_profiles.size());
  }
}

// Tests that disused profiles are suppressed when suppression is enabled and
// the input field is empty.
TEST_F(AddressSuggestionGeneratorTest,
       GetProfilesToSuggest_SuppressDisusedProfilesOnEmptyField) {
  // Set up 2 different profiles.
  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile1, "Marion1", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile1.set_use_date(AutofillClock::Now() - base::Days(200));
  address_data().AddProfile(profile1);

  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile2, "Marion2", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "456 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile2.set_use_date(AutofillClock::Now() - base::Days(20));
  address_data().AddProfile(profile2);

  // Query with empty string only returns profile2.
  {
    std::vector<AutofillProfile> suggested_profiles =
        GetProfilesToSuggestForTest(address_data(), ADDRESS_HOME_STREET_ADDRESS,
                                    std::u16string(), false, {});
    EXPECT_EQ(1U, suggested_profiles.size());
  }

  // Query with non-alpha-numeric string only returns profile2.
  {
    std::vector<AutofillProfile> suggested_profiles =
        GetProfilesToSuggestForTest(address_data(), ADDRESS_HOME_STREET_ADDRESS,
                                    u"--", false, {});
    EXPECT_EQ(1U, suggested_profiles.size());
  }

  // Query with prefix for profile1 returns profile1.
  {
    std::vector<AutofillProfile> suggested_profiles =
        GetProfilesToSuggestForTest(address_data(), ADDRESS_HOME_STREET_ADDRESS,
                                    u"123", false, {});
    ASSERT_EQ(1U, suggested_profiles.size());
    EXPECT_EQ(u"Marion1", suggested_profiles[0].GetRawInfo(NAME_FIRST));
  }

  // Query with prefix for profile2 returns profile2.
  {
    std::vector<AutofillProfile> suggested_profiles =
        GetProfilesToSuggestForTest(address_data(), ADDRESS_HOME_STREET_ADDRESS,
                                    u"456", false, {});
    EXPECT_EQ(1U, suggested_profiles.size());
    EXPECT_EQ(u"Marion2", suggested_profiles[0].GetRawInfo(NAME_FIRST));
  }
}

// Give two suggestions with the same name, and no other field to compare.
// Expect only one unique suggestion.
TEST_F(AddressSuggestionGeneratorTest, GetProfilesToSuggest_SingleDedupe) {
  AutofillProfile profile_1 = test::GetFullProfile();
  profile_1.set_use_count(10);
  AutofillProfile profile_2 = test::GetFullProfile();
  address_data().AddProfile(profile_1);
  address_data().AddProfile(profile_2);

  std::vector<AutofillProfile> profiles_to_suggest =
      GetProfilesToSuggestForTest(address_data(), NAME_FIRST, u"",
                                  /*field_is_autofilled=*/false, {});

  ASSERT_EQ(1U, profiles_to_suggest.size());
}

// Given two suggestions with the same name and one with a different, and also
// last name field to compare, Expect all profiles listed as unique suggestions.
TEST_F(AddressSuggestionGeneratorTest, GetProfilesToSuggest_MultipleDedupe) {
  std::vector<AutofillProfile> profiles(
      3, AutofillProfile(i18n_model_definition::kLegacyHierarchyCountryCode));
  profiles[0].SetRawInfo(NAME_FIRST, u"Bob");
  profiles[0].SetRawInfo(NAME_LAST, u"Morrison");
  profiles[0].set_use_count(10);
  address_data().AddProfile(profiles[0]);

  profiles[1].SetRawInfo(NAME_FIRST, u"Bob");
  profiles[1].SetRawInfo(NAME_LAST, u"Parker");
  profiles[1].set_use_count(5);
  address_data().AddProfile(profiles[1]);

  profiles[2].SetRawInfo(NAME_FIRST, u"Mary");
  profiles[2].SetRawInfo(NAME_LAST, u"Parker");
  address_data().AddProfile(profiles[2]);

  std::vector<AutofillProfile> profiles_to_suggest =
      GetProfilesToSuggestForTest(address_data(), NAME_FIRST, u"",
                                  /*field_is_autofilled=*/false,
                                  {NAME_FIRST, NAME_LAST});

  EXPECT_EQ(3U, profiles_to_suggest.size());
}

// Test the limit of number of deduplicated profiles.
TEST_F(AddressSuggestionGeneratorTest, GetProfilesToSuggest_DedupeLimit) {
  std::vector<AutofillProfile> profiles;
  for (size_t i = 0; i < kMaxDeduplicatedProfilesForSuggestion + 1; i++) {
    AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
    profile.SetRawInfo(NAME_FULL,
                       base::UTF8ToUTF16(base::StringPrintf("Bob %zu Doe", i)));
    profile.set_use_count(kMaxDeduplicatedProfilesForSuggestion + 10 - i);
    profiles.push_back(profile);
    address_data().AddProfile(profile);
  }

  std::vector<AutofillProfile> profiles_to_suggest =
      GetProfilesToSuggestForTest(address_data(), NAME_FULL, u"",
                                  /*field_is_autofilled=*/false, {NAME_FULL});

  ASSERT_EQ(kMaxDeduplicatedProfilesForSuggestion, profiles_to_suggest.size());

  // All profiles are different.
  for (size_t i = 0; i < profiles_to_suggest.size(); i++) {
    EXPECT_EQ(profiles_to_suggest[i].guid(), profiles[i].guid()) << i;
  }
}

TEST_F(AddressSuggestionGeneratorTest,
       GetProfilesToSuggest_EmptyMatchingProfiles) {
  ASSERT_EQ(0U, GetProfilesToSuggestForTest(address_data(), NAME_FIRST, u"",
                                            /*field_is_autofilled=*/false, {})
                    .size());
}

// Tests that `kAccount` profiles are preferred over `kLocalOrSyncable` profile
// in case of a duplicate.
TEST_F(AddressSuggestionGeneratorTest,
       GetProfilesToSuggest_kAccountPrecedence) {
  // Create two profiles that only differ by their record type.
  AutofillProfile profile_1(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile_1.SetRawInfo(NAME_FULL, u"First Last");
  test_api(profile_1).set_record_type(AutofillProfile::RecordType::kAccount);
  address_data().AddProfile(profile_1);

  AutofillProfile profile_2(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile_2.SetRawInfo(NAME_FULL, u"First Last");
  test_api(profile_2).set_record_type(
      AutofillProfile::RecordType::kLocalOrSyncable);
  // Set high use count for profile 2 so that it has greater ranking than
  // profile_1
  profile_2.set_use_count(100);
  address_data().AddProfile(profile_2);

  std::vector<AutofillProfile> profiles_to_suggest =
      GetProfilesToSuggestForTest(address_data(), NAME_FULL, u"",
                                  /*field_is_autofilled=*/false, {NAME_FULL});

  ASSERT_EQ(1u, profiles_to_suggest.size());
  EXPECT_EQ(profile_1.guid(), profiles_to_suggest[0].guid());
  EXPECT_EQ(AutofillProfile::RecordType::kAccount,
            profiles_to_suggest[0].record_type());
}

TEST_F(AddressSuggestionGeneratorTest,
       GetProfilesToSuggest_GetMatchingProfile) {
  AutofillProfile marion_profile(
      i18n_model_definition::kLegacyHierarchyCountryCode);
  marion_profile.SetRawInfo(NAME_FIRST, u"Marion");
  address_data().AddProfile(marion_profile);

  AutofillProfile bob_profile(
      i18n_model_definition::kLegacyHierarchyCountryCode);
  bob_profile.SetRawInfo(NAME_FIRST, u"Bob");
  address_data().AddProfile(bob_profile);

  std::vector<AutofillProfile> profiles_to_suggest =
      GetProfilesToSuggestForTest(address_data(), NAME_FIRST, u"Mar",
                                  /*field_is_autofilled=*/false, {});

  ASSERT_EQ(1U, profiles_to_suggest.size());
  EXPECT_EQ(marion_profile.guid(), profiles_to_suggest[0].guid());
}

TEST_F(AddressSuggestionGeneratorTest, GetProfilesToSuggest_NoMatchingProfile) {
  AutofillProfile bob_profile(
      i18n_model_definition::kLegacyHierarchyCountryCode);
  bob_profile.SetRawInfo(NAME_FIRST, u"Bob");
  address_data().AddProfile(bob_profile);

  std::vector<AutofillProfile> profiles_to_suggest =
      GetProfilesToSuggestForTest(address_data(), NAME_FIRST, u"Mar",
                                  /*field_is_autofilled=*/false, {});

  ASSERT_TRUE(profiles_to_suggest.empty());
}

TEST_F(AddressSuggestionGeneratorTest,
       GetProfilesToSuggest_EmptyProfilesInput) {
  std::vector<AutofillProfile> profiles_to_suggest =
      GetProfilesToSuggestForTest(address_data(), NAME_FIRST, u"Mar",
                                  /*field_is_autofilled=*/false, {});

  ASSERT_TRUE(profiles_to_suggest.empty());
}

// Tests that disused profiles get removed.
TEST_F(AddressSuggestionGeneratorTest,
       GetProfilesToSuggest_RemoveDisusedProfiles_FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillChangeDisusedAddressSuggestionTreatment);
  base::Time kCurrentTime = AutofillClock::Now();
  constexpr size_t kNumProfiles = 10;
  constexpr base::TimeDelta k30Days = base::Days(30);
  constexpr size_t kNbSuggestions =
      (kDisusedDataModelTimeDelta + base::Days(29)) / k30Days;

  // Set up the profile vectors with last use dates ranging from `kCurrentTime`
  // to 270 days ago, in 30 day increments.
  std::vector<AutofillProfile> profiles(
      kNumProfiles,
      AutofillProfile(i18n_model_definition::kLegacyHierarchyCountryCode));
  for (size_t i = 0; i < kNumProfiles; ++i) {
    profiles[i].SetRawInfo(
        NAME_FULL, base::UTF8ToUTF16(base::StringPrintf("Bob %zu Doe", i)));
    profiles[i].set_use_date(kCurrentTime - (i * k30Days));
    address_data().AddProfile(profiles[i]);
  }

  base::HistogramTester histogram_tester;
  std::vector<AutofillProfile> profiles_to_suggest =
      GetProfilesToSuggestForTest(address_data(), NAME_FULL, u"",
                                  /*field_is_autofilled=*/false, {NAME_FULL});

  ASSERT_EQ(kNbSuggestions, profiles_to_suggest.size());
  for (size_t i = 0; i < kNbSuggestions; ++i) {
    EXPECT_EQ(profiles[i].guid(), profiles_to_suggest[i].guid()) << i;
  }
  histogram_tester.ExpectUniqueSample(kAddressesSuppressedHistogramName,
                                      kNumProfiles - kNbSuggestions, 1);
}

// Tests that disused profiles get removed, but that this doesn't affect the
// first few profiles in the list, depending on the parameterization of the
// feature AutofillChangeDisusedAddressSuggestionTreatment.
TEST_F(AddressSuggestionGeneratorTest,
       GetProfilesToSuggest_RemoveDisusedProfiles_FeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillChangeDisusedAddressSuggestionTreatment,
      base::FieldTrialParams{
          {features::kNumberOfIgnoredSuggestions.name, "1"}});
  base::Time kDisusedTime =
      AutofillClock::Now() - kDisusedDataModelTimeDelta - base::Days(1);

  AutofillProfile profile_1 = test::GetFullProfile();
  AutofillProfile profile_2 = test::GetFullProfile2();
  profile_1.set_use_count(10);
  profile_1.set_use_date(kDisusedTime);
  profile_2.set_use_count(1);
  profile_2.set_use_date(kDisusedTime);
  address_data().AddProfile(profile_1);
  address_data().AddProfile(profile_2);

  base::HistogramTester histogram_tester;
  std::vector<AutofillProfile> profiles_to_suggest =
      GetProfilesToSuggestForTest(address_data(), NAME_FULL, u"",
                                  /*field_is_autofilled=*/false, {NAME_FULL});

  ASSERT_EQ(profiles_to_suggest.size(), 1u);
  EXPECT_EQ(profiles_to_suggest.front().guid(), profile_1.guid());
  histogram_tester.ExpectUniqueSample(kAddressesSuppressedHistogramName, 1, 1);
}

TEST_F(AddressSuggestionGeneratorTest, CreateSuggestionsFromProfiles) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");

  std::vector<Suggestion> suggestions = CreateSuggestionsFromProfilesForTest(
      {profile}, {ADDRESS_HOME_STREET_ADDRESS}, SuggestionType::kAddressEntry,
      ADDRESS_HOME_STREET_ADDRESS,
      /*trigger_field_max_length=*/0);
  ASSERT_FALSE(suggestions.empty());
  EXPECT_EQ(u"123 Zoo St., Second Line, Third line, unit 5",
            suggestions[0].main_text.value);
}

TEST_F(AddressSuggestionGeneratorTest, CreateSuggestionsUsingEmailOverride) {
  AutofillProfile profile1 = test::GetFullProfile();
  AutofillProfile profile2 = test::GetFullProfile2();

  std::vector<Suggestion> suggestions = CreateSuggestionsFromProfilesForTest(
      {profile1, profile2}, {EMAIL_ADDRESS}, SuggestionType::kAddressEntry,
      EMAIL_ADDRESS, /*trigger_field_max_length=*/0, false, "en-US",
      "plus-address-override@me.com",
      base::UTF16ToUTF8(profile2.GetRawInfo(EMAIL_ADDRESS)));
  ASSERT_EQ(suggestions.size(), 2u);
  EXPECT_EQ(profile1.GetRawInfo(EMAIL_ADDRESS), suggestions[0].main_text.value);
  EXPECT_EQ(u"plus-address-override@me.com", suggestions[1].main_text.value);
}

TEST_F(AddressSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_PhoneSubstring) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");

  std::vector<Suggestion> suggestions = CreateSuggestionsFromProfilesForTest(
      {profile}, {PHONE_HOME_WHOLE_NUMBER}, SuggestionType::kAddressEntry,
      PHONE_HOME_WHOLE_NUMBER,
      /*trigger_field_max_length=*/0);
  ASSERT_FALSE(suggestions.empty());
  EXPECT_EQ(u"+1 234-567-8910", suggestions[0].main_text.value);
}

TEST_F(AddressSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_PartialNameFieldHasFullNameMainText) {
  base::test::ScopedFeatureList features(
      features::kAutofillGranularFillingAvailable);
  AutofillProfile profile = test::GetFullProfile();

  EXPECT_THAT(
      CreateSuggestionsFromProfilesForTest({profile}, {NAME_FIRST, NAME_LAST},
                                           SuggestionType::kAddressEntry,
                                           NAME_FIRST,
                                           /*trigger_field_max_length=*/0),
      SuggestionVectorMainTextsAre(Suggestion::Text(
          profile.GetRawInfo(NAME_FULL), Suggestion::Text::IsPrimary(true))));
}

// TODO(crbug.com/40274514): Move AutofillChildrenSuggestionGeneratorTest.
// CreateSuggestionsFromProfiles_GroupFillingLabels_* tests under this fixture.
// Text fixture for label generation related tests. Parameterized by triggering
// field type since how we build labels depends highly on it.
class AutofillLabelSuggestionGeneratorTest
    : public AddressSuggestionGeneratorTest,
      public testing::WithParamInterface<FieldType> {
 public:
  std::u16string GetFullFormFillingLabel(const AutofillProfile& profile) {
    // Phone fields are a snow flake, they contain both `NAME_FULL` and
    // `ADDRESS_HOME_LINE1`.
    const std::u16string label_applied_to_phone_fields =
        profile.GetRawInfo(NAME_FULL) + u", " +
        profile.GetRawInfo(ADDRESS_HOME_LINE1);
    return GetTriggeringFieldType() == ADDRESS_HOME_STREET_ADDRESS
               ? profile.GetRawInfo(NAME_FULL)
           : GetTriggeringFieldType() == PHONE_HOME_WHOLE_NUMBER
               ? label_applied_to_phone_fields
               : profile.GetRawInfo(ADDRESS_HOME_LINE1);
  }

  FieldType GetTriggeringFieldType() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutofillGranularFillingAvailable};
};

INSTANTIATE_TEST_SUITE_P(AddressSuggestionGeneratorTest,
                         AutofillLabelSuggestionGeneratorTest,
                         ::testing::ValuesIn({NAME_FULL, ADDRESS_HOME_ZIP,
                                              ADDRESS_HOME_STREET_ADDRESS,
                                              PHONE_HOME_WHOLE_NUMBER}));

// Suggestions for `ADDRESS_HOME_LINE1` should have `NAME_FULL` as the label.
// Suggestions for name or address fields which do not include
// `ADDRESS_HOME_LINE1` should have `ADDRESS_HOME_LINE1` as the label.
TEST_P(
    AutofillLabelSuggestionGeneratorTest,
    CreateSuggestionsFromProfiles_FullFormFilling_SuggestionsHaveCorrectLabels) {
  AutofillProfile profile = test::GetFullProfile();
  FieldType triggering_field_type = GetTriggeringFieldType();
  const std::u16string full_form_filling_label =
      GetFullFormFillingLabel(profile);

  EXPECT_THAT(
      CreateSuggestionsFromProfilesForTest(
          {profile}, {NAME_FULL, ADDRESS_HOME_STREET_ADDRESS, ADDRESS_HOME_ZIP},
          SuggestionType::kAddressEntry, triggering_field_type,
          /*trigger_field_max_length=*/0),
      ElementsAre(AllOf(EqualLabels({{full_form_filling_label}}))));
}

TEST_P(
    AutofillLabelSuggestionGeneratorTest,
    CreateSuggestionsFromProfiles_FullFormFilling_SuggestionsNeedMoreLabelsForDifferentiation) {
  AutofillProfile profile1 = test::GetFullProfile();
  AutofillProfile profile2 = test::GetFullProfile();
  profile1.SetRawInfo(EMAIL_ADDRESS, u"hoa@gmail.com");
  profile2.SetRawInfo(EMAIL_ADDRESS, u"pham@gmail.com");

  // The only difference between the two profiles is the email address.
  // That's why the email address is part of the differentiating label.
  FieldType triggering_field_type = GetTriggeringFieldType();
  const std::u16string full_form_filling_label =
      GetFullFormFillingLabel(profile1) +
      l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESS_SUMMARY_SEPARATOR);

  EXPECT_THAT(
      CreateSuggestionsFromProfilesForTest(
          {profile1, profile2}, {NAME_FULL, ADDRESS_HOME_STREET_ADDRESS},
          SuggestionType::kAddressEntry, triggering_field_type,
          /*trigger_field_max_length=*/0),
      ElementsAre(
          AllOf(EqualLabels({{full_form_filling_label + u"hoa@gmail.com"}})),
          AllOf(EqualLabels({{full_form_filling_label + u"pham@gmail.com"}}))));
}

// The logic which adds the country as a differentiating label is slightly
// different than the logic which adds any other differentiating label. Since
// the country is the last candidate for a differentiating label, this test also
// prevents random label behaviour (such as non-differentiating label being
// chosen or label not showing at all).
TEST_P(
    AutofillLabelSuggestionGeneratorTest,
    CreateSuggestionsFromProfiles_FullFormFilling_CountryIsChosenAsDifferentiatingLabel) {
  AutofillProfile profile1 = test::GetFullProfile();
  AutofillProfile profile2 = profile1;
  profile2.SetRawInfo(ADDRESS_HOME_COUNTRY, u"CH");

  FieldType triggering_field_type = GetTriggeringFieldType();
  const std::u16string full_form_filling_label =
      GetFullFormFillingLabel(profile1) +
      l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESS_SUMMARY_SEPARATOR);

  EXPECT_THAT(
      CreateSuggestionsFromProfilesForTest(
          {profile1, profile2}, {NAME_FULL, ADDRESS_HOME_STREET_ADDRESS},
          SuggestionType::kAddressEntry, triggering_field_type,
          /*trigger_field_max_length=*/0),
      ElementsAre(
          AllOf(EqualLabels({{full_form_filling_label + u"United States"}})),
          AllOf(EqualLabels({{full_form_filling_label + u"Switzerland"}}))));
}

class AutofillChildrenSuggestionGeneratorTest
    : public AddressSuggestionGeneratorTest {
 public:
  std::vector<Suggestion> CreateSuggestionWithChildrenFromProfile(
      const AutofillProfile& profile,
      SuggestionType suggestion_type,
      FieldType trigger_field_type,
      const FieldTypeSet& field_types) {
    return CreateSuggestionsFromProfilesForTest(
        {profile}, field_types, suggestion_type, trigger_field_type,
        /*trigger_field_max_length=*/0, autofill_client()->IsOffTheRecord());
  }

  std::vector<Suggestion> CreateSuggestionWithChildrenFromProfile(
      const AutofillProfile& profile,
      SuggestionType suggestion_type,
      FieldType trigger_field_type) {
    return CreateSuggestionWithChildrenFromProfile(
        profile, suggestion_type, trigger_field_type, {trigger_field_type});
  }

  std::u16string GetFormattedInternationalNumber() {
    return base::UTF8ToUTF16(i18n::FormatPhoneForDisplay(
        base::UTF16ToUTF8(
            profile().GetInfo(PHONE_HOME_WHOLE_NUMBER, app_locale())),
        base::UTF16ToUTF8(profile().GetRawInfo(ADDRESS_HOME_COUNTRY))));
  }

  std::u16string GetFormattedNationalNumber() {
    return base::UTF8ToUTF16(i18n::FormatPhoneNationallyForDisplay(
        base::UTF16ToUTF8(
            profile().GetInfo(PHONE_HOME_WHOLE_NUMBER, app_locale())),
        base::UTF16ToUTF8(profile().GetRawInfo(ADDRESS_HOME_COUNTRY))));
  }

  const AutofillProfile& profile() const { return profile_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutofillGranularFillingAvailable};
  // The default profile used to generate suggestions. Uses a country with an
  // address model that contains all the field types required by the tests.
  const AutofillProfile profile_ =
      test::GetFullProfile(AddressCountryCode("AT"));
};

// Test that only "Fill address" is added when the target field is
// `ADDRESS_HOME_LINE1` and no other suggestion exist with the same
// `Suggestion::main_text` and `ADDRESS_HOME_LINE1`.
TEST_F(AutofillChildrenSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_GroupFillingLabels_AddOnlyFillAddress) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), SuggestionType::kFillFullAddress,
      /*trigger_field_type=*/ADDRESS_HOME_LINE1,
      /*field_types=*/{ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2});

  ASSERT_EQ(suggestions.size(), 1u);
  EXPECT_EQ(suggestions[0].labels, std::vector<std::vector<Suggestion::Text>>(
                                       {{Suggestion::Text(u"Fill address")}}));
}

// Test that a differentiating label is added when the `Suggestion::main_text`
// and detailing label are not unique across suggestions.
TEST_F(
    AutofillChildrenSuggestionGeneratorTest,
    CreateSuggestionsFromProfiles_GroupFillingLabels_AddFillAddressAndDifferentiatingLabel) {
  AutofillProfile profile_1 = test::GetFullProfile();
  profile_1.SetRawInfo(NAME_FULL, u"John Doe");

  AutofillProfile profile_2 = test::GetFullProfile();
  profile_2.SetRawInfo(NAME_FULL, u"John Lemon");

  // `profile_1` and `profile_2` have the same `ADDRESS_HOME_LINE1`, which
  // will lead to the necessity of a differentiating label (`NAME_FULL`).
  std::vector<Suggestion> suggestions = CreateSuggestionsFromProfilesForTest(
      {profile_1, profile_2}, {ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2},
      SuggestionType::kFillFullAddress, ADDRESS_HOME_LINE1,
      /*trigger_field_max_length=*/0);

  ASSERT_EQ(suggestions.size(), 2u);
  EXPECT_EQ(suggestions[0].labels,
            std::vector<std::vector<Suggestion::Text>>(
                {{Suggestion::Text(u"Fill address - John Doe")}}));
}

// Test similar to the one above. However also makes sure that
// `ADDRESS_HOME_LINE1` value is added to the label if
// the target field does not contain street address related information
// (ADDRESS_LINE1, ADDRESS_LINE2 and ADDRESS_STREET_NAME).
TEST_F(
    AutofillChildrenSuggestionGeneratorTest,
    CreateSuggestionsFromProfiles_GroupFillingLabels_AddFillAddressAddressLine1AndDifferentiatingLabel) {
  AutofillProfile profile_1 = test::GetFullProfile();
  profile_1.SetRawInfo(ADDRESS_HOME_CITY, u"Munich");

  AutofillProfile profile_2 = test::GetFullProfile();
  profile_2.SetRawInfo(ADDRESS_HOME_CITY, u"Frankfurt");

  // `profile_1` and `profile_2` have the same `ADDRESS_HOME_ZIP`, which
  // will lead to the necessity of a differentiating label
  // (`ADDRESS_HOME_CITY`).
  std::vector<Suggestion> suggestions = CreateSuggestionsFromProfilesForTest(
      {profile_1, profile_2},
      {ADDRESS_HOME_LINE1, ADDRESS_HOME_ZIP, ADDRESS_HOME_CITY},
      SuggestionType::kFillFullAddress, ADDRESS_HOME_ZIP,
      /*trigger_field_max_length=*/0);

  ASSERT_EQ(suggestions.size(), 2u);
  EXPECT_EQ(suggestions[0].labels,
            std::vector<std::vector<Suggestion::Text>>({{Suggestion::Text(
                u"Fill address - " +
                profile_1.GetInfo(ADDRESS_HOME_LINE1, app_locale()) +
                u", Munich")}}));
}

// When there is no need to detailing or differentiating label, we add only the
// granular filling label, either "Fill full name" or "Fill address".
TEST_F(AutofillChildrenSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_GroupFillingLabels_AddOnlyFillName) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(),
      /*suggestion_type=*/
      SuggestionType::kFillFullName,
      /*trigger_field_type=*/NAME_FIRST,
      /*field_types=*/{NAME_FIRST, NAME_LAST});

  ASSERT_EQ(suggestions.size(), 1u);
  EXPECT_EQ(suggestions[0].labels,
            std::vector<std::vector<Suggestion::Text>>(
                {{Suggestion::Text(u"Fill full name")}}));
}

// Test that no labels are added when filling targets only one field.
TEST_F(
    AutofillChildrenSuggestionGeneratorTest,
    CreateSuggestionsFromProfiles_GroupFillingLabels_SingleFieldFillingHasNoLabels) {
  AutofillProfile profile = test::GetFullProfile();

  std::vector<Suggestion> suggestions = CreateSuggestionsFromProfilesForTest(
      {profile}, {NAME_FULL}, SuggestionType::kFillFullName, NAME_FULL,
      /*trigger_field_max_length=*/0);

  ASSERT_EQ(suggestions.size(), 1u);
  EXPECT_EQ(suggestions[0].labels,
            std::vector<std::vector<Suggestion::Text>>({}));
}

TEST_F(AutofillChildrenSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_FirstLevelChildrenSuggestions) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), SuggestionType::kAddressEntry, NAME_FIRST);

  ASSERT_EQ(1U, suggestions.size());
  // Test root suggestion
  EXPECT_THAT(suggestions,
              ElementsAre(Field(
                  &Suggestion::main_text,
                  Suggestion::Text(profile().GetInfo(NAME_FULL, app_locale()),
                                   Suggestion::Text::IsPrimary(true)))));

  // The children suggestions should be.
  //
  // 1. fill full name
  // 2. first name
  // 3. middle name
  // 4. family name
  // 5. line separator
  // 6. company
  // 7. address line 1
  // 8. address line 2
  // 9. City
  // 10. Zip
  // 11. line separator
  // 12. phone number
  // 13. email
  // 14. line separator
  // 15. edit profile
  // 16. delete address
  ASSERT_EQ(16U, suggestions[0].children.size());
  EXPECT_THAT(
      suggestions[0].children,
      ElementsAre(
          EqualsSuggestion(SuggestionType::kFillFullName),
          EqualsFieldByFieldFillingSuggestion(
              SuggestionType::kAddressFieldByFieldFilling,
              profile().GetInfo(NAME_FIRST, app_locale()), NAME_FIRST,
              Suggestion::Guid(profile().guid())),
          EqualsFieldByFieldFillingSuggestion(
              SuggestionType::kAddressFieldByFieldFilling,
              profile().GetInfo(NAME_MIDDLE, app_locale()), NAME_MIDDLE,
              Suggestion::Guid(profile().guid())),
          EqualsFieldByFieldFillingSuggestion(
              SuggestionType::kAddressFieldByFieldFilling,
              profile().GetInfo(NAME_LAST, app_locale()), NAME_LAST,
              Suggestion::Guid(profile().guid())),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsFieldByFieldFillingSuggestion(
              SuggestionType::kAddressFieldByFieldFilling,
              profile().GetInfo(COMPANY_NAME, app_locale()), COMPANY_NAME,
              Suggestion::Guid(profile().guid())),
          EqualsFieldByFieldFillingSuggestion(
              SuggestionType::kAddressFieldByFieldFilling,
              profile().GetInfo(ADDRESS_HOME_LINE1, app_locale()),
              ADDRESS_HOME_LINE1, Suggestion::Guid(profile().guid())),
          EqualsFieldByFieldFillingSuggestion(
              SuggestionType::kAddressFieldByFieldFilling,
              profile().GetInfo(ADDRESS_HOME_LINE2, app_locale()),
              ADDRESS_HOME_LINE2, Suggestion::Guid(profile().guid())),
          EqualsFieldByFieldFillingSuggestion(
              SuggestionType::kAddressFieldByFieldFilling,
              profile().GetInfo(ADDRESS_HOME_CITY, app_locale()),
              ADDRESS_HOME_CITY, Suggestion::Guid(profile().guid())),
          EqualsFieldByFieldFillingSuggestion(
              SuggestionType::kAddressFieldByFieldFilling,
              profile().GetInfo(ADDRESS_HOME_ZIP, app_locale()),
              ADDRESS_HOME_ZIP, Suggestion::Guid(profile().guid())),
          EqualsSuggestion(SuggestionType::kSeparator),
          // Triggering field is not a phone number, international phone number
          // should be shown to the user.
          EqualsFieldByFieldFillingSuggestion(
              SuggestionType::kAddressFieldByFieldFilling,
              GetFormattedInternationalNumber(), PHONE_HOME_WHOLE_NUMBER,
              Suggestion::Guid(profile().guid())),
          EqualsFieldByFieldFillingSuggestion(
              SuggestionType::kAddressFieldByFieldFilling,
              profile().GetInfo(EMAIL_ADDRESS, app_locale()), EMAIL_ADDRESS,
              Suggestion::Guid(profile().guid())),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsSuggestion(SuggestionType::kEditAddressProfile),
          EqualsSuggestion(SuggestionType::kDeleteAddressProfile)));
}

TEST_F(AutofillChildrenSuggestionGeneratorTest,
       IncognitoMode_EditAndDeleteSuggestionsAreNotAdded) {
  autofill_client()->set_is_off_the_record(true);
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), SuggestionType::kAddressEntry, NAME_FIRST);

  ASSERT_EQ(1u, suggestions.size());
  ASSERT_GT(suggestions[0].children.size(), 0u);
  EXPECT_THAT(
      suggestions[0].children,
      Not(Contains(EqualsSuggestion(SuggestionType::kEditAddressProfile))))
      << "Children should not contain the 'Edit address' suggestion because "
         "there user is in incognito mode.";
  EXPECT_THAT(
      suggestions[0].children,
      Not(Contains(EqualsSuggestion(SuggestionType::kDeleteAddressProfile))))
      << "Children should not contain the 'Delete address' suggestion because "
         "there user is in incognito mode.";
}

TEST_F(AutofillChildrenSuggestionGeneratorTest,
       FillEverythingFromAddressProfile_FullFormFilling_NotAdded) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), SuggestionType::kAddressEntry, NAME_FIRST);

  ASSERT_EQ(1U, suggestions.size());
  ASSERT_FALSE(suggestions[0].children.empty());
  EXPECT_THAT(suggestions[0].children,
              Not(Contains(EqualsSuggestion(
                  SuggestionType::kFillEverythingFromAddressProfile))))
      << "Children should not contain the 'fill everything' suggestion because "
         "`suggestion_type` indicates full form filling.";
}

// TODO(crbug.com/40274514): Merge this test with
// `FillEverythingFromAddressProfile_FullFormFilling_NotAdded` once the feature
// is launched.
TEST_F(AutofillChildrenSuggestionGeneratorTest,
       FillEverythingFromAddressProfile_FullFormFilling_AddedAtTheTop) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillGranularFillingAvailable,
      {{features::
            kAutofillGranularFillingAvailableWithFillEverythingAtTheBottomParam
                .name,
        "false"}});

  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), SuggestionType::kAddressEntry, NAME_FIRST);

  ASSERT_EQ(1U, suggestions.size());
  ASSERT_FALSE(suggestions[0].children.empty());
  EXPECT_THAT(
      suggestions[0].children[0],
      EqualsSuggestion(SuggestionType::kFillEverythingFromAddressProfile))
      << "The first child suggestion should be 'fill everything', regardless "
         "of the filling mode.";
}

TEST_F(AutofillChildrenSuggestionGeneratorTest,
       FillEverythingFromAddressProfile_FieldByFieldFilling_Added) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), SuggestionType::kAddressFieldByFieldFilling, NAME_FIRST);

  ASSERT_EQ(1U, suggestions.size());
  ASSERT_FALSE(suggestions[0].children.empty());
  EXPECT_THAT(suggestions[0].children,
              Contains(EqualsSuggestion(
                  SuggestionType::kFillEverythingFromAddressProfile)))
      << "Children should contain the 'fill everything' suggestion because "
         "`suggestion_type` indicates field-by-field filling.";
}

// TODO(crbug.com/40274514): Merge this test with
// `FillEverythingFromAddressProfile_FieldByFieldFilling_Added` once the
// feature is launched.
TEST_F(AutofillChildrenSuggestionGeneratorTest,
       FillEverythingFromAddressProfile_FieldByFieldFilling_AddedAtTheTop) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillGranularFillingAvailable,
      {{features::
            kAutofillGranularFillingAvailableWithFillEverythingAtTheBottomParam
                .name,
        "false"}});

  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), SuggestionType::kAddressFieldByFieldFilling, NAME_FIRST);

  ASSERT_EQ(1U, suggestions.size());
  ASSERT_FALSE(suggestions[0].children.empty());
  EXPECT_THAT(
      suggestions[0].children[0],
      EqualsSuggestion(SuggestionType::kFillEverythingFromAddressProfile))
      << "The first child suggestion should be 'fill everything', regardless "
         "of the filling mode.";
}

TEST_F(AutofillChildrenSuggestionGeneratorTest,
       FillEverythingFromAddressProfile_GroupFilling_Added) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), SuggestionType::kFillFullName, NAME_FIRST);

  ASSERT_EQ(1U, suggestions.size());
  ASSERT_FALSE(suggestions[0].children.empty());
  EXPECT_THAT(suggestions[0].children,
              Contains(EqualsSuggestion(
                  SuggestionType::kFillEverythingFromAddressProfile)))
      << "Children should contain the 'fill everything' suggestion because "
         "`suggestion_type` indicates group filling.";
}

// TODO(crbug.com/40274514): Merge this test with
// `FillEverythingFromAddressProfile_GroupFilling_Added` once the feature is
// launched.
TEST_F(AutofillChildrenSuggestionGeneratorTest,
       FillEverythingFromAddressProfile_GroupFilling_AddedAtTheTop) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillGranularFillingAvailable,
      {{features::
            kAutofillGranularFillingAvailableWithFillEverythingAtTheBottomParam
                .name,
        "false"}});

  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), SuggestionType::kFillFullName, NAME_FIRST);

  ASSERT_EQ(1U, suggestions.size());
  ASSERT_FALSE(suggestions[0].children.empty());
  EXPECT_THAT(
      suggestions[0].children[0],
      EqualsSuggestion(SuggestionType::kFillEverythingFromAddressProfile))
      << "The first child suggestion should be 'fill everything', regardless "
         "of the filling mode.";
}

TEST_F(AutofillChildrenSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_SecondLevelChildrenSuggestions) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), SuggestionType::kAddressEntry, NAME_FIRST);

  ASSERT_EQ(1U, suggestions.size());
  // Suggestions should have two levels of children, The address line 1 (sixth
  // child) suggestion should have the following children: house number street
  // name.
  ASSERT_EQ(2U, suggestions[0].children[6].children.size());
  EXPECT_THAT(
      suggestions[0].children[6].children,
      ElementsAre(
          EqualsFieldByFieldFillingSuggestion(
              SuggestionType::kAddressFieldByFieldFilling,
              profile().GetInfo(ADDRESS_HOME_HOUSE_NUMBER, app_locale()),
              ADDRESS_HOME_HOUSE_NUMBER, Suggestion::Guid(profile().guid()),
              {{Suggestion::Text(l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_HOUSE_NUMBER_SUGGESTION_SECONDARY_TEXT))}}),
          EqualsFieldByFieldFillingSuggestion(
              SuggestionType::kAddressFieldByFieldFilling,
              profile().GetInfo(ADDRESS_HOME_STREET_NAME, app_locale()),
              ADDRESS_HOME_STREET_NAME, Suggestion::Guid(profile().guid()),
              {{Suggestion::Text(l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_STREET_NAME_SUGGESTION_SECONDARY_TEXT))}})));
}

TEST_F(AutofillChildrenSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_FieldByFieldFilling) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), SuggestionType::kAddressFieldByFieldFilling, NAME_FIRST);

  ASSERT_EQ(suggestions.size(), 1u);
  // Differently from other filling modes, where when focusing on a name field
  // the NAME_FULL is rendered in the main text, field-by-field filling always
  // displays the value that will actually be used to fill the field as main
  // text.
  EXPECT_THAT(suggestions[0],
              EqualsFieldByFieldFillingSuggestion(
                  SuggestionType::kAddressFieldByFieldFilling,
                  profile().GetInfo(NAME_FIRST, app_locale()), NAME_FIRST,
                  Suggestion::Guid(profile().guid()), {}));
}

TEST_F(AutofillChildrenSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_GroupFilling) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), SuggestionType::kFillFullName, NAME_FIRST,
      {NAME_FIRST, NAME_LAST});

  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ(suggestions[0].type, SuggestionType::kFillFullName);
  EXPECT_EQ(suggestions[0].icon, Suggestion::Icon::kNoIcon);
}

// Note that only full form filling has an icon.
TEST_F(AutofillChildrenSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_FullFormFilling) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), SuggestionType::kAddressEntry, NAME_FIRST,
      {NAME_FIRST, NAME_LAST});

  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ(suggestions[0].type, SuggestionType::kAddressEntry);
  EXPECT_EQ(suggestions[0].icon, Suggestion::Icon::kLocation);
}

TEST_F(
    AutofillChildrenSuggestionGeneratorTest,
    CreateSuggestionsFromProfiles_LastTargetedFieldsContainOnlyEmail_FieldByField) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), SuggestionType::kAddressFieldByFieldFilling, EMAIL_ADDRESS,
      {EMAIL_ADDRESS});

  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ(suggestions[0].type, SuggestionType::kAddressFieldByFieldFilling);
  // Email icon is used to create consistency with plus address suggestions.
  EXPECT_EQ(suggestions[0].icon, Suggestion::Icon::kEmail);
}

TEST_F(AutofillChildrenSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_LastTargetedFieldsAreNone_FieldByField) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), SuggestionType::kAddressFieldByFieldFilling, EMAIL_ADDRESS,
      {});

  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ(suggestions[0].type, SuggestionType::kAddressFieldByFieldFilling);
  // Email icon is used to create consistency with plus address suggestions.
  EXPECT_EQ(suggestions[0].icon, Suggestion::Icon::kEmail);
}

// Asserts that when the triggering field is a phone field, the phone number
// suggestion is of type `SuggestionType::kFillFullPhoneNumber`. In other
// scenarios, phone number is of type
// `SuggestionType::kAddressFieldByFieldFilling` as the user expressed intent
// to use their phone number their phone number on a "random" field.
TEST_F(
    AutofillChildrenSuggestionGeneratorTest,
    CreateSuggestionsFromProfiles_ChildrenSuggestionsPhoneField_International) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), SuggestionType::kAddressEntry, PHONE_HOME_WHOLE_NUMBER);

  ASSERT_EQ(1U, suggestions.size());
  // The child suggestions should be:
  //
  // 1. first name
  // 2. middle name
  // 3. family name
  // 4. line separator
  // 5. company
  // 6. address line 1
  // 7. address line 2
  // 8. City
  // 9. Zip
  // 10. line separator
  // 11. phone number
  // 12. email
  // 13. line separator
  // 14. edit profile
  // 15. delete address
  ASSERT_EQ(15U, suggestions[0].children.size());

  // Triggering field is international phone number type, international phone
  // number should be shown to the user.
  EXPECT_THAT(suggestions[0].children[10],
              EqualsSuggestion(SuggestionType::kFillFullPhoneNumber,
                               GetFormattedInternationalNumber()));
  EXPECT_THAT(suggestions[0].children[10].children, IsEmpty());
}

// Asserts that when the triggering field is a phone field, the phone number
// suggestion is of type `SuggestionType::kFillFullPhoneNumber`. In other
// scenarios, phone number is of type
// `SuggestionType::kAddressFieldByFieldFilling` as the user expressed intent
// to use their phone number on a "random" field.
TEST_F(
    AutofillChildrenSuggestionGeneratorTest,
    CreateSuggestionsFromProfiles_ChildrenSuggestionsPhoneField_CountryCode) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), SuggestionType::kAddressEntry, PHONE_HOME_COUNTRY_CODE);

  ASSERT_EQ(1U, suggestions.size());
  // The child suggestions should be:
  //
  // 1. first name
  // 2. middle name
  // 3. family name
  // 4. line separator
  // 5. company
  // 6. address line 1
  // 7. address line 2
  // 8. City
  // 9. Zip
  // 10. line separator
  // 11. phone number
  // 12. email
  // 13. line separator
  // 14. edit profile
  // 15. delete address
  ASSERT_EQ(15U, suggestions[0].children.size());

  // Triggering field is phone number country code, international phone number
  // should be shown to the user.
  EXPECT_THAT(suggestions[0].children[10],
              EqualsSuggestion(SuggestionType::kFillFullPhoneNumber,
                               GetFormattedInternationalNumber()));
  EXPECT_THAT(suggestions[0].children[10].children, IsEmpty());
}

// Asserts that when the triggering field is a phone field, the phone number
// suggestion is of type `SuggestionType::kFillFullPhoneNumber`. In other
// scenarios, phone number is of type
// `SuggestionType::kAddressFieldByFieldFilling` as the user expressed intent
// to use their phone number their phone number on a "random" field.
TEST_F(AutofillChildrenSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_ChildrenSuggestionsPhoneField_Local) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), SuggestionType::kAddressEntry, PHONE_HOME_CITY_AND_NUMBER);

  ASSERT_EQ(1U, suggestions.size());
  // The child suggestions should be:
  //
  // 1. first name
  // 2. middle name
  // 3. family name
  // 4. line separator
  // 5. company
  // 6. address line 1
  // 7. address line 2
  // 8. City
  // 9. Zip
  // 10. line separator
  // 11. phone number
  // 12. email
  // 13. line separator
  // 14. edit profile
  // 15. delete address
  ASSERT_EQ(15U, suggestions[0].children.size());
  // Triggering field is local phone number type, local phone number should
  // be shown to the user.
  EXPECT_THAT(suggestions[0].children[10],
              EqualsSuggestion(SuggestionType::kFillFullPhoneNumber,
                               GetFormattedNationalNumber()));
  EXPECT_THAT(suggestions[0].children[10].children, IsEmpty());
}

// Same as above but for email fields.
TEST_F(AutofillChildrenSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_ChildrenSuggestionsEmailField) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), SuggestionType::kAddressEntry, EMAIL_ADDRESS);

  // The child suggestions should be:
  //
  // 1. first name
  // 2. middle name
  // 3. family name
  // 4. line separator
  // 5. company
  // 6. address line 1
  // 7. address line 2
  // 8. City
  // 9. Zip
  // 10. line separator
  // 11. phone number
  // 12. email
  // 13. line separator
  // 14. edit profile
  // 15. delete address
  ASSERT_EQ(15U, suggestions[0].children.size());
  EXPECT_THAT(suggestions[0].children[11],
              Field(&Suggestion::type, SuggestionType::kFillFullEmail));
}

TEST_F(AutofillChildrenSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_ChildrenSuggestionsAddressField) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), SuggestionType::kAddressEntry, ADDRESS_HOME_LINE1);

  // The child suggestions should be:
  //
  // 1. first name
  // 2. middle name
  // 3. family name
  // 4. line separator
  // 5. fill full address
  // 6. company
  // 7. address line 1
  // 8. address line 2
  // 9. City
  // 10. Zip
  // 11. line separator
  // 12. phone number
  // 13. email
  // 14. line separator
  // 15. edit address
  // 16. delete address
  ASSERT_EQ(suggestions.size(), 1u);
  ASSERT_EQ(16U, suggestions[0].children.size());
  EXPECT_THAT(suggestions[0].children[4],
              Field(&Suggestion::type, SuggestionType::kFillFullAddress));
}

TEST_F(AutofillChildrenSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_ChildrenSuggestionsCompanyField) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), SuggestionType::kAddressEntry, COMPANY_NAME);

  // The child suggestions should be:
  //
  // 1. first name
  // 2. middle name
  // 3. family name
  // 4. line separator
  // 5. fill full address
  // 6. company
  // 7. address line 1
  // 8. address line 2
  // 9. City
  // 10. Zip
  // 11. line separator
  // 12. phone number
  // 13. email
  // 14. line separator
  // 15. edit address
  // 16. delete address
  ASSERT_EQ(suggestions.size(), 1u);
  ASSERT_EQ(16U, suggestions[0].children.size());
  EXPECT_THAT(suggestions[0].children[4],
              Field(&Suggestion::type, SuggestionType::kFillFullAddress));
}

TEST_F(
    AutofillChildrenSuggestionGeneratorTest,
    CreateSuggestionsFromProfiles_ChildrenSuggestions_HouseNumberAndStreetNameCanBeNestedUnderDifferentAddressLines) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  // Update the profile to have house number and street name information in
  // different address lines.
  profile.SetRawInfo(ADDRESS_HOME_LINE1, u"Amphitheatre Parkway, Brookling");
  profile.SetRawInfo(ADDRESS_HOME_LINE2, u"1600 Apartment 1");
  profile.SetRawInfo(ADDRESS_HOME_STREET_NAME, u"Amphitheatre Parkway");
  profile.SetRawInfo(ADDRESS_HOME_HOUSE_NUMBER, u"1600");
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile, SuggestionType::kAddressEntry, ADDRESS_HOME_LINE1);

  ASSERT_EQ(1u, suggestions.size());
  ASSERT_LE(3u, suggestions[0].children.size());
  // The address line 1 (sixth child) should have the street name as child.
  EXPECT_THAT(suggestions[0].children[1].children,
              ElementsAre(EqualsFieldByFieldFillingSuggestion(
                  SuggestionType::kAddressFieldByFieldFilling,
                  profile.GetInfo(ADDRESS_HOME_STREET_NAME, app_locale()),
                  ADDRESS_HOME_STREET_NAME, Suggestion::Guid(profile.guid()),
                  {{Suggestion::Text(l10n_util::GetStringUTF16(
                      IDS_AUTOFILL_STREET_NAME_SUGGESTION_SECONDARY_TEXT))}})));
  // The address line 2 (seventh child) should have the house number as child.
  EXPECT_THAT(
      suggestions[0].children[2].children,
      ElementsAre(EqualsFieldByFieldFillingSuggestion(
          SuggestionType::kAddressFieldByFieldFilling,
          profile.GetInfo(ADDRESS_HOME_HOUSE_NUMBER, app_locale()),
          ADDRESS_HOME_HOUSE_NUMBER, Suggestion::Guid(profile.guid()),
          {{Suggestion::Text(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_HOUSE_NUMBER_SUGGESTION_SECONDARY_TEXT))}})));
}

// This fixture contains tests for autofill being triggered from the context
// menu on a field which is not classified as an address.
class AutofillNonAddressFieldsSuggestionGeneratorTest
    : public AutofillChildrenSuggestionGeneratorTest {
 public:
  void SetUp() override {
    AutofillChildrenSuggestionGeneratorTest::SetUp();
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::
                                  kAutofillForUnclassifiedFieldsAvailable},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AutofillNonAddressFieldsSuggestionGeneratorTest,
       AllProfilesGenerateSuggestions) {
  address_data().AddProfile(test::GetFullProfile());
  address_data().AddProfile(test::GetFullProfile2());

  FormFieldData triggering_field;

  std::vector<Suggestion> suggestions = GetSuggestionsForProfiles(
      *autofill_client(), {UNKNOWN_TYPE}, triggering_field, UNKNOWN_TYPE,
      SuggestionType::kAddressEntry,
      AutofillSuggestionTriggerSource::kManualFallbackAddress, std::nullopt);
  EXPECT_EQ(suggestions.size(), 4ul);
  EXPECT_THAT(suggestions[0], EqualsSuggestion(SuggestionType::kAddressEntry));
  EXPECT_THAT(suggestions[1], EqualsSuggestion(SuggestionType::kAddressEntry));
  EXPECT_THAT(suggestions, ContainsAddressFooterSuggestions());
}

// Generally, a profile is displayed with name as main text and address as
// label. But with incomplete profiles, it might be problematic. This test
// creates various incomplete profiles and makes sure that a main text and a
// label are always chosen from the available fields (or only main_text if the
// profile has only one field).
TEST_F(AutofillNonAddressFieldsSuggestionGeneratorTest,
       SuggestionsAreCorrectAndExpectedLabelsAndIconsAreCreated) {
  std::vector<AutofillProfile> profiles(
      5, AutofillProfile(i18n_model_definition::kLegacyHierarchyCountryCode));
  profiles[0].SetRawInfo(NAME_FULL, u"John Doe");
  profiles[0].SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"Address 123");
  profiles[1].SetRawInfo(NAME_FULL, u"Johnas Dhonas");
  profiles[1].SetRawInfo(ADDRESS_HOME_CITY, u"New York");
  profiles[2].SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"Other Address 33");
  profiles[2].SetRawInfo(ADDRESS_HOME_CITY, u"Old City");
  profiles[3].SetRawInfo(ADDRESS_HOME_CITY, u"Munich");
  profiles[3].SetRawInfo(EMAIL_ADDRESS, u"munich@gmail.com");
  profiles[4].SetRawInfo(EMAIL_ADDRESS, u"other@gmail.com");

  std::vector<Suggestion> suggestions = CreateSuggestionsFromProfilesForTest(
      std::move(profiles), {UNKNOWN_TYPE}, SuggestionType::kAddressEntry,
      UNKNOWN_TYPE,
      /*trigger_field_max_length=*/0);

  ASSERT_EQ(5u, suggestions.size());
  EXPECT_THAT(
      suggestions,
      ElementsAre(
          AllOf(Field(&Suggestion::main_text,
                      Suggestion::Text(u"John Doe",
                                       Suggestion::Text::IsPrimary(true))),
                EqualLabels({{u"Address 123"}}),
                Field(&Suggestion::type, SuggestionType::kAddressEntry),
                Field(&Suggestion::is_acceptable, false),
                Field(&Suggestion::icon, Suggestion::Icon::kLocation)),
          AllOf(Field(&Suggestion::main_text,
                      Suggestion::Text(u"Johnas Dhonas",
                                       Suggestion::Text::IsPrimary(true))),
                EqualLabels({{u"New York"}}),
                Field(&Suggestion::type, SuggestionType::kAddressEntry),
                Field(&Suggestion::is_acceptable, false),
                Field(&Suggestion::icon, Suggestion::Icon::kLocation)),
          AllOf(Field(&Suggestion::main_text,
                      Suggestion::Text(u"Other Address 33",
                                       Suggestion::Text::IsPrimary(true))),
                EqualLabels({{u"Old City"}}),
                Field(&Suggestion::type, SuggestionType::kAddressEntry),
                Field(&Suggestion::is_acceptable, false),
                Field(&Suggestion::icon, Suggestion::Icon::kLocation)),
          AllOf(Field(&Suggestion::main_text,
                      Suggestion::Text(u"Munich",
                                       Suggestion::Text::IsPrimary(true))),
                EqualLabels({{u"munich@gmail.com"}}),
                Field(&Suggestion::type, SuggestionType::kAddressEntry),
                Field(&Suggestion::is_acceptable, false),
                Field(&Suggestion::icon, Suggestion::Icon::kLocation)),
          AllOf(Field(&Suggestion::main_text,
                      Suggestion::Text(u"other@gmail.com",
                                       Suggestion::Text::IsPrimary(true))),
                EqualLabels(std::vector<std::vector<Suggestion::Text>>{
                    {Suggestion::Text(u"")}}),
                Field(&Suggestion::type, SuggestionType::kAddressEntry),
                Field(&Suggestion::is_acceptable, false),
                Field(&Suggestion::icon, Suggestion::Icon::kLocation))));
}

// This test checks that the resulting string of
// `AutofillProfile::CreateDifferentiatingLabels()` is split correctly into main
// text and labels. In Japanese, the resulting string doesn't have separators.
TEST_F(AutofillNonAddressFieldsSuggestionGeneratorTest,
       MainTextAndLabelsAreCorrect_Japanese) {
  AutofillProfile profile(AddressCountryCode("JP"));
  profile.SetRawInfo(NAME_FULL, u"ミク初音");
  profile.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"港区六本木ヒルズ森タワー");
  profile.set_language_code("ja");

  std::vector<Suggestion> suggestions = CreateSuggestionsFromProfilesForTest(
      {profile}, {UNKNOWN_TYPE}, SuggestionType::kAddressEntry, UNKNOWN_TYPE,
      /*trigger_field_max_length=*/0);
  EXPECT_THAT(suggestions,
              ElementsAre(AllOf(
                  Field(&Suggestion::main_text,
                        Suggestion::Text(u"港区六本木ヒルズ森タワー",
                                         Suggestion::Text::IsPrimary(true))),
                  EqualLabels({{u"ミク初音"}}),
                  Field(&Suggestion::type, SuggestionType::kAddressEntry),
                  Field(&Suggestion::is_acceptable, false))));
}

// This test checks that the resulting string of
// `AutofillProfile::CreateDifferentiatingLabels()` is split correctly into main
// text and labels. In Arabic, the resulting string is separated by arabic
// comma.
TEST_F(AutofillNonAddressFieldsSuggestionGeneratorTest,
       MainTextAndLabelsAreCorrect_Arabic) {
  AutofillProfile profile(AddressCountryCode("EG"));
  profile.SetRawInfo(NAME_FULL, u"صاحب");
  profile.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"الملكي");
  profile.set_language_code("ar");

  std::vector<Suggestion> suggestions = CreateSuggestionsFromProfilesForTest(
      {profile}, {UNKNOWN_TYPE}, SuggestionType::kAddressEntry, UNKNOWN_TYPE,
      /*trigger_field_max_length=*/0);
  EXPECT_THAT(
      suggestions,
      ElementsAre(AllOf(
          Field(&Suggestion::main_text,
                Suggestion::Text(u"صاحب", Suggestion::Text::IsPrimary(true))),
          EqualLabels({{u"الملكي"}}),
          Field(&Suggestion::type, SuggestionType::kAddressEntry),
          Field(&Suggestion::is_acceptable, false))));
}

// This test checks that the resulting string of
// `AutofillProfile::CreateDifferentiatingLabels()` is split correctly into main
// text and labels. In Thai, the resulting string is separated by space.
TEST_F(AutofillNonAddressFieldsSuggestionGeneratorTest,
       MainTextAndLabelsAreCorrect_Thai) {
  AutofillProfile profile(AddressCountryCode("TH"));
  profile.SetRawInfo(NAME_FULL, u"แขวงลุมพินี");
  profile.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"57 ปาร์คเวนเชอร์");
  profile.set_language_code("th");

  std::vector<Suggestion> suggestions = CreateSuggestionsFromProfilesForTest(
      {profile}, {UNKNOWN_TYPE}, SuggestionType::kAddressEntry, UNKNOWN_TYPE,
      /*trigger_field_max_length=*/0);
  EXPECT_THAT(suggestions,
              ElementsAre(AllOf(
                  Field(&Suggestion::main_text,
                        Suggestion::Text(u"แขวงลุมพินี",
                                         Suggestion::Text::IsPrimary(true))),
                  EqualLabels({{u"57 ปาร์คเวนเชอร์"}}),
                  Field(&Suggestion::type, SuggestionType::kAddressEntry),
                  Field(&Suggestion::is_acceptable, false))));
}

// Tests that a non-address field suggestion has all the profile fields as
// children, and doesn't have children like "Fill address" or "Fill full
// name".
TEST_F(AutofillNonAddressFieldsSuggestionGeneratorTest,
       SuggestionHasCorrectChildren) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), SuggestionType::kAddressEntry, UNKNOWN_TYPE);

  // The child suggestions should be:
  //
  // 1. first name
  // 2. middle name
  // 3. family name
  // 4. line separator
  // 5. company
  // 6. address line 1
  // 7. address line 2
  // 8. City
  // 9. Zip
  // 10. line separator
  // 11. phone number
  // 12. email
  // 13. line separator
  // 14. edit address
  // 15. delete address
  ASSERT_EQ(suggestions.size(), 1u);
  ASSERT_EQ(15u, suggestions[0].children.size());

  EXPECT_THAT(
      suggestions[0].children,
      ElementsAre(
          EqualsFieldByFieldFillingSuggestion(
              SuggestionType::kAddressFieldByFieldFilling,
              profile().GetInfo(NAME_FIRST, app_locale()), NAME_FIRST,
              Suggestion::Guid(profile().guid())),
          EqualsFieldByFieldFillingSuggestion(
              SuggestionType::kAddressFieldByFieldFilling,
              profile().GetInfo(NAME_MIDDLE, app_locale()), NAME_MIDDLE,
              Suggestion::Guid(profile().guid())),
          EqualsFieldByFieldFillingSuggestion(
              SuggestionType::kAddressFieldByFieldFilling,
              profile().GetInfo(NAME_LAST, app_locale()), NAME_LAST,
              Suggestion::Guid(profile().guid())),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsFieldByFieldFillingSuggestion(
              SuggestionType::kAddressFieldByFieldFilling,
              profile().GetInfo(COMPANY_NAME, app_locale()), COMPANY_NAME,
              Suggestion::Guid(profile().guid())),
          EqualsFieldByFieldFillingSuggestion(
              SuggestionType::kAddressFieldByFieldFilling,
              profile().GetInfo(ADDRESS_HOME_LINE1, app_locale()),
              ADDRESS_HOME_LINE1, Suggestion::Guid(profile().guid())),
          EqualsFieldByFieldFillingSuggestion(
              SuggestionType::kAddressFieldByFieldFilling,
              profile().GetInfo(ADDRESS_HOME_LINE2, app_locale()),
              ADDRESS_HOME_LINE2, Suggestion::Guid(profile().guid())),
          EqualsFieldByFieldFillingSuggestion(
              SuggestionType::kAddressFieldByFieldFilling,
              profile().GetInfo(ADDRESS_HOME_CITY, app_locale()),
              ADDRESS_HOME_CITY, Suggestion::Guid(profile().guid())),
          EqualsFieldByFieldFillingSuggestion(
              SuggestionType::kAddressFieldByFieldFilling,
              profile().GetInfo(ADDRESS_HOME_ZIP, app_locale()),
              ADDRESS_HOME_ZIP, Suggestion::Guid(profile().guid())),
          EqualsSuggestion(SuggestionType::kSeparator),
          // Triggering field is not a phone number, international phone number
          // should be shown to the user.
          EqualsFieldByFieldFillingSuggestion(
              SuggestionType::kAddressFieldByFieldFilling,
              GetFormattedInternationalNumber(), PHONE_HOME_WHOLE_NUMBER,
              Suggestion::Guid(profile().guid())),
          EqualsFieldByFieldFillingSuggestion(
              SuggestionType::kAddressFieldByFieldFilling,
              profile().GetInfo(EMAIL_ADDRESS, app_locale()), EMAIL_ADDRESS,
              Suggestion::Guid(profile().guid())),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsSuggestion(SuggestionType::kEditAddressProfile),
          EqualsSuggestion(SuggestionType::kDeleteAddressProfile)));
}

// Tests the scenario when:
// - autofill is triggered from the context menu on a field which is classified
// as an address field;
// - there is no profile which has values to fill the respective field.
// In this scenario, suggestions should look the same as the ones for an
// unclassified field.
TEST_F(AddressSuggestionGeneratorTest,
       NoProfilesHaveValuesForClassifiedField_AddressManualFallback) {
  base::test::ScopedFeatureList features(
      features::kAutofillForUnclassifiedFieldsAvailable);
  AutofillProfile profile = test::GetIncompleteProfile1();
  ASSERT_FALSE(profile.HasRawInfo(PHONE_HOME_WHOLE_NUMBER));
  address_data().AddProfile(profile);

  std::vector<Suggestion> suggestions = GetSuggestionsForProfiles(
      *autofill_client(),
      {NAME_FULL, ADDRESS_HOME_STREET_ADDRESS, PHONE_HOME_WHOLE_NUMBER},
      FormFieldData(), PHONE_HOME_WHOLE_NUMBER, SuggestionType::kAddressEntry,
      AutofillSuggestionTriggerSource::kManualFallbackAddress, std::nullopt);
  ASSERT_EQ(3u, suggestions.size());
  EXPECT_EQ(suggestions[0].type, SuggestionType::kAddressEntry);
  // This is the check which actually verifies that the suggestion looks the
  // same as the ones for an unclassified field (such a suggestion has
  // `is_acceptable` as false).
  EXPECT_EQ(suggestions[0].is_acceptable, false);
  EXPECT_THAT(suggestions, ContainsAddressFooterSuggestions());
}

// Tests that regular suggestions are filtered by the triggering field's value,
// but manual fallback suggestions are not.
TEST_F(AddressSuggestionGeneratorTest, GetSuggestionsForProfiles_Filtering) {
  AutofillProfile profile1 = test::GetFullProfile();
  AutofillProfile profile2 = test::GetFullProfile2();
  address_data().AddProfile(profile1);
  address_data().AddProfile(profile2);

  // Create a triggering field those value prefix-matches `profile1`, but not
  // `profile2`.
  FormFieldData triggering_field;
  triggering_field.set_value(profile1.GetRawInfo(NAME_FIRST));
  ASSERT_FALSE(profile2.GetRawInfo(NAME_FIRST)
                   .starts_with(profile1.GetRawInfo(NAME_FIRST)));

  // Expect that regular suggestions filter.
  std::vector<Suggestion> address_suggestions = GetSuggestionsForProfiles(
      *autofill_client(), {NAME_FIRST}, triggering_field, NAME_FIRST,
      SuggestionType::kAddressEntry,
      AutofillSuggestionTriggerSource::kFormControlElementClicked,
      std::nullopt);
  EXPECT_EQ(address_suggestions.size(), 3ul);
  EXPECT_THAT(address_suggestions, ContainsAddressFooterSuggestions());

  // But manual fallback suggestions do not.
  std::vector<Suggestion> manual_fallback_suggestions =
      GetSuggestionsForProfiles(
          *autofill_client(), {NAME_FIRST}, triggering_field, NAME_FIRST,
          SuggestionType::kAddressEntry,
          AutofillSuggestionTriggerSource::kManualFallbackAddress,
          std::nullopt);
  EXPECT_EQ(manual_fallback_suggestions.size(), 4ul);
  EXPECT_THAT(manual_fallback_suggestions, ContainsAddressFooterSuggestions());
}

// Tests that regular suggestions are filtered by the last usage timestamp, but
// manual fallback suggestions are not.
TEST_F(AddressSuggestionGeneratorTest,
       GetProfilesToSuggest_TimestampFiltering) {
  AutofillProfile profile1 = test::GetFullProfile();
  AutofillProfile profile2 = test::GetFullProfile2();
  profile2.set_use_date(AutofillClock::Now() - kDisusedDataModelTimeDelta -
                        base::Days(1));
  address_data().AddProfile(profile1);
  address_data().AddProfile(profile2);

  std::vector<AutofillProfile> profiles_to_suggest =
      GetProfilesToSuggestForTest(
          address_data(), NAME_FIRST, /*field_contents=*/u"",
          /*field_is_autofilled=*/false, {NAME_FIRST},
          AutofillSuggestionTriggerSource::kFormControlElementClicked);
  // Expect that left click (or regular triggering) filters profiles.
  EXPECT_EQ(profiles_to_suggest.size(), 1u);

  std::vector<AutofillProfile> profiles_to_suggest_from_manual_fallback =
      GetProfilesToSuggestForTest(
          address_data(), NAME_FIRST, /*field_contents=*/u"",
          /*field_is_autofilled=*/false, {NAME_FIRST},
          AutofillSuggestionTriggerSource::kManualFallbackAddress);
  // But manual fallback triggering does not.
  EXPECT_EQ(profiles_to_suggest_from_manual_fallback.size(), 2u);
}

#if !BUILDFLAG(IS_IOS)
TEST_F(AddressSuggestionGeneratorTest, UndoAutofillOnAddressForm) {
  address_data().AddProfile(test::GetFullProfile());
  FormFieldData field;
  field.set_is_autofilled(true);
  std::vector<Suggestion> suggestions = GetSuggestionsForProfiles(
      *autofill_client(), {NAME_FIRST}, field, NAME_FIRST,
      SuggestionType::kAddressEntry, kDefaultTriggerSource, std::nullopt);
  EXPECT_THAT(suggestions,
              ElementsAre(EqualsSuggestion(SuggestionType::kAddressEntry),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          EqualsUndoAutofillSuggestion(),
                          EqualsManageAddressesSuggestion()));
}
#endif

TEST_F(AddressSuggestionGeneratorTest,
       TestAddressSuggestion_AddressField_ReturnSuggestion) {
  AutofillProfile profile = test::GetFullProfile();
  autofill_client()->set_test_addresses({profile});
  std::vector<Suggestion> suggestions = GetSuggestionsForProfiles(
      *autofill_client(), /*field_types=*/{NAME_FIRST}, FormFieldData(),
      NAME_FIRST, SuggestionType::kAddressEntry,
      AutofillSuggestionTriggerSource::kManualFallbackAddress, std::nullopt);

  // There should be one `SuggestionType::kDevtoolsTestAddresses`, one
  // `SuggestionType::kSeparator` and one `SuggestionType::kManageAddress`.
  ASSERT_EQ(suggestions.size(), 3u);
  EXPECT_EQ(suggestions[0].type, SuggestionType::kDevtoolsTestAddresses);
  EXPECT_EQ(suggestions[1].type, SuggestionType::kSeparator);
  EXPECT_EQ(suggestions[2].type, SuggestionType::kManageAddress);

  EXPECT_EQ(suggestions[0].main_text.value, u"Developer tools");
  EXPECT_EQ(suggestions[0].icon, Suggestion::Icon::kCode);
  EXPECT_EQ(suggestions[0].children.size(), 3u);
  EXPECT_FALSE(suggestions[0].is_acceptable);

  // The suggestion should have 3 children:
  // 1. Gives users feedback about what the children suggestions mean.
  // 2. Line separator.
  // 3. The actual test address for the US.
  EXPECT_EQ(suggestions[0].children[0].type,
            SuggestionType::kDevtoolsTestAddressByCountry);
  EXPECT_EQ(suggestions[0].children[1].type, SuggestionType::kSeparator);
  const Suggestion& test_address_child = suggestions[0].children.back();
  EXPECT_EQ(test_address_child.main_text.value, u"United States");
  EXPECT_EQ(test_address_child.GetBackendId<Suggestion::Guid>().value(),
            profile.guid());
  EXPECT_EQ(test_address_child.type, SuggestionType::kDevtoolsTestAddressEntry);
}

TEST_F(AddressSuggestionGeneratorTest,
       TestAddressSuggestion_NonAddressField_DoNotReturnSuggestion) {
  base::test::ScopedFeatureList features(
      features::kAutofillForUnclassifiedFieldsAvailable);
  AutofillProfile profile = test::GetFullProfile();
  autofill_client()->set_test_addresses({profile});
  std::vector<Suggestion> suggestions = GetSuggestionsForProfiles(
      *autofill_client(), /*field_types=*/{CREDIT_CARD_NUMBER}, FormFieldData(),
      CREDIT_CARD_NUMBER, SuggestionType::kAddressEntry,
      AutofillSuggestionTriggerSource::kManualFallbackAddress, std::nullopt);

  EXPECT_THAT(suggestions, IsEmpty());
}

}  // namespace
}  // namespace autofill
