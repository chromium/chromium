// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_suggestion_generator.h"

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/autofill_granular_filling_utils.h"
#include "components/autofill/core/browser/autofill_suggestion_generator_test_api.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_benefit.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/browser/metrics/payments/card_metadata_metrics.h"
#include "components/autofill/core/browser/mock_autofill_optimization_guide.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/personal_data_manager_test_base.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_test_helpers.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/form_field_data.h"
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

using testing::ElementsAreArray;
using testing::Field;
using testing::IsEmpty;
using testing::Matcher;
using testing::UnorderedElementsAreArray;

constexpr auto kDefaultTriggerSource =
    AutofillSuggestionTriggerSource::kFormControlElementClicked;

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
    PopupItemId id,
    const std::u16string& main_text,
    FieldType field_by_field_filling_type_used,
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
      EqualLabels(labels));
}

Matcher<Suggestion> EqualsIbanSuggestion(
    const std::u16string& text,
    const Suggestion::Payload& payload,
    const std::u16string& first_label_value) {
  return AllOf(Field(&Suggestion::popup_item_id, PopupItemId::kIbanEntry),
               Field(&Suggestion::main_text,
                     Suggestion::Text(text, Suggestion::Text::IsPrimary(true))),
               Field(&Suggestion::payload, payload),
               EqualLabels(first_label_value.empty()
                               ? std::vector<std::vector<Suggestion::Text>>{}
                               : std::vector<std::vector<Suggestion::Text>>{
                                     {Suggestion::Text(first_label_value)}}));
}

Matcher<Suggestion> EqualsClearFormSuggestion() {
  return EqualsSuggestion(PopupItemId::kClearForm,
#if BUILDFLAG(IS_ANDROID)
                          base::i18n::ToUpper(l10n_util::GetStringUTF16(
                              IDS_AUTOFILL_CLEAR_FORM_MENU_ITEM)),
#else
                          l10n_util::GetStringUTF16(
                              IDS_AUTOFILL_CLEAR_FORM_MENU_ITEM),
#endif
                          Suggestion::Icon::kClear);
}

#if !BUILDFLAG(IS_IOS)
Matcher<Suggestion> EqualsUndoAutofillSuggestion() {
  return EqualsSuggestion(PopupItemId::kClearForm,
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
      PopupItemId::kAutofillOptions,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_ADDRESSES),
      Suggestion::Icon::kSettings);
}

Matcher<Suggestion> EqualsManagePaymentsMethodsSuggestion(bool with_gpay_logo) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return EqualsSuggestion(
      PopupItemId::kAutofillOptions,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_PAYMENT_METHODS),
      with_gpay_logo ? Suggestion::Icon::kGooglePay
                     : Suggestion::Icon::kSettings);
#else
  return AllOf(EqualsSuggestion(PopupItemId::kAutofillOptions,
                                l10n_util::GetStringUTF16(
                                    IDS_AUTOFILL_MANAGE_PAYMENT_METHODS),
                                Suggestion::Icon::kSettings),
               Field(&Suggestion::trailing_icon,
                     with_gpay_logo ? Suggestion::Icon::kGooglePay
                                    : Suggestion::Icon::kNoIcon));
#endif
}

// Checks that `arg` contains necessary credit card footer suggestions. `arg`
// has to be of type std::vector<Suggestion>.
MATCHER_P(ContainsCreditCardFooterSuggestions, with_gpay_logo, "") {
  EXPECT_GT(arg.size(), 2ul);
  EXPECT_THAT(arg[arg.size() - 2], EqualsSuggestion(PopupItemId::kSeparator));
  EXPECT_THAT(arg.back(),
              EqualsManagePaymentsMethodsSuggestion(with_gpay_logo));
  return true;
}

// Checks that `arg` contains necessary address footer suggestions. `arg`
// has to be of type std::vector<Suggestion>.
MATCHER(ContainsAddressFooterSuggestions, "") {
  EXPECT_GT(arg.size(), 2ul);
  EXPECT_THAT(arg[arg.size() - 2], EqualsSuggestion(PopupItemId::kSeparator));
  EXPECT_THAT(arg.back(), EqualsManageAddressesSuggestion());
  return true;
}

}  // namespace

// TODO(crbug.com/1196021): Move GetSuggestionsForCreditCard tests and
// BrowserAutofillManagerTestForSharingNickname here from
// browser_autofill_manager_unittest.cc.
class AutofillSuggestionGeneratorTest : public testing::Test {
 public:
  void SetUp() override {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    personal_data().SetPrefService(autofill_client_.GetPrefs());
    personal_data().SetSyncServiceForTest(&sync_service_);
    suggestion_generator_ =
        std::make_unique<AutofillSuggestionGenerator>(autofill_client_);
    autofill_client_.set_autofill_offer_manager(
        std::make_unique<AutofillOfferManager>(
            &personal_data(),
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

  AutofillSuggestionGenerator& suggestion_generator() {
    return *suggestion_generator_.get();
  }

  TestPersonalDataManager& personal_data() {
    return *autofill_client_.GetPersonalDataManager();
  }

  const std::string& app_locale() { return personal_data().app_locale(); }

  TestAutofillClient* autofill_client() { return &autofill_client_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::SYSTEM_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClient autofill_client_;
  syncer::TestSyncService sync_service_;
  std::unique_ptr<AutofillSuggestionGenerator> suggestion_generator_;
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
  personal_data().AddProfile(profile_1);
  personal_data().AddProfile(profile_2);
  ASSERT_EQ(
      personal_data().address_data_manager().GetProfilesToSuggest().size(), 2u);

  std::vector<raw_ptr<const AutofillProfile, VectorExperimental>> profiles =
      test_api(suggestion_generator())
          .GetProfilesToSuggest(EMAIL_ADDRESS, u"Test@", false, {});

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
  personal_data().AddProfile(profile);
  personal_data().AddProfile(profile1);
  personal_data().AddProfile(profile2);
  personal_data().AddProfile(profile3);

  // Simulate a form with street address, city and state.
  FieldTypeSet types = {ADDRESS_HOME_CITY, ADDRESS_HOME_STATE};
  std::vector<raw_ptr<const AutofillProfile, VectorExperimental>> profiles =
      test_api(suggestion_generator())
          .GetProfilesToSuggest(ADDRESS_HOME_STREET_ADDRESS, u"123", false,
                                types);
  ASSERT_EQ(2U, profiles.size());
  EXPECT_EQ(profiles[0]->GetRawInfo(ADDRESS_HOME_STATE), u"CA");
  EXPECT_EQ(profiles[1]->GetRawInfo(ADDRESS_HOME_STATE), u"TX");
}

// Drawing takes noticeable time when there are more than 10 profiles.
// Therefore, we keep only the 10 first suggested profiles.
TEST_F(AutofillSuggestionGeneratorTest, GetProfilesToSuggest_SuggestionsLimit) {
  std::vector<AutofillProfile> profiles;
  for (size_t i = 0; i < 2 * kMaxDeduplicatedProfilesForSuggestion; i++) {
    AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
    test::SetProfileInfo(&profile, base::StringPrintf("Marion%zu", i).c_str(),
                         "Mitchell", "Morrison", "johnwayne@me.xyz", "Fox",
                         "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                         "Hollywood", "CA", "91601", "US", "12345678910");
    personal_data().AddProfile(profile);
    profiles.push_back(profile);
  }

  std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
      suggested_profiles =
          test_api(suggestion_generator())
              .GetProfilesToSuggest(NAME_FIRST, u"Ma", false, {});

  ASSERT_EQ(2 * kMaxDeduplicatedProfilesForSuggestion,
            personal_data().GetProfiles().size());
  ASSERT_EQ(kMaxDeduplicatedProfilesForSuggestion, suggested_profiles.size());
}

// Deduping takes noticeable time when there are more than 50 profiles.
// Therefore, keep only the 50 first pre-dedupe matching profiles.
TEST_F(AutofillSuggestionGeneratorTest, GetProfilesToSuggest_ProfilesLimit) {
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

    personal_data().AddProfile(profile);
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
  personal_data().AddProfile(profile);

  std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
      suggested_profiles =
          test_api(suggestion_generator())
              .GetProfilesToSuggest(NAME_FIRST, u"Ma", false, {});

  ASSERT_EQ(kMaxPrefixMatchedProfilesForSuggestion + 1,
            personal_data().GetProfiles().size());
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
  personal_data().AddProfile(profile3);

  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile1, "Marion1", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile1.set_use_date(AutofillClock::Now() - base::Days(1));
  profile1.set_use_count(10);
  personal_data().AddProfile(profile1);

  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile2, "Marion2", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile2.set_use_date(AutofillClock::Now() - base::Days(15));
  profile2.set_use_count(300);
  personal_data().AddProfile(profile2);

  std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
      suggested_profiles =
          test_api(suggestion_generator())
              .GetProfilesToSuggest(NAME_FIRST, u"Ma", false, {});
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
  personal_data().AddProfile(profile1);

  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile2, "Marion2", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  personal_data().AddProfile(profile2);

  AutofillProfile profile3(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile3, "Marion3", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  personal_data().AddProfile(profile3);

  // Verify that all the profiles are suggested.
  std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
      suggested_profiles =
          test_api(suggestion_generator())
              .GetProfilesToSuggest(NAME_FIRST, std::u16string(), false, {});
  EXPECT_EQ(3U, suggested_profiles.size());
}

// Tests that phone number types are correctly deduplicated for suggestions.
TEST_F(AutofillSuggestionGeneratorTest,
       GetProfilesToSuggest_PhoneNumberDeduplication) {
  // Set up 2 different profiles.
  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile1.SetRawInfo(NAME_FULL, u"First Middle Last");
  profile1.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"+491601234567");
  personal_data().AddProfile(profile1);

  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile2.SetRawInfo(NAME_FULL, u"First Middle Last");
  profile2.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"+491607654321");
  personal_data().AddProfile(profile2);

  {
    std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
        suggested_profiles =
            test_api(suggestion_generator())
                .GetProfilesToSuggest(NAME_FULL, std::u16string(), false,
                                      {NAME_FULL, PHONE_HOME_WHOLE_NUMBER});
    EXPECT_EQ(2U, suggested_profiles.size());
  }
  {
    std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
        suggested_profiles =
            test_api(suggestion_generator())
                .GetProfilesToSuggest(NAME_FULL, std::u16string(), false,
                                      {NAME_FULL, PHONE_HOME_COUNTRY_CODE,
                                       PHONE_HOME_CITY_AND_NUMBER});
    EXPECT_EQ(2U, suggested_profiles.size());
  }
  {
    std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
        suggested_profiles = test_api(suggestion_generator())
                                 .GetProfilesToSuggest(
                                     NAME_FULL, std::u16string(), false,
                                     {NAME_FULL, PHONE_HOME_COUNTRY_CODE,
                                      PHONE_HOME_CITY_CODE, PHONE_HOME_NUMBER});
    EXPECT_EQ(2U, suggested_profiles.size());
  }
  {
    std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
        suggested_profiles =
            test_api(suggestion_generator())
                .GetProfilesToSuggest(
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
  personal_data().AddProfile(profile1);

  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile2, "Marion2", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "456 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile2.set_use_date(AutofillClock::Now() - base::Days(20));
  personal_data().AddProfile(profile2);

  // Query with empty string only returns profile2.
  {
    std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
        suggested_profiles =
            test_api(suggestion_generator())
                .GetProfilesToSuggest(ADDRESS_HOME_STREET_ADDRESS,
                                      std::u16string(), false, {});
    EXPECT_EQ(1U, suggested_profiles.size());
  }

  // Query with non-alpha-numeric string only returns profile2.
  {
    std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
        suggested_profiles =
            test_api(suggestion_generator())
                .GetProfilesToSuggest(ADDRESS_HOME_STREET_ADDRESS, u"--", false,
                                      {});
    EXPECT_EQ(1U, suggested_profiles.size());
  }

  // Query with prefix for profile1 returns profile1.
  {
    std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
        suggested_profiles =
            test_api(suggestion_generator())
                .GetProfilesToSuggest(ADDRESS_HOME_STREET_ADDRESS, u"123",
                                      false, {});
    ASSERT_EQ(1U, suggested_profiles.size());
    EXPECT_EQ(u"Marion1", suggested_profiles[0]->GetRawInfo(NAME_FIRST));
  }

  // Query with prefix for profile2 returns profile2.
  {
    std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
        suggested_profiles =
            test_api(suggestion_generator())
                .GetProfilesToSuggest(ADDRESS_HOME_STREET_ADDRESS, u"456",
                                      false, {});
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
  personal_data().AddProfile(profile_1);
  personal_data().AddProfile(profile_2);

  std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
      profiles_to_suggest =
          test_api(suggestion_generator())
              .GetProfilesToSuggest(NAME_FIRST, u"",
                                    /*field_is_autofilled=*/false, {});

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
  personal_data().AddProfile(profiles[0]);

  profiles[1].SetRawInfo(NAME_FIRST, u"Bob");
  profiles[1].SetRawInfo(NAME_LAST, u"Parker");
  profiles[1].set_use_count(5);
  personal_data().AddProfile(profiles[1]);

  profiles[2].SetRawInfo(NAME_FIRST, u"Mary");
  profiles[2].SetRawInfo(NAME_LAST, u"Parker");
  personal_data().AddProfile(profiles[2]);

  std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
      profiles_to_suggest =
          test_api(suggestion_generator())
              .GetProfilesToSuggest(NAME_FIRST, u"",
                                    /*field_is_autofilled=*/false,
                                    {NAME_FIRST, NAME_LAST});

  EXPECT_EQ(3U, profiles_to_suggest.size());
}

// Test the limit of number of deduplicated profiles.
TEST_F(AutofillSuggestionGeneratorTest, GetProfilesToSuggest_DedupeLimit) {
  std::vector<AutofillProfile> profiles;
  for (size_t i = 0; i < kMaxDeduplicatedProfilesForSuggestion + 1; i++) {
    AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
    profile.SetRawInfo(NAME_FULL,
                       base::UTF8ToUTF16(base::StringPrintf("Bob %zu Doe", i)));
    profile.set_use_count(kMaxDeduplicatedProfilesForSuggestion + 10 - i);
    profiles.push_back(profile);
    personal_data().AddProfile(profile);
  }

  std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
      profiles_to_suggest =
          test_api(suggestion_generator())
              .GetProfilesToSuggest(NAME_FULL, u"",
                                    /*field_is_autofilled=*/false, {NAME_FULL});

  ASSERT_EQ(kMaxDeduplicatedProfilesForSuggestion, profiles_to_suggest.size());

  // All profiles are different.
  for (size_t i = 0; i < profiles_to_suggest.size(); i++) {
    EXPECT_EQ(profiles_to_suggest[i]->guid(), profiles[i].guid()) << i;
  }
}

TEST_F(AutofillSuggestionGeneratorTest,
       GetProfilesToSuggest_EmptyMatchingProfiles) {
  ASSERT_EQ(0U, test_api(suggestion_generator())
                    .GetProfilesToSuggest(NAME_FIRST, u"",
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
  personal_data().AddProfile(profile_1);

  AutofillProfile profile_2(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile_2.SetRawInfo(NAME_FULL, u"First Last");
  profile_2.set_source_for_testing(AutofillProfile::Source::kLocalOrSyncable);
  // Set high use count for profile 2 so that it has greater ranking than
  // profile_1
  profile_2.set_use_count(100);
  personal_data().AddProfile(profile_2);

  std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
      profiles_to_suggest =
          test_api(suggestion_generator())
              .GetProfilesToSuggest(NAME_FULL, u"",
                                    /*field_is_autofilled=*/false, {NAME_FULL});

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
  personal_data().AddProfile(marion_profile);

  AutofillProfile bob_profile(
      i18n_model_definition::kLegacyHierarchyCountryCode);
  bob_profile.SetRawInfo(NAME_FIRST, u"Bob");
  personal_data().AddProfile(bob_profile);

  std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
      profiles_to_suggest =
          test_api(suggestion_generator())
              .GetProfilesToSuggest(NAME_FIRST, u"Mar",
                                    /*field_is_autofilled=*/false, {});

  ASSERT_EQ(1U, profiles_to_suggest.size());
  EXPECT_EQ(marion_profile.guid(), profiles_to_suggest[0]->guid());
}

TEST_F(AutofillSuggestionGeneratorTest,
       GetProfilesToSuggest_NoMatchingProfile) {
  AutofillProfile bob_profile(
      i18n_model_definition::kLegacyHierarchyCountryCode);
  bob_profile.SetRawInfo(NAME_FIRST, u"Bob");
  personal_data().AddProfile(bob_profile);

  std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
      profiles_to_suggest =
          test_api(suggestion_generator())
              .GetProfilesToSuggest(NAME_FIRST, u"Mar",
                                    /*field_is_autofilled=*/false, {});

  ASSERT_TRUE(profiles_to_suggest.empty());
}

TEST_F(AutofillSuggestionGeneratorTest,
       GetProfilesToSuggest_EmptyProfilesInput) {
  std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
      profiles_to_suggest =
          test_api(suggestion_generator())
              .GetProfilesToSuggest(NAME_FIRST, u"Mar",
                                    /*field_is_autofilled=*/false, {});

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
    personal_data().AddProfile(profiles[i]);
  }

  // Filter the profiles while capturing histograms.
  base::HistogramTester histogram_tester;
  std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
      profiles_to_suggest =
          test_api(suggestion_generator())
              .GetProfilesToSuggest(NAME_FULL, u"",
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
      test_api(suggestion_generator())
          .CreateSuggestionsFromProfiles({&profile},
                                         {ADDRESS_HOME_STREET_ADDRESS},
                                         /*last_targeted_fields=*/std::nullopt,
                                         ADDRESS_HOME_STREET_ADDRESS,
                                         /*trigger_field_max_length=*/0);
  ASSERT_FALSE(suggestions.empty());
  EXPECT_EQ(u"123 Zoo St., Second Line, Third line, unit 5",
            suggestions[0].main_text.value);
}

TEST_F(AutofillSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_PhoneSubstring) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");

  std::vector<Suggestion> suggestions =
      test_api(suggestion_generator())
          .CreateSuggestionsFromProfiles({&profile}, {PHONE_HOME_WHOLE_NUMBER},
                                         /*last_targeted_fields=*/std::nullopt,
                                         PHONE_HOME_WHOLE_NUMBER,
                                         /*trigger_field_max_length=*/0);
  ASSERT_FALSE(suggestions.empty());
  EXPECT_EQ(u"+1 234-567-8910", suggestions[0].main_text.value);
}

TEST_F(AutofillSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_PartialNameFieldHasFullNameMainText) {
  base::test::ScopedFeatureList features(
      features::kAutofillGranularFillingAvailable);
  AutofillProfile profile = test::GetFullProfile();

  EXPECT_THAT(
      test_api(suggestion_generator())
          .CreateSuggestionsFromProfiles({&profile}, {NAME_FIRST, NAME_LAST},
                                         /*last_targeted_fields=*/std::nullopt,
                                         NAME_FIRST,
                                         /*trigger_field_max_length=*/0),
      SuggestionVectorMainTextsAre(Suggestion::Text(
          profile.GetRawInfo(NAME_FULL), Suggestion::Text::IsPrimary(true))));
}

// TODO(crbug.com/1459990): Move AutofillChildrenSuggestionGeneratorTest.
// CreateSuggestionsFromProfiles_GroupFillingLabels_* tests under this fixture.
// Text fixture for label generation related tests. Parameterized by triggering
// field type since how we build labels depends highly on it.
class AutofillLabelSuggestionGeneratorTest
    : public AutofillSuggestionGeneratorTest,
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

INSTANTIATE_TEST_SUITE_P(AutofillSuggestionGeneratorTest,
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
      test_api(suggestion_generator())
          .CreateSuggestionsFromProfiles(
              {&profile},
              {NAME_FULL, ADDRESS_HOME_STREET_ADDRESS, ADDRESS_HOME_ZIP},
              /*last_targeted_fields=*/std::nullopt, triggering_field_type,
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
      test_api(suggestion_generator())
          .CreateSuggestionsFromProfiles(
              {&profile1, &profile2}, {NAME_FULL, ADDRESS_HOME_STREET_ADDRESS},
              /*last_targeted_fields=*/std::nullopt, triggering_field_type,
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
      test_api(suggestion_generator())
          .CreateSuggestionsFromProfiles(
              {&profile1, &profile2}, {NAME_FULL, ADDRESS_HOME_STREET_ADDRESS},
              /*last_targeted_fields=*/std::nullopt, triggering_field_type,
              /*trigger_field_max_length=*/0),
      ElementsAre(
          AllOf(EqualLabels({{full_form_filling_label + u"United States"}})),
          AllOf(EqualLabels({{full_form_filling_label + u"Switzerland"}}))));
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// TODO(crbug.com/325646493): Clean up
// AutofillSuggestionGeneratorTest.AutofillCreditCardBenefitsLabelTest setup and
// parameters.
// Params:
// 1. Function reference to call which creates the appropriate credit card
// benefit for the unittest.
// 2. Issuer ID which is set for the credit card with benefits.
class AutofillCreditCardBenefitsLabelTest
    : public AutofillSuggestionGeneratorTest,
      public ::testing::WithParamInterface<
          std::tuple<base::FunctionRef<CreditCardBenefit()>, std::string>> {
 public:
  void SetUp() override {
    AutofillSuggestionGeneratorTest::SetUp();
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {features::kAutofillEnableCardBenefitsForAmericanExpress,
         features::kAutofillEnableCardBenefitsForCapitalOne,
         features::kAutofillEnableVirtualCardMetadata,
         features::kAutofillEnableCardProductName},
        /*disabled_features=*/{});

    std::u16string benefit_description;
    int64_t instrument_id;

    if (absl::holds_alternative<CreditCardFlatRateBenefit>(GetBenefit())) {
      CreditCardFlatRateBenefit benefit =
          absl::get<CreditCardFlatRateBenefit>(GetBenefit());
      personal_data().AddCreditCardBenefitForTest(benefit);
      benefit_description = benefit.benefit_description();
      instrument_id = *benefit.linked_card_instrument_id();
    } else if (absl::holds_alternative<CreditCardMerchantBenefit>(
                   GetBenefit())) {
      CreditCardMerchantBenefit benefit =
          absl::get<CreditCardMerchantBenefit>(GetBenefit());
      personal_data().AddCreditCardBenefitForTest(benefit);
      benefit_description = benefit.benefit_description();
      instrument_id = *benefit.linked_card_instrument_id();
      // Set the page URL in order to ensure that the merchant benefit is
      // displayed.
      autofill_client()->set_last_committed_primary_main_frame_url(
          benefit.merchant_domains().begin()->GetURL());
    } else if (absl::holds_alternative<CreditCardCategoryBenefit>(
                   GetBenefit())) {
      ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
                  autofill_client()->GetAutofillOptimizationGuide()),
              AttemptToGetEligibleCreditCardBenefitCategory)
          .WillByDefault(testing::Return(
              CreditCardCategoryBenefit::BenefitCategory::kSubscription));
      CreditCardCategoryBenefit benefit =
          absl::get<CreditCardCategoryBenefit>(GetBenefit());
      personal_data().AddCreditCardBenefitForTest(benefit);
      benefit_description = benefit.benefit_description();
      instrument_id = *benefit.linked_card_instrument_id();
    } else {
      NOTREACHED_NORETURN();
    }

    expected_benefit_text_ = l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_CREDIT_CARD_BENEFIT_TEXT_FOR_SUGGESTIONS,
        benefit_description);
    card_ = CreateServerCard(
        /*guid=*/"00000000-0000-0000-0000-000000000001",
        /*server_id=*/"server_id1",
        /*instrument_id=*/instrument_id);
    card_.set_issuer_id(std::get<1>(GetParam()));
    personal_data().AddServerCreditCard(card_);
  }

  CreditCardBenefit GetBenefit() const { return std::get<0>(GetParam())(); }

  const CreditCard& card() { return card_; }

  const std::u16string& expected_benefit_text() {
    return expected_benefit_text_;
  }

  // Checks that CreateCreditCardSuggestion appropriately labels cards with
  // benefits in MetadataLoggingContext.
  void DoBenefitSuggestionLabel_MetadataLoggingContextTest() {
    autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
    test_api(suggestion_generator())
        .CreateCreditCardSuggestionWithMetadataContext(
            card(), CREDIT_CARD_NUMBER,
            /*virtual_card_option=*/false,
            /*card_linked_offer_available=*/false, metadata_logging_context);
    EXPECT_THAT(metadata_logging_context.instrument_ids_with_benefits_available,
                testing::ElementsAre(card().instrument_id()));
  }

 private:
  std::u16string expected_benefit_text_;
  CreditCard card_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    AutofillSuggestionGeneratorTest,
    AutofillCreditCardBenefitsLabelTest,
    testing::Combine(testing::Values(&test::GetActiveCreditCardFlatRateBenefit,
                                     &test::GetActiveCreditCardCategoryBenefit,
                                     &test::GetActiveCreditCardMerchantBenefit),
                     ::testing::Values("amex", "capitalone")));

// Checks that for FPAN suggestions that the benefit description is displayed.
TEST_P(AutofillCreditCardBenefitsLabelTest, BenefitSuggestionLabel_Fpan) {
  EXPECT_THAT(
      test_api(suggestion_generator())
          .CreateCreditCardSuggestion(card(), CREDIT_CARD_NUMBER,
                                      /*virtual_card_option=*/false,
                                      /*card_linked_offer_available=*/false)
          .labels,
      testing::ElementsAre(
          std::vector<Suggestion::Text>{
              Suggestion::Text(expected_benefit_text())},
          std::vector<Suggestion::Text>{Suggestion::Text(card().GetInfo(
              CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, /*app_locale=*/"en-US"))}));
}

// Checks that feature_for_iph is set to display the credit card benefit IPH for
// FPAN suggestions with benefits labels.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       BenefitSuggestionFeatureForIph_Fpan) {
  EXPECT_EQ(
      test_api(suggestion_generator())
          .CreateCreditCardSuggestion(card(), CREDIT_CARD_NUMBER,
                                      /*virtual_card_option=*/false,
                                      /*card_linked_offer_available=*/false)
          .feature_for_iph,
      &feature_engagement::kIPHAutofillCreditCardBenefitFeature);
}

// Checks that feature_for_iph is set to display the virtual card IPH for
// virtual card suggestions with benefits labels.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       BenefitSuggestionFeatureForIph_VirtualCard) {
  EXPECT_EQ(
      test_api(suggestion_generator())
          .CreateCreditCardSuggestion(card(), CREDIT_CARD_NUMBER,
                                      /*virtual_card_option=*/true,
                                      /*card_linked_offer_available=*/false)
          .feature_for_iph,
      &feature_engagement::kIPHAutofillVirtualCardSuggestionFeature);
}

// Checks that for virtual cards suggestion the benefit description is shown
// with a virtual card label appended.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       BenefitSuggestionLabel_VirtualCard) {
  EXPECT_THAT(
      test_api(suggestion_generator())
          .CreateCreditCardSuggestion(card(), CREDIT_CARD_NUMBER,
                                      /*virtual_card_option=*/true,
                                      /*card_linked_offer_available=*/false)
          .labels,
      testing::ElementsAre(
          std::vector<Suggestion::Text>{
              Suggestion::Text(expected_benefit_text())},
          std::vector<Suggestion::Text>{
              Suggestion::Text(l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_OPTION_VALUE))}));
}

// Checks that for credit card suggestions with eligible benefits, the
// instrument id of the credit card is marked in the MetadataLoggingContext.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       BenefitSuggestionLabel_MetadataLoggingContext) {
  DoBenefitSuggestionLabel_MetadataLoggingContextTest();
}

// Checks that for credit card suggestions with eligible benefits, the
// instrument id of the credit card is marked in the MetadataLoggingContext. The
// instrument ids should also be available when the benefit flags are disabled.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       BenefitSuggestionLabel_MetadataLoggingContext_FlagsDisabled) {
  base::test::ScopedFeatureList disable_benefits;
  disable_benefits.InitWithFeatures(
      /*enabled_features=*/{}, /*disabled_features=*/{
          features::kAutofillEnableCardBenefitsForAmericanExpress,
          features::kAutofillEnableCardBenefitsForCapitalOne});
  DoBenefitSuggestionLabel_MetadataLoggingContextTest();
}

// Checks that the merchant benefit description is not displayed for suggestions
// where the webpage's URL is different from the benefit's applicable URL.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       BenefitSuggestionLabelNotDisplayed_MerchantUrlIsDifferent) {
  if (!absl::holds_alternative<CreditCardMerchantBenefit>(GetBenefit())) {
    GTEST_SKIP() << "This test should not run for non-merchant benefits.";
  }
  autofill_client()->set_last_committed_primary_main_frame_url(
      GURL("https://random-url.com"));
  // Merchant benefit description is not returned.
  EXPECT_THAT(
      test_api(suggestion_generator())
          .CreateCreditCardSuggestion(card(), CREDIT_CARD_NUMBER,
                                      /*virtual_card_option=*/false,
                                      /*card_linked_offer_available=*/false)
          .labels,
      testing::ElementsAre(
          std::vector<Suggestion::Text>{Suggestion::Text(card().GetInfo(
              CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, /*app_locale=*/"en-US"))}));
}

// Checks that the category benefit description is not displayed for suggestions
// where the webpage's category in the optimization guide is different from the
// benefit's applicable category.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       BenefitSuggestionLabelNotDisplayed_CategoryIsDifferent) {
  if (!absl::holds_alternative<CreditCardCategoryBenefit>(GetBenefit())) {
    GTEST_SKIP() << "This test should not run for non-category benefits.";
  }

  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
              autofill_client()->GetAutofillOptimizationGuide()),
          AttemptToGetEligibleCreditCardBenefitCategory)
      .WillByDefault(testing::Return(
          CreditCardCategoryBenefit::BenefitCategory::kUnknownBenefitCategory));

  // Category benefit description is not returned.
  EXPECT_THAT(
      test_api(suggestion_generator())
          .CreateCreditCardSuggestion(card(), CREDIT_CARD_NUMBER,
                                      /*virtual_card_option=*/false,
                                      /*card_linked_offer_available=*/false)
          .labels,
      testing::ElementsAre(
          std::vector<Suggestion::Text>{Suggestion::Text(card().GetInfo(
              CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, /*app_locale=*/"en-US"))}));
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

class AutofillChildrenSuggestionGeneratorTest
    : public AutofillSuggestionGeneratorTest {
 public:
  std::vector<Suggestion> CreateSuggestionWithChildrenFromProfile(
      const AutofillProfile& profile,
      std::optional<FieldTypeSet> last_targeted_fields,
      FieldType trigger_field_type,
      const FieldTypeSet& field_types) {
    return test_api(suggestion_generator())
        .CreateSuggestionsFromProfiles({&profile}, field_types,
                                       last_targeted_fields, trigger_field_type,
                                       /*trigger_field_max_length=*/0);
  }

  std::vector<Suggestion> CreateSuggestionWithChildrenFromProfile(
      const AutofillProfile& profile,
      std::optional<FieldTypeSet> last_targeted_fields,
      FieldType trigger_field_type) {
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

// Test that only "Fill address" is added when the target field is
// `ADDRESS_HOME_LINE1` and no other suggestion exist with the same
// `Suggestion::main_text` and `ADDRESS_HOME_LINE1`.
TEST_F(AutofillChildrenSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_GroupFillingLabels_AddOnlyFillAddress) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(),
      /*last_targeted_fields=*/
      GetAddressFieldsForGroupFilling(),
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
  std::vector<Suggestion> suggestions =
      test_api(suggestion_generator())
          .CreateSuggestionsFromProfiles(
              {&profile_1, &profile_2},
              {ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2},
              GetAddressFieldsForGroupFilling(), ADDRESS_HOME_LINE1,
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
  std::vector<Suggestion> suggestions =
      test_api(suggestion_generator())
          .CreateSuggestionsFromProfiles(
              {&profile_1, &profile_2},
              {ADDRESS_HOME_LINE1, ADDRESS_HOME_ZIP, ADDRESS_HOME_CITY},
              GetAddressFieldsForGroupFilling(), ADDRESS_HOME_ZIP,
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
      /*last_targeted_fields=*/
      GetFieldTypesOfGroup(FieldTypeGroup::kName),
      /*trigger_field_type=*/NAME_FIRST,
      /*field_types=*/{NAME_FIRST, NAME_LAST});

  ASSERT_EQ(suggestions.size(), 1u);
  EXPECT_EQ(suggestions[0].labels,
            std::vector<std::vector<Suggestion::Text>>(
                {{Suggestion::Text(u"Fill full name")}}));
}

// If the last targeted fields belong to a different group than the triggering
// field, the granular filling label should still match the triggering field.
TEST_F(
    AutofillChildrenSuggestionGeneratorTest,
    CreateSuggestionsFromProfiles_GroupFillingLabels_AddOnlyFillName_DifferentLastTargetedFields) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(),
      /*last_targeted_fields=*/
      GetAddressFieldsForGroupFilling(),
      /*trigger_field_type=*/NAME_FIRST,
      /*field_types=*/{NAME_FIRST, NAME_LAST});

  ASSERT_EQ(suggestions.size(), 1u);
  EXPECT_EQ(suggestions[0].labels,
            std::vector<std::vector<Suggestion::Text>>(
                {{Suggestion::Text(u"Fill full name")}}));
}

// If the last targeted fields belong to a different group than the triggering
// field, the granular filling label should still match the triggering field.
TEST_F(
    AutofillChildrenSuggestionGeneratorTest,
    CreateSuggestionsFromProfiles_GroupFillingLabels_AddOnlyFillAddress_DifferentLastTargetedFields) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(),
      /*last_targeted_fields=*/GetFieldTypesOfGroup(FieldTypeGroup::kName),
      /*trigger_field_type=*/ADDRESS_HOME_LINE1,
      /*field_types=*/{ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2});

  ASSERT_EQ(suggestions.size(), 1u);
  EXPECT_EQ(suggestions[0].labels, std::vector<std::vector<Suggestion::Text>>(
                                       {{Suggestion::Text(u"Fill address")}}));
}

// Test that no labels are added when filling targets only one field.
TEST_F(
    AutofillChildrenSuggestionGeneratorTest,
    CreateSuggestionsFromProfiles_GroupFillingLabels_SingleFieldFillingHasNoLabels) {
  AutofillProfile profile = test::GetFullProfile();

  std::vector<Suggestion> suggestions =
      test_api(suggestion_generator())
          .CreateSuggestionsFromProfiles(
              {&profile}, {NAME_FULL},
              GetFieldTypesOfGroup(FieldTypeGroup::kName), NAME_FULL,
              /*trigger_field_max_length=*/0);

  ASSERT_EQ(suggestions.size(), 1u);
  EXPECT_EQ(suggestions[0].labels,
            std::vector<std::vector<Suggestion::Text>>({{}}));
}

TEST_F(AutofillChildrenSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_FirstLevelChildrenSuggestions) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(),
      /*last_targeted_fields=*/kAllFieldTypes, NAME_FIRST);

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
          EqualsSuggestion(PopupItemId::kFillFullName),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kAddressFieldByFieldFilling,
              profile().GetInfo(NAME_FIRST, app_locale()), NAME_FIRST,
              Suggestion::Guid(profile().guid())),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kAddressFieldByFieldFilling,
              profile().GetInfo(NAME_MIDDLE, app_locale()), NAME_MIDDLE,
              Suggestion::Guid(profile().guid())),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kAddressFieldByFieldFilling,
              profile().GetInfo(NAME_LAST, app_locale()), NAME_LAST,
              Suggestion::Guid(profile().guid())),
          EqualsSuggestion(PopupItemId::kSeparator),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kAddressFieldByFieldFilling,
              profile().GetInfo(COMPANY_NAME, app_locale()), COMPANY_NAME,
              Suggestion::Guid(profile().guid())),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kAddressFieldByFieldFilling,
              profile().GetInfo(ADDRESS_HOME_LINE1, app_locale()),
              ADDRESS_HOME_LINE1, Suggestion::Guid(profile().guid())),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kAddressFieldByFieldFilling,
              profile().GetInfo(ADDRESS_HOME_LINE2, app_locale()),
              ADDRESS_HOME_LINE2, Suggestion::Guid(profile().guid())),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kAddressFieldByFieldFilling,
              profile().GetInfo(ADDRESS_HOME_CITY, app_locale()),
              ADDRESS_HOME_CITY, Suggestion::Guid(profile().guid())),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kAddressFieldByFieldFilling,
              profile().GetInfo(ADDRESS_HOME_ZIP, app_locale()),
              ADDRESS_HOME_ZIP, Suggestion::Guid(profile().guid())),
          EqualsSuggestion(PopupItemId::kSeparator),
          // Triggering field is not a phone number, international phone number
          // should be shown to the user.
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kAddressFieldByFieldFilling,
              GetFormattedInternationalNumber(), PHONE_HOME_WHOLE_NUMBER,
              Suggestion::Guid(profile().guid())),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kAddressFieldByFieldFilling,
              profile().GetInfo(EMAIL_ADDRESS, app_locale()), EMAIL_ADDRESS,
              Suggestion::Guid(profile().guid())),
          EqualsSuggestion(PopupItemId::kSeparator),
          EqualsSuggestion(PopupItemId::kEditAddressProfile),
          EqualsSuggestion(PopupItemId::kDeleteAddressProfile)));
}

TEST_F(AutofillChildrenSuggestionGeneratorTest,
       FillEverythingFromAddressProfile_NotAddedIfNoLastTargetedField) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(),
      /*last_targeted_fields=*/std::nullopt, NAME_FIRST);

  ASSERT_EQ(1U, suggestions.size());
  EXPECT_THAT(suggestions[0].children,
              Not(Contains(EqualsSuggestion(
                  PopupItemId::kFillEverythingFromAddressProfile))))
      << "Children should not contain the 'fill everything' suggestion because "
         "there is no `last_targeted_fields`.";
}

TEST_F(AutofillChildrenSuggestionGeneratorTest,
       IncognitoMode_EditAndDeleteSuggestionsAreNotAdded) {
  autofill_client()->set_is_off_the_record(true);
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(),
      /*last_targeted_fields=*/std::nullopt, NAME_FIRST);

  ASSERT_EQ(1u, suggestions.size());
  ASSERT_GT(suggestions[0].children.size(), 0u);
  EXPECT_THAT(suggestions[0].children,
              Not(Contains(EqualsSuggestion(PopupItemId::kEditAddressProfile))))
      << "Children should not contain the 'Edit address' suggestion because "
         "there user is in incognito mode.";
  EXPECT_THAT(
      suggestions[0].children,
      Not(Contains(EqualsSuggestion(PopupItemId::kDeleteAddressProfile))))
      << "Children should not contain the 'Delete address' suggestion because "
         "there user is in incognito mode.";
}

TEST_F(AutofillChildrenSuggestionGeneratorTest,
       FillEverythingFromAddressProfile_NotAddedIfLastTargetedAllFieldTypes) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(),
      /*last_targeted_fields=*/kAllFieldTypes, NAME_FIRST);

  ASSERT_EQ(1U, suggestions.size());
  EXPECT_THAT(suggestions[0].children,
              Not(Contains(EqualsSuggestion(
                  PopupItemId::kFillEverythingFromAddressProfile))))
      << "Children should not contain the 'fill everything' suggestion because "
         "the last targeted fields is `kAllFieldTypes`.";
}

TEST_F(AutofillChildrenSuggestionGeneratorTest,
       FillEverythingFromAddressProfile_AddedIfFieldByFieldFilling) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(),
      /*last_targeted_fields=*/FieldTypeSet{IBAN_VALUE}, NAME_FIRST);

  ASSERT_EQ(1U, suggestions.size());
  EXPECT_THAT(suggestions[0].children,
              Contains(EqualsSuggestion(
                  PopupItemId::kFillEverythingFromAddressProfile)))
      << "Children should contain the 'fill everything' suggestion because of "
         "the last field-by-field filling.";
}

TEST_F(AutofillChildrenSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_SecondLevelChildrenSuggestions) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(),
      /*last_targeted_fields=*/std::nullopt, NAME_FIRST);

  ASSERT_EQ(1U, suggestions.size());
  // Suggestions should have two levels of children, The address line 1 (sixth
  // child) suggestion should have the following children: house number street
  // name.
  ASSERT_EQ(2U, suggestions[0].children[6].children.size());
  EXPECT_THAT(
      suggestions[0].children[6].children,
      ElementsAre(
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kAddressFieldByFieldFilling,
              profile().GetInfo(ADDRESS_HOME_HOUSE_NUMBER, app_locale()),
              ADDRESS_HOME_HOUSE_NUMBER, Suggestion::Guid(profile().guid()),
              {{Suggestion::Text(l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_HOUSE_NUMBER_SUGGESTION_SECONDARY_TEXT))}}),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kAddressFieldByFieldFilling,
              profile().GetInfo(ADDRESS_HOME_STREET_NAME, app_locale()),
              ADDRESS_HOME_STREET_NAME, Suggestion::Guid(profile().guid()),
              {{Suggestion::Text(l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_STREET_NAME_SUGGESTION_SECONDARY_TEXT))}})));
}

TEST_F(
    AutofillChildrenSuggestionGeneratorTest,
    CreateSuggestionsFromProfiles_LastTargetedFieldsIsSingleField_FieldByFieldFilling) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), std::optional<FieldTypeSet>({NAME_LAST}), NAME_FIRST);

  ASSERT_EQ(suggestions.size(), 1u);
  // Differently from other filling modes, where when focusing on a name field
  // the NAME_FULL is rendered in the main text, field-by-field filling always
  // displays the value that will actually be used to fill the field as main
  // text.
  EXPECT_THAT(suggestions[0],
              EqualsFieldByFieldFillingSuggestion(
                  PopupItemId::kAddressFieldByFieldFilling,
                  profile().GetInfo(NAME_FIRST, app_locale()), NAME_FIRST,
                  Suggestion::Guid(profile().guid()), {{}}));
}

TEST_F(AutofillChildrenSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_LastTargetedFieldsIsGroup_GroupFilling) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), std::optional<FieldTypeSet>(GetAddressFieldsForGroupFilling()),
      NAME_FIRST, {NAME_FIRST, NAME_LAST});

  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ(suggestions[0].popup_item_id, PopupItemId::kFillFullName);
  EXPECT_EQ(suggestions[0].icon, Suggestion::Icon::kNoIcon);
}

// Note that only full form filling has an icon.
TEST_F(
    AutofillChildrenSuggestionGeneratorTest,
    CreateSuggestionsFromProfiles_LastTargetedFieldsAreAllServerFields_FullForm) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), kAllFieldTypes, NAME_FIRST, {NAME_FIRST, NAME_LAST});

  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ(suggestions[0].popup_item_id, PopupItemId::kAddressEntry);
  EXPECT_EQ(suggestions[0].icon, Suggestion::Icon::kLocation);
}

// Asserts that when the triggering field is a phone field, the phone number
// suggestion is of type `PopupItemId::kFillFullPhoneNumber`. In other
// scenarios, phone number is of type `PopupItemId::kAddressFieldByFieldFilling`
// as the user expressed intent to use their phone number their phone number on
// a "random" field.
TEST_F(
    AutofillChildrenSuggestionGeneratorTest,
    CreateSuggestionsFromProfiles_ChildrenSuggestionsPhoneField_International) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), kAllFieldTypes, PHONE_HOME_WHOLE_NUMBER);

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
              EqualsSuggestion(PopupItemId::kFillFullPhoneNumber,
                               GetFormattedInternationalNumber()));
  EXPECT_THAT(suggestions[0].children[10].children, IsEmpty());
}

// Asserts that when the triggering field is a phone field, the phone number
// suggestion is of type `PopupItemId::kFillFullPhoneNumber`. In other
// scenarios, phone number is of type `PopupItemId::kAddressFieldByFieldFilling`
// as the user expressed intent to use their phone number on a "random" field.
TEST_F(
    AutofillChildrenSuggestionGeneratorTest,
    CreateSuggestionsFromProfiles_ChildrenSuggestionsPhoneField_CountryCode) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), kAllFieldTypes, PHONE_HOME_COUNTRY_CODE);

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
              EqualsSuggestion(PopupItemId::kFillFullPhoneNumber,
                               GetFormattedInternationalNumber()));
  EXPECT_THAT(suggestions[0].children[10].children, IsEmpty());
}

// Asserts that when the triggering field is a phone field, the phone number
// suggestion is of type `PopupItemId::kFillFullPhoneNumber`. In other
// scenarios, phone number is of type `PopupItemId::kAddressFieldByFieldFilling`
// as the user expressed intent to use their phone number their phone number on
// a "random" field.
TEST_F(AutofillChildrenSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_ChildrenSuggestionsPhoneField_Local) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), kAllFieldTypes, PHONE_HOME_CITY_AND_NUMBER);

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
              EqualsSuggestion(PopupItemId::kFillFullPhoneNumber,
                               GetFormattedNationalNumber()));
  EXPECT_THAT(suggestions[0].children[10].children, IsEmpty());
}

// Same as above but for email fields.
TEST_F(AutofillChildrenSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_ChildrenSuggestionsEmailField) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), kAllFieldTypes, EMAIL_ADDRESS);

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
              Field(&Suggestion::popup_item_id, PopupItemId::kFillFullEmail));
}

TEST_F(AutofillChildrenSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_ChildrenSuggestionsAddressField) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), kAllFieldTypes, ADDRESS_HOME_LINE1);

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
              Field(&Suggestion::popup_item_id, PopupItemId::kFillFullAddress));
}

TEST_F(AutofillChildrenSuggestionGeneratorTest,
       CreateSuggestionsFromProfiles_ChildrenSuggestionsCompanyField) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), kAllFieldTypes, COMPANY_NAME);

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
              Field(&Suggestion::popup_item_id, PopupItemId::kFillFullAddress));
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
      profile, /*last_targeted_fields=*/std::nullopt, ADDRESS_HOME_LINE1);

  ASSERT_EQ(1u, suggestions.size());
  ASSERT_LE(3u, suggestions[0].children.size());
  // The address line 1 (sixth child) should have the street name as child.
  EXPECT_THAT(suggestions[0].children[1].children,
              ElementsAre(EqualsFieldByFieldFillingSuggestion(
                  PopupItemId::kAddressFieldByFieldFilling,
                  profile.GetInfo(ADDRESS_HOME_STREET_NAME, app_locale()),
                  ADDRESS_HOME_STREET_NAME, Suggestion::Guid(profile.guid()),
                  {{Suggestion::Text(l10n_util::GetStringUTF16(
                      IDS_AUTOFILL_STREET_NAME_SUGGESTION_SECONDARY_TEXT))}})));
  // The address line 2 (seventh child) should have the house number as child.
  EXPECT_THAT(
      suggestions[0].children[2].children,
      ElementsAre(EqualsFieldByFieldFillingSuggestion(
          PopupItemId::kAddressFieldByFieldFilling,
          profile.GetInfo(ADDRESS_HOME_HOUSE_NUMBER, app_locale()),
          ADDRESS_HOME_HOUSE_NUMBER, Suggestion::Guid(profile.guid()),
          {{Suggestion::Text(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_HOUSE_NUMBER_SUGGESTION_SECONDARY_TEXT))}})));
}

TEST_F(
    AutofillChildrenSuggestionGeneratorTest,
    CreateSuggestionsFromProfiles_GranularityNotFullForm_FillEverythingChildSuggestion) {
  // We set only a name field as `last_targeted_fields` to denote that the user
  // chose field by field filling.
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), std::optional<FieldTypeSet>({NAME_FIRST}), ADDRESS_HOME_LINE1);

  ASSERT_EQ(1U, suggestions.size());
  EXPECT_TRUE(base::ranges::any_of(suggestions[0].children, [](auto child) {
    return child.popup_item_id ==
           PopupItemId::kFillEverythingFromAddressProfile;
  }));
}

// This fixture contains tests for autofill being triggered from the context
// menu on a field which is not classified as an address.
class AutofillNonAddressFieldsSuggestionGeneratorTest
    : public AutofillChildrenSuggestionGeneratorTest {
 public:
  void SetUp() override {
    AutofillChildrenSuggestionGeneratorTest::SetUp();
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillGranularFillingAvailable,
                              features::
                                  kAutofillForUnclassifiedFieldsAvailable},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AutofillNonAddressFieldsSuggestionGeneratorTest,
       AllProfilesGenerateSuggestions) {
  personal_data().AddProfile(test::GetFullProfile());
  personal_data().AddProfile(test::GetFullProfile2());

  FormFieldData triggering_field;

  std::vector<Suggestion> suggestions =
      suggestion_generator().GetSuggestionsForProfiles(
          {UNKNOWN_TYPE}, triggering_field, UNKNOWN_TYPE,
          /*last_targeted_fields=*/std::nullopt,
          AutofillSuggestionTriggerSource::kManualFallbackAddress);
  EXPECT_EQ(suggestions.size(), 4ul);
  EXPECT_THAT(suggestions[0], EqualsSuggestion(PopupItemId::kAddressEntry));
  EXPECT_THAT(suggestions[1], EqualsSuggestion(PopupItemId::kAddressEntry));
  EXPECT_THAT(suggestions, ContainsAddressFooterSuggestions());
}

// Generally, a profile is displayed with name as main text and address as
// label. But with incomplete profiles, it might be problematic. This test
// creates various incomplete profiles and makes sure that a main text and a
// label are always chosen from the available fields (or only main_text if the
// profile has only one field).
TEST_F(AutofillNonAddressFieldsSuggestionGeneratorTest,
       SuggestionsAreCorrectAndExpectedLabelsAreCreated) {
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

  std::vector<Suggestion> suggestions =
      test_api(suggestion_generator())
          .CreateSuggestionsFromProfiles(
              {&profiles[0], &profiles[1], &profiles[2], &profiles[3],
               &profiles[4]},
              {UNKNOWN_TYPE},
              /*last_targeted_fields=*/std::nullopt, UNKNOWN_TYPE,
              /*trigger_field_max_length=*/0);

  ASSERT_EQ(5u, suggestions.size());
  EXPECT_THAT(
      suggestions,
      ElementsAre(
          AllOf(Field(&Suggestion::main_text,
                      Suggestion::Text(u"John Doe",
                                       Suggestion::Text::IsPrimary(true))),
                EqualLabels({{u"Address 123"}}),
                Field(&Suggestion::popup_item_id, PopupItemId::kAddressEntry),
                Field(&Suggestion::is_acceptable, false)),
          AllOf(Field(&Suggestion::main_text,
                      Suggestion::Text(u"Johnas Dhonas",
                                       Suggestion::Text::IsPrimary(true))),
                EqualLabels({{u"New York"}}),
                Field(&Suggestion::popup_item_id, PopupItemId::kAddressEntry),
                Field(&Suggestion::is_acceptable, false)),
          AllOf(Field(&Suggestion::main_text,
                      Suggestion::Text(u"Other Address 33",
                                       Suggestion::Text::IsPrimary(true))),
                EqualLabels({{u"Old City"}}),
                Field(&Suggestion::popup_item_id, PopupItemId::kAddressEntry),
                Field(&Suggestion::is_acceptable, false)),
          AllOf(Field(&Suggestion::main_text,
                      Suggestion::Text(u"Munich",
                                       Suggestion::Text::IsPrimary(true))),
                EqualLabels({{u"munich@gmail.com"}}),
                Field(&Suggestion::popup_item_id, PopupItemId::kAddressEntry),
                Field(&Suggestion::is_acceptable, false)),
          AllOf(Field(&Suggestion::main_text,
                      Suggestion::Text(u"other@gmail.com",
                                       Suggestion::Text::IsPrimary(true))),
                EqualLabels(std::vector<std::vector<Suggestion::Text>>{
                    {Suggestion::Text(u"")}}),
                Field(&Suggestion::popup_item_id, PopupItemId::kAddressEntry),
                Field(&Suggestion::is_acceptable, false))));
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

  std::vector<Suggestion> suggestions =
      test_api(suggestion_generator())
          .CreateSuggestionsFromProfiles({&profile}, {UNKNOWN_TYPE},
                                         /*last_targeted_fields=*/std::nullopt,
                                         UNKNOWN_TYPE,
                                         /*trigger_field_max_length=*/0);
  EXPECT_THAT(suggestions,
              ElementsAre(AllOf(
                  Field(&Suggestion::main_text,
                        Suggestion::Text(u"港区六本木ヒルズ森タワー",
                                         Suggestion::Text::IsPrimary(true))),
                  EqualLabels({{u"ミク初音"}}),
                  Field(&Suggestion::popup_item_id, PopupItemId::kAddressEntry),
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

  std::vector<Suggestion> suggestions =
      test_api(suggestion_generator())
          .CreateSuggestionsFromProfiles({&profile}, {UNKNOWN_TYPE},
                                         /*last_targeted_fields=*/std::nullopt,
                                         UNKNOWN_TYPE,
                                         /*trigger_field_max_length=*/0);
  EXPECT_THAT(
      suggestions,
      ElementsAre(AllOf(
          Field(&Suggestion::main_text,
                Suggestion::Text(u"صاحب", Suggestion::Text::IsPrimary(true))),
          EqualLabels({{u"الملكي"}}),
          Field(&Suggestion::popup_item_id, PopupItemId::kAddressEntry),
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

  std::vector<Suggestion> suggestions =
      test_api(suggestion_generator())
          .CreateSuggestionsFromProfiles({&profile}, {UNKNOWN_TYPE},
                                         /*last_targeted_fields=*/std::nullopt,
                                         UNKNOWN_TYPE,
                                         /*trigger_field_max_length=*/0);
  EXPECT_THAT(suggestions,
              ElementsAre(AllOf(
                  Field(&Suggestion::main_text,
                        Suggestion::Text(u"แขวงลุมพินี",
                                         Suggestion::Text::IsPrimary(true))),
                  EqualLabels({{u"57 ปาร์คเวนเชอร์"}}),
                  Field(&Suggestion::popup_item_id, PopupItemId::kAddressEntry),
                  Field(&Suggestion::is_acceptable, false))));
}

// Tests that a non-address field suggestion has all the profile fields as
// children, and doesn't have children like "Fill address" or "Fill full
// name".
TEST_F(AutofillNonAddressFieldsSuggestionGeneratorTest,
       SuggestionHasCorrectChildren) {
  std::vector<Suggestion> suggestions = CreateSuggestionWithChildrenFromProfile(
      profile(), std::nullopt, UNKNOWN_TYPE);

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
              PopupItemId::kAddressFieldByFieldFilling,
              profile().GetInfo(NAME_FIRST, app_locale()), NAME_FIRST,
              Suggestion::Guid(profile().guid())),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kAddressFieldByFieldFilling,
              profile().GetInfo(NAME_MIDDLE, app_locale()), NAME_MIDDLE,
              Suggestion::Guid(profile().guid())),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kAddressFieldByFieldFilling,
              profile().GetInfo(NAME_LAST, app_locale()), NAME_LAST,
              Suggestion::Guid(profile().guid())),
          EqualsSuggestion(PopupItemId::kSeparator),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kAddressFieldByFieldFilling,
              profile().GetInfo(COMPANY_NAME, app_locale()), COMPANY_NAME,
              Suggestion::Guid(profile().guid())),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kAddressFieldByFieldFilling,
              profile().GetInfo(ADDRESS_HOME_LINE1, app_locale()),
              ADDRESS_HOME_LINE1, Suggestion::Guid(profile().guid())),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kAddressFieldByFieldFilling,
              profile().GetInfo(ADDRESS_HOME_LINE2, app_locale()),
              ADDRESS_HOME_LINE2, Suggestion::Guid(profile().guid())),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kAddressFieldByFieldFilling,
              profile().GetInfo(ADDRESS_HOME_CITY, app_locale()),
              ADDRESS_HOME_CITY, Suggestion::Guid(profile().guid())),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kAddressFieldByFieldFilling,
              profile().GetInfo(ADDRESS_HOME_ZIP, app_locale()),
              ADDRESS_HOME_ZIP, Suggestion::Guid(profile().guid())),
          EqualsSuggestion(PopupItemId::kSeparator),
          // Triggering field is not a phone number, international phone number
          // should be shown to the user.
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kAddressFieldByFieldFilling,
              GetFormattedInternationalNumber(), PHONE_HOME_WHOLE_NUMBER,
              Suggestion::Guid(profile().guid())),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kAddressFieldByFieldFilling,
              profile().GetInfo(EMAIL_ADDRESS, app_locale()), EMAIL_ADDRESS,
              Suggestion::Guid(profile().guid())),
          EqualsSuggestion(PopupItemId::kSeparator),
          EqualsSuggestion(PopupItemId::kEditAddressProfile),
          EqualsSuggestion(PopupItemId::kDeleteAddressProfile)));
}

// Tests the scenario when:
// - autofill is triggered from the context menu on a field which is classified
// as an address field;
// - there is no profile which has values to fill the respective field.
// In this scenario, suggestions should look the same as the ones for an
// unclassified field.
TEST_F(AutofillSuggestionGeneratorTest,
       NoProfilesHaveValuesForClassifiedField_AddressManualFallback) {
  base::test::ScopedFeatureList features(
      features::kAutofillForUnclassifiedFieldsAvailable);
  AutofillProfile profile = test::GetIncompleteProfile1();
  ASSERT_FALSE(profile.HasRawInfo(PHONE_HOME_WHOLE_NUMBER));
  personal_data().AddProfile(profile);

  std::vector<Suggestion> suggestions =
      suggestion_generator().GetSuggestionsForProfiles(
          {NAME_FULL, ADDRESS_HOME_STREET_ADDRESS, PHONE_HOME_WHOLE_NUMBER},
          FormFieldData(), PHONE_HOME_WHOLE_NUMBER,
          /*last_targeted_fields=*/std::nullopt,
          AutofillSuggestionTriggerSource::kManualFallbackAddress);
  ASSERT_EQ(3u, suggestions.size());
  EXPECT_EQ(suggestions[0].popup_item_id, PopupItemId::kAddressEntry);
  // This is the check which actually verifies that the suggestion looks the
  // same as the ones for an unclassified field (such a suggestion has
  // `is_acceptable` as false).
  EXPECT_EQ(suggestions[0].is_acceptable, false);
  EXPECT_THAT(suggestions, ContainsAddressFooterSuggestions());
}

// Tests the scenario when:
// - autofill is triggered from the context menu on a field which is classified
// as a credit card field;
// - there is no card which has values to fill the respective field (or the
// field is a CVC which cannot be filled this way).
// In this scenario, suggestions should look the same as the ones for an
// unclassified field.
TEST_F(AutofillSuggestionGeneratorTest,
       NoProfilesHaveValuesForClassifiedField_PaymentsManualFallback) {
  base::test::ScopedFeatureList features(
      features::kAutofillForUnclassifiedFieldsAvailable);
  CreditCard card = test::GetIncompleteCreditCard();
  ASSERT_FALSE(card.HasRawInfo(PHONE_HOME_WHOLE_NUMBER));
  personal_data().AddCreditCard(card);

  bool with_offer;
  bool with_cvc;
  autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
  std::vector<Suggestion> suggestions =
      suggestion_generator().GetSuggestionsForCreditCards(
          FormFieldData(), CREDIT_CARD_NAME_FULL,
          AutofillSuggestionTriggerSource::kManualFallbackPayments,
          /*should_show_scan_credit_card=*/false,
          /*should_show_cards_from_account=*/false, with_offer, with_cvc,
          metadata_logging_context);

  ASSERT_EQ(3u, suggestions.size());
  EXPECT_EQ(suggestions[0].popup_item_id, PopupItemId::kCreditCardEntry);
  // This is the check which actually verifies that the suggestion looks the
  // same as the ones for an unclassified field (such a suggestion has
  // `is_acceptable` as false).
  EXPECT_EQ(suggestions[0].is_acceptable, false);
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/false));

  suggestions = suggestion_generator().GetSuggestionsForCreditCards(
      FormFieldData(), CREDIT_CARD_VERIFICATION_CODE,
      AutofillSuggestionTriggerSource::kManualFallbackPayments,
      /*should_show_scan_credit_card=*/false,
      /*should_show_cards_from_account=*/false, with_offer, with_cvc,
      metadata_logging_context);

  ASSERT_EQ(3u, suggestions.size());
  EXPECT_EQ(suggestions[0].popup_item_id, PopupItemId::kCreditCardEntry);
  EXPECT_EQ(suggestions[0].is_acceptable, false);
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/false));
}

// Tests that regular suggestions are filtered by the triggering field's value,
// but manual fallback suggestions are not.
TEST_F(AutofillSuggestionGeneratorTest, GetSuggestionsForProfiles_Filtering) {
  AutofillProfile profile1 = test::GetFullProfile();
  AutofillProfile profile2 = test::GetFullProfile2();
  personal_data().AddProfile(profile1);
  personal_data().AddProfile(profile2);

  // Create a triggering field those value prefix-matches `profile1`, but not
  // `profile2`.
  FormFieldData triggering_field;
  triggering_field.set_value(profile1.GetRawInfo(NAME_FIRST));
  ASSERT_FALSE(profile2.GetRawInfo(NAME_FIRST)
                   .starts_with(profile1.GetRawInfo(NAME_FIRST)));

  // Expect that regular suggestions filter.
  std::vector<Suggestion> address_suggestions =
      suggestion_generator().GetSuggestionsForProfiles(
          {NAME_FIRST}, triggering_field, NAME_FIRST,
          /*last_targeted_fields=*/std::nullopt,
          AutofillSuggestionTriggerSource::kFormControlElementClicked);
  EXPECT_EQ(address_suggestions.size(), 3ul);
  EXPECT_THAT(address_suggestions, ContainsAddressFooterSuggestions());

  // But manual fallback suggestions do not.
  std::vector<Suggestion> manual_fallback_suggestions =
      suggestion_generator().GetSuggestionsForProfiles(
          {NAME_FIRST}, triggering_field, NAME_FIRST,
          /*last_targeted_fields=*/std::nullopt,
          AutofillSuggestionTriggerSource::kManualFallbackAddress);
  EXPECT_EQ(manual_fallback_suggestions.size(), 4ul);
  EXPECT_THAT(manual_fallback_suggestions, ContainsAddressFooterSuggestions());
}

// Tests that regular suggestions are filtered by the last usage timestamp, but
// manual fallback suggestions are not.
TEST_F(AutofillSuggestionGeneratorTest,
       GetProfilesToSuggest_TimestampFiltering) {
  AutofillProfile profile1 = test::GetFullProfile();
  AutofillProfile profile2 = test::GetFullProfile2();
  profile2.set_use_date(AutofillClock::Now() - kDisusedDataModelTimeDelta -
                        base::Days(1));
  personal_data().AddProfile(profile1);
  personal_data().AddProfile(profile2);

  std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
      profiles_to_suggest =
          test_api(suggestion_generator())
              .GetProfilesToSuggest(
                  NAME_FIRST, /*field_contents=*/u"",
                  /*field_is_autofilled=*/false, {NAME_FIRST},
                  AutofillSuggestionTriggerSource::kFormControlElementClicked);
  // Expect that left click (or regular triggering) filters profiles.
  EXPECT_EQ(profiles_to_suggest.size(), 1u);

  std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
      profiles_to_suggest_from_manual_fallback =
          test_api(suggestion_generator())
              .GetProfilesToSuggest(
                  NAME_FIRST, /*field_contents=*/u"",
                  /*field_is_autofilled=*/false, {NAME_FIRST},
                  AutofillSuggestionTriggerSource::kManualFallbackAddress);
  // But manual fallback triggering does not.
  EXPECT_EQ(profiles_to_suggest_from_manual_fallback.size(), 2u);
}

// TODO(crbug.com/1441410): Clean up when the feature is launched.
TEST_F(AutofillSuggestionGeneratorTest, ClearAddressFormSuggestion) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kAutofillUndo);

  personal_data().AddProfile(test::GetFullProfile());
  FormFieldData field;
  field.is_autofilled = true;
  std::vector<Suggestion> suggestions =
      suggestion_generator().GetSuggestionsForProfiles(
          {NAME_FIRST}, field, NAME_FIRST,
          /*last_targeted_fields=*/std::nullopt,
          AutofillSuggestionTriggerSource::kFormControlElementClicked);
  EXPECT_THAT(suggestions,
              ElementsAre(EqualsSuggestion(PopupItemId::kAddressEntry),
                          EqualsSuggestion(PopupItemId::kSeparator),
                          EqualsClearFormSuggestion(),
                          EqualsManageAddressesSuggestion()));
}

#if !BUILDFLAG(IS_IOS)
TEST_F(AutofillSuggestionGeneratorTest, UndoAutofillOnAddressForm) {
  base::test::ScopedFeatureList features(features::kAutofillUndo);

  personal_data().AddProfile(test::GetFullProfile());
  FormFieldData field;
  field.is_autofilled = true;
  std::vector<Suggestion> suggestions =
      suggestion_generator().GetSuggestionsForProfiles(
          {NAME_FIRST}, field, NAME_FIRST,
          /*last_targeted_fields=*/std::nullopt, kDefaultTriggerSource);
  EXPECT_THAT(suggestions,
              ElementsAre(EqualsSuggestion(PopupItemId::kAddressEntry),
                          EqualsSuggestion(PopupItemId::kSeparator),
                          EqualsUndoAutofillSuggestion(),
                          EqualsManageAddressesSuggestion()));
}
#endif

TEST_F(AutofillSuggestionGeneratorTest,
       RemoveExpiredCreditCardsNotUsedSinceTimestamp) {
  const base::Time kNow = AutofillClock::Now();
  const base::Time kDisuseTime =
      kNow - kDisusedDataModelTimeDelta - base::Days(1);
  size_t card_number = 4111111111111111ul;

  std::vector<CreditCard> credit_cards;
  for (bool is_local : {false, true}) {
    for (bool is_expired : {false, true}) {
      for (bool is_disused : {false, true}) {
        // Create a credit card based on the current iteration.
        CreditCard credit_card =
            is_expired ? test::GetExpiredCreditCard() : test::GetCreditCard();
        credit_card.SetNumber(base::NumberToString16(card_number++));
        credit_card.set_use_date(is_disused ? kDisuseTime : kNow);
        if (is_local) {
          credit_card.set_record_type(CreditCard::RecordType::kLocalCard);
          personal_data().AddCreditCard(credit_card);
        } else {
          credit_card.set_record_type(
              CreditCard::RecordType::kMaskedServerCard);
          personal_data().AddServerCreditCard(credit_card);
        }
        credit_cards.push_back(credit_card);
      }
    }
  }
  base::HistogramTester histogram_tester;
  std::vector<CreditCard> cards_to_suggest =
      test_api(suggestion_generator())
          .GetOrderedCardsToSuggest(
              FormFieldData(), UNKNOWN_TYPE, /*suppress_disused_cards=*/true,
              /*prefix_match=*/false, /*include_virtual_cards=*/false);

  // Expect that only the last card (disused, expired and local) is removed.
  credit_cards.pop_back();
  EXPECT_THAT(cards_to_suggest, UnorderedElementsAreArray(credit_cards));

  constexpr char kHistogramName[] = "Autofill.CreditCardsSuppressedForDisuse";
  histogram_tester.ExpectTotalCount(kHistogramName, 1);
  histogram_tester.ExpectBucketCount(kHistogramName, 1, 1);
}

TEST_F(AutofillSuggestionGeneratorTest,
       ManualFallback_UnusedExpiredCardsAreNotSuppressed) {
  CreditCard local_card = test::GetCreditCard();
  local_card.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"04");
  local_card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2000");
  local_card.set_use_date(AutofillClock::Now() - kDisusedDataModelTimeDelta -
                          base::Days(1));
  personal_data().AddCreditCard(local_card);

  bool with_offer;
  bool with_cvc;
  autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
  std::vector<Suggestion> suggestions =
      suggestion_generator().GetSuggestionsForCreditCards(
          FormFieldData(), UNKNOWN_TYPE,
          AutofillSuggestionTriggerSource::kManualFallbackPayments,
          /*should_show_scan_credit_card=*/false,
          /*should_show_cards_from_account=*/false, with_offer, with_cvc,
          metadata_logging_context);

  EXPECT_FALSE(suggestions.empty());
}

TEST_F(AutofillSuggestionGeneratorTest, GetServerCardForLocalCard) {
  CreditCard server_card = CreateServerCard();
  server_card.SetNumber(u"4111111111111111");
  personal_data().AddServerCreditCard(server_card);
  CreditCard local_card =
      CreateLocalCard("00000000-0000-0000-0000-000000000002");

  // The server card should be returned if the local card is passed in.
  const CreditCard* result =
      personal_data().payments_data_manager().GetServerCardForLocalCard(
          &local_card);
  ASSERT_TRUE(result);
  EXPECT_EQ(server_card.guid(), result->guid());

  // Should return nullptr if a server card is passed in.
  EXPECT_FALSE(
      personal_data().payments_data_manager().GetServerCardForLocalCard(
          &server_card));

  // Should return nullptr if no server card has the same information as the
  // local card.
  server_card.SetNumber(u"5454545454545454");
  personal_data().test_payments_data_manager().ClearCreditCards();
  personal_data().AddServerCreditCard(server_card);
  EXPECT_FALSE(
      personal_data().payments_data_manager().GetServerCardForLocalCard(
          &local_card));
}

// The suggestions of credit cards with card linked offers are moved to the
// front. This test checks that the order of the other cards remains stable.
TEST_F(AutofillSuggestionGeneratorTest,
       GetSuggestionsForCreditCards_StableSortBasedOnOffer) {
  // Create three server cards.
  personal_data().test_payments_data_manager().ClearCreditCards();
  personal_data().AddServerCreditCard(CreateServerCard(
      /*guid=*/"00000000-0000-0000-0000-000000000001",
      /*server_id=*/"server_id1", /*instrument_id=*/1));
  personal_data().AddServerCreditCard(CreateServerCard(
      /*guid=*/"00000000-0000-0000-0000-000000000002",
      /*server_id=*/"server_id2", /*instrument_id=*/2));
  personal_data().AddServerCreditCard(CreateServerCard(
      /*guid=*/"00000000-0000-0000-0000-000000000003",
      /*server_id=*/"server_id3", /*instrument_id=*/3));

  // Create a card linked offer and attach it to server_card2.
  AutofillOfferData offer_data = test::GetCardLinkedOfferData1();
  offer_data.SetMerchantOriginForTesting({GURL("http://www.example1.com")});
  offer_data.SetEligibleInstrumentIdForTesting({2});
  autofill_client()->set_last_committed_primary_main_frame_url(
      GURL("http://www.example1.com"));
  personal_data().AddAutofillOfferData(offer_data);

  bool with_offer;
  bool with_cvc;
  autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
  std::vector<Suggestion> suggestions =
      suggestion_generator().GetSuggestionsForCreditCards(
          FormFieldData(), CREDIT_CARD_NUMBER, kDefaultTriggerSource,
          /*should_show_scan_credit_card=*/false,
          /*should_show_cards_from_account=*/false, with_offer, with_cvc,
          metadata_logging_context);

  EXPECT_TRUE(with_offer);
  ASSERT_EQ(suggestions.size(), 5U);
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
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/true));
}

// Ensures we appropriately generate suggestions for virtual cards on a
// standalone CVC field.
TEST_F(AutofillSuggestionGeneratorTest,
       GetSuggestionsForVirtualCardStandaloneCvc) {
  CreditCard server_card = CreateServerCard();
  personal_data().AddServerCreditCard(server_card);

  base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
      virtual_card_guid_to_last_four_map;
  virtual_card_guid_to_last_four_map.insert(
      {server_card.guid(), VirtualCardUsageData::VirtualCardLastFour(u"1234")});
  autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
  std::vector<Suggestion> suggestions =
      suggestion_generator().GetSuggestionsForVirtualCardStandaloneCvc(
          FormFieldData(), metadata_logging_context,
          virtual_card_guid_to_last_four_map);

  ASSERT_EQ(suggestions.size(), 3U);
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/true));
}

TEST_F(AutofillSuggestionGeneratorTest,
       GetSuggestionsForVirtualCardStandaloneCvc_ClearForm) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kAutofillUndo);
  CreditCard server_card = CreateServerCard();
  personal_data().AddServerCreditCard(CreateServerCard());

  base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
      virtual_card_guid_to_last_four_map;
  virtual_card_guid_to_last_four_map.insert(
      {server_card.guid(), VirtualCardUsageData::VirtualCardLastFour(u"4444")});
  autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
  FormFieldData field;
  field.is_autofilled = true;
  std::vector<Suggestion> suggestions =
      suggestion_generator().GetSuggestionsForVirtualCardStandaloneCvc(
          field, metadata_logging_context, virtual_card_guid_to_last_four_map);

  EXPECT_THAT(
      suggestions,
      ElementsAre(
          EqualsSuggestion(PopupItemId::kVirtualCreditCardEntry),
          EqualsSuggestion(PopupItemId::kSeparator),
          EqualsClearFormSuggestion(),
          EqualsManagePaymentsMethodsSuggestion(/*with_gpay_logo=*/true)));
}

#if !BUILDFLAG(IS_IOS)
// TODO(crbug.com/1441410): Clean up when the feature is launched.
TEST_F(AutofillSuggestionGeneratorTest,
       GetSuggestionsForVirtualCardStandaloneCvc_UndoAutofill) {
  base::test::ScopedFeatureList features(features::kAutofillUndo);
  CreditCard server_card = CreateServerCard();
  personal_data().AddServerCreditCard(CreateServerCard());

  base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
      virtual_card_guid_to_last_four_map;
  virtual_card_guid_to_last_four_map.insert(
      {server_card.guid(), VirtualCardUsageData::VirtualCardLastFour(u"4444")});
  autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
  FormFieldData field;
  field.is_autofilled = true;
  std::vector<Suggestion> suggestions =
      suggestion_generator().GetSuggestionsForVirtualCardStandaloneCvc(
          field, metadata_logging_context, virtual_card_guid_to_last_four_map);

  EXPECT_THAT(
      suggestions,
      ElementsAre(
          EqualsSuggestion(PopupItemId::kVirtualCreditCardEntry),
          EqualsSuggestion(PopupItemId::kSeparator),
          EqualsUndoAutofillSuggestion(),
          EqualsManagePaymentsMethodsSuggestion(/*with_gpay_logo=*/true)));
}
#endif

// Ensures we appropriately generate suggestions for credit saved with CVC.
TEST_F(AutofillSuggestionGeneratorTest, GetCardSuggestionsWithCvc) {
  CreditCard card = test::WithCvc(test::GetMaskedServerCard2());
  personal_data().AddServerCreditCard(card);

  bool with_offer;
  bool with_cvc;
  autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
  std::vector<Suggestion> suggestions =
      suggestion_generator().GetSuggestionsForCreditCards(
          FormFieldData(), CREDIT_CARD_NUMBER, kDefaultTriggerSource,
          /*should_show_scan_credit_card=*/false,
          /*should_show_cards_from_account=*/false, with_offer, with_cvc,
          metadata_logging_context);

  ASSERT_EQ(suggestions.size(), 3U);
  EXPECT_TRUE(with_cvc);
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/true));
}

// Verifies that the GPay logo is set correctly.
TEST_F(AutofillSuggestionGeneratorTest, ShouldDisplayGpayLogo) {
  // GPay logo should be displayed if suggestions were all for server cards;
  {
    // Create two server cards.
    personal_data().AddServerCreditCard(CreateServerCard(
        /*guid=*/"00000000-0000-0000-0000-000000000001",
        /*server_id=*/"server_id1", /*instrument_id=*/1));
    personal_data().AddServerCreditCard(CreateServerCard(
        /*guid=*/"00000000-0000-0000-0000-000000000002",
        /*server_id=*/"server_id2", /*instrument_id=*/2));

    bool with_offer;
    bool with_cvc;
    autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
    std::vector<Suggestion> suggestions =
        suggestion_generator().GetSuggestionsForCreditCards(
            FormFieldData(), CREDIT_CARD_NUMBER, kDefaultTriggerSource,
            /*should_show_scan_credit_card=*/false,
            /*should_show_cards_from_account=*/false, with_offer, with_cvc,
            metadata_logging_context);

    EXPECT_EQ(suggestions.size(), 4U);
    EXPECT_THAT(suggestions,
                ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/true));
  }

  personal_data().test_payments_data_manager().ClearCreditCards();

  // GPay logo should not be displayed if at least one local card was in the
  // suggestions.
  {
    // Create one server card and one local card.
    auto local_card = CreateLocalCard(
        /*guid=*/"00000000-0000-0000-0000-000000000001");
    local_card.SetNumber(u"5454545454545454");
    personal_data().AddCreditCard(local_card);
    personal_data().AddServerCreditCard(CreateServerCard(
        /*guid=*/"00000000-0000-0000-0000-000000000002",
        /*server_id=*/"server_id2", /*instrument_id=*/2));

    bool with_offer;
    bool with_cvc;
    autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
    std::vector<Suggestion> suggestions =
        suggestion_generator().GetSuggestionsForCreditCards(
            FormFieldData(), CREDIT_CARD_NUMBER, kDefaultTriggerSource,
            /*should_show_scan_credit_card=*/false,
            /*should_show_cards_from_account=*/false, with_offer, with_cvc,
            metadata_logging_context);

    EXPECT_EQ(suggestions.size(), 4U);
    EXPECT_THAT(suggestions,
                ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/false));
  }

  personal_data().test_payments_data_manager().ClearCreditCards();

  // GPay logo should be displayed if there was an unused expired local card in
  // the suggestions.
  {
    // Create one server card and one unused expired local card.
    auto local_card = CreateLocalCard(
        /*guid=*/"00000000-0000-0000-0000-000000000001");
    local_card.SetNumber(u"5454545454545454");
    local_card.SetExpirationYear(2020);
    local_card.set_use_date(AutofillClock::Now() - base::Days(365));
    personal_data().AddCreditCard(local_card);
    personal_data().AddServerCreditCard(CreateServerCard(
        /*guid=*/"00000000-0000-0000-0000-000000000002",
        /*server_id=*/"server_id2", /*instrument_id=*/2));

    bool with_offer;
    bool with_cvc;
    autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
    std::vector<Suggestion> suggestions =
        suggestion_generator().GetSuggestionsForCreditCards(
            FormFieldData(), CREDIT_CARD_NUMBER, kDefaultTriggerSource,
            /*should_show_scan_credit_card=*/false,
            /*should_show_cards_from_account=*/false, with_offer, with_cvc,
            metadata_logging_context);

    EXPECT_EQ(suggestions.size(), 3U);
    EXPECT_THAT(suggestions,
                ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/true));
  }
}

TEST_F(AutofillSuggestionGeneratorTest, NoSuggestionsWhenNoUserData) {
  bool with_offer;
  bool with_cvc;
  autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
  FormFieldData field;
  field.is_autofilled = true;
  std::vector<Suggestion> suggestions =
      suggestion_generator().GetSuggestionsForCreditCards(
          field, CREDIT_CARD_NUMBER, kDefaultTriggerSource,
          /*should_show_scan_credit_card=*/true,
          /*should_show_cards_from_account=*/true, with_offer, with_cvc,
          metadata_logging_context);

  EXPECT_TRUE(suggestions.empty());
}

TEST_F(AutofillSuggestionGeneratorTest, ShouldShowScanCreditCard) {
  personal_data().AddCreditCard(test::GetCreditCard());
  bool with_offer;
  bool with_cvc;
  autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
  std::vector<Suggestion> suggestions =
      suggestion_generator().GetSuggestionsForCreditCards(
          FormFieldData(), CREDIT_CARD_NUMBER, kDefaultTriggerSource,
          /*should_show_scan_credit_card=*/true,
          /*should_show_cards_from_account=*/false, with_offer, with_cvc,
          metadata_logging_context);

  EXPECT_EQ(suggestions.size(), 4ul);
  EXPECT_THAT(suggestions[0], EqualsSuggestion(PopupItemId::kCreditCardEntry));
  EXPECT_THAT(
      suggestions[1],
      EqualsSuggestion(PopupItemId::kScanCreditCard,
                       l10n_util::GetStringUTF16(IDS_AUTOFILL_SCAN_CREDIT_CARD),
                       Suggestion::Icon::kScanCreditCard));
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/false));
}

TEST_F(AutofillSuggestionGeneratorTest, ShouldShowCardsFromAccount) {
  personal_data().AddCreditCard(test::GetCreditCard());
  bool with_offer;
  bool with_cvc;
  autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
  std::vector<Suggestion> suggestions =
      suggestion_generator().GetSuggestionsForCreditCards(
          FormFieldData(), CREDIT_CARD_NUMBER, kDefaultTriggerSource,
          /*should_show_scan_credit_card=*/false,
          /*should_show_cards_from_account=*/true, with_offer, with_cvc,
          metadata_logging_context);

  EXPECT_EQ(suggestions.size(), 4ul);
  EXPECT_THAT(suggestions[0], EqualsSuggestion(PopupItemId::kCreditCardEntry));
  EXPECT_THAT(suggestions[1],
              EqualsSuggestion(
                  PopupItemId::kShowAccountCards,
                  l10n_util::GetStringUTF16(IDS_AUTOFILL_SHOW_ACCOUNT_CARDS),
                  Suggestion::Icon::kGoogle));
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/false));
}

// TODO(crbug.com/1441410): Clean up when the feature is launched.
TEST_F(AutofillSuggestionGeneratorTest,
       FieldWasAutofilled_ClearCreditCardFormSuggestion) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kAutofillUndo);
  personal_data().AddCreditCard(test::GetCreditCard());
  bool with_offer;
  bool with_cvc;
  autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
  FormFieldData field;
  field.is_autofilled = true;
  std::vector<Suggestion> suggestions =
      suggestion_generator().GetSuggestionsForCreditCards(
          field, CREDIT_CARD_NUMBER, kDefaultTriggerSource,
          /*should_show_scan_credit_card=*/false,
          /*should_show_cards_from_account=*/false, with_offer, with_cvc,
          metadata_logging_context);

  EXPECT_THAT(suggestions,
              ElementsAre(EqualsSuggestion(PopupItemId::kCreditCardEntry),
                          EqualsSuggestion(PopupItemId::kSeparator),
                          EqualsClearFormSuggestion(),
                          EqualsManagePaymentsMethodsSuggestion(
                              /*with_gpay_logo=*/false)));
}

#if !BUILDFLAG(IS_IOS)
TEST_F(AutofillSuggestionGeneratorTest,
       FieldWasAutofilled_UndoAutofillOnCreditCardForm) {
  base::test::ScopedFeatureList features(features::kAutofillUndo);
  personal_data().AddCreditCard(test::GetCreditCard());
  bool with_offer;
  bool with_cvc;
  autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
  FormFieldData field;
  field.is_autofilled = true;
  std::vector<Suggestion> suggestions =
      suggestion_generator().GetSuggestionsForCreditCards(
          field, CREDIT_CARD_NUMBER, kDefaultTriggerSource,
          /*should_show_scan_credit_card=*/false,
          /*should_show_cards_from_account=*/false, with_offer, with_cvc,
          metadata_logging_context);

  EXPECT_THAT(suggestions,
              ElementsAre(EqualsSuggestion(PopupItemId::kCreditCardEntry),
                          EqualsSuggestion(PopupItemId::kSeparator),
                          EqualsUndoAutofillSuggestion(),
                          EqualsManagePaymentsMethodsSuggestion(
                              /*with_gpay_logo=*/false)));
}
#endif

// Test that the virtual card option is shown when all of the prerequisites are
// met.
TEST_F(AutofillSuggestionGeneratorTest, ShouldShowVirtualCardOption) {
  // Create a server card.
  CreditCard server_card =
      CreateServerCard(/*guid=*/"00000000-0000-0000-0000-000000000001");
  server_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  personal_data().AddServerCreditCard(server_card);

  // Create a local card with same information.
  CreditCard local_card =
      CreateLocalCard(/*guid=*/"00000000-0000-0000-0000-000000000002");

  // If all prerequisites are met, it should return true.
  EXPECT_TRUE(test_api(suggestion_generator())
                  .ShouldShowVirtualCardOption(&server_card));
  EXPECT_TRUE(test_api(suggestion_generator())
                  .ShouldShowVirtualCardOption(&local_card));
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
  personal_data().AddServerCreditCard(server_card);
  autofill_client()->ResetAutofillOptimizationGuide();

  // Create a local card with same information.
  CreditCard local_card =
      CreateLocalCard(/*guid=*/"00000000-0000-0000-0000-000000000002");

  // If all prerequisites are met, it should return true.
  EXPECT_TRUE(test_api(suggestion_generator())
                  .ShouldShowVirtualCardOption(&server_card));
  EXPECT_TRUE(test_api(suggestion_generator())
                  .ShouldShowVirtualCardOption(&local_card));
}

// Test that the virtual card option is shown even if the merchant is opted-out
// of virtual cards.
TEST_F(AutofillSuggestionGeneratorTest,
       ShouldShowVirtualCardOption_InDisabledStateForOptedOutMerchants) {
  base::test::ScopedFeatureList features(
      features::kAutofillEnableVcnGrayOutForMerchantOptOut);

  // Create an enrolled server card.
  CreditCard server_card =
      test::GetMaskedServerCardEnrolledIntoVirtualCardNumber();
  personal_data().AddServerCreditCard(server_card);

  // Even if the URL is opted-out of virtual cards for `server_card`, display
  // the virtual card suggestion.
  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
              autofill_client()->GetAutofillOptimizationGuide()),
          ShouldBlockFormFieldSuggestion)
      .WillByDefault(testing::Return(true));
  EXPECT_TRUE(test_api(suggestion_generator())
                  .ShouldShowVirtualCardOption(&server_card));
}

// Test that the virtual card option is not shown if the merchant is opted-out
// of virtual cards.
TEST_F(AutofillSuggestionGeneratorTest,
       ShouldNotShowVirtualCardOption_MerchantOptedOutOfVirtualCards) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(
      features::kAutofillEnableVcnGrayOutForMerchantOptOut);
  // Create an enrolled server card.
  CreditCard server_card =
      CreateServerCard(/*guid=*/"00000000-0000-0000-0000-000000000001");
  server_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  personal_data().AddServerCreditCard(server_card);

  // Create a local card with same information.
  CreditCard local_card =
      CreateLocalCard(/*guid=*/"00000000-0000-0000-0000-000000000002");

  // If the URL is opted-out of virtual cards for `server_card`, do not display
  // the virtual card suggestion.
  auto* optimization_guide = autofill_client()->GetAutofillOptimizationGuide();
  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(optimization_guide),
          ShouldBlockFormFieldSuggestion)
      .WillByDefault(testing::Return(true));
  EXPECT_FALSE(test_api(suggestion_generator())
                   .ShouldShowVirtualCardOption(&server_card));
  EXPECT_FALSE(test_api(suggestion_generator())
                   .ShouldShowVirtualCardOption(&local_card));
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
  personal_data().AddServerCreditCard(server_card);

  // Create a local card with same information.
  CreditCard local_card =
      CreateLocalCard(/*guid=*/"00000000-0000-0000-0000-000000000002");

  // For server card not enrolled, both local and server card should return
  // false.
  EXPECT_FALSE(test_api(suggestion_generator())
                   .ShouldShowVirtualCardOption(&server_card));
  EXPECT_FALSE(test_api(suggestion_generator())
                   .ShouldShowVirtualCardOption(&local_card));
}

// Test that the virtual card option is not shown for a local card with no
// server card duplicate.
TEST_F(AutofillSuggestionGeneratorTest,
       ShouldNotShowVirtualCardOption_LocalCardWithoutServerCardDuplicate) {
  // Create a local card with same information.
  CreditCard local_card =
      CreateLocalCard(/*guid=*/"00000000-0000-0000-0000-000000000002");

  // The local card does not have a server duplicate, should return false.
  EXPECT_FALSE(test_api(suggestion_generator())
                   .ShouldShowVirtualCardOption(&local_card));
}

TEST_F(AutofillSuggestionGeneratorTest, GetTouchToFillIbansToSuggest) {
  Iban local_iban1;
  local_iban1.set_value(std::u16string(test::kIbanValue16));
  local_iban1.set_identifier(
      Iban::Guid(personal_data().AddAsLocalIban(local_iban1)));
  local_iban1.set_record_type(Iban::kLocalIban);
  Iban server_iban1 = test::GetServerIban2();
  Iban server_iban2 = test::GetServerIban3();
  personal_data().AddServerIban(server_iban1);
  personal_data().AddServerIban(server_iban2);

  std::vector<Iban> available_ibans =
      suggestion_generator().GetTouchToFillIbansToSuggest();

  EXPECT_THAT(available_ibans,
              ElementsAreArray({server_iban2, server_iban1, local_iban1}));
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
                           Suggestion::Guid(iban0.guid()), iban0.nickname()));

  EXPECT_THAT(
      iban_suggestions[1],
      EqualsIbanSuggestion(iban1.GetIdentifierStringForAutofillDisplay(),
                           Suggestion::Guid(iban1.guid()), iban1.nickname()));

  EXPECT_THAT(
      iban_suggestions[2],
      EqualsIbanSuggestion(iban2.GetIdentifierStringForAutofillDisplay(),
                           Suggestion::Guid(iban2.guid()), iban2.nickname()));

  EXPECT_THAT(
      iban_suggestions[3],
      EqualsIbanSuggestion(iban3.GetIdentifierStringForAutofillDisplay(),
                           Suggestion::Guid(iban3.guid()), iban3.nickname()));

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

  EXPECT_THAT(
      iban_suggestions[2],
      EqualsIbanSuggestion(local_iban1.GetIdentifierStringForAutofillDisplay(),
                           Suggestion::Guid(local_iban1.guid()),
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
  EXPECT_THAT(promo_code_suggestions[0],
              EqualLabels({{u"test_value_prop_text_1"}}));
  EXPECT_EQ(promo_code_suggestions[0].GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(Suggestion::Guid("1")));
  EXPECT_EQ(promo_code_suggestions[0].popup_item_id,
            PopupItemId::kMerchantPromoCodeEntry);

  EXPECT_EQ(promo_code_suggestions[1].main_text.value, u"test_promo_code_2");
  EXPECT_EQ(promo_code_suggestions[1].GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(Suggestion::Guid("2")));
  EXPECT_THAT(promo_code_suggestions[1],
              EqualLabels({{u"test_value_prop_text_2"}}));
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
  EXPECT_THAT(promo_code_suggestions[0],
              EqualLabels({{u"test_value_prop_text_1"}}));
  EXPECT_FALSE(
      absl::holds_alternative<GURL>(promo_code_suggestions[0].payload));
  EXPECT_EQ(promo_code_suggestions[0].popup_item_id,
            PopupItemId::kMerchantPromoCodeEntry);
}

TEST_F(AutofillSuggestionGeneratorTest, TestAddressSuggestion) {
  AutofillProfile profile = test::GetFullProfile();
  autofill_client()->set_test_addresses({profile});
  std::vector<Suggestion> suggestions =
      test_api(suggestion_generator())
          .CreateSuggestionsFromProfiles(
              {&profile}, /*field_types=*/{NAME_FIRST},
              /*last_targeted_fields=*/kAllFieldTypes, NAME_FIRST,
              /*trigger_field_max_length=*/0);

  // There should be test address suggestion and one regular profile
  // suggestion.
  ASSERT_EQ(suggestions.size(), 2u);
  EXPECT_EQ(suggestions[0].popup_item_id, PopupItemId::kDevtoolsTestAddresses);
  EXPECT_EQ(suggestions[1].popup_item_id, PopupItemId::kAddressEntry);

  EXPECT_EQ(suggestions[0].main_text.value, u"Devtools");
  EXPECT_THAT(suggestions[0], EqualLabels({{u"Address test data"}}));
  EXPECT_EQ(suggestions[0].icon, Suggestion::Icon::kCode);
  EXPECT_EQ(suggestions[0].children.size(), 1u);
  EXPECT_FALSE(suggestions[0].is_acceptable);

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
                              features::kAutofillEnableCardProductName,
                              features::
                                  kAutofillEnableVcnGrayOutForMerchantOptOut},
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
      test_api(suggestion_generator())
          .CreateCreditCardSuggestion(server_card, CREDIT_CARD_NAME_FULL,
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
    EXPECT_THAT(virtual_card_name_field_suggestion,
                EqualLabels({{CreditCard::GetObfuscatedStringForCardDigits(
                    /*obfuscation_length=*/2, u"1111")}}));
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
  EXPECT_EQ(virtual_card_name_field_suggestion.is_acceptable, true);
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
      test_api(suggestion_generator())
          .CreateCreditCardSuggestion(server_card, CREDIT_CARD_NUMBER,
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
  EXPECT_EQ(virtual_card_number_field_suggestion.is_acceptable, true);
  if (keyboard_accessory_enabled()) {
    // For the keyboard accessory, there is no label.
    ASSERT_TRUE(virtual_card_number_field_suggestion.labels.empty());
  } else {
    // For Desktop/Android dropdown, and on iOS, "Virtual card" is the label.
    EXPECT_THAT(virtual_card_number_field_suggestion,
                EqualLabels({{u"Virtual card"}}));
  }
}

// Verify that the suggestion's texts are populated correctly for a masked
// server card suggestion when the cardholder name field is focused.
TEST_F(AutofillCreditCardSuggestionContentTest,
       CreateCreditCardSuggestion_MaskedServerCardMetadata_NameField) {
  CreditCard server_card = CreateServerCard();

  // Name field suggestion for non-virtual cards.
  Suggestion real_card_name_field_suggestion =
      test_api(suggestion_generator())
          .CreateCreditCardSuggestion(server_card, CREDIT_CARD_NAME_FULL,
                                      /*virtual_card_option=*/false,
                                      /*card_linked_offer_available=*/false);

  // Only the name is displayed on the first line.
  EXPECT_EQ(real_card_name_field_suggestion.main_text.value, u"Elvis Presley");
  EXPECT_EQ(real_card_name_field_suggestion.minor_text.value, u"");

#if BUILDFLAG(IS_IOS)
  // For IOS, the label is "..1111" or "....1111".
  EXPECT_THAT(real_card_name_field_suggestion,
              EqualLabels({{CreditCard::GetObfuscatedStringForCardDigits(
                  ios_obfuscation_length(), u"1111")}}));
#else
  if (keyboard_accessory_enabled()) {
    // For the keyboard accessory, the label is "..1111".
    EXPECT_THAT(real_card_name_field_suggestion,
                EqualLabels({{CreditCard::GetObfuscatedStringForCardDigits(
                    /*obfuscation_length=*/2, u"1111")}}));
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
      test_api(suggestion_generator())
          .CreateCreditCardSuggestion(server_card, CREDIT_CARD_NUMBER,
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
  EXPECT_THAT(
      real_card_number_field_suggestion,
      EqualLabels(
          {{base::StrCat({base::UTF8ToUTF16(test::NextMonth()), u"/",
                          base::UTF8ToUTF16(test::NextYear().substr(2))})}}));
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Verify that the suggestion's texts are populated correctly for a masked
// server card suggestion when payments manual fallback is triggered.
TEST_F(AutofillCreditCardSuggestionContentTest,
       CreateCreditCardSuggestion_ManualFallback) {
  CreditCard server_card = CreateServerCard();

  Suggestion server_card_suggestion =
      test_api(suggestion_generator())
          .CreateCreditCardSuggestion(server_card, UNKNOWN_TYPE,
                                      /*virtual_card_option=*/false,
                                      /*card_linked_offer_available=*/false);

  // Only the name is displayed on the first line.
  EXPECT_EQ(server_card_suggestion.popup_item_id,
            PopupItemId::kCreditCardEntry);
  EXPECT_EQ(server_card_suggestion.is_acceptable, false);
  // For Desktop, split the first line and populate the card name and
  // the last 4 digits separately.
  EXPECT_EQ(server_card_suggestion.main_text.value, u"Visa");
  EXPECT_EQ(server_card_suggestion.minor_text.value,
            server_card.ObfuscatedNumberWithVisibleLastFourDigits(4));

  // The label is the expiration date formatted as mm/yy.
  EXPECT_THAT(server_card_suggestion,
              EqualLabels({{server_card.GetInfo(
                  CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, app_locale())}}));

  EXPECT_EQ(server_card_suggestion.acceptance_a11y_announcement,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_A11Y_ANNOUNCE_EXPANDABLE_ONLY_ENTRY));
}

// Verify that the virtual credit card suggestion has the correct
// `Suggestion::popup_item_id, AX label and is selectable.
TEST_F(AutofillCreditCardSuggestionContentTest,
       CreateCreditCardSuggestion_ManualFallback_VirtualCreditCard) {
  CreditCard enrolled_card = test::GetVirtualCard();

  Suggestion enrolled_card_suggestion =
      test_api(suggestion_generator())
          .CreateCreditCardSuggestion(enrolled_card, UNKNOWN_TYPE,
                                      /*virtual_card_option=*/true,
                                      /*card_linked_offer_available=*/false);

  // Only the name is displayed on the first line.
  EXPECT_EQ(enrolled_card_suggestion.popup_item_id,
            PopupItemId::kVirtualCreditCardEntry);
  EXPECT_EQ(enrolled_card_suggestion.is_acceptable, true);
  EXPECT_EQ(enrolled_card_suggestion.acceptance_a11y_announcement,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_A11Y_ANNOUNCE_VIRTUAL_CARD_MANUAL_FALLBACK_ENTRY));
}

// Verify that the virtual credit card suggestion has the correct labels.
TEST_F(AutofillCreditCardSuggestionContentTest,
       CreateCreditCardSuggestion_ManualFallback_VirtualCreditCard_Labels) {
  CreditCard enrolled_card = test::GetVirtualCard();

  Suggestion enrolled_card_suggestion =
      test_api(suggestion_generator())
          .CreateCreditCardSuggestion(enrolled_card, UNKNOWN_TYPE,
                                      /*virtual_card_option=*/true,
                                      /*card_linked_offer_available=*/false);

  // For Desktop, split the first line and populate the card name and
  // the last 4 digits separately.
  EXPECT_EQ(enrolled_card_suggestion.main_text.value, u"Mastercard");
  EXPECT_EQ(enrolled_card_suggestion.minor_text.value,
            enrolled_card.ObfuscatedNumberWithVisibleLastFourDigits(4));

  // The label is the expiration date formatted as mm/yy.
  EXPECT_EQ(enrolled_card_suggestion.labels.size(), 2U);
  EXPECT_EQ(enrolled_card_suggestion.labels[0].size(), 1U);
  EXPECT_EQ(
      enrolled_card_suggestion.labels[0][0].value,
      enrolled_card.GetInfo(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, app_locale()));
  EXPECT_EQ(enrolled_card_suggestion.labels[1].size(), 1U);
  EXPECT_EQ(enrolled_card_suggestion.labels[1][0].value,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_OPTION_VALUE));
}

// Verify that the virtual credit card suggestion has no nested suggestions.
TEST_F(
    AutofillCreditCardSuggestionContentTest,
    CreateCreditCardSuggestion_ManualFallback_VirtualCreditCard_NestedSuggestions) {
  CreditCard enrolled_card =
      test::GetMaskedServerCardEnrolledIntoVirtualCardNumber();

  Suggestion enrolled_card_suggestion =
      test_api(suggestion_generator())
          .CreateCreditCardSuggestion(enrolled_card, UNKNOWN_TYPE,
                                      /*virtual_card_option=*/true,
                                      /*card_linked_offer_available=*/false);

  EXPECT_TRUE(enrolled_card_suggestion.children.empty());
}

// Verify that the nested suggestion's texts are populated correctly for a
// masked server card suggestion when payments manual fallback is triggered.
TEST_F(AutofillCreditCardSuggestionContentTest,
       CreateCreditCardSuggestion_ManualFallback_NestedSuggestions) {
  CreditCard server_card = test::GetMaskedServerCard();

  Suggestion server_card_suggestion =
      test_api(suggestion_generator())
          .CreateCreditCardSuggestion(server_card, UNKNOWN_TYPE,
                                      /*virtual_card_option=*/false,
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
              PopupItemId::kCreditCardFieldByFieldFilling,
              server_card.GetInfo(CREDIT_CARD_NAME_FULL, app_locale()),
              CREDIT_CARD_NAME_FULL, Suggestion::Guid(server_card.guid())),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kCreditCardFieldByFieldFilling,
              server_card.ObfuscatedNumberWithVisibleLastFourDigits(12),
              CREDIT_CARD_NUMBER, Suggestion::Guid(server_card.guid()),
              {{Suggestion::Text(l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_PAYMENTS_MANUAL_FALLBACK_AUTOFILL_POPUP_CC_NUMBER_SUGGESTION_LABEL))}}),
          AllOf(Field(&Suggestion::popup_item_id, PopupItemId::kSeparator)),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kCreditCardFieldByFieldFilling,
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
      test_api(suggestion_generator())
          .CreateCreditCardSuggestion(credit_card, UNKNOWN_TYPE,
                                      /*virtual_card_option=*/false,
                                      /*card_linked_offer_available=*/false);

  // The child suggestions should be:
  //
  // 1. Credit card full name
  // 2. Credit card number
  EXPECT_THAT(
      server_card_suggestion.children,
      ElementsAre(
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kCreditCardFieldByFieldFilling,
              credit_card.GetInfo(CREDIT_CARD_NAME_FULL, app_locale()),
              CREDIT_CARD_NAME_FULL, Suggestion::Guid(credit_card.guid())),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kCreditCardFieldByFieldFilling,
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
      test_api(suggestion_generator())
          .CreateCreditCardSuggestion(credit_card, UNKNOWN_TYPE,
                                      /*virtual_card_option=*/false,
                                      /*card_linked_offer_available=*/false);

  // The child suggestions should be:
  //
  // 1. Credit card expiry date
  EXPECT_THAT(
      server_card_suggestion.children,
      ElementsAre(EqualsFieldByFieldFillingSuggestion(
          PopupItemId::kCreditCardFieldByFieldFilling,
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
      test_api(suggestion_generator())
          .CreateCreditCardSuggestion(server_card, UNKNOWN_TYPE,
                                      /*virtual_card_option=*/false,
                                      /*card_linked_offer_available=*/false);

  // The expiry date child suggestions should be:
  //
  // 1. Expiry year.
  // 2. Expiry month.
  EXPECT_THAT(
      server_card_suggestion.children[3].children,
      ElementsAre(
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kCreditCardFieldByFieldFilling,
              server_card.GetInfo(CREDIT_CARD_EXP_MONTH, app_locale()),
              CREDIT_CARD_EXP_MONTH, Suggestion::Guid(server_card.guid()),
              {{Suggestion::Text(l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_PAYMENTS_MANUAL_FALLBACK_AUTOFILL_POPUP_CC_EXPIRY_MONTH_SUGGESTION_LABEL))}}),
          EqualsFieldByFieldFillingSuggestion(
              PopupItemId::kCreditCardFieldByFieldFilling,
              server_card.GetInfo(CREDIT_CARD_EXP_2_DIGIT_YEAR, app_locale()),
              CREDIT_CARD_EXP_2_DIGIT_YEAR,
              Suggestion::Guid(server_card.guid()),
              {{Suggestion::Text(l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_PAYMENTS_MANUAL_FALLBACK_AUTOFILL_POPUP_CC_EXPIRY_YEAR_SUGGESTION_LABEL))}})));
}

// Verify that manual fallback credit card suggestions are not filtered.
TEST_F(AutofillCreditCardSuggestionContentTest,
       GetSuggestionsForCreditCards_ManualFallbackSuggestionsNotFiltered) {
  personal_data().AddServerCreditCard(CreateServerCard());

  FormFieldData field_data;
  field_data.set_value(u"$$$");
  bool with_offer;
  bool with_cvc;
  autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
  std::vector<Suggestion> suggestions =
      suggestion_generator().GetSuggestionsForCreditCards(
          field_data, UNKNOWN_TYPE,
          AutofillSuggestionTriggerSource::kManualFallbackPayments,
          /*should_show_scan_credit_card=*/false,
          /*should_show_cards_from_account=*/false, with_offer, with_cvc,
          metadata_logging_context);

  // Credit card suggestions should not depend on the field's value.
  EXPECT_EQ(suggestions.size(), 3U);
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/true));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Verify that the suggestion's texts are populated correctly for a local and
// server card suggestion when the CVC field is focused.
TEST_F(AutofillCreditCardSuggestionContentTest,
       GetSuggestionsForCreditCards_CvcField) {
  // Create one server card and one local card with CVC.
  CreditCard local_card = CreateLocalCard();
  // We used last 4 to deduplicate local card and server card so we should set
  // local card with different last 4.
  local_card.SetNumber(u"5454545454545454");
  personal_data().AddCreditCard(std::move(local_card));
  personal_data().AddServerCreditCard(CreateServerCard());

  bool with_offer;
  bool with_cvc;
  autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
  const std::vector<Suggestion> suggestions =
      suggestion_generator().GetSuggestionsForCreditCards(
          FormFieldData(), CREDIT_CARD_VERIFICATION_CODE, kDefaultTriggerSource,
          /*should_show_scan_credit_card=*/false,
          /*should_show_cards_from_account=*/false, with_offer, with_cvc,
          metadata_logging_context);

  // Both local card and server card suggestion should be shown when CVC field
  // is focused.
  ASSERT_EQ(suggestions.size(), 4U);
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(suggestions[0].main_text.value, u"CVC");
  EXPECT_EQ(suggestions[1].main_text.value, u"CVC");
  EXPECT_EQ(suggestions[0].minor_text.value, u"");
  EXPECT_EQ(suggestions[1].minor_text.value, u"");
#else
  EXPECT_EQ(suggestions[0].main_text.value, u"CVC for Visa");
  EXPECT_EQ(suggestions[1].main_text.value, u"CVC for Mastercard");
  EXPECT_EQ(suggestions[0].minor_text.value, u"");
  EXPECT_EQ(suggestions[1].minor_text.value, u"");
#endif
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/false));
}

// Verify that the suggestion's texts are populated correctly for a duplicate
// local and server card suggestion when the CVC field is focused.
TEST_F(AutofillCreditCardSuggestionContentTest,
       GetSuggestionsForCreditCards_Duplicate_CvcField) {
  // Create 2 duplicate local and server card with same last 4.
  personal_data().AddCreditCard(CreateLocalCard());
  personal_data().AddServerCreditCard(CreateServerCard());

  bool with_offer;
  bool with_cvc;
  autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
  const std::vector<Suggestion> suggestions =
      suggestion_generator().GetSuggestionsForCreditCards(
          FormFieldData(), CREDIT_CARD_VERIFICATION_CODE, kDefaultTriggerSource,
          /*should_show_scan_credit_card=*/false,
          /*should_show_cards_from_account=*/false, with_offer, with_cvc,
          metadata_logging_context);

  // Only 1 suggestion + footer should be shown when CVC field is focused.
  ASSERT_EQ(suggestions.size(), 3U);
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/true));
}

// Verify that the FPAN and VCN suggestion's texts are populated correctly for a
// enrolled card when the CVC field is focused.
TEST_F(AutofillCreditCardSuggestionContentTest,
       GetSuggestionsForCreditCards_VirtualCard_CvcField) {
  // Create a server card with CVC that enrolled to virtual card.
  CreditCard server_card = CreateServerCard();
  server_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  personal_data().AddServerCreditCard(std::move(server_card));

  bool with_offer;
  bool with_cvc;
  autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
  const std::vector<Suggestion> suggestions =
      suggestion_generator().GetSuggestionsForCreditCards(
          FormFieldData(), CREDIT_CARD_VERIFICATION_CODE, kDefaultTriggerSource,
          /*should_show_scan_credit_card=*/false,
          /*should_show_cards_from_account=*/false, with_offer, with_cvc,
          metadata_logging_context);

  // Both FPAN and VCN suggestion should be shown when CVC field is focused.
  ASSERT_EQ(suggestions.size(), 4U);

#if !BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(suggestions[0].main_text.value, u"CVC");
  EXPECT_EQ(suggestions[1].main_text.value, u"CVC");
  EXPECT_EQ(suggestions[0].minor_text.value, u"");
  EXPECT_EQ(suggestions[1].minor_text.value, u"");
#else
  EXPECT_EQ(suggestions[0].main_text.value, u"Virtual card  CVC for Visa");
  EXPECT_EQ(suggestions[1].main_text.value, u"CVC for Visa");
  EXPECT_EQ(suggestions[0].minor_text.value, u"");
  EXPECT_EQ(suggestions[1].minor_text.value, u"");
#endif
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/true));
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
  personal_data().AddServerCreditCard(std::move(server_card));
  personal_data().AddCreditCard(CreateLocalCard());

  bool with_offer;
  bool with_cvc;
  autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
  const std::vector<Suggestion> suggestions =
      suggestion_generator().GetSuggestionsForCreditCards(
          FormFieldData(), CREDIT_CARD_VERIFICATION_CODE, kDefaultTriggerSource,
          /*should_show_scan_credit_card=*/false,
          /*should_show_cards_from_account=*/false, with_offer, with_cvc,
          metadata_logging_context);

  // Both FPAN and VCN suggestion should be shown when CVC field is focused.
  ASSERT_EQ(suggestions.size(), 4U);
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/true));
}

#if BUILDFLAG(IS_IOS)
TEST_F(AutofillCreditCardSuggestionContentTest,
       GetSuggestionsForCreditCards_LargeKeyboardAccessoryFormat) {
  // Enable formatting for large keyboard accessories.
  autofill_client()->set_format_for_large_keyboard_accessory(true);

  CreditCard server_card = CreateServerCard();

  int obfuscation_length = ios_obfuscation_length();

  const std::u16string obfuscated_number =
      CreditCard::GetObfuscatedStringForCardDigits(obfuscation_length, u"1111");
  const std::u16string name_full =
      server_card.GetRawInfo(CREDIT_CARD_NAME_FULL);
  const std::u16string exp_date =
      server_card.GetRawInfo(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR);
  const std::u16string card_type = server_card.GetRawInfo(CREDIT_CARD_TYPE);
  const std::u16string type_and_number =
      base::StrCat({card_type, u"  ", obfuscated_number});

  Suggestion card_number_field_suggestion =
      test_api(suggestion_generator())
          .CreateCreditCardSuggestion(server_card, CREDIT_CARD_NUMBER,
                                      /*virtual_card_option=*/false,
                                      /*card_linked_offer_available=*/false);

  // From the credit card number field, the suggestion should show the card type
  // and number and the label should show the expiration date.
  EXPECT_EQ(card_number_field_suggestion.main_text.value, type_and_number);
  EXPECT_THAT(card_number_field_suggestion, EqualLabels({{exp_date}}));

  card_number_field_suggestion =
      test_api(suggestion_generator())
          .CreateCreditCardSuggestion(server_card, CREDIT_CARD_NAME_FULL,
                                      /*virtual_card_option=*/false,
                                      /*card_linked_offer_available=*/false);

  // From the credit card name field, the suggestion should show the full name
  // and the label should show the card type and number.
  EXPECT_EQ(card_number_field_suggestion.main_text.value,
            base::StrCat({name_full}));
  EXPECT_THAT(card_number_field_suggestion, EqualLabels({{type_and_number}}));

  card_number_field_suggestion =
      test_api(suggestion_generator())
          .CreateCreditCardSuggestion(server_card, CREDIT_CARD_EXP_MONTH,
                                      /*virtual_card_option=*/false,
                                      /*card_linked_offer_available=*/false);

  // From a credit card expiry field, the suggestion should show the expiration
  // date and the label should show the card type and number.
  EXPECT_EQ(card_number_field_suggestion.main_text.value,
            base::StrCat({exp_date}));
  EXPECT_THAT(card_number_field_suggestion, EqualLabels({{type_and_number}}));

  server_card.set_record_type(CreditCard::RecordType::kVirtualCard);
  card_number_field_suggestion =
      test_api(suggestion_generator())
          .CreateCreditCardSuggestion(server_card, CREDIT_CARD_NUMBER,
                                      /*virtual_card_option=*/false,
                                      /*card_linked_offer_available=*/false);

  // From a virtual credit card, the suggestion should show the card name and
  // the label should show the card's virtual status, type and number.
  EXPECT_EQ(card_number_field_suggestion.main_text.value,
            base::StrCat({server_card.CardNameForAutofillDisplay(
                server_card.nickname())}));
  EXPECT_THAT(
      card_number_field_suggestion,
      EqualLabels({{l10n_util::GetStringUTF16(
                        IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_OPTION_VALUE) +
                    u" • " + card_type + u" " + obfuscated_number}}));
}

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
      test_api(suggestion_generator())
          .CreateCreditCardSuggestion(server_card, CREDIT_CARD_NAME_FULL,
                                      /*virtual_card_option=*/false,
                                      /*card_linked_offer_available=*/false);

  EXPECT_THAT(card_name_field_suggestion,
              EqualLabels({{CreditCard::GetObfuscatedStringForCardDigits(
                  expected_obfuscation_length(), u"1111")}}));

  // Card number field suggestion.
  Suggestion card_number_field_suggestion =
      test_api(suggestion_generator())
          .CreateCreditCardSuggestion(server_card, CREDIT_CARD_NUMBER,
                                      /*virtual_card_option=*/false,
                                      /*card_linked_offer_available=*/false);

  EXPECT_EQ(
      card_number_field_suggestion.main_text.value,
      base::StrCat({u"Visa  ", CreditCard::GetObfuscatedStringForCardDigits(
                                   expected_obfuscation_length(), u"1111")}));
}

#endif  // BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_ANDROID)
// The 2 boolean params denote if kAutofillEnableVcnGrayOutForMerchantOptOut
// is turned on and if merchant accepts VCN.
class AutofillCreditCardSuggestionContentVcnMerchantOptOutTest
    : public AutofillCreditCardSuggestionContentTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  bool is_flag_enabled() { return std::get<0>(GetParam()); }

  bool is_merchant_opted_out() { return std::get<1>(GetParam()); }

  int expected_message_id() {
    return is_flag_enabled() && is_merchant_opted_out()
               ? IDS_AUTOFILL_VIRTUAL_CARD_DISABLED_SUGGESTION_OPTION_VALUE
               : IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_OPTION_VALUE;
  }

 private:
  void SetUp() override {
    AutofillCreditCardSuggestionContentTest::SetUp();
    scoped_feature_list_.InitWithFeatureState(
        features::kAutofillEnableVcnGrayOutForMerchantOptOut,
        is_flag_enabled());

    ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
                autofill_client()->GetAutofillOptimizationGuide()),
            ShouldBlockFormFieldSuggestion)
        .WillByDefault(testing::Return(is_merchant_opted_out()));
  }
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    AutofillCreditCardSuggestionContentVcnMerchantOptOutTest,
    testing::Combine(testing::Bool(), testing::Bool()));

// Verify that the suggestion's texts are populated correctly for a virtual
// card suggestion when the cardholder name field is focused and the merchant
// has opted-out of virtual cards.
TEST_P(
    AutofillCreditCardSuggestionContentVcnMerchantOptOutTest,
    CreateCreditCardSuggestion_VirtualCardMetadata_MerchantOptOut_NameField) {
  CreditCard server_card = test::GetVirtualCard();

  // Name field suggestion for virtual cards.
  Suggestion virtual_card_name_field_suggestion =
      test_api(suggestion_generator())
          .CreateCreditCardSuggestion(server_card, CREDIT_CARD_NAME_FULL,
                                      /*virtual_card_option=*/true,
                                      /*card_linked_offer_available=*/false);

  // `is_acceptable` is false only when the flag is enabled and merchant has
  // opted out of VCN.
  EXPECT_EQ(virtual_card_name_field_suggestion.is_acceptable,
            (!is_merchant_opted_out() || !is_flag_enabled()));
  // `apply_deactivated_style` is true only when flag is enabled and merchant
  // has opted out of VCN.
  EXPECT_EQ(virtual_card_name_field_suggestion.apply_deactivated_style,
            (is_merchant_opted_out() && is_flag_enabled()));
  // The virtual card text should be populated in the labels to be shown in
  // a new line.
  ASSERT_EQ(virtual_card_name_field_suggestion.labels[1].size(), 1U);
  EXPECT_EQ(virtual_card_name_field_suggestion.labels[1][0].value,
            l10n_util::GetStringUTF16(expected_message_id()));
}

// Verify that the suggestion's texts are populated correctly for a virtual
// card suggestion when the card number field is focused and merchant has
// opted-out of virtual cards.
TEST_P(
    AutofillCreditCardSuggestionContentVcnMerchantOptOutTest,
    CreateCreditCardSuggestion_VirtualCardMetadata_MerchantOptOut_NumberField) {
  CreditCard server_card = test::GetVirtualCard();

  // Card number field suggestion for virtual cards.
  Suggestion virtual_card_number_field_suggestion =
      test_api(suggestion_generator())
          .CreateCreditCardSuggestion(server_card, CREDIT_CARD_NUMBER,
                                      /*virtual_card_option=*/true,
                                      /*card_linked_offer_available=*/false);

  // `is_acceptable` is false only when flag is enabled and merchant has opted
  // out of VCN.
  EXPECT_EQ(virtual_card_number_field_suggestion.is_acceptable,
            (!is_merchant_opted_out() || !is_flag_enabled()));
  // `apply_deactivated_style` is true only when flag is enabled and merchant
  // has opted out of VCN.
  EXPECT_EQ(virtual_card_number_field_suggestion.apply_deactivated_style,
            (is_merchant_opted_out() && is_flag_enabled()));
  // For Desktop/Android dropdown, and on iOS, "Virtual card" is the label.
  EXPECT_THAT(
      virtual_card_number_field_suggestion,
      EqualLabels({{l10n_util::GetStringUTF16(expected_message_id())}}));
}
#endif  // BUILDFLAG(!IS_ANDROID)

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
  personal_data().AddCardArtImage(card_art_url, fake_image);

  Suggestion virtual_card_suggestion =
      test_api(suggestion_generator())
          .CreateCreditCardSuggestion(server_card, CREDIT_CARD_NUMBER,
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
      test_api(suggestion_generator())
          .CreateCreditCardSuggestion(server_card, CREDIT_CARD_NUMBER,
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
      test_api(suggestion_generator())
          .CreateCreditCardSuggestion(local_card, CREDIT_CARD_NUMBER,
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
  personal_data().AddServerCreditCard(server_card);
  personal_data().AddCardArtImage(card_art_url, fake_image);

  // Create a local card with same information.
  CreditCard local_card =
      CreateLocalCard(/*guid=*/"00000000-0000-0000-0000-000000000002");

  Suggestion virtual_card_suggestion =
      test_api(suggestion_generator())
          .CreateCreditCardSuggestion(local_card, CREDIT_CARD_NUMBER,
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
      test_api(suggestion_generator())
          .CreateCreditCardSuggestion(local_card, CREDIT_CARD_NUMBER,
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
    personal_data().AddServerCreditCard(server_card);

    bool with_offer;
    bool with_cvc;
    autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
    suggestion_generator().GetSuggestionsForCreditCards(
        FormFieldData(), CREDIT_CARD_NUMBER, kDefaultTriggerSource,
        /*should_show_scan_credit_card=*/false,
        /*should_show_cards_from_account=*/false, with_offer, with_cvc,
        metadata_logging_context);

    EXPECT_TRUE(
        metadata_logging_context.instruments_with_metadata_available.empty());
    EXPECT_FALSE(metadata_logging_context.card_product_description_shown);
    EXPECT_FALSE(metadata_logging_context.card_art_image_shown);

    // Verify that a record is added that a Capital One card suggestion
    // was generated, and it did not have metadata.
    base::flat_map<std::string, bool>
        expected_issuer_or_network_to_metadata_availability = {
            {server_card.issuer_id(), false}, {server_card.network(), false}};
    EXPECT_EQ(
        metadata_logging_context.issuer_or_network_to_metadata_availability,
        expected_issuer_or_network_to_metadata_availability);
  }

  personal_data().test_payments_data_manager().ClearCreditCards();

  {
    // Create a server card with card product description & card art image.
    CreditCard server_card_with_metadata = CreateServerCard();
    server_card_with_metadata.set_issuer_id(kCapitalOneCardIssuerId);
    server_card_with_metadata.set_product_description(u"product_description");
    server_card_with_metadata.set_card_art_url(
        GURL("https://www.example.com/card-art.png"));
    personal_data().AddServerCreditCard(server_card_with_metadata);

    bool with_offer;
    bool with_cvc;
    autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
    suggestion_generator().GetSuggestionsForCreditCards(
        FormFieldData(), CREDIT_CARD_NUMBER, kDefaultTriggerSource,
        /*should_show_scan_credit_card=*/false,
        /*should_show_cards_from_account=*/false, with_offer, with_cvc,
        metadata_logging_context);

    EXPECT_TRUE(
        metadata_logging_context.instruments_with_metadata_available.contains(
            server_card_with_metadata.instrument_id()));
    EXPECT_EQ(metadata_logging_context.card_product_description_shown,
              card_product_description_enabled());
    EXPECT_EQ(metadata_logging_context.card_art_image_shown,
              card_art_image_enabled());

    // Verify that a record is added that a Capital One card suggestion
    // was generated, and it had metadata.
    base::flat_map<std::string, bool> expected_issuer_to_metadata_availability =
        {{server_card_with_metadata.issuer_id(), true},
         {server_card_with_metadata.network(), true}};
    EXPECT_EQ(
        metadata_logging_context.issuer_or_network_to_metadata_availability,
        expected_issuer_to_metadata_availability);
  }
}

// TODO(crbug.com/332595462): Improve card art url unittest coverage to include
// potential edge cases.
//  Verifies that the custom icon is set correctly. The card art should be shown
//  when the metadata card art flag is enabled. Capital One virtual card icon is
//  an exception which should only and always be shown for virtual cards.
TEST_P(AutofillSuggestionGeneratorTestForMetadata,
       GetSuggestionsForCreditCards_CustomCardIcon) {
  // Create a server card.
  CreditCard server_card = CreateServerCard();
  GURL card_art_url =
      GURL(card_has_capital_one_icon() ? kCapitalOneCardArtUrl
                                       : "https://www.example.com/card-art");
  server_card.set_card_art_url(card_art_url);
  server_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  gfx::Image fake_image = CustomIconForTest();
  personal_data().AddServerCreditCard(server_card);
  personal_data().AddCardArtImage(card_art_url, fake_image);

  bool unused_with_offer;
  bool unused_with_cvc;
  autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
  std::vector<Suggestion> suggestions =
      suggestion_generator().GetSuggestionsForCreditCards(
          FormFieldData(), CREDIT_CARD_NUMBER, kDefaultTriggerSource,
          /*should_show_scan_credit_card=*/false,
          /*should_show_cards_from_account=*/false, unused_with_offer,
          unused_with_cvc, metadata_logging_context);

  // Suggestions in `suggestions` are persisted in order of their presentation
  // to the user in the Autofill dropdown and currently virtual cards are shown
  // before their associated FPAN suggestion.
  Suggestion virtual_card_suggestion = suggestions[0];
  Suggestion fpan_card_suggestion = suggestions[1];

  // Verify that for virtual cards, the custom icon is shown if the card art is
  // the Capital One virtual card art or if the metadata card art is enabled.
  EXPECT_EQ(VerifyCardArtImageExpectation(virtual_card_suggestion, card_art_url,
                                          fake_image),
            card_has_capital_one_icon() || card_art_image_enabled());

  // Verify that for FPAN, the custom icon is shown if the card art is not the
  // Capital One virtual card art and the metadata card art is enabled.
  EXPECT_EQ(VerifyCardArtImageExpectation(fpan_card_suggestion, card_art_url,
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
      test_api(suggestion_generator())
          .CreateCreditCardSuggestion(server_card1, CREDIT_CARD_NUMBER,
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
      test_api(suggestion_generator())
          .CreateCreditCardSuggestion(server_card1, CREDIT_CARD_NUMBER,
                                      /*virtual_card_option=*/false,
                                      /*card_linked_offer_available=*/true);

  EXPECT_EQ(real_card_suggestion.popup_item_id, PopupItemId::kCreditCardEntry);
  EXPECT_EQ(real_card_suggestion.GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(
                Suggestion::Guid("00000000-0000-0000-0000-000000000001")));

  if (keyboard_accessory_offer_enabled()) {
#if BUILDFLAG(IS_ANDROID)
    EXPECT_EQ(real_card_suggestion.labels.size(), 1U);
    EXPECT_EQ(real_card_suggestion.feature_for_iph,
              &feature_engagement::kIPHKeyboardAccessoryPaymentOfferFeature);
#endif
  } else {
    ASSERT_EQ(real_card_suggestion.labels.size(), 2U);
    ASSERT_EQ(real_card_suggestion.labels[1].size(), 1U);
    EXPECT_EQ(real_card_suggestion.labels[1][0].value,
              l10n_util::GetStringUTF16(IDS_AUTOFILL_OFFERS_CASHBACK));
  }
}

}  // namespace autofill
