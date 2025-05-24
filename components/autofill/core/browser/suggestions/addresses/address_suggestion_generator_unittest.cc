// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/addresses/address_suggestion_generator.h"

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/addresses/test_address_data_manager.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_test_api.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_test_helpers.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
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
using testing::Property;

constexpr char kAddressesSuppressedHistogramName[] =
    "Autofill.AddressesSuppressedForDisuse";

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
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
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

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

// TODO(crbug.com/40100455): Add tests for generating icons for suggestions.
class AddressSuggestionGeneratorTest : public testing::Test {
 public:
  void SetUp() override {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    address_data().SetPrefService(autofill_client_.GetPrefs());
    address_data().SetSyncServiceForTest(&sync_service_);
  }

  TestAddressDataManager& address_data() {
    return autofill_client_.GetPersonalDataManager()
        .test_address_data_manager();
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

// Tests that `SuggestionType::AddressEntryOnTyping` suggestions are returned
// when the triggering field contents matches profile data.
TEST_F(AddressSuggestionGeneratorTest,
       GetSuggestionsOnTypingForProfile_ReturnMatchingSuggestions) {
  AutofillProfile profile_1(i18n_model_definition::kLegacyHierarchyCountryCode);
  AutofillProfile profile_2(i18n_model_definition::kLegacyHierarchyCountryCode);
  AutofillProfile profile_3(i18n_model_definition::kLegacyHierarchyCountryCode);

  profile_1.SetRawInfo(NAME_FULL, u"Jef dean");
  profile_2.SetRawInfo(NAME_FULL, u"Larry page");
  profile_2.SetRawInfo(ADDRESS_HOME_ZIP, u"4398125");
  profile_3.SetRawInfo(NAME_FULL, u"Sundar pichai");

  address_data().AddProfile(profile_1);
  address_data().AddProfile(profile_2);
  address_data().AddProfile(profile_3);

  ASSERT_EQ(address_data().GetProfilesToSuggest().size(), 3u);

  // Expects that no suggestion is returned if the field content matches
  // `NAME_FULL` prefix from the top profile but the field content
  // has only 1 character.
  EXPECT_EQ(GetSuggestionsOnTypingForProfile(address_data(), u"W").size(), 0u);
  // Expects that no suggestion is returned if the field content matches
  // `NAME_FULL` prefix from the top profile but the field content
  // has only 2 characters.
  EXPECT_EQ(GetSuggestionsOnTypingForProfile(address_data(), u"Su").size(), 0u);
  // Expects that suggestions are returned if the field content matches
  // prefix data from the top profile, even when the field content
  // has more than 3 characters. Note that a suggestion for `FIRST_NAME` is not
  // returned because the string value it would fill in the field and the typed
  // data is not large enough.
  EXPECT_THAT(
      GetSuggestionsOnTypingForProfile(address_data(), u"Sund"),
      ElementsAre(EqualsSuggestion(SuggestionType::kAddressEntryOnTyping,
                                   u"Sundar pichai"),
                  EqualsSuggestion(SuggestionType::kSeparator),
                  EqualsSuggestion(SuggestionType::kManageAddress)));
  // Expects that suggestions are returned if the field content matches
  // prefix data from the second profile when the field content
  // has more than 3 characters.
  EXPECT_THAT(
      GetSuggestionsOnTypingForProfile(address_data(), u"Larr"),
      ElementsAre(EqualsSuggestion(SuggestionType::kAddressEntryOnTyping,
                                   u"Larry page"),
                  EqualsSuggestion(SuggestionType::kSeparator),
                  EqualsSuggestion(SuggestionType::kManageAddress)));
  // Expects NO suggestions are returned if the field content matches
  // prefix data from the third profile, even when the field content
  // has more than 3 characters.
  EXPECT_EQ(GetSuggestionsOnTypingForProfile(address_data(), u"Jef").size(),
            0u);
  // Expects that for data that require less prefix matching characters (like
  // `ADDRESS_HOME_ZIP`) only two matching characters are enough to create
  // suggestions.
  EXPECT_THAT(
      GetSuggestionsOnTypingForProfile(address_data(), u"43"),
      ElementsAre(
          EqualsSuggestion(SuggestionType::kAddressEntryOnTyping, u"4398125"),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsSuggestion(SuggestionType::kManageAddress)));
  // However 1 matching digit is not enough to return a suggestion.
  EXPECT_THAT(GetSuggestionsOnTypingForProfile(address_data(), u"4").size(),
              0u);
}

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
  profile.usage_history().set_use_count(5);
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
    profile.usage_history().set_use_count(12);
    profile.usage_history().set_use_date(AutofillClock::Now() - base::Days(1));

    address_data().AddProfile(profile);
    profiles.push_back(profile);
  }

  // Add another profile that matches, but that will get stripped out.
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile, "Marie", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "000 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile.usage_history().set_use_count(1);
  profile.usage_history().set_use_date(AutofillClock::Now() - base::Days(7));
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
  profile3.usage_history().set_use_date(AutofillClock::Now() - base::Days(1));
  profile3.usage_history().set_use_count(5);
  address_data().AddProfile(profile3);

  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile1, "Marion1", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile1.usage_history().set_use_date(AutofillClock::Now() - base::Days(1));
  profile1.usage_history().set_use_count(10);
  address_data().AddProfile(profile1);

  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile2, "Marion2", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile2.usage_history().set_use_date(AutofillClock::Now() - base::Days(15));
  profile2.usage_history().set_use_count(300);
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
  profile1.usage_history().set_use_date(AutofillClock::Now() - base::Days(200));
  profile1.usage_history().set_modification_date(AutofillClock::Now() -
                                                 base::Days(200));
  address_data().AddProfile(profile1);

  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile2, "Marion2", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "456 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile2.usage_history().set_use_date(AutofillClock::Now() - base::Days(20));
  profile2.usage_history().set_modification_date(AutofillClock::Now() -
                                                 base::Days(20));
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
  profile_1.usage_history().set_use_count(10);
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
  profiles[0].usage_history().set_use_count(10);
  address_data().AddProfile(profiles[0]);

  profiles[1].SetRawInfo(NAME_FIRST, u"Bob");
  profiles[1].SetRawInfo(NAME_LAST, u"Parker");
  profiles[1].usage_history().set_use_count(5);
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
    profile.usage_history().set_use_count(
        kMaxDeduplicatedProfilesForSuggestion + 10 - i);
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
  profile_2.usage_history().set_use_count(100);
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

// Tests that disused profiles get removed, but that this doesn't affect the
// first few profiles in the list.
TEST_F(AddressSuggestionGeneratorTest,
       GetProfilesToSuggest_RemoveDisusedProfiles) {
  base::Time kDisusedTime =
      AutofillClock::Now() - kDisusedDataModelTimeDelta - base::Days(1);

  AutofillProfile profile_1 = test::GetFullProfile();
  AutofillProfile profile_2 = test::GetFullProfile2();
  profile_1.usage_history().set_use_count(10);
  profile_1.usage_history().set_modification_date(kDisusedTime);
  profile_1.usage_history().set_use_date(kDisusedTime);
  profile_2.usage_history().set_use_count(1);
  profile_2.usage_history().set_modification_date(kDisusedTime);
  profile_2.usage_history().set_use_date(kDisusedTime);
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

// Tests that disused profiles stop being disused
TEST_F(AddressSuggestionGeneratorTest,
       GetProfilesToSuggest_DisusedProfiles_ResetWhenUpdatingProfiles) {
  base::Time kDisusedTime =
      base::Time::Now() - kDisusedDataModelTimeDelta - base::Days(1);

  AutofillProfile profile_1 = test::GetFullProfile();
  AutofillProfile profile_2 = test::GetFullProfile2();
  profile_1.usage_history().set_modification_date(kDisusedTime);
  profile_1.usage_history().set_use_date(base::Time::Now());
  profile_2.usage_history().set_modification_date(kDisusedTime);
  profile_2.usage_history().set_use_date(kDisusedTime);
  address_data().AddProfile(profile_1);
  address_data().AddProfile(profile_2);

  // Expect to get one hidden profile because `profile_2` is disused.
  ASSERT_EQ(
      GetProfilesToSuggestForTest(address_data(), NAME_FULL, u"",
                                  /*field_is_autofilled=*/false, {NAME_FULL})
          .size(),
      1u);

  // Simulate that `profile_2` was somehow updated (could be either through the
  // settings page or through accepting an update prompt).
  profile_2.SetRawInfo(NAME_FULL, u"Modified Name");
  // This is needed because `TestAddressDataManager::UpdateProfile()` doesn't
  // simulate date modifications. In reality this is done by simply calling
  // `AddressDataManager::UpdateProfile()`.
  profile_2.usage_history().set_modification_date(base::Time::Now());
  address_data().UpdateProfile(profile_2);

  // Expect now to see both profiles since the modification of `profile_2` means
  // that it is a relevant profile.
  EXPECT_EQ(
      GetProfilesToSuggestForTest(address_data(), NAME_FULL, u"",
                                  /*field_is_autofilled=*/false, {NAME_FULL})
          .size(),
      2u);
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
      EMAIL_ADDRESS, /*trigger_field_max_length=*/0, "en-US",
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

// Tests that suggestions are filtered by the triggering field's value.
TEST_F(AddressSuggestionGeneratorTest,
       GetSuggestionsForProfiles_PrefixMatching) {
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

  // Expect that suggestions are prefix matched.
  std::vector<Suggestion> address_suggestions = GetSuggestionsForProfiles(
      *autofill_client(), {NAME_FIRST}, triggering_field, NAME_FIRST,
      SuggestionType::kAddressEntry, std::nullopt);
  EXPECT_EQ(address_suggestions.size(), 3ul);
  EXPECT_THAT(address_suggestions, ContainsAddressFooterSuggestions());
}

// Tests that suggestions are filtered by the triggering field's value.
TEST_F(
    AddressSuggestionGeneratorTest,
    GetSuggestionsForProfiles_RemoveFieldByFieldFillingSuggestionsMatchingFieldContent) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillImproveAddressFieldSwapping};
  AutofillProfile profile1 = test::GetFullProfile();
  AutofillProfile profile2 = test::GetFullProfile2();
  address_data().AddProfile(profile1);
  address_data().AddProfile(profile2);

  // Create a triggering field that was autofilled with `profile1`.
  FormFieldData triggering_field;
  triggering_field.set_value(profile1.GetRawInfo(NAME_FULL));
  triggering_field.set_is_autofilled(true);

  // Expect that only the second address yields a suggestion because the first
  // one would be removed for exactly matching the field's content.
  EXPECT_THAT(
      GetSuggestionsForProfiles(
          *autofill_client(), {NAME_FULL}, triggering_field, NAME_FULL,
          SuggestionType::kAddressFieldByFieldFilling, std::nullopt),
      ElementsAre(EqualsSuggestion(SuggestionType::kAddressFieldByFieldFilling,
                                   profile2.GetRawInfo(NAME_FULL)),
                  EqualsSuggestion(SuggestionType::kSeparator),
                  EqualsSuggestion(SuggestionType::kUndoOrClear),
                  EqualsSuggestion(SuggestionType::kManageAddress)));

  // Remove the second address so that the used-for-filling address becomes the
  // only address in storage.
  address_data().RemoveProfile(profile2.guid());

  // Expect here that the first address yields a suggestion regardless, because
  // otherwise there would be no address suggestions at all and we would not
  // show the popup, making the user unable to use the footer suggestions.
  EXPECT_THAT(
      GetSuggestionsForProfiles(
          *autofill_client(), {NAME_FULL}, triggering_field, NAME_FULL,
          SuggestionType::kAddressFieldByFieldFilling, std::nullopt),
      ElementsAre(EqualsSuggestion(SuggestionType::kAddressFieldByFieldFilling,
                                   profile1.GetRawInfo(NAME_FULL)),
                  EqualsSuggestion(SuggestionType::kSeparator),
                  EqualsSuggestion(SuggestionType::kUndoOrClear),
                  EqualsSuggestion(SuggestionType::kManageAddress)));
}

// Tests that Home/Work icons are correctly assigned.
TEST_F(AddressSuggestionGeneratorTest, TestAddressSuggestion_HomeAndWorkIcons) {
  base::test::ScopedFeatureList features(
      features::kAutofillEnableSupportForHomeAndWork);

  AutofillProfile profile_default = test::GetFullProfile();
  AutofillProfile profile_home = test::GetFullProfile();
  AutofillProfile profile_work = test::GetFullProfile();

  test_api(profile_home)
      .set_record_type(AutofillProfile::RecordType::kAccountHome);
  test_api(profile_work)
      .set_record_type(AutofillProfile::RecordType::kAccountWork);

  std::vector<Suggestion> suggestions = CreateSuggestionsFromProfilesForTest(
      {profile_default, profile_home, profile_work}, {NAME_FIRST, NAME_LAST},
      SuggestionType::kAddressEntry, NAME_FIRST,
      /*trigger_field_max_length=*/0);

  raw_ptr<const base::Feature> kIphFeature =
      &feature_engagement::kIPHAutofillHomeWorkProfileSuggestionFeature;
  EXPECT_THAT(
      suggestions,
      ElementsAre(
          AllOf(HasIcon(Suggestion::Icon::kAccount), HasNoIphFeature()),
          AllOf(HasIcon(Suggestion::Icon::kHome), HasIphFeature(kIphFeature)),
          AllOf(HasIcon(Suggestion::Icon::kWork), HasIphFeature(kIphFeature))));

  suggestions = CreateSuggestionsFromProfilesForTest(
      {profile_default, profile_home, profile_work}, {NAME_FIRST, NAME_LAST},
      SuggestionType::kAddressEntry, EMAIL_ADDRESS,
      /*trigger_field_max_length=*/0);

  // If trigger field is email address, don't show home and work icons.
  EXPECT_THAT(suggestions, Each(AllOf(HasIcon(Suggestion::Icon::kEmail),
                                      HasNoIphFeature())));
}

// Tests that Home/Work icons are not used if the H&W feature is disabled.
TEST_F(AddressSuggestionGeneratorTest,
       TestAddressSuggestion_HomeAndWorkIcons_FeatureDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(
      features::kAutofillEnableSupportForHomeAndWork);

  AutofillProfile profile_default = test::GetFullProfile();
  AutofillProfile profile_home = test::GetFullProfile();
  AutofillProfile profile_work = test::GetFullProfile();

  test_api(profile_home)
      .set_record_type(AutofillProfile::RecordType::kAccountHome);
  test_api(profile_work)
      .set_record_type(AutofillProfile::RecordType::kAccountWork);

  std::vector<Suggestion> suggestions = CreateSuggestionsFromProfilesForTest(
      {profile_default, profile_home, profile_work}, {NAME_FIRST, NAME_LAST},
      SuggestionType::kAddressEntry, NAME_FIRST,
      /*trigger_field_max_length=*/0);

  // Default icons are expected.
  EXPECT_THAT(suggestions, Each(AllOf(HasIcon(Suggestion::Icon::kAccount),
                                      HasNoIphFeature())));
}

#if !BUILDFLAG(IS_IOS)
TEST_F(AddressSuggestionGeneratorTest, UndoAutofillOnAddressForm) {
  address_data().AddProfile(test::GetFullProfile());
  FormFieldData field;
  field.set_is_autofilled(true);
  std::vector<Suggestion> suggestions = GetSuggestionsForProfiles(
      *autofill_client(), {NAME_FIRST}, field, NAME_FIRST,
      SuggestionType::kAddressEntry, std::nullopt);
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
      NAME_FIRST, SuggestionType::kAddressEntry, std::nullopt);

  // There should be one `SuggestionType::kDevtoolsTestAddresses`, one
  // `SuggestionType::kSeparator` and one `SuggestionType::kManageAddress`.
  ASSERT_EQ(suggestions.size(), 3u);
  EXPECT_EQ(suggestions[0].type, SuggestionType::kDevtoolsTestAddresses);
  EXPECT_EQ(suggestions[1].type, SuggestionType::kSeparator);
  EXPECT_EQ(suggestions[2].type, SuggestionType::kManageAddress);

  EXPECT_EQ(suggestions[0].main_text.value, u"Developer tools");
  EXPECT_EQ(suggestions[0].icon, Suggestion::Icon::kCode);
  EXPECT_EQ(suggestions[0].children.size(), 3u);
  EXPECT_FALSE(suggestions[0].IsAcceptable());

  // The suggestion should have 3 children:
  // 1. Gives users feedback about what the children suggestions mean.
  // 2. Line separator.
  // 3. The actual test address for the US.
  EXPECT_EQ(suggestions[0].children[0].type,
            SuggestionType::kDevtoolsTestAddressByCountry);
  EXPECT_EQ(suggestions[0].children[1].type, SuggestionType::kSeparator);
  const Suggestion& test_address_child = suggestions[0].children.back();
  EXPECT_EQ(test_address_child.main_text.value, u"United States");
  EXPECT_EQ(test_address_child.GetPayload<Suggestion::AutofillProfilePayload>()
                .guid.value(),
            profile.guid());
  EXPECT_EQ(test_address_child.type, SuggestionType::kDevtoolsTestAddressEntry);
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Text fixture for label generation related tests. Parameterized by triggering
// field type since how we build labels depends highly on it.
class AddressLabelSuggestionGeneratorTest
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
      features::kAutofillImprovedLabels};
};

INSTANTIATE_TEST_SUITE_P(AddressSuggestionGeneratorTest,
                         AddressLabelSuggestionGeneratorTest,
                         ::testing::ValuesIn({NAME_FULL, ADDRESS_HOME_ZIP,
                                              ADDRESS_HOME_STREET_ADDRESS,
                                              PHONE_HOME_WHOLE_NUMBER}));

TEST_F(AddressLabelSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_PartialNameFieldHasFullNameMainText) {
  base::test::ScopedFeatureList features(features::kAutofillImprovedLabels);
  AutofillProfile profile = test::GetFullProfile();

  EXPECT_THAT(
      CreateSuggestionsFromProfilesForTest({profile}, {NAME_FIRST, NAME_LAST},
                                           SuggestionType::kAddressEntry,
                                           NAME_FIRST,
                                           /*trigger_field_max_length=*/0),
      SuggestionVectorMainTextsAre(Suggestion::Text(
          profile.GetRawInfo(NAME_FULL), Suggestion::Text::IsPrimary(true))));
}

// Tests that suggestions for alternative name fields have the alternative name
// as the main text.
TEST_F(AddressLabelSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_AlternativeNameFieldMainText) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({features::kAutofillImprovedLabels,
                             features::kAutofillSupportPhoneticNameForJP},
                            {});
  AutofillProfile profile(AddressCountryCode("JP"));
  test::SetProfileInfo(&profile, "firstName", "middleName", "lastName",
                       "mail@mail.com", "company", "line1", "line2", "city",
                       "state", "zip", "JP", "phone");
  profile.SetRawInfo(ALTERNATIVE_GIVEN_NAME, u"あおい");
  profile.SetRawInfo(ALTERNATIVE_FAMILY_NAME, u"やまもと");
  profile.FinalizeAfterImport();

  // Suggestions for alternative name fields should have the alternative name
  // as the main text.
  EXPECT_THAT(CreateSuggestionsFromProfilesForTest(
                  {profile}, {ALTERNATIVE_GIVEN_NAME, ALTERNATIVE_FAMILY_NAME},
                  SuggestionType::kAddressEntry, ALTERNATIVE_GIVEN_NAME,
                  /*trigger_field_max_length=*/0),
              SuggestionVectorMainTextsAre(
                  Suggestion::Text(profile.GetRawInfo(ALTERNATIVE_GIVEN_NAME),
                                   Suggestion::Text::IsPrimary(true))));
}

// Suggestions for `ADDRESS_HOME_LINE1` should have `NAME_FULL` as the label.
// Suggestions for name or address fields which do not include
// `ADDRESS_HOME_LINE1` should have `ADDRESS_HOME_LINE1` as the label.
TEST_P(AddressLabelSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_SuggestionsHaveCorrectLabels) {
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
    AddressLabelSuggestionGeneratorTest,
    CreateSuggestionsFromProfiles_SuggestionsNeedMoreLabelsForDifferentiation) {
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
TEST_P(AddressLabelSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_CountryIsChosenAsDifferentiatingLabel) {
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
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace
}  // namespace autofill
