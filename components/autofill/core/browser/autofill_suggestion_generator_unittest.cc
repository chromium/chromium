// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/autofill_granular_filling_utils.h"
#include "components/autofill/core/browser/autofill_suggestion_generator.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/browser/metrics/payments/card_metadata_metrics.h"
#include "components/autofill/core/browser/mock_autofill_optimization_guide.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/personal_data_manager_test_base.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/ui/label_formatter_utils.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/test/test_sync_service.h"
#include "components/webdata/common/web_data_results.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/mock_resource_bundle_delegate.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/resources/grit/ui_resources.h"

using gfx::test::AreImagesEqual;

namespace autofill {

namespace {

using testing::Field;
using testing::IsEmpty;
using testing::Matcher;

constexpr Suggestion::Icon kAddressEntryIcon = Suggestion::Icon::kAccount;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
std::vector<std::vector<Suggestion::Text>> ConstructLabelLineMatrix(
    const std::vector<std::u16string>& parts) {
  return {{Suggestion::Text(ConstructLabelLine(parts))}};
}
#endif

Matcher<Suggestion> EqualsSuggestion(PopupItemId id) {
  return Field(&Suggestion::popup_item_id, id);
}

Matcher<Suggestion> EqualsSuggestion(PopupItemId id,
                                     const std::u16string& main_text) {
  return AllOf(
      Field(&Suggestion::popup_item_id, id),
      Field(&Suggestion::main_text,
            Suggestion::Text(main_text, Suggestion::Text::IsPrimary(true))));
}

Matcher<Suggestion> EqualsFieldByFieldFillingSuggestion(
    PopupItemId id,
    const std::u16string& main_text,
    ServerFieldType field_by_field_filling_type_used,
    const Suggestion::Payload& payload,
    const std::vector<std::vector<Suggestion::Text>>& labels = {}) {
  return AllOf(
      Field(&Suggestion::popup_item_id, id),
      Field(&Suggestion::main_text,
            Suggestion::Text(main_text, Suggestion::Text::IsPrimary(true))),
      Field(&Suggestion::payload, payload),
      Field(&Suggestion::icon, Suggestion::Icon::kNoIcon),
      Field(&Suggestion::field_by_field_filling_type_used,
            std::optional(field_by_field_filling_type_used)),
      Field(&Suggestion::labels, labels));
}

Matcher<Suggestion> EqualsIbanSuggestion(
    const std::u16string& text,
    const Suggestion::Payload& payload,
    const std::u16string& first_label_value) {
  return AllOf(Field(&Suggestion::popup_item_id, PopupItemId::kIbanEntry),
               Field(&Suggestion::main_text,
                     Suggestion::Text(text, Suggestion::Text::IsPrimary(true))),
               Field(&Suggestion::payload, payload),
               Field(&Suggestion::labels,
                     first_label_value.empty()
                         ? std::vector<std::vector<Suggestion::Text>>{}
                         : std::vector<std::vector<Suggestion::Text>>{
                               {Suggestion::Text(first_label_value)}}));
}

}  // namespace

// Test component for tests to access implementation details in
// AutofillSuggestionGenerator.
class TestAutofillSuggestionGenerator : public AutofillSuggestionGenerator {
 public:
  TestAutofillSuggestionGenerator(AutofillClient* autofill_client,
                                  PersonalDataManager* personal_data)
      : AutofillSuggestionGenerator(autofill_client, personal_data) {}

  Suggestion CreateCreditCardSuggestion(
      const CreditCard& credit_card,
      ServerFieldType trigger_field_type,
      bool virtual_card_option,
      bool card_linked_offer_available) const {
    return AutofillSuggestionGenerator::CreateCreditCardSuggestion(
        credit_card, trigger_field_type, virtual_card_option,
        card_linked_offer_available);
  }
};

// TODO(crbug.com/1196021): Move GetSuggestionsForCreditCard tests and
// BrowserAutofillManagerTestForSharingNickname here from
// browser_autofill_manager_unittest.cc.
class AutofillSuggestionGeneratorTest : public testing::Test {
 public:
  void SetUp() override {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    personal_data()->Init(/*profile_database=*/database_,
                          /*account_database=*/nullptr,
                          /*pref_service=*/autofill_client_.GetPrefs(),
                          /*local_state=*/autofill_client_.GetPrefs(),
                          /*identity_manager=*/nullptr,
                          /*history_service=*/nullptr,
                          /*sync_service=*/&sync_service_,
                          /*strike_database=*/nullptr,
                          /*image_fetcher=*/nullptr);
    suggestion_generator_ = std::make_unique<TestAutofillSuggestionGenerator>(
        &autofill_client_, personal_data());
    autofill_client_.set_autofill_offer_manager(
        std::make_unique<AutofillOfferManager>(
            personal_data(),
            /*coupon_service_delegate=*/nullptr, /*shopping_service=*/nullptr));
  }

  void TearDown() override {
    if (did_set_up_image_resource_for_test_) {
      CleanUpIbanImageResources();
      did_set_up_image_resource_for_test_ = false;
    }
  }

  CreditCard CreateServerCard(
      const std::string& guid = "00000000-0000-0000-0000-000000000001",
      const std::string& server_id = "server_id1",
      int instrument_id = 1) {
    CreditCard server_card(CreditCard::RecordType::kMaskedServerCard, "a123");
    test::SetCreditCardInfo(&server_card, "Elvis Presley", "1111" /* Visa */,
                            test::NextMonth().c_str(), test::NextYear().c_str(),
                            "1", /*cvc=*/u"123");
    server_card.SetNetworkForMaskedCard(kVisaCard);
    server_card.set_server_id(server_id);
    server_card.set_guid(guid);
    server_card.set_instrument_id(instrument_id);
    return server_card;
  }

  CreditCard CreateLocalCard(
      const std::string& guid = "00000000-0000-0000-0000-000000000001") {
    CreditCard local_card(guid, test::kEmptyOrigin);
    test::SetCreditCardInfo(&local_card, "Elvis Presley", "4111111111111111",
                            test::NextMonth().c_str(), test::NextYear().c_str(),
                            "1", /*cvc=*/u"123");
    return local_card;
  }

  gfx::Image CustomIconForTest() { return gfx::test::CreateImage(32, 32); }

  void SetUpIbanImageResources() {
    original_resource_bundle_ =
        ui::ResourceBundle::SwapSharedInstanceForTesting(nullptr);
    ui::ResourceBundle::InitSharedInstanceWithLocale(
        "en-US", &mock_resource_delegate_,
        ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
    ON_CALL(mock_resource_delegate_, GetImageNamed(IDR_AUTOFILL_IBAN))
        .WillByDefault(testing::Return(CustomIconForTest()));
    did_set_up_image_resource_for_test_ = true;
  }

  void CleanUpIbanImageResources() {
    ui::ResourceBundle::CleanupSharedInstance();
    ui::ResourceBundle::SwapSharedInstanceForTesting(
        original_resource_bundle_.ExtractAsDangling());
  }

  bool VerifyCardArtImageExpectation(Suggestion& suggestion,
                                     const GURL& expected_url,
                                     const gfx::Image& expected_image) {
#if BUILDFLAG(IS_ANDROID)
    return suggestion.custom_icon_url == expected_url;
#else
    return AreImagesEqual(suggestion.custom_icon, expected_image);
#endif
  }

  TestAutofillSuggestionGenerator* suggestion_generator() {
    return suggestion_generator_.get();
  }

  TestPersonalDataManager* personal_data() {
    return autofill_client_.GetPersonalDataManager();
  }

  const std::string& app_locale() { return personal_data()->app_locale(); }

  TestAutofillClient* autofill_client() { return &autofill_client_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_async_parse_form_{
      features::kAutofillParseAsync};
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::SYSTEM_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClient autofill_client_;
  syncer::TestSyncService sync_service_;
  std::unique_ptr<TestAutofillSuggestionGenerator> suggestion_generator_;
  scoped_refptr<AutofillWebDataService> database_;
  testing::NiceMock<ui::MockResourceBundleDelegate> mock_resource_delegate_;
  raw_ptr<ui::ResourceBundle> original_resource_bundle_;
  // Tracks whether SetUpIbanImageResources() has been called, so that the
  // created images can be cleaned up when the test has finished.
  bool did_set_up_image_resource_for_test_ = false;
};

// Tests that special characters will be used while prefix matching the user's
// field input with the available emails to suggest.
TEST_F(AutofillSuggestionGeneratorTest,
       GetProfilesToSuggest_UseSpecialCharactersInEmail) {
  AutofillProfile profile_1(i18n_model_definition::kLegacyHierarchyCountryCode);
  AutofillProfile profile_2(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile_1.SetRawInfo(EMAIL_ADDRESS, u"test@email.xyz");
  profile_2.SetRawInfo(EMAIL_ADDRESS, u"test1@email.xyz");
  personal_data()->AddProfile(profile_1);
  personal_data()->AddProfile(profile_2);
  ASSERT_EQ(personal_data()->GetProfilesToSuggest().size(), 2u);

  std::vector<const AutofillProfile*> profiles =
      suggestion_generator()->GetProfilesToSuggest(EMAIL_ADDRESS, u"Test@",
                                                   false, {});

  ASSERT_EQ(profiles.size(), 1u);
  EXPECT_EQ(*profiles[0], profile_1);
}

TEST_F(AutofillSuggestionGeneratorTest, GetProfilesToSuggest_HideSubsets) {
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
  personal_data()->AddProfile(profile);
  personal_data()->AddProfile(profile1);
  personal_data()->AddProfile(profile2);
  personal_data()->AddProfile(profile3);

  // Simulate a form with street address, city and state.
  ServerFieldTypeSet types = {ADDRESS_HOME_CITY, ADDRESS_HOME_STATE};
  std::vector<const AutofillProfile*> profiles =
      suggestion_generator()->GetProfilesToSuggest(ADDRESS_HOME_STREET_ADDRESS,
                                                   u"123", false, types);
  ASSERT_EQ(2U, profiles.size());
  EXPECT_EQ(profiles[0]->GetRawInfo(ADDRESS_HOME_STATE), u"CA");
  EXPECT_EQ(profiles[1]->GetRawInfo(ADDRESS_HOME_STATE), u"TX");
}

// Drawing takes noticeable time when there are more than 10 profiles.
// Therefore, we keep only the 10 first suggested profiles.
TEST_F(AutofillSuggestionGeneratorTest, GetProfilesToSuggest_SuggestionsLimit) {
  std::vector<AutofillProfile> profiles;
  for (size_t i = 0;
       i < 2 * AutofillSuggestionGenerator::kMaxUniqueSuggestedProfilesCount;
       i++) {
    AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
    test::SetProfileInfo(&profile, base::StringPrintf("Marion%zu", i).c_str(),
                         "Mitchell", "Morrison", "johnwayne@me.xyz", "Fox",
                         "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                         "Hollywood", "CA", "91601", "US", "12345678910");
    personal_data()->AddProfile(profile);
    profiles.push_back(profile);
  }

  std::vector<const AutofillProfile*> suggested_profiles =
      suggestion_generator()->GetProfilesToSuggest(NAME_FIRST, u"Ma", false,
                                                   {});

  ASSERT_EQ(2 * AutofillSuggestionGenerator::kMaxUniqueSuggestedProfilesCount,
            personal_data()->GetProfiles().size());
  ASSERT_EQ(AutofillSuggestionGenerator::kMaxUniqueSuggestedProfilesCount,
            suggested_profiles.size());
}

// Deduping takes noticeable time when there are more than 50 profiles.
// Therefore, keep only the 50 first pre-dedupe matching profiles.
TEST_F(AutofillSuggestionGeneratorTest, GetProfilesToSuggest_ProfilesLimit) {
  std::vector<AutofillProfile> profiles;
  for (size_t i = 0;
       i < AutofillSuggestionGenerator::kMaxSuggestedProfilesCount; i++) {
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

    personal_data()->AddProfile(profile);
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
  personal_data()->AddProfile(profile);

  std::vector<const AutofillProfile*> suggested_profiles =
      suggestion_generator()->GetProfilesToSuggest(NAME_FIRST, u"Ma", false,
                                                   {});

  ASSERT_EQ(AutofillSuggestionGenerator::kMaxSuggestedProfilesCount + 1,
            personal_data()->GetProfiles().size());
  ASSERT_EQ(1U, suggested_profiles.size());
  EXPECT_EQ(suggested_profiles.front()->GetRawInfo(NAME_FIRST),
            profiles.front().GetRawInfo(NAME_FIRST));
}

// Tests that GetProfilesToSuggest orders its suggestions based on the
// ranking formula.
TEST_F(AutofillSuggestionGeneratorTest, GetProfilesToSuggest_Ranking) {
  // Set up the profiles. They are named with number suffixes X so the X is the
  // order in which they should be ordered by the ranking formula.
  AutofillProfile profile3(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile3, "Marion3", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile3.set_use_date(AutofillClock::Now() - base::Days(1));
  profile3.set_use_count(5);
  personal_data()->AddProfile(profile3);

  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile1, "Marion1", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile1.set_use_date(AutofillClock::Now() - base::Days(1));
  profile1.set_use_count(10);
  personal_data()->AddProfile(profile1);

  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile2, "Marion2", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile2.set_use_date(AutofillClock::Now() - base::Days(15));
  profile2.set_use_count(300);
  personal_data()->AddProfile(profile2);

  std::vector<const AutofillProfile*> suggested_profiles =
      suggestion_generator()->GetProfilesToSuggest(NAME_FIRST, u"Ma", false,
                                                   {});
  ASSERT_EQ(3U, suggested_profiles.size());
  EXPECT_EQ(suggested_profiles[0]->GetRawInfo(NAME_FIRST), u"Marion1");
  EXPECT_EQ(suggested_profiles[1]->GetRawInfo(NAME_FIRST), u"Marion2");
  EXPECT_EQ(suggested_profiles[2]->GetRawInfo(NAME_FIRST), u"Marion3");
}

// Tests that GetProfilesToSuggest returns all profiles suggestions.
TEST_F(AutofillSuggestionGeneratorTest,
       GetProfilesToSuggest_NumberOfSuggestions) {
  // Set up 3 different profiles.
  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile1, "Marion1", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  personal_data()->AddProfile(profile1);

  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile2, "Marion2", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  personal_data()->AddProfile(profile2);

  AutofillProfile profile3(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile3, "Marion3", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  personal_data()->AddProfile(profile3);

  // Verify that all the profiles are suggested.
  std::vector<const AutofillProfile*> suggested_profiles =
      suggestion_generator()->GetProfilesToSuggest(NAME_FIRST, std::u16string(),
                                                   false, {});
  EXPECT_EQ(3U, suggested_profiles.size());
}

// Tests that phone number types are correctly deduplicated for suggestions.
TEST_F(AutofillSuggestionGeneratorTest,
       GetProfilesToSuggest_PhoneNumberDeduplication) {
  // Set up 2 different profiles.
  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile1.SetRawInfo(NAME_FULL, u"First Middle Last");
  profile1.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"+491601234567");
  personal_data()->AddProfile(profile1);

  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile2.SetRawInfo(NAME_FULL, u"First Middle Last");
  profile2.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"+491607654321");
  personal_data()->AddProfile(profile2);

  {
    std::vector<const AutofillProfile*> suggested_profiles =
        suggestion_generator()->GetProfilesToSuggest(
            NAME_FULL, std::u16string(), false,
            {NAME_FULL, PHONE_HOME_WHOLE_NUMBER});
    EXPECT_EQ(2U, suggested_profiles.size());
  }
  {
    std::vector<const AutofillProfile*> suggested_profiles =
        suggestion_generator()->GetProfilesToSuggest(
            NAME_FULL, std::u16string(), false,
            {NAME_FULL, PHONE_HOME_COUNTRY_CODE, PHONE_HOME_CITY_AND_NUMBER});
    EXPECT_EQ(2U, suggested_profiles.size());
  }
  {
    std::vector<const AutofillProfile*> suggested_profiles =
        suggestion_generator()->GetProfilesToSuggest(
            NAME_FULL, std::u16string(), false,
            {NAME_FULL, PHONE_HOME_COUNTRY_CODE, PHONE_HOME_CITY_CODE,
             PHONE_HOME_NUMBER});
    EXPECT_EQ(2U, suggested_profiles.size());
  }
  {
    std::vector<const AutofillProfile*> suggested_profiles =
        suggestion_generator()->GetProfilesToSuggest(
            NAME_FULL, std::u16string(), false,
            {NAME_FULL, PHONE_HOME_COUNTRY_CODE, PHONE_HOME_CITY_CODE});
    EXPECT_EQ(1U, suggested_profiles.size());
  }
}

// Tests that disused profiles are suppressed when suppression is enabled and
// the input field is empty.
TEST_F(AutofillSuggestionGeneratorTest,
       GetProfilesToSuggest_SuppressDisusedProfilesOnEmptyField) {
  // Set up 2 different profiles.
  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile1, "Marion1", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile1.set_use_date(AutofillClock::Now() - base::Days(200));
  personal_data()->AddProfile(profile1);

  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile2, "Marion2", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "456 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile2.set_use_date(AutofillClock::Now() - base::Days(20));
  personal_data()->AddProfile(profile2);

  // Query with empty string only returns profile2.
  {
    std::vector<const AutofillProfile*> suggested_profiles =
        suggestion_generator()->GetProfilesToSuggest(
            ADDRESS_HOME_STREET_ADDRESS, std::u16string(), false, {});
    EXPECT_EQ(1U, suggested_profiles.size());
  }

  // Query with non-alpha-numeric string only returns profile2.
  {
    std::vector<const AutofillProfile*> suggested_profiles =
        suggestion_generator()->GetProfilesToSuggest(
            ADDRESS_HOME_STREET_ADDRESS, u"--", false, {});
    EXPECT_EQ(1U, suggested_profiles.size());
  }

  // Query with prefix for profile1 returns profile1.
  {
    std::vector<const AutofillProfile*> suggested_profiles =
        suggestion_generator()->GetProfilesToSuggest(
            ADDRESS_HOME_STREET_ADDRESS, u"123", false, {});
    ASSERT_EQ(1U, suggested_profiles.size());
    EXPECT_EQ(u"Marion1", suggested_profiles[0]->GetRawInfo(NAME_FIRST));
  }

  // Query with prefix for profile2 returns profile2.
  {
    std::vector<const AutofillProfile*> suggested_profiles =
        suggestion_generator()->GetProfilesToSuggest(
            ADDRESS_HOME_STREET_ADDRESS, u"456", false, {});
    EXPECT_EQ(1U, suggested_profiles.size());
    EXPECT_EQ(u"Marion2", suggested_profiles[0]->GetRawInfo(NAME_FIRST));
  }
}

// Give two suggestions with the same name, and no other field to compare.
// Expect only one unique suggestion.
TEST_F(AutofillSuggestionGeneratorTest, GetProfilesToSuggest_SingleDedupe) {
  AutofillProfile profile_1 = test::GetFullProfile();
  profile_1.set_use_count(10);
  AutofillProfile profile_2 = test::GetFullProfile();
  personal_data()->AddProfile(profile_1);
  personal_data()->AddProfile(profile_2);

  std::vector<const AutofillProfile*> profiles_to_suggest =
      suggestion_generator()->GetProfilesToSuggest(
          NAME_FIRST, u"", /*field_is_autofilled=*/false, {});

  ASSERT_EQ(1U, profiles_to_suggest.size());
}

// Given two suggestions with the same name and one with a different, and also
// last name field to compare, Expect all profiles listed as unique suggestions.
TEST_F(AutofillSuggestionGeneratorTest, GetProfilesToSuggest_MultipleDedupe) {
  std::vector<AutofillProfile> profiles(
      3, AutofillProfile(i18n_model_definition::kLegacyHierarchyCountryCode));
  profiles[0].SetRawInfo(NAME_FIRST, u"Bob");
  profiles[0].SetRawInfo(NAME_LAST, u"Morrison");
  profiles[0].set_use_count(10);
  personal_data()->AddProfile(profiles[0]);

  profiles[1].SetRawInfo(NAME_FIRST, u"Bob");
  profiles[1].SetRawInfo(NAME_LAST, u"Parker");
  profiles[1].set_use_count(5);
  personal_data()->AddProfile(profiles[1]);

  profiles[2].SetRawInfo(NAME_FIRST, u"Mary");
  profiles[2].SetRawInfo(NAME_LAST, u"Parker");
  personal_data()->AddProfile(profiles[2]);

  std::vector<const AutofillProfile*> profiles_to_suggest =
      suggestion_generator()->GetProfilesToSuggest(
          NAME_FIRST, u"", /*field_is_autofilled=*/false,
          {NAME_FIRST, NAME_LAST});

  EXPECT_EQ(3U, profiles_to_suggest.size());
}

// Test the limit of number of deduplicated profiles.
TEST_F(AutofillSuggestionGeneratorTest, GetProfilesToSuggest_DedupeLimit) {
  std::vector<AutofillProfile> profiles;
  for (size_t i = 0;
       i < AutofillSuggestionGenerator::kMaxUniqueSuggestedProfilesCount + 1;
       i++) {
    AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
    profile.SetRawInfo(NAME_FULL,
                       base::UTF8ToUTF16(base::StringPrintf("Bob %zu Doe", i)));
    profile.set_use_count(
        AutofillSuggestionGenerator::kMaxUniqueSuggestedProfilesCount + 10 - i);
    profiles.push_back(profile);
    personal_data()->AddProfile(profile);
  }

  std::vector<const AutofillProfile*> profiles_to_suggest =
      suggestion_generator()->GetProfilesToSuggest(
          NAME_FULL, u"", /*field_is_autofilled=*/false, {NAME_FULL});

  ASSERT_EQ(AutofillSuggestionGenerator::kMaxUniqueSuggestedProfilesCount,
            profiles_to_suggest.size());

  // All profiles are different.
  for (size_t i = 0; i < profiles_to_suggest.size(); i++) {
    EXPECT_EQ(profiles_to_suggest[i]->guid(), profiles[i].guid()) << i;
  }
}

TEST_F(AutofillSuggestionGeneratorTest,
       GetProfilesToSuggest_EmptyMatchingProfiles) {
  ASSERT_EQ(0U, suggestion_generator()
                    ->GetProfilesToSuggest(NAME_FIRST, u"",
                                           /*field_is_autofilled=*/false, {})
                    .size());
}

// Tests that `kAccount` profiles are preferred over `kLocalOrSyncable` profile
// in case of a duplicate.
TEST_F(AutofillSuggestionGeneratorTest,
       GetProfilesToSuggest_kAccountPrecedence) {
  // Create two profiles that only differ by their source.
  AutofillProfile profile_1(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile_1.SetRawInfo(NAME_FULL, u"First Last");
  profile_1.set_source_for_testing(AutofillProfile::Source::kAccount);
  personal_data()->AddProfile(profile_1);

  AutofillProfile profile_2(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile_2.SetRawInfo(NAME_FULL, u"First Last");
  profile_2.set_source_for_testing(AutofillProfile::Source::kLocalOrSyncable);
  // Set high use count for profile 2 so that it has greater ranking than
  // profile_1
  profile_2.set_use_count(100);
  personal_data()->AddProfile(profile_2);

  std::vector<const AutofillProfile*> profiles_to_suggest =
      suggestion_generator()->GetProfilesToSuggest(
          NAME_FULL, u"", /*field_is_autofilled=*/false, {NAME_FULL});

  ASSERT_EQ(1u, profiles_to_suggest.size());
  EXPECT_EQ(profile_1.guid(), profiles_to_suggest[0]->guid());
  EXPECT_EQ(AutofillProfile::Source::kAccount,
            profiles_to_suggest[0]->source());
}

TEST_F(AutofillSuggestionGeneratorTest,
       GetProfilesToSuggest_GetMatchingProfile) {
  AutofillProfile marion_profile(
      i18n_model_definition::kLegacyHierarchyCountryCode);
  marion_profile.SetRawInfo(NAME_FIRST, u"Marion");
  personal_data()->AddProfile(marion_profile);

  AutofillProfile bob_profile(
      i18n_model_definition::kLegacyHierarchyCountryCode);
  bob_profile.SetRawInfo(NAME_FIRST, u"Bob");
  personal_data()->AddProfile(bob_profile);

  std::vector<const AutofillProfile*> profiles_to_suggest =
      suggestion_generator()->GetProfilesToSuggest(
          NAME_FIRST, u"Mar", /*field_is_autofilled=*/false, {});

  ASSERT_EQ(1U, profiles_to_suggest.size());
  EXPECT_EQ(marion_profile.guid(), profiles_to_suggest[0]->guid());
}

TEST_F(AutofillSuggestionGeneratorTest,
       GetProfilesToSuggest_NoMatchingProfile) {
  AutofillProfile bob_profile(
      i18n_model_definition::kLegacyHierarchyCountryCode);
  bob_profile.SetRawInfo(NAME_FIRST, u"Bob");
  personal_data()->AddProfile(bob_profile);

  std::vector<const AutofillProfile*> profiles_to_suggest =
      suggestion_generator()->GetProfilesToSuggest(
          NAME_FIRST, u"Mar", /*field_is_autofilled=*/false, {});

  ASSERT_TRUE(profiles_to_suggest.empty());
}

TEST_F(AutofillSuggestionGeneratorTest,
       GetProfilesToSuggest_EmptyProfilesInput) {
  std::vector<const AutofillProfile*> profiles_to_suggest =
      suggestion_generator()->GetProfilesToSuggest(
          NAME_FIRST, u"Mar", /*field_is_autofilled=*/false, {});

  ASSERT_TRUE(profiles_to_suggest.empty());
}

// Tests that disused profiles get removed.
TEST_F(AutofillSuggestionGeneratorTest,
       GetProfilesToSuggest_RemoveProfilesNotUsedSinceTimestamp) {
  const char kAddressesSuppressedHistogramName[] =
      "Autofill.AddressesSuppressedForDisuse";
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
    personal_data()->AddProfile(profiles[i]);
  }

  // Filter the profiles while capturing histograms.
  base::HistogramTester histogram_tester;
  std::vector<const AutofillProfile*> profiles_to_suggest =
      suggestion_generator()->GetProfilesToSuggest(
          NAME_FULL, u"",
          /*field_is_autofilled=*/false, {NAME_FULL});

  // Validate that we get the expected filtered profiles and histograms.
  ASSERT_EQ(kNbSuggestions, profiles_to_suggest.size());
  for (size_t i = 0; i < kNbSuggestions; ++i) {
    EXPECT_EQ(profiles[i].guid(), profiles_to_suggest[i]->guid()) << i;
  }
  histogram_tester.ExpectTotalCount(kAddressesSuppressedHistogramName, 1);
  histogram_tester.ExpectBucketCount(kAddressesSuppressedHistogramName,
                                     kNumProfiles - kNbSuggestions, 1);
}

TEST_F(AutofillSuggestionGeneratorTest, CreateSuggestionsFromProfiles) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");

  std::vector<Suggestion> suggestions =
      suggestion_generator()->CreateSuggestionsFromProfiles(
          {&profile}, {ADDRESS_HOME_STREET_ADDRESS},
          /*last_targeted_fields=*/absl::nullopt, ADDRESS_HOME_STREET_ADDRESS,
          /*trigger_field_max_length=*/0);
  ASSERT_FALSE(suggestions.empty());
  EXPECT_EQ(u"123 Zoo St., Second Line, Third line, unit 5",
            suggestions[0].main_text.value);
}

TEST_F(AutofillSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_PhoneSubstring_NoImprovedDisambiguation) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndDisableFeature(
      features::kAutofillUseImprovedLabelDisambiguation);

  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");

  std::vector<Suggestion> suggestions =
      suggestion_generator()->CreateSuggestionsFromProfiles(
          {&profile}, {PHONE_HOME_WHOLE_NUMBER},
          /*last_targeted_fields=*/absl::nullopt, PHONE_HOME_WHOLE_NUMBER,
          /*trigger_field_max_length=*/0);
  ASSERT_FALSE(suggestions.empty());
  EXPECT_EQ(u"12345678910", suggestions[0].main_text.value);
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(AutofillSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_LogProfileSuggestionsMadeWithFormatter) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile, "Hoa", "", "Pham", "hoa.pham@comcast.net", "",
                       "401 Merrimack St", "", "Lowell", "MA", "01852", "US",
                       "19786744120");

  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(
      features::kAutofillUseImprovedLabelDisambiguation);

  base::HistogramTester histogram_tester;
  EXPECT_THAT(
      suggestion_generator()->CreateSuggestionsFromProfiles(
          {&profile},
          {NAME_FIRST, NAME_LAST, EMAIL_ADDRESS, PHONE_HOME_WHOLE_NUMBER},
          /*last_targeted_fields=*/absl::nullopt, NAME_FIRST,
          /*trigger_field_max_length=*/0),
      ElementsAre(testing::Field(
          &Suggestion::main_text,
          Suggestion::Text(u"Hoa", Suggestion::Text::IsPrimary(true)))));
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileSuggestionsMadeWithFormatter", true, 1);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(AutofillSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_ForContactForm) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile, "Hoa", "", "Pham", "hoa.pham@comcast.net", "",
                       "401 Merrimack St", "", "Lowell", "MA", "01852", "US",
                       "19786744120");

  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(
      features::kAutofillUseImprovedLabelDisambiguation);

  EXPECT_THAT(
      suggestion_generator()->CreateSuggestionsFromProfiles(
          {&profile},
          {NAME_FIRST, NAME_LAST, EMAIL_ADDRESS, PHONE_HOME_WHOLE_NUMBER},
          /*last_targeted_fields=*/absl::nullopt, NAME_FIRST,
          /*trigger_field_max_length=*/0),
      ElementsAre(AllOf(
          testing::Field(&Suggestion::labels,
                         ConstructLabelLineMatrix(
                             {u"(978) 674-4120", u"hoa.pham@comcast.net"})),
          testing::Field(&Suggestion::icon, kAddressEntryIcon))));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(AutofillSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_AddressForm) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile, "Hoa", "", "Pham", "hoa.pham@comcast.net", "",
                       "401 Merrimack St", "", "Lowell", "MA", "01852", "US",
                       "19786744120");

  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(
      features::kAutofillUseImprovedLabelDisambiguation);

  EXPECT_THAT(suggestion_generator()->CreateSuggestionsFromProfiles(
                  {&profile},
                  {NAME_FULL, ADDRESS_HOME_STREET_ADDRESS, ADDRESS_HOME_CITY,
                   ADDRESS_HOME_STATE, ADDRESS_HOME_ZIP},
                  /*last_targeted_fields=*/absl::nullopt, NAME_FULL,
                  /*trigger_field_max_length=*/0),
              ElementsAre(AllOf(
                  testing::Field(&Suggestion::labels,
                                 ConstructLabelLineMatrix(
                                     {u"401 Merrimack St, Lowell, MA 01852"})),
                  testing::Field(&Suggestion::icon, kAddressEntryIcon))));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(AutofillSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_AddressPhoneForm) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile, "Hoa", "", "Pham", "hoa.pham@comcast.net", "",
                       "401 Merrimack St", "", "Lowell", "MA", "01852", "US",
                       "19786744120");

  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(
      features::kAutofillUseImprovedLabelDisambiguation);

  EXPECT_THAT(
      suggestion_generator()->CreateSuggestionsFromProfiles(
          {&profile},
          {NAME_FULL, ADDRESS_HOME_STREET_ADDRESS, PHONE_HOME_WHOLE_NUMBER},
          /*last_targeted_fields=*/absl::nullopt, NAME_FULL,
          /*trigger_field_max_length=*/0),
      ElementsAre(
          AllOf(testing::Field(&Suggestion::labels,
                               ConstructLabelLineMatrix(
                                   {u"(978) 674-4120", u"401 Merrimack St"})),
                testing::Field(&Suggestion::icon, kAddressEntryIcon))));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(AutofillSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_AddressEmailForm) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile, "Hoa", "", "Pham", "hoa.pham@comcast.net", "",
                       "401 Merrimack St", "", "Lowell", "MA", "01852", "US",
                       "19786744120");

  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(
      features::kAutofillUseImprovedLabelDisambiguation);

  EXPECT_THAT(
      suggestion_generator()->CreateSuggestionsFromProfiles(
          {&profile}, {NAME_FULL, ADDRESS_HOME_STREET_ADDRESS, EMAIL_ADDRESS},
          /*last_targeted_fields=*/absl::nullopt, NAME_FULL,
          /*trigger_field_max_length=*/0),
      ElementsAre(AllOf(
          testing::Field(&Suggestion::labels,
                         ConstructLabelLineMatrix(
                             {u"401 Merrimack St", u"hoa.pham@comcast.net"})),
          testing::Field(&Suggestion::icon, kAddressEntryIcon))));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(AutofillSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_FormWithOneProfile) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile, "Hoa", "", "Pham", "hoa.pham@comcast.net", "",
                       "401 Merrimack St", "", "Lowell", "MA", "01852", "US",
                       "19786744120");

  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(
      features::kAutofillUseImprovedLabelDisambiguation);

  EXPECT_THAT(
      suggestion_generator()->CreateSuggestionsFromProfiles(
          {&profile},
          {NAME_FULL, ADDRESS_HOME_STREET_ADDRESS, EMAIL_ADDRESS,
           PHONE_HOME_WHOLE_NUMBER},
          /*last_targeted_fields=*/absl::nullopt, NAME_FULL,
          /*trigger_field_max_length=*/0),
      ElementsAre(
          AllOf(testing::Field(&Suggestion::labels,
                               ConstructLabelLineMatrix({u"401 Merrimack St"})),
                testing::Field(&Suggestion::icon, kAddressEntryIcon))));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(AutofillSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_AddressContactFormWithProfiles) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitWithFeatures(
      /*enabled_features=*/{features::
                                kAutofillEnableRankingFormulaAddressProfiles,
                            features::kAutofillUseImprovedLabelDisambiguation},
      /*disabled_features=*/{});

  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile1, "Hoa", "", "Pham", "hoa.pham@comcast.net", "",
                       "401 Merrimack St", "", "Lowell", "MA", "01852", "US",
                       "19786744120");

  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile2, "Hoa", "", "Pham", "hp@aol.com", "",
                       "216 Broadway St", "", "Lowell", "MA", "01854", "US",
                       "19784523366");

  EXPECT_THAT(
      suggestion_generator()->CreateSuggestionsFromProfiles(
          {&profile1, &profile2},
          {NAME_FULL, ADDRESS_HOME_STREET_ADDRESS, EMAIL_ADDRESS,
           PHONE_HOME_WHOLE_NUMBER},
          /*last_targeted_fields=*/absl::nullopt, NAME_FULL,
          /*trigger_field_max_length=*/0),
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
TEST_F(AutofillSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_MobileShowOne) {
  std::map<std::string, std::string> parameters;
  parameters[features::kAutofillUseMobileLabelDisambiguationParameterName] =
      features::kAutofillUseMobileLabelDisambiguationParameterShowOne;
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeatureWithParameters(
      features::kAutofillUseMobileLabelDisambiguation, parameters);

  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile1, "Hoa", "", "Pham", "hoa.pham@comcast.net", "",
                       "401 Merrimack St", "", "Lowell", "MA", "01852", "US",
                       "19786744120");
  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile2, "María", "", "Lòpez", "maria@aol.com", "",
                       "11 Elkins St", "", "Boston", "MA", "02127", "US",
                       "6172686862");

  // Tests a form with name, email address, and phone number fields.
  EXPECT_THAT(
      suggestion_generator()->CreateSuggestionsFromProfiles(
          {&profile1, &profile2},
          {NAME_FIRST, NAME_LAST, EMAIL_ADDRESS, PHONE_HOME_WHOLE_NUMBER},
          /*last_targeted_fields=*/absl::nullopt, EMAIL_ADDRESS,
          /*trigger_field_max_length=*/0),
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
      suggestion_generator()->CreateSuggestionsFromProfiles(
          {&profile1, &profile2},
          {NAME_FULL, ADDRESS_HOME_STREET_ADDRESS, ADDRESS_HOME_CITY,
           EMAIL_ADDRESS, PHONE_HOME_WHOLE_NUMBER},
          /*last_targeted_fields=*/absl::nullopt, EMAIL_ADDRESS,
          /*trigger_field_max_length=*/0),
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
TEST_F(AutofillSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_MobileShowAll) {
  std::map<std::string, std::string> parameters;
  parameters[features::kAutofillUseMobileLabelDisambiguationParameterName] =
      features::kAutofillUseMobileLabelDisambiguationParameterShowAll;
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeatureWithParameters(
      features::kAutofillUseMobileLabelDisambiguation, parameters);

  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile1, "Hoa", "", "Pham", "hoa.pham@comcast.net", "",
                       "401 Merrimack St", "", "Lowell", "MA", "01852", "US",
                       "19786744120");
  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile2, "María", "", "Lòpez", "maria@aol.com", "",
                       "11 Elkins St", "", "Boston", "MA", "02127", "US",
                       "6172686862");

  // Tests a form with name, email address, and phone number fields.
  EXPECT_THAT(
      suggestion_generator()->CreateSuggestionsFromProfiles(
          {&profile1, &profile2},
          {NAME_FIRST, NAME_LAST, EMAIL_ADDRESS, PHONE_HOME_WHOLE_NUMBER},
          /*last_targeted_fields=*/absl::nullopt, EMAIL_ADDRESS,
          /*trigger_field_max_length=*/0),
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
      suggestion_generator()->CreateSuggestionsFromProfiles(
          {&profile1, &profile2},
          {NAME_FULL, ADDRESS_HOME_STREET_ADDRESS, ADDRESS_HOME_CITY,
           EMAIL_ADDRESS, PHONE_HOME_WHOLE_NUMBER},
          /*last_targeted_fields=*/absl::nullopt, EMAIL_ADDRESS,
          /*trigger_field_max_length=*/0),
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

class AutofillChildrenSuggestionsGenenarationTest
    : public AutofillSuggestionGeneratorTest {
 public:
  std::vector<Suggestion> CreateSuggestionWithChildrenFromProfile(
      const AutofillProfile& profile,
      absl::optional<ServerFieldTypeSet> last_targeted_fields,
      ServerFieldType trigger_field_type,
      const ServerFieldTypeSet& field_types) {
    return suggestion_generator()->CreateSuggestionsFromProfiles(
        {&profile}, field_types, last_targeted_fields, trigger_field_type,
        /*trigger_field_max_length=*/0);
  }

  std::vector<Suggestion> CreateSuggestionWithChildrenFromProfile(
      const AutofillProfile& profile,
      absl::optional<ServerFieldTypeSet> last_targeted_fields,
      ServerFieldType trigger_field_type) {
    return CreateSuggestionWithChildrenFromProfile(
        profile, last_targeted_fields, trigger_field_type,
        {trigger_field_type});
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
  // The default profile used to generate suggestions.
  const AutofillProfile profile_ = test::GetFullProfile();
};

// Test that the differentiating label is added when the suggestion main text
// and granular filling label are not unique across suggestions.
TEST_F(
    AutofillChildrenSuggestionsGenenarationTest,
    CreateSuggestionsFromProfiles_GroupFillingLabels_AddFillAddressAndDifferentiatingLabel) {
  AutofillProfile profile_1 = test::GetFullProfile();
  profile_1.SetRawInfo(ADDRESS_HOME_ZIP, u"1234");

  AutofillProfile profile_2 = test::GetFullProfile();
  profile_2.SetRawInfo(ADDRESS_HOME_ZIP, u"4321");

  // `profile_1` and `profile_2` have the same `ADDRESS_HOME_LINE1`, which
  // will lead to the necessity of a differentiating label (`ADDRESS_HOME_ZIP`).
  std::vector<Suggestion> suggestions =
      suggestion_generator()->CreateSuggestionsFromProfiles(
          {&profile_1, &profile_2}, {ADDRESS_HOME_LINE1, ADDRESS_HOME_ZIP},
          GetServerFieldTypesOfGroup(FieldTypeGroup::kName), ADDRESS_HOME_LINE1,
          /*trigger_field_max_length=*/0);

  ASSERT_EQ(suggestions.size(), 2u);
  EXPECT_EQ(suggestions[0].labels,
            std::vector<std::vector<Suggestion::Text>>(
                {{Suggestion::Text(u"Fill full address"),
                  Suggestion::Text(u"-"), Suggestion::Text(u"1234")}}));
}

TEST_F(AutofillChildrenSuggestionsGenenarationTest,
       CreateSuggestionsFromProfiles_GroupFillingLabels_AddOnlyFillAddress) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(),
      /*last_targeted_fields=*/
      GetServerFieldTypesOfGroup(FieldTypeGroup::kName), ADDRESS_HOME_ADDRESS,
      /*field_types=*/{NAME_FIRST});

  ASSERT_EQ(suggestions.size(), 1u);
  EXPECT_EQ(suggestions[0].labels,
            std::vector<std::vector<Suggestion::Text>>(
                {{Suggestion::Text(u"Fill full address")}}));
}

// When there is no differentiating label, we add only the granular filling
// label, either "Fill full name" or "Fill address".
TEST_F(AutofillChildrenSuggestionsGenenarationTest,
       CreateSuggestionsFromProfiles_GroupFillingLabels_AddOnlyFillName) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(),
      /*last_targeted_fields=*/
      GetServerFieldTypesOfGroup(FieldTypeGroup::kName), NAME_FIRST);

  ASSERT_EQ(suggestions.size(), 1u);
  EXPECT_EQ(suggestions[0].labels,
            std::vector<std::vector<Suggestion::Text>>(
                {{Suggestion::Text(u"Fill full name")}}));
}

// Test that the differentiating label is added when the suggestion main text
// and granular filling label are not unique across suggestions.
TEST_F(
    AutofillChildrenSuggestionsGenenarationTest,
    CreateSuggestionsFromProfiles_GroupFillingLabels_AddFillNameAndDifferentiatingLabel) {
  AutofillProfile profile_1 = test::GetFullProfile();
  profile_1.SetRawInfo(NAME_FULL, u"Cersei Lannister");

  AutofillProfile profile_2 = test::GetFullProfile();
  profile_2.SetRawInfo(NAME_FULL, u"Cersei Baratheon");

  // `profile_1` and `profile_2` have the same `NAME_FIRST`, which will lead to
  // the necessity of a differentiating label (`NAME_FULL`).
  std::vector<Suggestion> suggestions =
      suggestion_generator()->CreateSuggestionsFromProfiles(
          {&profile_1, &profile_2}, {NAME_FIRST, NAME_MIDDLE},
          GetServerFieldTypesOfGroup(FieldTypeGroup::kName), NAME_FIRST,
          /*trigger_field_max_length=*/0);

  ASSERT_EQ(suggestions.size(), 2u);
  EXPECT_EQ(suggestions[0].labels,
            std::vector<std::vector<Suggestion::Text>>(
                {{Suggestion::Text(u"Fill full name"), Suggestion::Text(u"-"),
                  Suggestion::Text(u"Cersei Lannister")}}));
}

TEST_F(AutofillChildrenSuggestionsGenenarationTest,
       CreateSuggestionsFromProfiles_FirstLevelChildrenSuggestions) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(),
      /*last_targeted_fields=*/kAllServerFieldTypes, NAME_FIRST);

  ASSERT_EQ(1U, suggestions.size());
  // Test root suggestion
  EXPECT_THAT(suggestions,
              ElementsAre(Field(
                  &Suggestion::main_text,
                  Suggestion::Text(profile().GetInfo(NAME_FIRST, app_locale()),
                                   Suggestion::Text::IsPrimary(true)))));

  // The children suggestions should be.
  //
  // 1. fill full name
  // 2. first name
  // 3. middle name
  // 4. family name
  // 5. line separator
  // 6. address line 1
  // 7. address line 2
  // 8. Zip
  // 9. line separator
  // 10. phone number
  // 11. email
  // 12. line separator
  // 13. edit profile
  // 14. delete address
  ASSERT_EQ(14U, suggestions[0].children.size());
  EXPECT_THAT(
      suggestions[0].children,
      ElementsAre(
          EqualsSuggestion(PopupItemId::kFillFullName),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kFieldByFieldFilling,
              profile().GetInfo(NAME_FIRST, app_locale()), NAME_FIRST,
              Suggestion::Guid(profile().guid())),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kFieldByFieldFilling,
              profile().GetInfo(NAME_MIDDLE, app_locale()), NAME_MIDDLE,
              Suggestion::Guid(profile().guid())),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kFieldByFieldFilling,
              profile().GetInfo(NAME_LAST, app_locale()), NAME_LAST,
              Suggestion::Guid(profile().guid())),
          EqualsSuggestion(PopupItemId::kSeparator),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kFieldByFieldFilling,
              profile().GetInfo(ADDRESS_HOME_LINE1, app_locale()),
              ADDRESS_HOME_LINE1, Suggestion::Guid(profile().guid())),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kFieldByFieldFilling,
              profile().GetInfo(ADDRESS_HOME_LINE2, app_locale()),
              ADDRESS_HOME_LINE2, Suggestion::Guid(profile().guid())),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kFieldByFieldFilling,
              profile().GetInfo(ADDRESS_HOME_ZIP, app_locale()),
              ADDRESS_HOME_ZIP, Suggestion::Guid(profile().guid())),
          EqualsSuggestion(PopupItemId::kSeparator),
          // Triggering field is not a phone number, international phone number
          // should be shown to the user.
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kFieldByFieldFilling,
              GetFormattedInternationalNumber(), PHONE_HOME_WHOLE_NUMBER,
              Suggestion::Guid(profile().guid())),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kFieldByFieldFilling,
              profile().GetInfo(EMAIL_ADDRESS, app_locale()), EMAIL_ADDRESS,
              Suggestion::Guid(profile().guid())),
          EqualsSuggestion(PopupItemId::kSeparator),
          EqualsSuggestion(PopupItemId::kEditAddressProfile),
          EqualsSuggestion(PopupItemId::kDeleteAddressProfile)));
}

TEST_F(AutofillChildrenSuggestionsGenenarationTest,
       CreateSuggestionsFromProfiles_SecondLevelChildrenSuggestions) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(),
      /*last_targeted_fields=*/absl::nullopt, NAME_FIRST);

  ASSERT_EQ(1U, suggestions.size());
  // Suggestions should have two levels of children, The address line 1 (sixth
  // child) suggestion should have the following children: house number street
  // name.
  ASSERT_EQ(2U, suggestions[0].children[5].children.size());
  EXPECT_THAT(
      suggestions[0].children[5].children,
      ElementsAre(
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kFieldByFieldFilling,
              profile().GetInfo(ADDRESS_HOME_HOUSE_NUMBER, app_locale()),
              ADDRESS_HOME_HOUSE_NUMBER, Suggestion::Guid(profile().guid()),
              {{Suggestion::Text(u"Building number")}}),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kFieldByFieldFilling,
              profile().GetInfo(ADDRESS_HOME_STREET_NAME, app_locale()),
              ADDRESS_HOME_STREET_NAME, Suggestion::Guid(profile().guid()),
              {{Suggestion::Text(u"Street")}})));
}

TEST_F(
    AutofillChildrenSuggestionsGenenarationTest,
    CreateSuggestionsFromProfiles_LastTargetedFieldsIsSingleField_FieldByFieldFilling) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), absl::optional<ServerFieldTypeSet>({NAME_LAST}), NAME_FIRST);

  ASSERT_EQ(suggestions.size(), 1u);
  EXPECT_THAT(suggestions[0],
              EqualsFieldByFieldFillingSuggestion(
                  PopupItemId::kFieldByFieldFilling,
                  profile().GetInfo(NAME_FIRST, app_locale()), NAME_FIRST,
                  Suggestion::Guid(profile().guid()), {{}}));
}

TEST_F(AutofillChildrenSuggestionsGenenarationTest,
       CreateSuggestionsFromProfiles_LastTargetedFieldsIsGroup_GroupFilling) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(),
      absl::optional<ServerFieldTypeSet>(GetAddressFieldsForGroupFilling()),
      NAME_FIRST, {NAME_FIRST, NAME_LAST});

  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ(suggestions[0].popup_item_id, PopupItemId::kFillFullName);
  EXPECT_EQ(suggestions[0].icon, Suggestion::Icon::kNoIcon);
}

// Note that only full form filling has an icon.
TEST_F(
    AutofillChildrenSuggestionsGenenarationTest,
    CreateSuggestionsFromProfiles_LastTargetedFieldsAreAllServerFields_FullForm) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), kAllServerFieldTypes, NAME_FIRST, {NAME_FIRST, NAME_LAST});

  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ(suggestions[0].popup_item_id, PopupItemId::kAddressEntry);
  EXPECT_EQ(suggestions[0].icon, Suggestion::Icon::kLocation);
}

// Fallback to full form (PopupItemId::kAddressEntry) when the last targeted
// fields are a group but the triggering field does not match any group.
TEST_F(AutofillChildrenSuggestionsGenenarationTest,
       CreateSuggestionsFromProfiles_LastTargetedFieldsAreGroup_Fallback) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), kAllServerFieldTypes, CREDIT_CARD_TYPE);
  EXPECT_EQ(suggestions[0].popup_item_id, PopupItemId::kAddressEntry);
}

// Asserts that when the triggering field is a phone field, the phone number
// suggestion is of type `PopupItemId::kFillFullPhoneNumber`. In other
// scenarios, phone number is of type `PopupItemId::kFieldByFieldFilling` as the
// user expressed intent to use their phone number their phone number on a
// "random" field.
TEST_F(
    AutofillChildrenSuggestionsGenenarationTest,
    CreateSuggestionsFromProfiles_ChildrenSuggestionsPhoneField_Intenational) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), kAllServerFieldTypes, PHONE_HOME_WHOLE_NUMBER);

  ASSERT_EQ(1U, suggestions.size());
  // The child suggestions should be:
  //
  // 1. first name
  // 2. middle name
  // 3. family name
  // 4. line separator
  // 5. address line 1
  // 6. address line 2
  // 7. Zip
  // 8. line separator
  // 9. phone number
  // 10. email
  // 11. line separator
  // 12. edit profile
  // 13. delete address
  ASSERT_EQ(13U, suggestions[0].children.size());

  // Triggering field is international phone number type, international phone
  // number should be shown to the user.
  EXPECT_THAT(suggestions[0].children[8],
              EqualsSuggestion(PopupItemId::kFillFullPhoneNumber,
                               GetFormattedInternationalNumber()));
  EXPECT_THAT(suggestions[0].children[8].children, IsEmpty());
}

// Asserts that when the triggering field is a phone field, the phone number
// suggestion is of type `PopupItemId::kFillFullPhoneNumber`. In other
// scenarios, phone number is of type `PopupItemId::kFieldByFieldFilling` as the
// user expressed intent to use their phone number on a "random" field.
TEST_F(
    AutofillChildrenSuggestionsGenenarationTest,
    CreateSuggestionsFromProfiles_ChildrenSuggestionsPhoneField_CountryCode) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), kAllServerFieldTypes, PHONE_HOME_COUNTRY_CODE);

  ASSERT_EQ(1U, suggestions.size());
  // The child suggestions should be:
  //
  // 1. first name
  // 2. middle name
  // 3. family name
  // 4. line separator
  // 5. address line 1
  // 6. address line 2
  // 7. Zip
  // 8. line separator
  // 9. phone number
  // 10. email
  // 11. line separator
  // 12. edit profile
  // 13. delete address
  ASSERT_EQ(13U, suggestions[0].children.size());

  // Triggering field is phone number country code, international phone number
  // should be shown to the user.
  EXPECT_THAT(suggestions[0].children[8],
              EqualsSuggestion(PopupItemId::kFillFullPhoneNumber,
                               GetFormattedInternationalNumber()));
  EXPECT_THAT(suggestions[0].children[8].children, IsEmpty());
}

// Asserts that when the triggering field is a phone field, the phone number
// suggestion is of type `PopupItemId::kFillFullPhoneNumber`. In other
// scenarios, phone number is of type `PopupItemId::kFieldByFieldFilling` as the
// user expressed intent to use their phone number their phone number on a
// "random" field.
TEST_F(AutofillChildrenSuggestionsGenenarationTest,
       CreateSuggestionsFromProfiles_ChildrenSuggestionsPhoneField_Local) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), kAllServerFieldTypes, PHONE_HOME_CITY_AND_NUMBER);

  ASSERT_EQ(1U, suggestions.size());
  // The child suggestions should be:
  //
  // 1. first name
  // 2. middle name
  // 3. family name
  // 4. line separator
  // 5. address line 1
  // 6. address line 2
  // 7. Zip
  // 8. line separator
  // 9. phone number
  // 10. email
  // 11. line separator
  // 12. edit profile
  // 13. delete address
  ASSERT_EQ(13U, suggestions[0].children.size());
  // Triggering field is local phone number type, local phone number should
  // be shown to the user.
  EXPECT_THAT(suggestions[0].children[8],
              EqualsSuggestion(PopupItemId::kFillFullPhoneNumber,
                               GetFormattedNationalNumber()));
  EXPECT_THAT(suggestions[0].children[8].children, IsEmpty());
}

// Same as above but for email fields.
TEST_F(AutofillChildrenSuggestionsGenenarationTest,
       CreateSuggestionsFromProfiles_ChildrenSuggestionsEmailField) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), kAllServerFieldTypes, EMAIL_ADDRESS);

  // The child suggestions should be:
  //
  // 1. first name
  // 2. middle name
  // 3. family name
  // 4. line separator
  // 5. address line 1
  // 6. address line 2
  // 7. Zip
  // 8. line separator
  // 9. phone number
  // 10. email
  // 11. line separator
  // 12. edit profile
  // 13. delete address
  ASSERT_EQ(13U, suggestions[0].children.size());
  EXPECT_THAT(suggestions[0].children[9],
              Field(&Suggestion::popup_item_id, PopupItemId::kFillFullEmail));
}

TEST_F(AutofillChildrenSuggestionsGenenarationTest,
       CreateSuggestionsFromProfiles_ChildrenSuggestionsAddressField) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), kAllServerFieldTypes, ADDRESS_HOME_LINE1);

  // The child suggestions should be:
  //
  // 1. first name
  // 2. middle name
  // 3. family name
  // 4. line separator
  // 5. fill full address
  // 6. address line 1
  // 7. address line 2
  // 8. Zip
  // 9. line separator
  // 10. phone number
  // 11. email
  // 12. line separator
  // 13. edit address
  // 14. delete address
  ASSERT_EQ(suggestions.size(), 1u);
  ASSERT_EQ(14U, suggestions[0].children.size());
  EXPECT_THAT(suggestions[0].children[4],
              Field(&Suggestion::popup_item_id, PopupItemId::kFillFullAddress));
}

TEST_F(
    AutofillChildrenSuggestionsGenenarationTest,
    CreateSuggestionsFromProfiles_ChildrenSuggestions_HouseNumberAndStreetNameCanBeNestedUnderDifferentAddressLines) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  // Update the profile to have house number and street name information in
  // different address lines.
  profile.SetRawInfo(ADDRESS_HOME_LINE1, u"Amphitheatre Parkway, Brookling");
  profile.SetRawInfo(ADDRESS_HOME_LINE2, u"1600 Apartment 1");
  profile.SetRawInfo(ADDRESS_HOME_STREET_NAME, u"Amphitheatre Parkway");
  profile.SetRawInfo(ADDRESS_HOME_HOUSE_NUMBER, u"1600");
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile, /*last_targeted_fields=*/absl::nullopt, ADDRESS_HOME_LINE1);

  ASSERT_EQ(1u, suggestions.size());
  ASSERT_LE(3u, suggestions[0].children.size());
  // The address line 1 (sixth child) should have the street name as child.
  EXPECT_THAT(suggestions[0].children[1].children,
              ElementsAre(EqualsFieldByFieldFillingSuggestion(
                  PopupItemId::kFieldByFieldFilling,
                  profile.GetInfo(ADDRESS_HOME_STREET_NAME, app_locale()),
                  ADDRESS_HOME_STREET_NAME, Suggestion::Guid(profile.guid()),
                  {{Suggestion::Text(u"Street")}})));
  // The address line 2 (seventh child) should have the house number as child.
  EXPECT_THAT(suggestions[0].children[2].children,
              ElementsAre(EqualsFieldByFieldFillingSuggestion(
                  PopupItemId::kFieldByFieldFilling,
                  profile.GetInfo(ADDRESS_HOME_HOUSE_NUMBER, app_locale()),
                  ADDRESS_HOME_HOUSE_NUMBER, Suggestion::Guid(profile.guid()),
                  {{Suggestion::Text(u"Building number")}})));
}

TEST_F(
    AutofillChildrenSuggestionsGenenarationTest,
    CreateSuggestionsFromProfiles_GranularityNotFullForm_FillEverythingChildSuggestion) {
  // We set only a name field as `last_targeted_fields` to denote that the user
  // chose field by field filling.
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), absl::optional<ServerFieldTypeSet>({NAME_FIRST}),
      ADDRESS_HOME_LINE1);

  ASSERT_EQ(1U, suggestions.size());
  EXPECT_TRUE(base::ranges::any_of(suggestions[0].children, [](auto child) {
    return child.popup_item_id ==
           PopupItemId::kFillEverythingFromAddressProfile;
  }));
}

// TODO(crbug.com/1477646): Investigate AssignLabelsAndDeduplicate and remove
// the test if it is not needed.
TEST_F(AutofillSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_DiscardDuplicateSuggestions) {
  std::vector<AutofillProfile> profiles(
      3, AutofillProfile(i18n_model_definition::kLegacyHierarchyCountryCode));
  for (AutofillProfile& profile : profiles) {
    profile.SetRawInfo(NAME_FULL, u"Jon Snow");
    profile.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"2 Beyond-the-Wall Rd");
  }
  profiles[1].SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"1 Winterfell Ln");
  std::vector<Suggestion> suggestions =
      suggestion_generator()->CreateSuggestionsFromProfiles(
          {&profiles[0], &profiles[1], &profiles[2]},
          {NAME_FULL, ADDRESS_HOME_STREET_ADDRESS},
          /*last_targeted_fields=*/absl::nullopt, NAME_FULL,
          /*trigger_field_max_length=*/0);

  // Suggestions are sorted from highest to lowest rank, so check that
  // duplicates with a lower rank are removed.
  EXPECT_THAT(
      suggestions,
      ElementsAre(
          AllOf(Field(&Suggestion::main_text,
                      Suggestion::Text(u"Jon Snow",
                                       Suggestion::Text::IsPrimary(true))),
                Field(&Suggestion::labels,
                      std::vector<std::vector<Suggestion::Text>>{
                          {Suggestion::Text(u"2 Beyond-the-Wall Rd")}})),
          AllOf(Field(&Suggestion::main_text,
                      Suggestion::Text(u"Jon Snow",
                                       Suggestion::Text::IsPrimary(true))),
                Field(&Suggestion::labels,
                      std::vector<std::vector<Suggestion::Text>>{
                          {Suggestion::Text(u"1 Winterfell Ln")}}))));
}

// TODO(crbug.com/1477646): Investigate AssignLabelsAndDeduplicate and remove
// the test if it is not needed.
TEST_F(AutofillSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_KeepNonDuplicateSuggestions) {
  AutofillProfile profile_1(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile_1.SetRawInfo(NAME_FIRST, u"Sansa");
  profile_1.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"1 Winterfell Ln");

  AutofillProfile profile_2(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile_2.SetRawInfo(NAME_FIRST, u"Sansa");

  AutofillProfile profile_3(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile_3.SetRawInfo(NAME_FIRST, u"Brienne");
  profile_3.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"1 Winterfell Ln");

  std::vector<Suggestion> suggestions =
      suggestion_generator()->CreateSuggestionsFromProfiles(
          {&profile_1, &profile_2, &profile_3},
          {NAME_FIRST, ADDRESS_HOME_STREET_ADDRESS},
          /*last_targeted_fields=*/absl::nullopt, NAME_FIRST,
          /*trigger_field_max_length=*/0);

  EXPECT_THAT(
      suggestions,
      ElementsAre(
          AllOf(Field(&Suggestion::main_text,
                      Suggestion::Text(u"Sansa",
                                       Suggestion::Text::IsPrimary(true))),
                Field(&Suggestion::labels,
                      std::vector<std::vector<Suggestion::Text>>{
                          {Suggestion::Text(u"1 Winterfell Ln")}})),
          AllOf(Field(&Suggestion::main_text,
                      Suggestion::Text(u"Sansa",
                                       Suggestion::Text::IsPrimary(true))),
                Field(&Suggestion::labels,
                      std::vector<std::vector<Suggestion::Text>>{})),
          AllOf(Field(&Suggestion::main_text,
                      Suggestion::Text(u"Brienne",
                                       Suggestion::Text::IsPrimary(true))),
                Field(&Suggestion::labels,
                      std::vector<std::vector<Suggestion::Text>>{
                          {Suggestion::Text(u"1 Winterfell Ln")}}))));
}

// TODO(crbug.com/1477646): Investigate AssignLabelsAndDeduplicate and remove
// the test if it is not needed.
TEST_F(AutofillSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_SameStringInValueAndLabel) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"Mañana Road");
  profile.SetRawInfo(ADDRESS_HOME_STREET_NAME, u"manana road");

  std::vector<Suggestion> suggestions =
      suggestion_generator()->CreateSuggestionsFromProfiles(
          {&profile}, {ADDRESS_HOME_STREET_NAME, ADDRESS_HOME_STREET_ADDRESS},
          /*last_targeted_fields=*/absl::nullopt, ADDRESS_HOME_STREET_ADDRESS,
          /*trigger_field_max_length=*/0);

  EXPECT_THAT(suggestions,
              ElementsAre(AllOf(
                  Field(&Suggestion::main_text,
                        Suggestion::Text(u"Mañana Road",
                                         Suggestion::Text::IsPrimary(true))),
                  Field(&Suggestion::labels,
                        std::vector<std::vector<Suggestion::Text>>{}))));
}

// Tests that regular suggestions are filtered by the triggering field's value,
// but manual fallback suggestions are not.
TEST_F(AutofillSuggestionGeneratorTest, GetSuggestionsForProfiles_Filtering) {
  AutofillProfile profile1 = test::GetFullProfile();
  AutofillProfile profile2 = test::GetFullProfile2();
  personal_data()->AddProfile(profile1);
  personal_data()->AddProfile(profile2);

  // Create a triggering field those value prefix-matches `profile1`, but not
  // `profile2`.
  FormFieldData triggering_field;
  triggering_field.value = profile1.GetRawInfo(NAME_FIRST);
  ASSERT_FALSE(profile2.GetRawInfo(NAME_FIRST)
                   .starts_with(profile1.GetRawInfo(NAME_FIRST)));

  // Expect that regular suggestions filter.
  EXPECT_EQ(suggestion_generator()
                ->GetSuggestionsForProfiles(
                    {NAME_FIRST}, triggering_field, NAME_FIRST,
                    /*last_targeted_fields=*/absl::nullopt,
                    AutofillSuggestionTriggerSource::kFormControlElementClicked)
                .size(),
            1u);
  // But manual fallback suggestions do not.
  EXPECT_EQ(suggestion_generator()
                ->GetSuggestionsForProfiles(
                    {NAME_FIRST}, triggering_field, NAME_FIRST,
                    /*last_targeted_fields=*/absl::nullopt,
                    AutofillSuggestionTriggerSource::kManualFallbackAddress)
                .size(),
            2u);
}

TEST_F(AutofillSuggestionGeneratorTest,
       RemoveExpiredCreditCardsNotUsedSinceTimestamp) {
  const char kHistogramName[] = "Autofill.CreditCardsSuppressedForDisuse";
  const base::Time kNow = AutofillClock::Now();
  constexpr size_t kNumCards = 10;

  // We construct a card vector as below, number indicate days of last used
  // from |kNow|:
  // [30, 90, 150, 210, 270, 0, 60, 120, 180, 240]
  // |expires at 2999     |, |expired at 2001   |
  std::vector<CreditCard> all_card_data;
  std::vector<CreditCard*> all_card_ptrs;
  all_card_data.reserve(kNumCards);
  all_card_ptrs.reserve(kNumCards);
  for (size_t i = 0; i < kNumCards; ++i) {
    constexpr base::TimeDelta k30Days = base::Days(30);
    all_card_data.emplace_back(
        base::Uuid::GenerateRandomV4().AsLowercaseString(),
        "https://example.com");
    if (i < 5) {
      all_card_data.back().set_use_date(kNow - (i + i + 1) * k30Days);
      test::SetCreditCardInfo(&all_card_data.back(), "Clyde Barrow",
                              "378282246310005" /* American Express */, "04",
                              "2999", "1");
    } else {
      all_card_data.back().set_use_date(kNow - (i + i - 10) * k30Days);
      test::SetCreditCardInfo(&all_card_data.back(), "John Dillinger",
                              "4234567890123456" /* Visa */, "04", "2001", "1");
    }
    all_card_ptrs.push_back(&all_card_data.back());
  }

  // Verify that only expired disused card are removed. Note that only the last
  // two cards have use dates more than 175 days ago and are expired.
  {
    // Create a working copy of the card pointers.
    std::vector<CreditCard*> cards(all_card_ptrs);

    // The first 8 are either not expired or having use dates more recent
    // than 175 days ago.
    std::vector<CreditCard*> expected_cards(cards.begin(), cards.begin() + 8);

    // Filter the cards while capturing histograms.
    base::HistogramTester histogram_tester;
    AutofillSuggestionGenerator::
        RemoveExpiredLocalCreditCardsNotUsedSinceTimestamp(
            kNow - base::Days(175), cards);

    // Validate that we get the expected filtered cards and histograms.
    EXPECT_EQ(expected_cards, cards);
    histogram_tester.ExpectTotalCount(kHistogramName, 1);
    histogram_tester.ExpectBucketCount(kHistogramName, 2, 1);
  }

  // Reverse the card order and verify that only expired and disused cards
  // are removed. Note that the first three cards, post reversal,
  // have use dates more then 115 days ago.
  {
    // Create a reversed working copy of the card pointers.
    std::vector<CreditCard*> cards(all_card_ptrs.rbegin(),
                                   all_card_ptrs.rend());

    // The last 7 cards have use dates more recent than 115 days ago.
    std::vector<CreditCard*> expected_cards(cards.begin() + 3, cards.end());

    // Filter the cards while capturing histograms.
    base::HistogramTester histogram_tester;
    AutofillSuggestionGenerator::
        RemoveExpiredLocalCreditCardsNotUsedSinceTimestamp(
            kNow - base::Days(115), cards);

    // Validate that we get the expected filtered cards and histograms.
    EXPECT_EQ(expected_cards, cards);
    histogram_tester.ExpectTotalCount(kHistogramName, 1);
    histogram_tester.ExpectBucketCount(kHistogramName, 3, 1);
  }
  // Randomize the card order and validate that the filtered list retains
  // that order. Note that the three cards have use dates more then 115
  // days ago and are expired.
  {
    // A handy constant.
    const base::Time k115DaysAgo = kNow - base::Days(115);

    // Created a shuffled primary copy of the card pointers.
    std::vector<CreditCard*> shuffled_cards(all_card_ptrs);
    base::RandomShuffle(shuffled_cards.begin(), shuffled_cards.end());

    // Copy the shuffled card pointer collections to use as the working
    // set.
    std::vector<CreditCard*> cards(shuffled_cards);

    // Filter the cards while capturing histograms.
    base::HistogramTester histogram_tester;
    AutofillSuggestionGenerator::
        RemoveExpiredLocalCreditCardsNotUsedSinceTimestamp(k115DaysAgo, cards);

    // Validate that we have the right cards. Iterate of the the shuffled
    // primary copy and the filtered copy at the same time. making sure that
    // the elements in the filtered copy occur in the same order as the shuffled
    // primary. Along the way, validate that the elements in and out of the
    // filtered copy have appropriate use dates and expiration states.
    EXPECT_EQ(7u, cards.size());
    auto it = shuffled_cards.begin();
    for (const CreditCard* card : cards) {
      for (; it != shuffled_cards.end() && (*it) != card; ++it) {
        EXPECT_LT((*it)->use_date(), k115DaysAgo);
        ASSERT_TRUE((*it)->IsExpired(kNow));
      }
      ASSERT_TRUE(it != shuffled_cards.end());
      ASSERT_TRUE(card->use_date() > k115DaysAgo || !card->IsExpired(kNow));
      ++it;
    }
    for (; it != shuffled_cards.end(); ++it) {
      EXPECT_LT((*it)->use_date(), k115DaysAgo);
      ASSERT_TRUE((*it)->IsExpired(kNow));
    }

    // Validate the histograms.
    histogram_tester.ExpectTotalCount(kHistogramName, 1);
    histogram_tester.ExpectBucketCount(kHistogramName, 3, 1);
  }

  // Verify all cards are retained if they're sufficiently recently
  // used.
  {
    // Create a working copy of the card pointers.
    std::vector<CreditCard*> cards(all_card_ptrs);

    // Filter the cards while capturing histograms.
    base::HistogramTester histogram_tester;
    AutofillSuggestionGenerator::
        RemoveExpiredLocalCreditCardsNotUsedSinceTimestamp(
            kNow - base::Days(720), cards);

    // Validate that we get the expected filtered cards and histograms.
    EXPECT_EQ(all_card_ptrs, cards);
    histogram_tester.ExpectTotalCount(kHistogramName, 1);
    histogram_tester.ExpectBucketCount(kHistogramName, 0, 1);
  }

  // Verify all cards are removed if they're all disused and expired.
  {
    // Create a working copy of the card pointers.
    std::vector<CreditCard*> cards(all_card_ptrs);
    for (auto it = all_card_ptrs.begin(); it < all_card_ptrs.end(); it++) {
      (*it)->SetExpirationYear(2001);
    }

    // Filter the cards while capturing histograms.
    base::HistogramTester histogram_tester;
    AutofillSuggestionGenerator::
        RemoveExpiredLocalCreditCardsNotUsedSinceTimestamp(kNow + base::Days(1),
                                                           cards);

    // Validate that we get the expected filtered cards and histograms.
    EXPECT_TRUE(cards.empty());
    histogram_tester.ExpectTotalCount(kHistogramName, 1);
    histogram_tester.ExpectBucketCount(kHistogramName, kNumCards, 1);
  }

  // Verify all expired and disused server cards are not removed.
  {
    // Create a working copy of the card pointers. And set one card to be a
    // masked server card.
    std::vector<CreditCard*> cards(all_card_ptrs);
    for (auto it = all_card_ptrs.begin(); it < all_card_ptrs.end(); it++) {
      (*it)->SetExpirationYear(2001);
    }
    cards[0]->set_record_type(CreditCard::RecordType::kMaskedServerCard);

    // Filter the cards while capturing histograms.
    base::HistogramTester histogram_tester;
    AutofillSuggestionGenerator::
        RemoveExpiredLocalCreditCardsNotUsedSinceTimestamp(kNow + base::Days(1),
                                                           cards);

    // Validate that we get the expected filtered cards and histograms.
    EXPECT_EQ(1U, cards.size());
    histogram_tester.ExpectTotalCount(kHistogramName, 1);
    histogram_tester.ExpectBucketCount(kHistogramName, kNumCards - 1, 1);
  }
}

TEST_F(AutofillSuggestionGeneratorTest, GetServerCardForLocalCard) {
  CreditCard server_card = CreateServerCard();
  server_card.SetNumber(u"4111111111111111");
  personal_data()->AddServerCreditCard(server_card);
  CreditCard local_card =
      CreateLocalCard("00000000-0000-0000-0000-000000000002");

  // The server card should be returned if the local card is passed in.
  const CreditCard* result =
      personal_data()->GetServerCardForLocalCard(&local_card);
  ASSERT_TRUE(result);
  EXPECT_EQ(server_card.guid(), result->guid());

  // Should return nullptr if a server card is passed in.
  EXPECT_FALSE(personal_data()->GetServerCardForLocalCard(&server_card));

  // Should return nullptr if no server card has the same information as the
  // local card.
  server_card.SetNumber(u"5454545454545454");
  personal_data()->ClearCreditCards();
  personal_data()->AddServerCreditCard(server_card);
  EXPECT_FALSE(personal_data()->GetServerCardForLocalCard(&local_card));
}

// The suggestions of credit cards with card linked offers are moved to the
// front. This test checks that the order of the other cards remains stable.
TEST_F(AutofillSuggestionGeneratorTest,
       GetSuggestionsForCreditCards_StableSortBasedOnOffer) {
  // Create three server cards.
  personal_data()->ClearCreditCards();
  personal_data()->AddServerCreditCard(CreateServerCard(
      /*guid=*/"00000000-0000-0000-0000-000000000001",
      /*server_id=*/"server_id1", /*instrument_id=*/1));
  personal_data()->AddServerCreditCard(CreateServerCard(
      /*guid=*/"00000000-0000-0000-0000-000000000002",
      /*server_id=*/"server_id2", /*instrument_id=*/2));
  personal_data()->AddServerCreditCard(CreateServerCard(
      /*guid=*/"00000000-0000-0000-0000-000000000003",
      /*server_id=*/"server_id3", /*instrument_id=*/3));

  // Create a card linked offer and attach it to server_card2.
  AutofillOfferData offer_data = test::GetCardLinkedOfferData1();
  offer_data.SetMerchantOriginForTesting({GURL("http://www.example1.com")});
  offer_data.SetEligibleInstrumentIdForTesting({2});
  autofill_client()->set_last_committed_primary_main_frame_url(
      GURL("http://www.example1.com"));
  personal_data()->AddAutofillOfferData(offer_data);

  bool should_display_gpay_logo;
  bool with_offer;
  autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
  std::vector<Suggestion> suggestions =
      suggestion_generator()->GetSuggestionsForCreditCards(
          FormFieldData(), CREDIT_CARD_NUMBER, should_display_gpay_logo,
          with_offer, metadata_logging_context);

  EXPECT_TRUE(with_offer);
  ASSERT_EQ(suggestions.size(), 3U);
  // The suggestion with card linked offer available should be ranked to the
  // top.
  EXPECT_EQ(suggestions[0].GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(
                Suggestion::Guid("00000000-0000-0000-0000-000000000002")));
  // The other suggestions should have their relative ranking unchanged.
  EXPECT_EQ(suggestions[1].GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(
                Suggestion::Guid("00000000-0000-0000-0000-000000000003")));
  EXPECT_EQ(suggestions[2].GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(
                Suggestion::Guid("00000000-0000-0000-0000-000000000001")));
}

// Ensures we appropriately generate suggestions for virtual cards on a
// standalone CVC field.
TEST_F(AutofillSuggestionGeneratorTest,
       GetSuggestionsForVirtualCardStandaloneCvc) {
  personal_data()->ClearCreditCards();
  CreditCard virtual_card = test::GetVirtualCard();
  virtual_card.set_guid("1234");
  personal_data()->AddServerCreditCard(virtual_card);

  base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
      virtual_card_guid_to_last_four_map;
  virtual_card_guid_to_last_four_map.insert(
      {virtual_card.guid(),
       VirtualCardUsageData::VirtualCardLastFour(u"1234")});
  autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
  std::vector<Suggestion> suggestions =
      suggestion_generator()->GetSuggestionsForVirtualCardStandaloneCvc(
          metadata_logging_context, virtual_card_guid_to_last_four_map);

  ASSERT_EQ(suggestions.size(), 1U);
}

// Verifies that the `should_display_gpay_logo` is set correctly.
TEST_F(AutofillSuggestionGeneratorTest, ShouldDisplayGpayLogo) {
  // `should_display_gpay_logo` should be true if suggestions were all for
  // server cards.
  {
    // Create two server cards.
    personal_data()->AddServerCreditCard(CreateServerCard(
        /*guid=*/"00000000-0000-0000-0000-000000000001",
        /*server_id=*/"server_id1", /*instrument_id=*/1));
    personal_data()->AddServerCreditCard(CreateServerCard(
        /*guid=*/"00000000-0000-0000-0000-000000000002",
        /*server_id=*/"server_id2", /*instrument_id=*/2));

    bool should_display_gpay_logo;
    bool with_offer;
    autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
    std::vector<Suggestion> suggestions =
        suggestion_generator()->GetSuggestionsForCreditCards(
            FormFieldData(), CREDIT_CARD_NUMBER, should_display_gpay_logo,
            with_offer, metadata_logging_context);

    EXPECT_EQ(suggestions.size(), 2U);
    EXPECT_TRUE(should_display_gpay_logo);
  }

  personal_data()->ClearCreditCards();

  // `should_display_gpay_logo` should be false if at least one local card was
  // in the suggestions.
  {
    // Create one server card and one local card.
    auto local_card = CreateLocalCard(
        /*guid=*/"00000000-0000-0000-0000-000000000001");
    local_card.SetNumber(u"5454545454545454");
    personal_data()->AddCreditCard(local_card);
    personal_data()->AddServerCreditCard(CreateServerCard(
        /*guid=*/"00000000-0000-0000-0000-000000000002",
        /*server_id=*/"server_id2", /*instrument_id=*/2));

    bool should_display_gpay_logo;
    bool with_offer;
    autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
    std::vector<Suggestion> suggestions =
        suggestion_generator()->GetSuggestionsForCreditCards(
            FormFieldData(), CREDIT_CARD_NUMBER, should_display_gpay_logo,
            with_offer, metadata_logging_context);

    EXPECT_EQ(suggestions.size(), 2U);
    EXPECT_FALSE(should_display_gpay_logo);
  }

  personal_data()->ClearCreditCards();

  // `should_display_gpay_logo` should be true if there was an unused expired
  // local card in the suggestions.
  {
    // Create one server card and one unused expired local card.
    auto local_card = CreateLocalCard(
        /*guid=*/"00000000-0000-0000-0000-000000000001");
    local_card.SetNumber(u"5454545454545454");
    local_card.SetExpirationYear(2020);
    local_card.set_use_date(AutofillClock::Now() - base::Days(365));
    personal_data()->AddCreditCard(local_card);
    personal_data()->AddServerCreditCard(CreateServerCard(
        /*guid=*/"00000000-0000-0000-0000-000000000002",
        /*server_id=*/"server_id2", /*instrument_id=*/2));

    bool should_display_gpay_logo;
    bool with_offer;
    autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
    std::vector<Suggestion> suggestions =
        suggestion_generator()->GetSuggestionsForCreditCards(
            FormFieldData(), CREDIT_CARD_NUMBER, should_display_gpay_logo,
            with_offer, metadata_logging_context);

    EXPECT_EQ(suggestions.size(), 1U);
    EXPECT_TRUE(should_display_gpay_logo);
  }

  personal_data()->ClearCreditCards();

  // `should_display_gpay_logo` should be true if there was no card at all.
  {
    bool should_display_gpay_logo;
    bool with_offer;
    autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
    std::vector<Suggestion> suggestions =
        suggestion_generator()->GetSuggestionsForCreditCards(
            FormFieldData(), CREDIT_CARD_NUMBER, should_display_gpay_logo,
            with_offer, metadata_logging_context);

    EXPECT_TRUE(suggestions.empty());
    EXPECT_TRUE(should_display_gpay_logo);
  }
}

// Test that the virtual card option is shown when all of the prerequisites are
// met.
TEST_F(AutofillSuggestionGeneratorTest, ShouldShowVirtualCardOption) {
  // Create a server card.
  CreditCard server_card =
      CreateServerCard(/*guid=*/"00000000-0000-0000-0000-000000000001");
  server_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  personal_data()->AddServerCreditCard(server_card);

  // Create a local card with same information.
  CreditCard local_card =
      CreateLocalCard(/*guid=*/"00000000-0000-0000-0000-000000000002");

  // If all prerequisites are met, it should return true.
  EXPECT_TRUE(
      suggestion_generator()->ShouldShowVirtualCardOption(&server_card));
  EXPECT_TRUE(suggestion_generator()->ShouldShowVirtualCardOption(&local_card));
}

// Test that the virtual card option is shown when the autofill optimization
// guide is not present.
TEST_F(AutofillSuggestionGeneratorTest,
       ShouldShowVirtualCardOption_AutofillOptimizationGuideNotPresent) {
  // Create a server card.
  CreditCard server_card =
      CreateServerCard(/*guid=*/"00000000-0000-0000-0000-000000000001");
  server_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  personal_data()->AddServerCreditCard(server_card);
  autofill_client()->ResetAutofillOptimizationGuide();

  // Create a local card with same information.
  CreditCard local_card =
      CreateLocalCard(/*guid=*/"00000000-0000-0000-0000-000000000002");

  // If all prerequisites are met, it should return true.
  EXPECT_TRUE(
      suggestion_generator()->ShouldShowVirtualCardOption(&server_card));
  EXPECT_TRUE(suggestion_generator()->ShouldShowVirtualCardOption(&local_card));
}

// Test that the virtual card option is not shown if the merchant is opted-out
// of virtual cards.
TEST_F(AutofillSuggestionGeneratorTest,
       ShouldNotShowVirtualCardOption_MerchantOptedOutOfVirtualCards) {
  // Create an enrolled server card.
  CreditCard server_card =
      CreateServerCard(/*guid=*/"00000000-0000-0000-0000-000000000001");
  server_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  personal_data()->AddServerCreditCard(server_card);

  // Create a local card with same information.
  CreditCard local_card =
      CreateLocalCard(/*guid=*/"00000000-0000-0000-0000-000000000002");

  // If the URL is opted-out of virtual cards for `server_card`, do not display
  // the virtual card suggestion.
  auto* optimization_guide = autofill_client()->GetAutofillOptimizationGuide();
  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(optimization_guide),
          ShouldBlockFormFieldSuggestion)
      .WillByDefault(testing::Return(true));
  EXPECT_FALSE(
      suggestion_generator()->ShouldShowVirtualCardOption(&server_card));
  EXPECT_FALSE(
      suggestion_generator()->ShouldShowVirtualCardOption(&local_card));
}

// Test that the virtual card option is not shown if the server card we might be
// showing a virtual card option for is not enrolled into virtual card.
TEST_F(AutofillSuggestionGeneratorTest,
       ShouldNotShowVirtualCardOption_ServerCardNotEnrolledInVirtualCard) {
  // Create an unenrolled server card.
  CreditCard server_card =
      CreateServerCard(/*guid=*/"00000000-0000-0000-0000-000000000001");
  server_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kUnspecified);
  personal_data()->AddServerCreditCard(server_card);

  // Create a local card with same information.
  CreditCard local_card =
      CreateLocalCard(/*guid=*/"00000000-0000-0000-0000-000000000002");

  // For server card not enrolled, both local and server card should return
  // false.
  EXPECT_FALSE(
      suggestion_generator()->ShouldShowVirtualCardOption(&server_card));
  EXPECT_FALSE(
      suggestion_generator()->ShouldShowVirtualCardOption(&local_card));
}

// Test that the virtual card option is not shown for a local card with no
// server card duplicate.
TEST_F(AutofillSuggestionGeneratorTest,
       ShouldNotShowVirtualCardOption_LocalCardWithoutServerCardDuplicate) {
  // Create a local card with same information.
  CreditCard local_card =
      CreateLocalCard(/*guid=*/"00000000-0000-0000-0000-000000000002");

  // The local card does not have a server duplicate, should return false.
  EXPECT_FALSE(
      suggestion_generator()->ShouldShowVirtualCardOption(&local_card));
}

TEST_F(AutofillSuggestionGeneratorTest, GetLocalIbanSuggestions) {
  SetUpIbanImageResources();

  auto MakeLocalIban = [](const std::u16string& value,
                          const std::u16string& nickname) {
    Iban iban(Iban::Guid(base::Uuid::GenerateRandomV4().AsLowercaseString()));
    iban.set_value(value);
    if (!nickname.empty())
      iban.set_nickname(nickname);
    return iban;
  };
  Iban iban0 =
      MakeLocalIban(u"CH56 0483 5012 3456 7800 9", u"My doctor's IBAN");
  Iban iban1 =
      MakeLocalIban(u"DE91 1000 0000 0123 4567 89", u"My brother's IBAN");
  Iban iban2 =
      MakeLocalIban(u"GR96 0810 0010 0000 0123 4567 890", u"My teacher's IBAN");
  Iban iban3 = MakeLocalIban(u"PK70 BANK 0000 1234 5678 9000", u"");

  std::vector<Suggestion> iban_suggestions =
      AutofillSuggestionGenerator::GetSuggestionsForIbans(
          {&iban0, &iban1, &iban2, &iban3});

  // There are 6 suggestions, 4 for IBAN suggestions, followed by a separator,
  // and followed by "Manage payment methods..." which redirects to the Chrome
  // payment methods settings page.
  ASSERT_EQ(iban_suggestions.size(), 6u);

  EXPECT_THAT(
      iban_suggestions[0],
      EqualsIbanSuggestion(iban0.GetIdentifierStringForAutofillDisplay(),
                           Suggestion::ValueToFill(iban0.GetStrippedValue()),
                           iban0.nickname()));

  EXPECT_THAT(
      iban_suggestions[1],
      EqualsIbanSuggestion(iban1.GetIdentifierStringForAutofillDisplay(),
                           Suggestion::ValueToFill(iban1.GetStrippedValue()),
                           iban1.nickname()));

  EXPECT_THAT(
      iban_suggestions[2],
      EqualsIbanSuggestion(iban2.GetIdentifierStringForAutofillDisplay(),
                           Suggestion::ValueToFill(iban2.GetStrippedValue()),
                           iban2.nickname()));

  EXPECT_THAT(
      iban_suggestions[3],
      EqualsIbanSuggestion(iban3.GetIdentifierStringForAutofillDisplay(),
                           Suggestion::ValueToFill(iban3.GetStrippedValue()),
                           iban3.nickname()));

  EXPECT_EQ(iban_suggestions[4].popup_item_id, PopupItemId::kSeparator);

  EXPECT_EQ(iban_suggestions[5].main_text.value,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_PAYMENT_METHODS));
  EXPECT_EQ(iban_suggestions[5].popup_item_id, PopupItemId::kAutofillOptions);
}

TEST_F(AutofillSuggestionGeneratorTest, GetServerIbanSuggestions) {
  SetUpIbanImageResources();

  Iban server_iban1 = test::GetServerIban();
  Iban server_iban2 = test::GetServerIban2();
  Iban server_iban3 = test::GetServerIban3();

  std::vector<Suggestion> iban_suggestions =
      AutofillSuggestionGenerator::GetSuggestionsForIbans(
          {&server_iban1, &server_iban2, &server_iban3});

  // There are 5 suggestions, 3 for IBAN suggestions, followed by a separator,
  // and followed by "Manage payment methods..." which redirects to the Chrome
  // payment methods settings page.
  ASSERT_EQ(iban_suggestions.size(), 5u);

  EXPECT_THAT(
      iban_suggestions[0],
      EqualsIbanSuggestion(server_iban1.GetIdentifierStringForAutofillDisplay(),
                           Suggestion::BackendId(Suggestion::InstrumentId(
                               server_iban1.instrument_id())),
                           server_iban1.nickname()));

  EXPECT_THAT(
      iban_suggestions[1],
      EqualsIbanSuggestion(server_iban2.GetIdentifierStringForAutofillDisplay(),
                           Suggestion::BackendId(Suggestion::InstrumentId(
                               server_iban2.instrument_id())),
                           server_iban2.nickname()));

  EXPECT_THAT(
      iban_suggestions[2],
      EqualsIbanSuggestion(server_iban3.GetIdentifierStringForAutofillDisplay(),
                           Suggestion::BackendId(Suggestion::InstrumentId(
                               server_iban3.instrument_id())),
                           server_iban3.nickname()));

  EXPECT_EQ(iban_suggestions[3].popup_item_id, PopupItemId::kSeparator);

  EXPECT_EQ(iban_suggestions[4].main_text.value,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_PAYMENT_METHODS));
  EXPECT_EQ(iban_suggestions[4].popup_item_id, PopupItemId::kAutofillOptions);
}

TEST_F(AutofillSuggestionGeneratorTest, GetLocalAndServerIbanSuggestions) {
  SetUpIbanImageResources();

  Iban server_iban1 = test::GetServerIban();
  Iban server_iban2 = test::GetServerIban2();
  Iban local_iban1 = test::GetLocalIban();

  std::vector<Suggestion> iban_suggestions =
      AutofillSuggestionGenerator::GetSuggestionsForIbans(
          {&server_iban1, &server_iban2, &local_iban1});

  // There are 5 suggestions, 3 for IBAN suggestions, followed by a separator,
  // and followed by "Manage payment methods..." which redirects to the Chrome
  // payment methods settings page.
  ASSERT_EQ(iban_suggestions.size(), 5u);

  EXPECT_THAT(
      iban_suggestions[0],
      EqualsIbanSuggestion(server_iban1.GetIdentifierStringForAutofillDisplay(),
                           Suggestion::BackendId(Suggestion::InstrumentId(
                               server_iban1.instrument_id())),
                           server_iban1.nickname()));

  EXPECT_THAT(
      iban_suggestions[1],
      EqualsIbanSuggestion(server_iban2.GetIdentifierStringForAutofillDisplay(),
                           Suggestion::BackendId(Suggestion::InstrumentId(
                               server_iban2.instrument_id())),
                           server_iban2.nickname()));

  EXPECT_THAT(iban_suggestions[2],
              EqualsIbanSuggestion(
                  local_iban1.GetIdentifierStringForAutofillDisplay(),
                  Suggestion::ValueToFill(local_iban1.GetStrippedValue()),
                  local_iban1.nickname()));

  EXPECT_EQ(iban_suggestions[3].popup_item_id, PopupItemId::kSeparator);

  EXPECT_EQ(iban_suggestions[4].main_text.value,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_PAYMENT_METHODS));
  EXPECT_EQ(iban_suggestions[4].popup_item_id, PopupItemId::kAutofillOptions);
}

TEST_F(AutofillSuggestionGeneratorTest,
       GetPromoCodeSuggestionsFromPromoCodeOffers_ValidPromoCodes) {
  std::vector<const AutofillOfferData*> promo_code_offers;

  base::Time expiry = AutofillClock::Now() + base::Days(2);
  std::vector<GURL> merchant_origins;
  DisplayStrings display_strings;
  display_strings.value_prop_text = "test_value_prop_text_1";
  std::string promo_code = "test_promo_code_1";
  AutofillOfferData offer1 = AutofillOfferData::FreeListingCouponOffer(
      /*offer_id=*/1, expiry, merchant_origins,
      /*offer_details_url=*/GURL("https://offer-details-url.com/"),
      display_strings, promo_code);

  promo_code_offers.push_back(&offer1);

  DisplayStrings display_strings2;
  display_strings2.value_prop_text = "test_value_prop_text_2";
  std::string promo_code2 = "test_promo_code_2";
  AutofillOfferData offer2 = AutofillOfferData::FreeListingCouponOffer(
      /*offer_id=*/2, expiry, merchant_origins,
      /*offer_details_url=*/GURL("https://offer-details-url.com/"),
      display_strings2, promo_code2);

  promo_code_offers.push_back(&offer2);

  std::vector<Suggestion> promo_code_suggestions =
      AutofillSuggestionGenerator::GetPromoCodeSuggestionsFromPromoCodeOffers(
          promo_code_offers);
  EXPECT_TRUE(promo_code_suggestions.size() == 4);

  EXPECT_EQ(promo_code_suggestions[0].main_text.value, u"test_promo_code_1");
  EXPECT_EQ(promo_code_suggestions[0].GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(Suggestion::Guid("1")));
  ASSERT_EQ(promo_code_suggestions[0].labels.size(), 1U);
  ASSERT_EQ(promo_code_suggestions[0].labels[0].size(), 1U);
  EXPECT_EQ(promo_code_suggestions[0].labels[0][0].value,
            u"test_value_prop_text_1");
  EXPECT_EQ(promo_code_suggestions[0].GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(Suggestion::Guid("1")));
  EXPECT_EQ(promo_code_suggestions[0].popup_item_id,
            PopupItemId::kMerchantPromoCodeEntry);

  EXPECT_EQ(promo_code_suggestions[1].main_text.value, u"test_promo_code_2");
  EXPECT_EQ(promo_code_suggestions[1].GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(Suggestion::Guid("2")));
  ASSERT_EQ(promo_code_suggestions[1].labels.size(), 1U);
  ASSERT_EQ(promo_code_suggestions[1].labels[0].size(), 1U);
  EXPECT_EQ(promo_code_suggestions[1].labels[0][0].value,
            u"test_value_prop_text_2");
  EXPECT_EQ(promo_code_suggestions[1].GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(Suggestion::Guid("2")));
  EXPECT_EQ(promo_code_suggestions[1].popup_item_id,
            PopupItemId::kMerchantPromoCodeEntry);

  EXPECT_EQ(promo_code_suggestions[2].popup_item_id, PopupItemId::kSeparator);

  EXPECT_EQ(promo_code_suggestions[3].main_text.value,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_PROMO_CODE_SUGGESTIONS_FOOTER_TEXT));
  EXPECT_EQ(promo_code_suggestions[3].GetPayload<GURL>(),
            offer1.GetOfferDetailsUrl().spec());
  EXPECT_EQ(promo_code_suggestions[3].popup_item_id,
            PopupItemId::kSeePromoCodeDetails);
}

TEST_F(AutofillSuggestionGeneratorTest,
       GetPromoCodeSuggestionsFromPromoCodeOffers_InvalidPromoCodeURL) {
  std::vector<const AutofillOfferData*> promo_code_offers;
  AutofillOfferData offer;
  offer.SetPromoCode("test_promo_code_1");
  offer.SetValuePropTextInDisplayStrings("test_value_prop_text_1");
  offer.SetOfferIdForTesting(1);
  offer.SetOfferDetailsUrl(GURL("invalid-url"));
  promo_code_offers.push_back(&offer);

  std::vector<Suggestion> promo_code_suggestions =
      AutofillSuggestionGenerator::GetPromoCodeSuggestionsFromPromoCodeOffers(
          promo_code_offers);
  EXPECT_TRUE(promo_code_suggestions.size() == 1);

  EXPECT_EQ(promo_code_suggestions[0].main_text.value, u"test_promo_code_1");
  ASSERT_EQ(promo_code_suggestions[0].labels.size(), 1U);
  ASSERT_EQ(promo_code_suggestions[0].labels[0].size(), 1U);
  EXPECT_EQ(promo_code_suggestions[0].labels[0][0].value,
            u"test_value_prop_text_1");
  EXPECT_FALSE(
      absl::holds_alternative<GURL>(promo_code_suggestions[0].payload));
  EXPECT_EQ(promo_code_suggestions[0].popup_item_id,
            PopupItemId::kMerchantPromoCodeEntry);
}

TEST_F(AutofillSuggestionGeneratorTest, TestAddressSuggestion) {
  AutofillProfile profile = test::GetFullProfile();
  personal_data()->set_test_addresses({profile});
  std::vector<Suggestion> suggestions =
      suggestion_generator()->CreateSuggestionsFromProfiles(
          {&profile}, /*field_types=*/{NAME_FIRST},
          /*last_targeted_fields=*/kAllServerFieldTypes, NAME_FIRST,
          /*trigger_field_max_length=*/0);

  // Therere should be test address suggestion and one regular profile
  // suggestion.
  ASSERT_EQ(suggestions.size(), 2u);
  EXPECT_EQ(suggestions[0].popup_item_id, PopupItemId::kDevtoolsTestAddresses);
  EXPECT_EQ(suggestions[1].popup_item_id, PopupItemId::kAddressEntry);

  EXPECT_EQ(suggestions[0].main_text.value, u"Devtools");
  EXPECT_EQ(suggestions[0].labels.size(), 1u);
  EXPECT_EQ(suggestions[0].labels[0].size(), 1u);
  EXPECT_EQ(suggestions[0].labels[0][0],
            Suggestion::Text(u"Address test data"));
  EXPECT_EQ(suggestions[0].icon, Suggestion::Icon::kCode);
  EXPECT_EQ(suggestions[0].children.size(), 1u);

  const Suggestion& child = suggestions[0].children.back();
  EXPECT_EQ(child.main_text.value, u"United States");
  EXPECT_EQ(child.GetBackendId<Suggestion::Guid>().value(), profile.guid());
  EXPECT_EQ(child.popup_item_id, PopupItemId::kDevtoolsTestAddressEntry);
}

// This class helps test the credit card contents that are displayed in
// Autofill suggestions. It covers suggestions on Desktop/Android dropdown,
// and on Android keyboard accessory.
class AutofillCreditCardSuggestionContentTest
    : public AutofillSuggestionGeneratorTest {
 public:
  AutofillCreditCardSuggestionContentTest() {
    feature_list_metadata_.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillEnableVirtualCardMetadata,
                              features::kAutofillEnableCardProductName},
        /*disabled_features=*/{});
  }

  ~AutofillCreditCardSuggestionContentTest() override = default;

  bool keyboard_accessory_enabled() const {
#if BUILDFLAG(IS_ANDROID)
    return true;
#else
    return false;
#endif
  }

#if BUILDFLAG(IS_IOS)
  // Return the obfuscation length for the last four digits on iOS.
  // Although this depends on the kAutofillUseTwoDotsForLastFourDigits flag,
  // that flag is not tested explicitly by this test; see
  // AutofillCreditCardSuggestionIOSObfuscationLengthContentTest instead.
  int ios_obfuscation_length() const {
    return base::FeatureList::IsEnabled(
               features::kAutofillUseTwoDotsForLastFourDigits)
               ? 2
               : 4;
  }
#endif

 private:
  base::test::ScopedFeatureList feature_list_metadata_;
};

// Verify that the suggestion's texts are populated correctly for a virtual card
// suggestion when the cardholder name field is focused.
TEST_F(AutofillCreditCardSuggestionContentTest,
       CreateCreditCardSuggestion_VirtualCardMetadata_NameField) {
  CreditCard server_card = CreateServerCard();

  // Name field suggestion for virtual cards.
  Suggestion virtual_card_name_field_suggestion =
      suggestion_generator()->CreateCreditCardSuggestion(
          server_card, CREDIT_CARD_NAME_FULL,
          /*virtual_card_option=*/true,
          /*card_linked_offer_available=*/false);

  if (keyboard_accessory_enabled()) {
    // For the keyboard accessory, the "Virtual card" label is added as a prefix
    // to the cardholder name.
    EXPECT_EQ(virtual_card_name_field_suggestion.main_text.value,
              u"Virtual card  Elvis Presley");
    EXPECT_EQ(virtual_card_name_field_suggestion.minor_text.value, u"");
  } else {
    // On other platforms, the cardholder name is shown on the first line.
    EXPECT_EQ(virtual_card_name_field_suggestion.main_text.value,
              u"Elvis Presley");
    EXPECT_EQ(virtual_card_name_field_suggestion.minor_text.value, u"");
  }

#if BUILDFLAG(IS_IOS)
  // There should be 2 lines of labels:
  // 1. Obfuscated last 4 digits "..1111" or "....1111".
  // 2. Virtual card label.
  ASSERT_EQ(virtual_card_name_field_suggestion.labels.size(), 2U);
  ASSERT_EQ(virtual_card_name_field_suggestion.labels[0].size(), 1U);
  EXPECT_EQ(virtual_card_name_field_suggestion.labels[0][0].value,
            CreditCard::GetObfuscatedStringForCardDigits(
                ios_obfuscation_length(), u"1111"));
#else
  if (keyboard_accessory_enabled()) {
    // There should be only 1 line of label: obfuscated last 4 digits "..1111".
    ASSERT_EQ(virtual_card_name_field_suggestion.labels.size(), 1U);
    ASSERT_EQ(virtual_card_name_field_suggestion.labels[0].size(), 1U);
    EXPECT_EQ(virtual_card_name_field_suggestion.labels[0][0].value,
              CreditCard::GetObfuscatedStringForCardDigits(
                  /*obfuscation_length=*/2, u"1111"));
  } else {
    // There should be 2 lines of labels:
    // 1. Card name + obfuscated last 4 digits "CardName  ....1111". Card name
    // and last four are populated separately.
    // 2. Virtual card label.
    ASSERT_EQ(virtual_card_name_field_suggestion.labels.size(), 2U);
    ASSERT_EQ(virtual_card_name_field_suggestion.labels[0].size(), 2U);
    EXPECT_EQ(virtual_card_name_field_suggestion.labels[0][0].value, u"Visa");
    EXPECT_EQ(virtual_card_name_field_suggestion.labels[0][1].value,
              CreditCard::GetObfuscatedStringForCardDigits(
                  /*obfuscation_length=*/4, u"1111"));
  }
#endif

  if (!keyboard_accessory_enabled()) {
    // The virtual card text should be populated in the labels to be shown in a
    // new line.
    ASSERT_EQ(virtual_card_name_field_suggestion.labels[1].size(), 1U);
    EXPECT_EQ(virtual_card_name_field_suggestion.labels[1][0].value,
              u"Virtual card");
  }
}

// Verify that the suggestion's texts are populated correctly for a virtual card
// suggestion when the card number field is focused.
TEST_F(AutofillCreditCardSuggestionContentTest,
       CreateCreditCardSuggestion_VirtualCardMetadata_NumberField) {
  CreditCard server_card = CreateServerCard();

  // Card number field suggestion for virtual cards.
  Suggestion virtual_card_number_field_suggestion =
      suggestion_generator()->CreateCreditCardSuggestion(
          server_card, CREDIT_CARD_NUMBER,
          /*virtual_card_option=*/true,
          /*card_linked_offer_available=*/false);

#if BUILDFLAG(IS_IOS)
  // Only card number is displayed on the first line.
  EXPECT_EQ(
      virtual_card_number_field_suggestion.main_text.value,
      base::StrCat({u"Visa  ", CreditCard::GetObfuscatedStringForCardDigits(
                                   ios_obfuscation_length(), u"1111")}));
  EXPECT_EQ(virtual_card_number_field_suggestion.minor_text.value, u"");
#else
  if (keyboard_accessory_enabled()) {
    // For the keyboard accessory, the "Virtual card" label is added as a prefix
    // to the card number. The obfuscated last four digits are shown in a
    // separate view.
    EXPECT_EQ(virtual_card_number_field_suggestion.main_text.value,
              u"Virtual card  Visa");
    EXPECT_EQ(virtual_card_number_field_suggestion.minor_text.value,
              CreditCard::GetObfuscatedStringForCardDigits(
                  /*obfuscation_length=*/2, u"1111"));
  } else {
    // Card name and the obfuscated last four digits are shown separately.
    EXPECT_EQ(virtual_card_number_field_suggestion.main_text.value, u"Visa");
    EXPECT_EQ(virtual_card_number_field_suggestion.minor_text.value,
              CreditCard::GetObfuscatedStringForCardDigits(
                  /*obfuscation_length=*/4, u"1111"));
  }
#endif

  if (keyboard_accessory_enabled()) {
    // For the keyboard accessory, there is no label.
    ASSERT_TRUE(virtual_card_number_field_suggestion.labels.empty());
  } else {
    // For Desktop/Android dropdown, and on iOS, "Virtual card" is the label.
    ASSERT_EQ(virtual_card_number_field_suggestion.labels.size(), 1U);
    ASSERT_EQ(virtual_card_number_field_suggestion.labels[0].size(), 1U);
    EXPECT_EQ(virtual_card_number_field_suggestion.labels[0][0].value,
              u"Virtual card");
  }
}

// Verify that the suggestion's texts are populated correctly for a masked
// server card suggestion when the cardholder name field is focused.
TEST_F(AutofillCreditCardSuggestionContentTest,
       CreateCreditCardSuggestion_MaskedServerCardMetadata_NameField) {
  CreditCard server_card = CreateServerCard();

  // Name field suggestion for non-virtual cards.
  Suggestion real_card_name_field_suggestion =
      suggestion_generator()->CreateCreditCardSuggestion(
          server_card, CREDIT_CARD_NAME_FULL,
          /*virtual_card_option=*/false,
          /*card_linked_offer_available=*/false);

  // Only the name is displayed on the first line.
  EXPECT_EQ(real_card_name_field_suggestion.main_text.value, u"Elvis Presley");
  EXPECT_EQ(real_card_name_field_suggestion.minor_text.value, u"");

#if BUILDFLAG(IS_IOS)
  // For IOS, the label is "..1111" or "....1111".
  ASSERT_EQ(real_card_name_field_suggestion.labels.size(), 1U);
  ASSERT_EQ(real_card_name_field_suggestion.labels[0].size(), 1U);
  EXPECT_EQ(real_card_name_field_suggestion.labels[0][0].value,
            CreditCard::GetObfuscatedStringForCardDigits(
                ios_obfuscation_length(), u"1111"));
#else
  if (keyboard_accessory_enabled()) {
    // For the keyboard accessory, the label is "..1111".
    ASSERT_EQ(real_card_name_field_suggestion.labels.size(), 1U);
    ASSERT_EQ(real_card_name_field_suggestion.labels[0].size(), 1U);
    EXPECT_EQ(real_card_name_field_suggestion.labels[0][0].value,
              CreditCard::GetObfuscatedStringForCardDigits(
                  /*obfuscation_length=*/2, u"1111"));
  } else {
    // For Desktop/Android, the label is "CardName  ....1111". Card name and
    // last four are shown separately.
    ASSERT_EQ(real_card_name_field_suggestion.labels.size(), 1U);
    ASSERT_EQ(real_card_name_field_suggestion.labels[0].size(), 2U);
    EXPECT_EQ(real_card_name_field_suggestion.labels[0][0].value, u"Visa");
    EXPECT_EQ(real_card_name_field_suggestion.labels[0][1].value,
              CreditCard::GetObfuscatedStringForCardDigits(
                  /*obfuscation_length=*/4, u"1111"));
  }
#endif
}

// Verify that the suggestion's texts are populated correctly for a masked
// server card suggestion when the card number field is focused.
TEST_F(AutofillCreditCardSuggestionContentTest,
       CreateCreditCardSuggestion_MaskedServerCardMetadata_NumberField) {
  CreditCard server_card = CreateServerCard();

  // Card number field suggestion for non-virtual cards.
  Suggestion real_card_number_field_suggestion =
      suggestion_generator()->CreateCreditCardSuggestion(
          server_card, CREDIT_CARD_NUMBER,
          /*virtual_card_option=*/false,
          /*card_linked_offer_available=*/false);

#if BUILDFLAG(IS_IOS)
  // Only the card number is displayed on the first line.
  EXPECT_EQ(
      real_card_number_field_suggestion.main_text.value,
      base::StrCat({u"Visa  ", CreditCard::GetObfuscatedStringForCardDigits(
                                   ios_obfuscation_length(), u"1111")}));
  EXPECT_EQ(real_card_number_field_suggestion.minor_text.value, u"");
#else
  // For Desktop/Android, split the first line and populate the card name and
  // the last 4 digits separately.
  EXPECT_EQ(real_card_number_field_suggestion.main_text.value, u"Visa");
  EXPECT_EQ(real_card_number_field_suggestion.minor_text.value,
            CreditCard::GetObfuscatedStringForCardDigits(
                /*obfuscation_length=*/keyboard_accessory_enabled() ? 2 : 4,
                u"1111"));
#endif

  // The label is the expiration date formatted as mm/yy.
  ASSERT_EQ(real_card_number_field_suggestion.labels.size(), 1U);
  ASSERT_EQ(real_card_number_field_suggestion.labels[0].size(), 1U);
  EXPECT_EQ(real_card_number_field_suggestion.labels[0][0].value,
            base::StrCat({base::UTF8ToUTF16(test::NextMonth()), u"/",
                          base::UTF8ToUTF16(test::NextYear().substr(2))}));
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Verify that the suggestion's texts are populated correctly for a masked
// server card suggestion when payments manual fallback is triggered.
TEST_F(AutofillCreditCardSuggestionContentTest,
       CreateCreditCardSuggestion_ManualFallback) {
  CreditCard server_card = CreateServerCard();

  Suggestion server_card_suggestion =
      suggestion_generator()->CreateCreditCardSuggestion(
          server_card, UNKNOWN_TYPE, /*virtual_card_option=*/false,
          /*card_linked_offer_available=*/false);

  // Only the name is displayed on the first line.
  EXPECT_EQ(server_card_suggestion.popup_item_id,
            PopupItemId::kEntryNotSelectable);
  // For Desktop, split the first line and populate the card name and
  // the last 4 digits separately.
  EXPECT_EQ(server_card_suggestion.main_text.value, u"Visa");
  EXPECT_EQ(server_card_suggestion.minor_text.value,
            server_card.ObfuscatedNumberWithVisibleLastFourDigits(4));

  // The label is the expiration date formatted as mm/yy.
  EXPECT_EQ(server_card_suggestion.labels.size(), 1U);
  EXPECT_EQ(server_card_suggestion.labels[0].size(), 1U);
  EXPECT_EQ(
      server_card_suggestion.labels[0][0].value,
      server_card.GetInfo(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, app_locale()));

  EXPECT_EQ(server_card_suggestion.acceptance_a11y_announcement,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_A11Y_ANNOUNCE_EXPANDABLE_ONLY_ENTRY));
}

// Verify that the nested suggestion's texts are populated correctly for a
// masked server card suggestion when payments manual fallback is triggered.
TEST_F(AutofillCreditCardSuggestionContentTest,
       CreateCreditCardSuggestion_ManualFallback_NestedSuggestions) {
  CreditCard server_card = test::GetMaskedServerCard();

  Suggestion server_card_suggestion =
      suggestion_generator()->CreateCreditCardSuggestion(
          server_card, UNKNOWN_TYPE, /*virtual_card_option=*/false,
          /*card_linked_offer_available=*/false);

  // The child suggestions should be:
  //
  // 1. Credit card full name
  // 2. Credit card number
  // 3. Separator
  // 4. Credit card expiry date
  EXPECT_THAT(
      server_card_suggestion.children,
      ElementsAre(
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kFieldByFieldFilling,
              server_card.GetInfo(CREDIT_CARD_NAME_FULL, app_locale()),
              CREDIT_CARD_NAME_FULL, Suggestion::Guid(server_card.guid())),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kFieldByFieldFilling,
              server_card.ObfuscatedNumberWithVisibleLastFourDigits(12),
              CREDIT_CARD_NUMBER, Suggestion::Guid(server_card.guid()),
              {{Suggestion::Text(l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_PAYMENTS_MANUAL_FALLBACK_AUTOFILL_POPUP_CC_NUMBER_SUGGESTION_LABEL))}}),
          AllOf(Field(&Suggestion::popup_item_id, PopupItemId::kSeparator)),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kFieldByFieldFilling,
              server_card.GetInfo(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
                                  app_locale()),
              CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
              Suggestion::Guid(server_card.guid()),
              {{Suggestion::Text(l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_PAYMENTS_MANUAL_FALLBACK_AUTOFILL_POPUP_CC_EXPIRY_DATE_SUGGESTION_LABEL))}})));
}

// Verify that the nested suggestion's texts are populated correctly for a
// credit card with no expiry date set.
TEST_F(
    AutofillCreditCardSuggestionContentTest,
    CreateCreditCardSuggestion_ManualFallback_NoExpiryDate_NestedSuggestions) {
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, /*name_on_card=*/"Cardholder name",
                          /*card_number=*/"1111222233334444",
                          /*expiration_month=*/nullptr,
                          /*expiration_year*/ nullptr,
                          /*billing_address_id=*/"", /*cvc=*/u"123");

  Suggestion server_card_suggestion =
      suggestion_generator()->CreateCreditCardSuggestion(
          credit_card, UNKNOWN_TYPE, /*virtual_card_option=*/false,
          /*card_linked_offer_available=*/false);

  // The child suggestions should be:
  //
  // 1. Credit card full name
  // 2. Credit card number
  EXPECT_THAT(
      server_card_suggestion.children,
      ElementsAre(
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kFieldByFieldFilling,
              credit_card.GetInfo(CREDIT_CARD_NAME_FULL, app_locale()),
              CREDIT_CARD_NAME_FULL, Suggestion::Guid(credit_card.guid())),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kFieldByFieldFilling,
              credit_card.ObfuscatedNumberWithVisibleLastFourDigits(12),
              CREDIT_CARD_NUMBER, Suggestion::Guid(credit_card.guid()),
              {{Suggestion::Text(l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_PAYMENTS_MANUAL_FALLBACK_AUTOFILL_POPUP_CC_NUMBER_SUGGESTION_LABEL))}})));
}

// Verify that the nested suggestion's texts are populated correctly for a
// credit card with no cardholder name and credit card number.
TEST_F(
    AutofillCreditCardSuggestionContentTest,
    CreateCreditCardSuggestion_ManualFallback_NoNameAndNumber_NestedSuggestions) {
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, /*name_on_card=*/nullptr,
                          /*card_number=*/nullptr, test::NextMonth().c_str(),
                          test::NextYear().c_str(),
                          /*billing_address_id=*/"", /*cvc=*/u"123");

  Suggestion server_card_suggestion =
      suggestion_generator()->CreateCreditCardSuggestion(
          credit_card, UNKNOWN_TYPE, /*virtual_card_option=*/false,
          /*card_linked_offer_available=*/false);

  // The child suggestions should be:
  //
  // 1. Credit card expiry date
  EXPECT_THAT(
      server_card_suggestion.children,
      ElementsAre(EqualsFieldByFieldFillingSuggestion(
          PopupItemId::kFieldByFieldFilling,
          credit_card.GetInfo(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, app_locale()),
          CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
          Suggestion::Guid(credit_card.guid()),
          {{Suggestion::Text(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_PAYMENTS_MANUAL_FALLBACK_AUTOFILL_POPUP_CC_EXPIRY_DATE_SUGGESTION_LABEL))}})));
}

// Verify nested suggestions of the expiry date suggestion.
TEST_F(AutofillCreditCardSuggestionContentTest,
       CreateCreditCardSuggestion_ManualFallback_NestedExpiryDateSuggestions) {
  CreditCard server_card = CreateServerCard();

  Suggestion server_card_suggestion =
      suggestion_generator()->CreateCreditCardSuggestion(
          server_card, UNKNOWN_TYPE, /*virtual_card_option=*/false,
          /*card_linked_offer_available=*/false);

  // The expiry date child suggestions should be:
  //
  // 1. Expiry year.
  // 2. Expiry month.
  EXPECT_THAT(
      server_card_suggestion.children[3].children,
      ElementsAre(
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kFieldByFieldFilling,
              server_card.GetInfo(CREDIT_CARD_EXP_2_DIGIT_YEAR, app_locale()),
              CREDIT_CARD_EXP_2_DIGIT_YEAR,
              Suggestion::Guid(server_card.guid()),
              {{Suggestion::Text(l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_PAYMENTS_MANUAL_FALLBACK_AUTOFILL_POPUP_CC_EXPIRY_YEAR_SUGGESTION_LABEL))}}),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kFieldByFieldFilling,
              server_card.GetInfo(CREDIT_CARD_EXP_MONTH, app_locale()),
              CREDIT_CARD_EXP_MONTH, Suggestion::Guid(server_card.guid()),
              {{Suggestion::Text(l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_PAYMENTS_MANUAL_FALLBACK_AUTOFILL_POPUP_CC_EXPIRY_MONTH_SUGGESTION_LABEL))}})));
}

// Verify that manual fallback credit card suggestions are not filtered.
TEST_F(AutofillCreditCardSuggestionContentTest,
       GetSuggestionsForCreditCards_ManualFallbackSuggestionsNotFiltered) {
  personal_data()->AddServerCreditCard(CreateServerCard());

  FormFieldData field_data;
  field_data.value = u"$$$";
  bool should_display_gpay_logo;
  bool with_offer;
  autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
  std::vector<Suggestion> suggestions =
      suggestion_generator()->GetSuggestionsForCreditCards(
          field_data, UNKNOWN_TYPE, should_display_gpay_logo, with_offer,
          metadata_logging_context);

  // Credit card suggestions should not depend on the field's value.
  EXPECT_EQ(suggestions.size(), 1U);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_ANDROID)
// Verify that the suggestion's texts are populated correctly for a local and
// server card suggestion when the CVC field is focused.
TEST_F(AutofillCreditCardSuggestionContentTest,
       GetSuggestionsForCreditCards_CvcField) {
  // Create one server card and one local card with CVC.
  CreditCard local_card = CreateLocalCard();
  // We used last 4 to deduplicate local card and server card so we should set
  // local card with different last 4.
  local_card.SetNumber(u"5454545454545454");
  personal_data()->AddCreditCard(std::move(local_card));
  personal_data()->AddServerCreditCard(CreateServerCard());

  bool should_display_gpay_logo;
  bool with_offer;
  autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
  const std::vector<Suggestion> suggestions =
      suggestion_generator()->GetSuggestionsForCreditCards(
          FormFieldData(), CREDIT_CARD_VERIFICATION_CODE,
          should_display_gpay_logo, with_offer, metadata_logging_context);

  // Both local card and server card suggestion should be shown when CVC field
  // is focused.
  ASSERT_EQ(suggestions.size(), 2U);
  EXPECT_EQ(suggestions[0].main_text.value,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_CVC_SUGGESTION_MAIN_TEXT));
  EXPECT_EQ(suggestions[1].main_text.value,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_CVC_SUGGESTION_MAIN_TEXT));
}

// Verify that the suggestion's texts are populated correctly for a duplicate
// local and server card suggestion when the CVC field is focused.
TEST_F(AutofillCreditCardSuggestionContentTest,
       GetSuggestionsForCreditCards_Duplicate_CvcField) {
  // Create 2 duplicate local and server card with same last 4.
  personal_data()->AddCreditCard(CreateLocalCard());
  personal_data()->AddServerCreditCard(CreateServerCard());

  bool should_display_gpay_logo;
  bool with_offer;
  autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
  const std::vector<Suggestion> suggestions =
      suggestion_generator()->GetSuggestionsForCreditCards(
          FormFieldData(), CREDIT_CARD_VERIFICATION_CODE,
          should_display_gpay_logo, with_offer, metadata_logging_context);

  // Only 1 suggestion should be shown when CVC field is focused.
  ASSERT_EQ(suggestions.size(), 1U);
  EXPECT_EQ(suggestions[0].main_text.value,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_CVC_SUGGESTION_MAIN_TEXT));
}

// Verify that the FPAN and VCN suggestion's texts are populated correctly for a
// enrolled card when the CVC field is focused.
TEST_F(AutofillCreditCardSuggestionContentTest,
       GetSuggestionsForCreditCards_VirtualCard_CvcField) {
  // Create a server card with CVC that enrolled to virtual card.
  CreditCard server_card = CreateServerCard();
  server_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  personal_data()->AddServerCreditCard(std::move(server_card));

  bool should_display_gpay_logo;
  bool with_offer;
  autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
  const std::vector<Suggestion> suggestions =
      suggestion_generator()->GetSuggestionsForCreditCards(
          FormFieldData(), CREDIT_CARD_VERIFICATION_CODE,
          should_display_gpay_logo, with_offer, metadata_logging_context);

  // Both FPAN and VCN suggestion should be shown when CVC field is focused.
  ASSERT_EQ(suggestions.size(), 2U);
  EXPECT_EQ(suggestions[0].main_text.value,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_CVC_SUGGESTION_MAIN_TEXT));
  EXPECT_EQ(suggestions[1].main_text.value,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_CVC_SUGGESTION_MAIN_TEXT));
}

// Verify that the FPAN and VCN suggestion's texts are populated correctly for a
// enrolled card when the CVC field is focused.
TEST_F(AutofillCreditCardSuggestionContentTest,
       GetSuggestionsForCreditCards_VirtualCard_Duplicate_CvcField) {
  // Create duplicate local and server card with CVC that enrolled to virtual
  // card.
  CreditCard server_card = CreateServerCard();
  server_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  personal_data()->AddServerCreditCard(std::move(server_card));
  personal_data()->AddCreditCard(CreateLocalCard());

  bool should_display_gpay_logo;
  bool with_offer;
  autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
  const std::vector<Suggestion> suggestions =
      suggestion_generator()->GetSuggestionsForCreditCards(
          FormFieldData(), CREDIT_CARD_VERIFICATION_CODE,
          should_display_gpay_logo, with_offer, metadata_logging_context);

  // Both FPAN and VCN suggestion should be shown when CVC field is focused.
  ASSERT_EQ(suggestions.size(), 2U);
  EXPECT_EQ(suggestions[0].main_text.value,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_CVC_SUGGESTION_MAIN_TEXT));
  EXPECT_EQ(suggestions[1].main_text.value,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_CVC_SUGGESTION_MAIN_TEXT));
}

#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_IOS)
// Tests that credit card suggestions on iOS use the correct number of '•'
// characters depending on the kAutofillUseTwoDotsForLastFourDigits feature.
class AutofillCreditCardSuggestionIOSObfuscationLengthContentTest
    : public AutofillSuggestionGeneratorTest,
      public testing::WithParamInterface<bool> {
 public:
  AutofillCreditCardSuggestionIOSObfuscationLengthContentTest() {
    feature_list_.InitWithFeatureState(
        features::kAutofillUseTwoDotsForLastFourDigits, GetParam());
  }

  ~AutofillCreditCardSuggestionIOSObfuscationLengthContentTest() override =
      default;

  int expected_obfuscation_length() const { return GetParam() ? 2 : 4; }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    AutofillCreditCardSuggestionContentTest,
    AutofillCreditCardSuggestionIOSObfuscationLengthContentTest,
    testing::Bool());

TEST_P(AutofillCreditCardSuggestionIOSObfuscationLengthContentTest,
       CreateCreditCardSuggestion_CorrectObfuscationLength) {
  CreditCard server_card = CreateServerCard();

  // Name field suggestion.
  Suggestion card_name_field_suggestion =
      suggestion_generator()->CreateCreditCardSuggestion(
          server_card, CREDIT_CARD_NAME_FULL,
          /*virtual_card_option=*/false,
          /*card_linked_offer_available=*/false);

  ASSERT_EQ(card_name_field_suggestion.labels.size(), 1U);
  ASSERT_EQ(card_name_field_suggestion.labels[0].size(), 1U);
  EXPECT_EQ(card_name_field_suggestion.labels[0][0].value,
            CreditCard::GetObfuscatedStringForCardDigits(
                expected_obfuscation_length(), u"1111"));

  // Card number field suggestion.
  Suggestion card_number_field_suggestion =
      suggestion_generator()->CreateCreditCardSuggestion(
          server_card, CREDIT_CARD_NUMBER,
          /*virtual_card_option=*/false,
          /*card_linked_offer_available=*/false);

  EXPECT_EQ(
      card_number_field_suggestion.main_text.value,
      base::StrCat({u"Visa  ", CreditCard::GetObfuscatedStringForCardDigits(
                                   expected_obfuscation_length(), u"1111")}));
}

#endif  // BUILDFLAG(IS_IOS)

class AutofillSuggestionGeneratorTestForMetadata
    : public AutofillSuggestionGeneratorTest,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  AutofillSuggestionGeneratorTestForMetadata() {
    feature_list_card_product_description_.InitWithFeatureState(
        features::kAutofillEnableCardProductName, std::get<0>(GetParam()));
    feature_list_card_art_image_.InitWithFeatureState(
        features::kAutofillEnableCardArtImage, std::get<1>(GetParam()));
  }

  ~AutofillSuggestionGeneratorTestForMetadata() override = default;

  bool card_product_description_enabled() const {
    return std::get<0>(GetParam());
  }
  bool card_art_image_enabled() const { return std::get<1>(GetParam()); }
  bool card_has_capital_one_icon() const { return std::get<2>(GetParam()); }

 private:
  base::test::ScopedFeatureList feature_list_card_product_description_;
  base::test::ScopedFeatureList feature_list_card_art_image_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         AutofillSuggestionGeneratorTestForMetadata,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

TEST_P(AutofillSuggestionGeneratorTestForMetadata,
       CreateCreditCardSuggestion_ServerCard) {
  // Create a server card.
  CreditCard server_card = CreateServerCard();
  GURL card_art_url = GURL("https://www.example.com/card-art");
  server_card.set_card_art_url(card_art_url);
  gfx::Image fake_image = CustomIconForTest();
  personal_data()->AddCardArtImage(card_art_url, fake_image);

  Suggestion virtual_card_suggestion =
      suggestion_generator()->CreateCreditCardSuggestion(
          server_card, CREDIT_CARD_NUMBER,
          /*virtual_card_option=*/true,
          /*card_linked_offer_available=*/false);

  EXPECT_EQ(virtual_card_suggestion.popup_item_id,
            PopupItemId::kVirtualCreditCardEntry);
  EXPECT_EQ(virtual_card_suggestion.GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(
                Suggestion::Guid("00000000-0000-0000-0000-000000000001")));
  EXPECT_EQ(VerifyCardArtImageExpectation(virtual_card_suggestion, card_art_url,
                                          fake_image),
            card_art_image_enabled());

  Suggestion real_card_suggestion =
      suggestion_generator()->CreateCreditCardSuggestion(
          server_card, CREDIT_CARD_NUMBER,
          /*virtual_card_option=*/false,
          /*card_linked_offer_available=*/false);

  EXPECT_EQ(real_card_suggestion.popup_item_id, PopupItemId::kCreditCardEntry);
  EXPECT_EQ(real_card_suggestion.GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(
                Suggestion::Guid("00000000-0000-0000-0000-000000000001")));
  EXPECT_EQ(VerifyCardArtImageExpectation(real_card_suggestion, card_art_url,
                                          fake_image),
            card_art_image_enabled());
}

TEST_P(AutofillSuggestionGeneratorTestForMetadata,
       CreateCreditCardSuggestion_LocalCard_NoServerDuplicate) {
  // Create a local card.
  CreditCard local_card = CreateLocalCard();

  Suggestion real_card_suggestion =
      suggestion_generator()->CreateCreditCardSuggestion(
          local_card, CREDIT_CARD_NUMBER,
          /*virtual_card_option=*/false,
          /*card_linked_offer_available=*/false);

  EXPECT_EQ(real_card_suggestion.popup_item_id, PopupItemId::kCreditCardEntry);
  EXPECT_EQ(real_card_suggestion.GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(
                Suggestion::Guid("00000000-0000-0000-0000-000000000001")));
  EXPECT_TRUE(VerifyCardArtImageExpectation(real_card_suggestion, GURL(),
                                            gfx::Image()));
}

TEST_P(AutofillSuggestionGeneratorTestForMetadata,
       CreateCreditCardSuggestion_LocalCard_ServerDuplicate) {
  // Create a server card.
  CreditCard server_card =
      CreateServerCard(/*guid=*/"00000000-0000-0000-0000-000000000001");

  GURL card_art_url = GURL("https://www.example.com/card-art");
  server_card.set_card_art_url(card_art_url);
  gfx::Image fake_image = CustomIconForTest();
  personal_data()->AddServerCreditCard(server_card);
  personal_data()->AddCardArtImage(card_art_url, fake_image);

  // Create a local card with same information.
  CreditCard local_card =
      CreateLocalCard(/*guid=*/"00000000-0000-0000-0000-000000000002");

  Suggestion virtual_card_suggestion =
      suggestion_generator()->CreateCreditCardSuggestion(
          local_card, CREDIT_CARD_NUMBER,
          /*virtual_card_option=*/true,
          /*card_linked_offer_available=*/false);

  EXPECT_EQ(virtual_card_suggestion.popup_item_id,
            PopupItemId::kVirtualCreditCardEntry);
  EXPECT_EQ(virtual_card_suggestion.GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(
                Suggestion::Guid("00000000-0000-0000-0000-000000000001")));
  EXPECT_EQ(VerifyCardArtImageExpectation(virtual_card_suggestion, card_art_url,
                                          fake_image),
            card_art_image_enabled());

  Suggestion real_card_suggestion =
      suggestion_generator()->CreateCreditCardSuggestion(
          local_card, CREDIT_CARD_NUMBER,
          /*virtual_card_option=*/false,
          /*card_linked_offer_available=*/false);

  EXPECT_EQ(real_card_suggestion.popup_item_id, PopupItemId::kCreditCardEntry);
  EXPECT_EQ(real_card_suggestion.GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(
                Suggestion::Guid("00000000-0000-0000-0000-000000000002")));
  EXPECT_EQ(VerifyCardArtImageExpectation(real_card_suggestion, card_art_url,
                                          fake_image),
            card_art_image_enabled());
}

// Verifies that the `metadata_logging_context` is correctly set.
TEST_P(AutofillSuggestionGeneratorTestForMetadata,
       GetSuggestionsForCreditCards_MetadataLoggingContext) {
  {
    // Create one server card with no metadata.
    CreditCard server_card = CreateServerCard();
    server_card.set_issuer_id(kCapitalOneCardIssuerId);
    if (card_has_capital_one_icon()) {
      server_card.set_card_art_url(GURL(kCapitalOneCardArtUrl));
    }
    personal_data()->AddServerCreditCard(server_card);

    bool should_display_gpay_logo;
    bool with_offer;
    autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
    suggestion_generator()->GetSuggestionsForCreditCards(
        FormFieldData(), CREDIT_CARD_NUMBER, should_display_gpay_logo,
        with_offer, metadata_logging_context);

    EXPECT_FALSE(metadata_logging_context.card_metadata_available);
    EXPECT_FALSE(metadata_logging_context.card_product_description_shown);
    EXPECT_FALSE(metadata_logging_context.card_art_image_shown);

    // Verify that a record is added that a Capital One card suggestion
    // was generated, and it did not have metadata.
    base::flat_map<std::string, bool> expected_issuer_to_metadata_availability =
        {{kCapitalOneCardIssuerId, false}};
    EXPECT_EQ(metadata_logging_context.issuer_to_metadata_availability,
              expected_issuer_to_metadata_availability);
  }

  personal_data()->ClearCreditCards();

  {
    // Create a server card with card product description & card art image.
    CreditCard server_card_with_metadata = CreateServerCard();
    server_card_with_metadata.set_issuer_id(kCapitalOneCardIssuerId);
    server_card_with_metadata.set_product_description(u"product_description");
    server_card_with_metadata.set_card_art_url(
        GURL("https://www.example.com/card-art.png"));
    personal_data()->AddServerCreditCard(server_card_with_metadata);

    bool should_display_gpay_logo;
    bool with_offer;
    autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
    suggestion_generator()->GetSuggestionsForCreditCards(
        FormFieldData(), CREDIT_CARD_NUMBER, should_display_gpay_logo,
        with_offer, metadata_logging_context);

    EXPECT_TRUE(metadata_logging_context.card_metadata_available);
    EXPECT_EQ(metadata_logging_context.card_product_description_shown,
              card_product_description_enabled());
    EXPECT_EQ(metadata_logging_context.card_art_image_shown,
              card_art_image_enabled());

    // Verify that a record is added that a Capital One card suggestion
    // was generated, and it had metadata.
    base::flat_map<std::string, bool> expected_issuer_to_metadata_availability =
        {{kCapitalOneCardIssuerId, true}};
    EXPECT_EQ(metadata_logging_context.issuer_to_metadata_availability,
              expected_issuer_to_metadata_availability);
  }
}

// Verifies that the custom icon is set correctly. The card art should be shown
// when the metadata card art flag is enabled. Capital One virtual card icon is
// an exception which should only and always be shown for virtual cards.
TEST_P(AutofillSuggestionGeneratorTestForMetadata,
       CreateCreditCardSuggestion_CustomCardIcon) {
  // Create a server card.
  CreditCard server_card = CreateServerCard();
  GURL card_art_url =
      GURL(card_has_capital_one_icon() ? kCapitalOneCardArtUrl
                                       : "https://www.example.com/card-art");
  server_card.set_card_art_url(card_art_url);
  gfx::Image fake_image = CustomIconForTest();
  personal_data()->AddCardArtImage(card_art_url, fake_image);

  Suggestion virtual_card_suggestion =
      suggestion_generator()->CreateCreditCardSuggestion(
          server_card, CREDIT_CARD_NUMBER,
          /*virtual_card_option=*/true,
          /*card_linked_offer_available=*/false);

  // Verify that for virtual cards, the custom icon is shown if the card art is
  // the Capital One virtual card art or if the metadata card art is enabled.
  EXPECT_EQ(VerifyCardArtImageExpectation(virtual_card_suggestion, card_art_url,
                                          fake_image),
            card_has_capital_one_icon() || card_art_image_enabled());

  Suggestion real_card_suggestion =
      suggestion_generator()->CreateCreditCardSuggestion(
          server_card, CREDIT_CARD_NUMBER,
          /*virtual_card_option=*/false,
          /*card_linked_offer_available=*/false);

  // Verify that for FPAN, the custom icon is shown if the card art is not the
  // Capital One virtual card art and the metadata card art is enabled.
  EXPECT_EQ(VerifyCardArtImageExpectation(real_card_suggestion, card_art_url,
                                          fake_image),
            !card_has_capital_one_icon() && card_art_image_enabled());
}

class AutofillSuggestionGeneratorTestForOffer
    : public AutofillSuggestionGeneratorTest,
      public testing::WithParamInterface<bool> {
 public:
  AutofillSuggestionGeneratorTestForOffer() {
#if BUILDFLAG(IS_ANDROID)
    keyboard_accessory_offer_enabled_ = GetParam();
    if (keyboard_accessory_offer_enabled_) {
      scoped_feature_keyboard_accessory_offer_.InitWithFeatures(
          {features::kAutofillEnableOffersInClankKeyboardAccessory}, {});
    } else {
      scoped_feature_keyboard_accessory_offer_.InitWithFeatures(
          {}, {features::kAutofillEnableOffersInClankKeyboardAccessory});
    }
#endif
  }
  ~AutofillSuggestionGeneratorTestForOffer() override = default;

  bool keyboard_accessory_offer_enabled() {
#if BUILDFLAG(IS_ANDROID)
    return keyboard_accessory_offer_enabled_;
#else
    return false;
#endif
  }

#if BUILDFLAG(IS_ANDROID)
 private:
  bool keyboard_accessory_offer_enabled_;
  base::test::ScopedFeatureList scoped_feature_keyboard_accessory_offer_;
#endif
};

INSTANTIATE_TEST_SUITE_P(All,
                         AutofillSuggestionGeneratorTestForOffer,
                         testing::Bool());

// Test to make sure the suggestion gets populated with the right content if the
// card has card linked offer available.
TEST_P(AutofillSuggestionGeneratorTestForOffer,
       CreateCreditCardSuggestion_ServerCardWithOffer) {
  // Create a server card.
  CreditCard server_card1 =
      CreateServerCard(/*guid=*/"00000000-0000-0000-0000-000000000001");

  Suggestion virtual_card_suggestion =
      suggestion_generator()->CreateCreditCardSuggestion(
          server_card1, CREDIT_CARD_NUMBER,
          /*virtual_card_option=*/true,
          /*card_linked_offer_available=*/true);

  EXPECT_EQ(virtual_card_suggestion.popup_item_id,
            PopupItemId::kVirtualCreditCardEntry);
  EXPECT_EQ(virtual_card_suggestion.GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(
                Suggestion::Guid("00000000-0000-0000-0000-000000000001")));
  // Ensures CLO text is not shown for virtual card option.
  EXPECT_EQ(virtual_card_suggestion.labels.size(), 1U);

  Suggestion real_card_suggestion =
      suggestion_generator()->CreateCreditCardSuggestion(
          server_card1, CREDIT_CARD_NUMBER,
          /*virtual_card_option=*/false,
          /*card_linked_offer_available=*/true);

  EXPECT_EQ(real_card_suggestion.popup_item_id, PopupItemId::kCreditCardEntry);
  EXPECT_EQ(real_card_suggestion.GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(
                Suggestion::Guid("00000000-0000-0000-0000-000000000001")));

  if (keyboard_accessory_offer_enabled()) {
#if BUILDFLAG(IS_ANDROID)
    EXPECT_EQ(real_card_suggestion.labels.size(), 1U);
    EXPECT_EQ(
        real_card_suggestion.feature_for_iph,
        feature_engagement::kIPHKeyboardAccessoryPaymentOfferFeature.name);
#endif
  } else {
    ASSERT_EQ(real_card_suggestion.labels.size(), 2U);
    ASSERT_EQ(real_card_suggestion.labels[1].size(), 1U);
    EXPECT_EQ(real_card_suggestion.labels[1][0].value,
              l10n_util::GetStringUTF16(IDS_AUTOFILL_OFFERS_CASHBACK));
  }
}

}  // namespace autofill
