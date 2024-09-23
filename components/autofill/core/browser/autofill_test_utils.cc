// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_test_utils.h"

#include <cstdint>
#include <iterator>
#include <string>

#include "base/functional/overloaded.h"
#include "base/memory/raw_ptr.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_test_api.h"
#include "components/autofill/core/browser/data_model/bank_account.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_test_api.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/randomized_encoder.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/browser/webdata/payments/payments_autofill_table.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/form_field_data_predictions.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/testing_pref_store.h"
#include "components/security_interstitials/core/pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

using base::ASCIIToUTF16;
using FieldPrediction = ::autofill::AutofillQueryResponse::FormSuggestion::
    FieldSuggestion::FieldPrediction;

namespace autofill {

bool operator==(const FormFieldDataPredictions& a,
                const FormFieldDataPredictions& b) = default;

bool operator==(const FormDataPredictions& a, const FormDataPredictions& b) {
  return test::WithoutUnserializedData(a.data).SameFormAs(
             test::WithoutUnserializedData(b.data)) &&
         a.signature == b.signature && a.fields == b.fields;
}

namespace test {

namespace {

std::string GetRandomCardNumber() {
  const size_t length = 16;
  std::string value;
  value.reserve(length);
  for (size_t i = 0; i < length; ++i)
    value.push_back(static_cast<char>(base::RandInt('0', '9')));
  return value;
}

base::Time GetArbitraryPastTime() {
  return AutofillClock::Now() - base::Days(5);
}

base::Time GetArbitraryFutureTime() {
  return AutofillClock::Now() + base::Days(10);
}

}  // namespace

void SetFormGroupValues(FormGroup& form_group,
                        const std::vector<FormGroupValue>& values) {
  for (const auto& value : values) {
    form_group.SetRawInfoWithVerificationStatus(
        value.type, base::UTF8ToUTF16(value.value), value.verification_status);
  }
}

void VerifyFormGroupValues(const FormGroup& form_group,
                           const std::vector<FormGroupValue>& values,
                           bool ignore_status) {
  for (const auto& value : values) {
    SCOPED_TRACE(testing::Message()
                 << "Expected for type " << FieldTypeToStringView(value.type)
                 << "\n\t" << value.value << " with status "
                 << (ignore_status ? "(ignored)" : "")
                 << value.verification_status << "\nFound:"
                 << "\n\t" << form_group.GetRawInfo(value.type)
                 << " with status "
                 << form_group.GetVerificationStatus(value.type));

    EXPECT_EQ(form_group.GetRawInfo(value.type),
              base::UTF8ToUTF16(value.value));
    if (!ignore_status) {
      EXPECT_EQ(form_group.GetVerificationStatus(value.type),
                value.verification_status);
    }
  }
}

std::unique_ptr<AutofillTestingPrefService> PrefServiceForTesting() {
  auto pref_service = std::make_unique<AutofillTestingPrefService>();
  user_prefs::PrefRegistrySyncable* registry = pref_service->registry();
  signin::IdentityManager::RegisterProfilePrefs(registry);
  registry->RegisterBooleanPref(
      RandomizedEncoder::kUrlKeyedAnonymizedDataCollectionEnabled, false);
  registry->RegisterBooleanPref(::prefs::kMixedFormsWarningsEnabled, true);
  registry->RegisterStringPref(prefs::kAutofillStatesDataDir, "");
  prefs::RegisterProfilePrefs(registry);
  return pref_service;
}

std::unique_ptr<PrefService> PrefServiceForTesting(
    user_prefs::PrefRegistrySyncable* registry) {
  prefs::RegisterProfilePrefs(registry);

  PrefServiceFactory factory;
  factory.set_user_prefs(base::MakeRefCounted<TestingPrefStore>());
  return factory.Create(registry);
}

[[nodiscard]] FormData CreateTestAddressFormData(const char* unique_id) {
  FormData form;
  form.set_host_frame(MakeLocalFrameToken());
  form.set_renderer_id(MakeFormRendererId());
  form.set_name(u"MyForm" + ASCIIToUTF16(unique_id ? unique_id : ""));
  form.set_button_titles({std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)});
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  form.set_is_action_empty(true);
  form.set_main_frame_origin(
      url::Origin::Create(GURL("https://myform_root.com/form.html")));
  form.set_submission_event(
      mojom::SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION);

  form.set_fields(
      {CreateTestFormField("First Name", "firstname", "",
                           FormControlType::kInputText),
       CreateTestFormField("Middle Name", "middlename", "",
                           FormControlType::kInputText),
       CreateTestFormField("Last Name", "lastname", "",
                           FormControlType::kInputText),
       CreateTestFormField("Address Line 1", "addr1", "",
                           FormControlType::kInputText),
       CreateTestFormField("Address Line 2", "addr2", "",
                           FormControlType::kInputText),
       CreateTestFormField("City", "city", "", FormControlType::kInputText),
       CreateTestFormField("State", "state", "", FormControlType::kInputText),
       CreateTestFormField("Postal Code", "zipcode", "",
                           FormControlType::kInputText),
       CreateTestFormField("Country", "country", "",
                           FormControlType::kInputText),
       CreateTestFormField("Phone Number", "phonenumber", "",
                           FormControlType::kInputTelephone),
       CreateTestFormField("Email", "email", "",
                           FormControlType::kInputEmail)});
  return form;
}

inline void check_and_set(
    FormGroup* profile,
    FieldType type,
    const char* value,
    VerificationStatus status = VerificationStatus::kObserved) {
  if (value) {
    profile->SetRawInfoWithVerificationStatus(type, base::UTF8ToUTF16(value),
                                              status);
  }
}

AutofillProfile GetFullValidProfileForCanada() {
  AutofillProfile profile(AddressCountryCode("CA"));
  SetProfileInfo(&profile, "Alice", "", "Wonderland", "alice@wonderland.ca",
                 "Fiction", "666 Notre-Dame Ouest", "Apt 8", "Montreal", "QC",
                 "H3B 2T9", "CA", "15141112233");
  return profile;
}

AutofillProfile GetFullProfile(AddressCountryCode country_code) {
  AutofillProfile profile(country_code);
  SetProfileInfo(&profile, "John", "H.", "Doe", "johndoe@hades.com",
                 "Underworld", "666 Erebus St.", "Apt 8", "Elysium", "CA",
                 "91111", country_code->c_str(), "16502111111");
  return profile;
}

AutofillProfile GetFullProfile2(AddressCountryCode country_code) {
  AutofillProfile profile(country_code);
  SetProfileInfo(&profile, "Jane", "A.", "Smith", "jsmith@example.com", "ACME",
                 "123 Main Street", "Unit 1", "Greensdale", "MI", "48838",
                 country_code->c_str(), "13105557889");
  return profile;
}

AutofillProfile GetFullCanadianProfile() {
  AutofillProfile profile(AddressCountryCode("CA"));
  SetProfileInfo(&profile, "Wayne", "", "Gretzky", "wayne@hockey.com", "NHL",
                 "123 Hockey rd.", "Apt 8", "Moncton", "New Brunswick",
                 "E1A 0A6", "CA", "15068531212");
  return profile;
}

AutofillProfile GetIncompleteProfile1() {
  AutofillProfile profile(AddressCountryCode("US"));
  SetProfileInfo(&profile, "John", "H.", "Doe", "jsmith@example.com", "ACME",
                 "123 Main Street", "Unit 1", "Greensdale", "MI", "48838", "US",
                 "");
  return profile;
}

AutofillProfile GetIncompleteProfile2() {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  SetProfileInfo(&profile, "", "", "", "jsmith@example.com", "", "", "", "", "",
                 "", "", "");
  return profile;
}

void SetProfileCategory(
    AutofillProfile& profile,
    autofill_metrics::AutofillProfileRecordTypeCategory category) {
  switch (category) {
    case autofill_metrics::AutofillProfileRecordTypeCategory::kLocalOrSyncable:
      test_api(profile).set_record_type(
          AutofillProfile::RecordType::kLocalOrSyncable);
      break;
    case autofill_metrics::AutofillProfileRecordTypeCategory::kAccountChrome:
    case autofill_metrics::AutofillProfileRecordTypeCategory::kAccountNonChrome:
      test_api(profile).set_record_type(AutofillProfile::RecordType::kAccount);
      // Any value that is not kInitialCreatorOrModifierChrome works.
      const int kInitialCreatorOrModifierNonChrome =
          AutofillProfile::kInitialCreatorOrModifierChrome + 1;
      profile.set_initial_creator_id(
          category == autofill_metrics::AutofillProfileRecordTypeCategory::
                          kAccountChrome
              ? AutofillProfile::kInitialCreatorOrModifierChrome
              : kInitialCreatorOrModifierNonChrome);
      break;
  }
}

std::string GetStrippedValue(const char* value) {
  std::u16string stripped_value;
  base::RemoveChars(base::UTF8ToUTF16(value), base::kWhitespaceUTF16,
                    &stripped_value);
  return base::UTF16ToUTF8(stripped_value);
}

Iban GetLocalIban() {
  Iban iban(Iban::Guid(base::Uuid::GenerateRandomV4().AsLowercaseString()));
  iban.set_value(base::UTF8ToUTF16(std::string(kIbanValue)));
  iban.set_nickname(u"Nickname for Iban");
  return iban;
}

Iban GetLocalIban2() {
  Iban iban(Iban::Guid(base::Uuid::GenerateRandomV4().AsLowercaseString()));
  iban.set_value(base::UTF8ToUTF16(std::string(kIbanValue_1)));
  iban.set_nickname(u"My doctor's IBAN");
  return iban;
}

Iban GetServerIban() {
  Iban iban(Iban::InstrumentId(1234567));
  iban.set_prefix(u"FR76");
  iban.set_suffix(u"0189");
  iban.set_nickname(u"My doctor's IBAN");
  return iban;
}

Iban GetServerIban2() {
  Iban iban(Iban::InstrumentId(1234568));
  iban.set_prefix(u"BE71");
  iban.set_suffix(u"8676");
  iban.set_nickname(u"My sister's IBAN");
  return iban;
}

Iban GetServerIban3() {
  Iban iban(Iban::InstrumentId(1234569));
  iban.set_prefix(u"DE91");
  iban.set_suffix(u"6789");
  iban.set_nickname(u"My IBAN");
  return iban;
}

CreditCard GetCreditCard() {
  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         kEmptyOrigin);
  SetCreditCardInfo(&credit_card, "Test User", "4111111111111111" /* Visa */,
                    NextMonth().c_str(), NextYear().c_str(), "1");
  return credit_card;
}

CreditCard GetCreditCard2() {
  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         kEmptyOrigin);
  SetCreditCardInfo(&credit_card, "Someone Else", "378282246310005" /* AmEx */,
                    NextMonth().c_str(), TenYearsFromNow().c_str(), "1");
  return credit_card;
}

CreditCard GetExpiredCreditCard() {
  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         kEmptyOrigin);
  SetCreditCardInfo(&credit_card, "Test User", "4111111111111111" /* Visa */,
                    NextMonth().c_str(), LastYear().c_str(), "1");
  return credit_card;
}

CreditCard GetIncompleteCreditCard() {
  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         kEmptyOrigin);
  SetCreditCardInfo(&credit_card, "", "4111111111111111" /* Visa */,
                    NextMonth().c_str(), NextYear().c_str(), "1");
  return credit_card;
}

CreditCard GetVerifiedCreditCard() {
  CreditCard credit_card(GetCreditCard());
  credit_card.set_origin(kSettingsOrigin);
  return credit_card;
}

CreditCard GetVerifiedCreditCard2() {
  CreditCard credit_card(GetCreditCard2());
  credit_card.set_origin(kSettingsOrigin);
  return credit_card;
}

CreditCard GetMaskedServerCard() {
  CreditCard credit_card(CreditCard::RecordType::kMaskedServerCard, "a123");
  test::SetCreditCardInfo(&credit_card, "Bonnie Parker",
                          "2109" /* Mastercard */, NextMonth().c_str(),
                          NextYear().c_str(), "1");
  credit_card.SetNetworkForMaskedCard(kMasterCard);
  credit_card.set_instrument_id(1);
  return credit_card;
}

CreditCard GetMaskedServerCard2() {
  CreditCard credit_card(CreditCard::RecordType::kMaskedServerCard, "b456");
  test::SetCreditCardInfo(&credit_card, "Rick Roman", "2109" /* Mastercard */,
                          NextMonth().c_str(), NextYear().c_str(), "");
  credit_card.SetNetworkForMaskedCard(kMasterCard);
  credit_card.set_instrument_id(2);
  return credit_card;
}

CreditCard GetMaskedServerCardWithLegacyId() {
  CreditCard credit_card(CreditCard::RecordType::kMaskedServerCard, "a123");
  test::SetCreditCardInfo(&credit_card, "Bonnie Parker",
                          "2109" /* Mastercard */, NextMonth().c_str(),
                          NextYear().c_str(), "1");
  credit_card.SetNetworkForMaskedCard(kMasterCard);
  return credit_card;
}

CreditCard GetMaskedServerCardWithNonLegacyId() {
  CreditCard credit_card(CreditCard::RecordType::kMaskedServerCard, 1);
  test::SetCreditCardInfo(&credit_card, "Bonnie Parker",
                          "2109" /* Mastercard */, NextMonth().c_str(),
                          NextYear().c_str(), "1");
  credit_card.SetNetworkForMaskedCard(kMasterCard);
  return credit_card;
}

CreditCard GetMaskedServerCardVisa() {
  CreditCard credit_card(CreditCard::RecordType::kMaskedServerCard, "a123");
  test::SetCreditCardInfo(&credit_card, "Bonnie Parker", "1111" /* Visa */,
                          NextMonth().c_str(), NextYear().c_str(), "1");
  credit_card.SetNetworkForMaskedCard(kVisaCard);
  return credit_card;
}

CreditCard GetMaskedServerCardAmex() {
  CreditCard credit_card(CreditCard::RecordType::kMaskedServerCard, "b456");
  test::SetCreditCardInfo(&credit_card, "Justin Thyme", "8431" /* Amex */,
                          NextMonth().c_str(), NextYear().c_str(), "1");
  credit_card.SetNetworkForMaskedCard(kAmericanExpressCard);
  return credit_card;
}

CreditCard GetMaskedServerCardWithNickname() {
  CreditCard credit_card(CreditCard::RecordType::kMaskedServerCard, "c789");
  test::SetCreditCardInfo(&credit_card, "Test user", "1111" /* Visa */,
                          NextMonth().c_str(), NextYear().c_str(), "1");
  credit_card.SetNetworkForMaskedCard(kVisaCard);
  credit_card.SetNickname(u"Test nickname");
  return credit_card;
}

CreditCard GetMaskedServerCardEnrolledIntoVirtualCardNumber() {
  CreditCard credit_card = GetMaskedServerCard();
  credit_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  return credit_card;
}

CreditCard GetFullServerCard() {
  CreditCard credit_card(CreditCard::RecordType::kFullServerCard, "c123");
  test::SetCreditCardInfo(&credit_card, "Full Carter",
                          "4111111111111111" /* Visa */, NextMonth().c_str(),
                          NextYear().c_str(), "1");
  return credit_card;
}

CreditCard GetVirtualCard() {
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Lorem Ipsum",
                          "5555555555554444",  // Mastercard
                          "10", test::NextYear().c_str(), "1");
  credit_card.set_record_type(CreditCard::RecordType::kVirtualCard);
  credit_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  test_api(credit_card).set_network_for_card(kMasterCard);
  return credit_card;
}

CreditCard GetRandomCreditCard(CreditCard::RecordType record_type) {
  constexpr static std::array<std::string_view, 10> kNetworks = {
      kAmericanExpressCard,
      kDinersCard,
      kDiscoverCard,
      kEloCard,
      kGenericCard,
      kJCBCard,
      kMasterCard,
      kMirCard,
      kUnionPay,
      kVisaCard,
  };
  base::Time::Exploded now;
  AutofillClock::Now().LocalExplode(&now);

  CreditCard credit_card =
      (record_type == CreditCard::RecordType::kLocalCard)
          ? CreditCard(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                       kEmptyOrigin)
          : CreditCard(
                record_type,
                base::Uuid::GenerateRandomV4().AsLowercaseString().substr(24));
  test::SetCreditCardInfo(
      &credit_card, "Justin Thyme", GetRandomCardNumber().c_str(),
      base::StringPrintf("%d", base::RandInt(1, 12)).c_str(),
      base::StringPrintf("%d", now.year + base::RandInt(1, 4)).c_str(), "1");
  if (record_type == CreditCard::RecordType::kMaskedServerCard) {
    credit_card.SetNetworkForMaskedCard(
        kNetworks[base::RandInt(0, kNetworks.size() - 1)]);
  }

  return credit_card;
}

CreditCard WithCvc(CreditCard credit_card, std::u16string cvc) {
  credit_card.set_cvc(cvc);
  return credit_card;
}

CreditCardCloudTokenData GetCreditCardCloudTokenData1() {
  CreditCardCloudTokenData data;
  data.masked_card_id = "data1_id";
  data.suffix = u"1111";
  data.exp_month = 1;
  base::StringToInt(NextYear(), &data.exp_year);
  data.card_art_url = "fake url 1";
  data.instrument_token = "fake token 1";
  return data;
}

CreditCardCloudTokenData GetCreditCardCloudTokenData2() {
  CreditCardCloudTokenData data;
  data.masked_card_id = "data2_id";
  data.suffix = u"2222";
  data.exp_month = 2;
  base::StringToInt(NextYear(), &data.exp_year);
  data.exp_year += 1;
  data.card_art_url = "fake url 2";
  data.instrument_token = "fake token 2";
  return data;
}

AutofillOfferData GetCardLinkedOfferData1(int64_t offer_id) {
  // Sets the expiry to be 45 days later.
  base::Time expiry = AutofillClock::Now() + base::Days(45);
  GURL offer_details_url = GURL("http://www.example1.com");
  std::vector<GURL> merchant_origins{offer_details_url};
  DisplayStrings display_strings;
  display_strings.value_prop_text = "Get 5% off your purchase";
  display_strings.see_details_text = "See details";
  display_strings.usage_instructions_text =
      "Check out with this card to activate";
  std::string offer_reward_amount = "5%";
  std::vector<int64_t> eligible_instrument_id{111111};

  return AutofillOfferData::GPayCardLinkedOffer(
      offer_id, expiry, merchant_origins, offer_details_url, display_strings,
      eligible_instrument_id, offer_reward_amount);
}

AutofillOfferData GetCardLinkedOfferData2(int64_t offer_id) {
  // Sets the expiry to be 40 days later.
  base::Time expiry = AutofillClock::Now() + base::Days(40);
  GURL offer_details_url = GURL("http://www.example2.com");
  std::vector<GURL> merchant_origins{offer_details_url};
  DisplayStrings display_strings;
  display_strings.value_prop_text = "Get $10 off your purchase";
  display_strings.see_details_text = "See details";
  display_strings.usage_instructions_text =
      "Check out with this card to activate";
  std::string offer_reward_amount = "$10";
  std::vector<int64_t> eligible_instrument_id{222222};

  return AutofillOfferData::GPayCardLinkedOffer(
      offer_id, expiry, merchant_origins, offer_details_url, display_strings,
      eligible_instrument_id, offer_reward_amount);
}

AutofillOfferData GetPromoCodeOfferData(GURL origin,
                                        bool is_expired,
                                        int64_t offer_id) {
  // Sets the expiry to be later if not expired, or earlier if expired.
  base::Time expiry = is_expired ? AutofillClock::Now() - base::Days(1)
                                 : AutofillClock::Now() + base::Days(35);
  std::vector<GURL> merchant_origins{origin};
  DisplayStrings display_strings;
  display_strings.value_prop_text = "5% off on shoes. Up to $50.";
  display_strings.see_details_text = "See details";
  display_strings.usage_instructions_text =
      "Click the promo code field at checkout to autofill it.";
  std::string promo_code = "5PCTOFFSHOES";
  GURL offer_details_url = GURL("https://pay.google.com");

  return AutofillOfferData::GPayPromoCodeOffer(
      offer_id, expiry, merchant_origins, offer_details_url, display_strings,
      promo_code);
}

VirtualCardUsageData GetVirtualCardUsageData1() {
  return VirtualCardUsageData(
      VirtualCardUsageData::UsageDataId(
          "VirtualCardUsageData|12345|google|https://www.google.com"),
      VirtualCardUsageData::InstrumentId(12345),
      VirtualCardUsageData::VirtualCardLastFour(u"1234"),
      url::Origin::Create(GURL("https://www.google.com")));
}

VirtualCardUsageData GetVirtualCardUsageData2() {
  return VirtualCardUsageData(
      VirtualCardUsageData::UsageDataId(
          "VirtualCardUsageData|23456|google|https://www.pay.google.com"),
      VirtualCardUsageData::InstrumentId(23456),
      VirtualCardUsageData::VirtualCardLastFour(u"2345"),
      url::Origin::Create(GURL("https://www.pay.google.com")));
}

std::vector<CardUnmaskChallengeOption> GetCardUnmaskChallengeOptions(
    const std::vector<CardUnmaskChallengeOptionType>& types) {
  std::vector<CardUnmaskChallengeOption> challenge_options;
  for (CardUnmaskChallengeOptionType type : types) {
    switch (type) {
      case CardUnmaskChallengeOptionType::kSmsOtp:
        challenge_options.emplace_back(CardUnmaskChallengeOption(
            CardUnmaskChallengeOption::ChallengeOptionId("123"), type,
            /*challenge_info=*/u"xxx-xxx-3547",
            /*challenge_input_length=*/6U));
        break;
      case CardUnmaskChallengeOptionType::kCvc:
        challenge_options.emplace_back(CardUnmaskChallengeOption(
            CardUnmaskChallengeOption::ChallengeOptionId("234"), type,
            /*challenge_info=*/
            u"3 digit security code on the back of your card",
            /*challenge_input_length=*/3U,
            /*cvc_position=*/CvcPosition::kBackOfCard));
        break;
      case CardUnmaskChallengeOptionType::kEmailOtp:
        challenge_options.emplace_back(
            CardUnmaskChallengeOption::ChallengeOptionId("345"), type,
            /*challenge_info=*/u"a******b@google.com",
            /*challenge_input_length=*/6U);
        break;
      case CardUnmaskChallengeOptionType::kThreeDomainSecure: {
        CardUnmaskChallengeOption challenge_option;
        challenge_option.id =
            CardUnmaskChallengeOption::ChallengeOptionId("456");
        challenge_option.type = type;
        Vcn3dsChallengeOptionMetadata metadata;
        metadata.url_to_open = GURL("https://www.example.com");
        metadata.success_query_param_name = "token";
        metadata.failure_query_param_name = "failure";
        challenge_option.vcn_3ds_metadata = std::move(metadata);
        challenge_options.emplace_back(std::move(challenge_option));
        break;
      }
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }
  return challenge_options;
}

CreditCardFlatRateBenefit GetActiveCreditCardFlatRateBenefit() {
  return CreditCardFlatRateBenefit(
      CreditCardBenefitBase::BenefitId("id1"),
      CreditCardBenefitBase::LinkedCardInstrumentId(1234),
      /*benefit_description=*/u"Get 2% cashback on any purchase",
      /*start_time=*/GetArbitraryPastTime(),
      /*expiry_time=*/GetArbitraryFutureTime());
}

CreditCardCategoryBenefit GetActiveCreditCardCategoryBenefit() {
  return CreditCardCategoryBenefit(
      CreditCardBenefitBase::BenefitId("id2"),
      CreditCardBenefitBase::LinkedCardInstrumentId(2234),
      CreditCardCategoryBenefit::BenefitCategory::kSubscription,
      /*benefit_description=*/u"Get 2x points on purchases on this website",
      /*start_time=*/GetArbitraryPastTime(),
      /*expiry_time=*/GetArbitraryFutureTime());
}

CreditCardMerchantBenefit GetActiveCreditCardMerchantBenefit() {
  return CreditCardMerchantBenefit(
      CreditCardBenefitBase::BenefitId("id3"),
      CreditCardBenefitBase::LinkedCardInstrumentId(3234),
      /*benefit_description=*/u"Get 2x points on purchases on this website",
      GetOriginsForMerchantBenefit(),
      /*start_time=*/GetArbitraryPastTime(),
      /*expiry_time=*/GetArbitraryFutureTime());
}

base::flat_set<url::Origin> GetOriginsForMerchantBenefit() {
  return {url::Origin::Create(GURL("http://www.example.com")),
          url::Origin::Create(GURL("http://www.example3.com"))};
}

void SetUpCreditCardAndBenefitData(
    CreditCard& card,
    const CreditCardBenefit& benefit,
    const std::string& issuer_id,
    TestPersonalDataManager& personal_data,
    AutofillOptimizationGuide* optimization_guide) {
  absl::visit(
      base::Overloaded{
          [&card](const CreditCardFlatRateBenefit& flat_rate_benefit) {
            card.set_instrument_id(
                *flat_rate_benefit.linked_card_instrument_id());
          },
          [&card](const CreditCardMerchantBenefit& merchant_benefit) {
            card.set_instrument_id(
                *merchant_benefit.linked_card_instrument_id());
          },
          [&card, &optimization_guide](
              const CreditCardCategoryBenefit& category_benefit) {
            card.set_instrument_id(
                *category_benefit.linked_card_instrument_id());
            ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
                        optimization_guide),
                    AttemptToGetEligibleCreditCardBenefitCategory)
                .WillByDefault(testing::Return(
                    CreditCardCategoryBenefit::BenefitCategory::kSubscription));
          }},
      benefit);
  personal_data.payments_data_manager().AddCreditCardBenefitForTest(benefit);
  card.set_issuer_id(issuer_id);
  personal_data.test_payments_data_manager().AddServerCreditCard(card);
}

void SetProfileInfo(AutofillProfile* profile,
                    const char* first_name,
                    const char* middle_name,
                    const char* last_name,
                    const char* email,
                    const char* company,
                    const char* address1,
                    const char* address2,
                    const char* dependent_locality,
                    const char* city,
                    const char* state,
                    const char* zipcode,
                    const char* country,
                    const char* phone,
                    bool finalize,
                    VerificationStatus status) {
  check_and_set(profile, NAME_FIRST, first_name, status);
  check_and_set(profile, NAME_MIDDLE, middle_name, status);
  check_and_set(profile, NAME_LAST, last_name, status);
  check_and_set(profile, EMAIL_ADDRESS, email, status);
  check_and_set(profile, COMPANY_NAME, company, status);
  check_and_set(profile, ADDRESS_HOME_LINE1, address1, status);
  check_and_set(profile, ADDRESS_HOME_LINE2, address2, status);
  check_and_set(profile, ADDRESS_HOME_DEPENDENT_LOCALITY, dependent_locality,
                status);
  check_and_set(profile, ADDRESS_HOME_CITY, city, status);
  check_and_set(profile, ADDRESS_HOME_STATE, state, status);
  check_and_set(profile, ADDRESS_HOME_ZIP, zipcode, status);
  check_and_set(profile, ADDRESS_HOME_COUNTRY, country, status);
  check_and_set(profile, PHONE_HOME_WHOLE_NUMBER, phone, status);
  if (finalize)
    profile->FinalizeAfterImport();
}

void SetProfileInfo(AutofillProfile* profile,
                    const char* first_name,
                    const char* middle_name,
                    const char* last_name,
                    const char* email,
                    const char* company,
                    const char* address1,
                    const char* address2,
                    const char* city,
                    const char* state,
                    const char* zipcode,
                    const char* country,
                    const char* phone,
                    bool finalize,
                    VerificationStatus status) {
  // Set the country first to ensure that the proper address model is used.
  check_and_set(profile, ADDRESS_HOME_COUNTRY, country, status);

  check_and_set(profile, NAME_FIRST, first_name, status);
  check_and_set(profile, NAME_MIDDLE, middle_name, status);
  check_and_set(profile, NAME_LAST, last_name, status);
  check_and_set(profile, EMAIL_ADDRESS, email, status);
  check_and_set(profile, COMPANY_NAME, company, status);
  check_and_set(profile, ADDRESS_HOME_LINE1, address1, status);
  check_and_set(profile, ADDRESS_HOME_LINE2, address2, status);
  check_and_set(profile, ADDRESS_HOME_CITY, city, status);
  check_and_set(profile, ADDRESS_HOME_STATE, state, status);
  check_and_set(profile, ADDRESS_HOME_ZIP, zipcode, status);
  check_and_set(profile, PHONE_HOME_WHOLE_NUMBER, phone, status);
  if (finalize)
    profile->FinalizeAfterImport();
}

void SetProfileInfoWithGuid(AutofillProfile* profile,
                            const char* guid,
                            const char* first_name,
                            const char* middle_name,
                            const char* last_name,
                            const char* email,
                            const char* company,
                            const char* address1,
                            const char* address2,
                            const char* city,
                            const char* state,
                            const char* zipcode,
                            const char* country,
                            const char* phone,
                            bool finalize,
                            VerificationStatus status) {
  if (guid)
    profile->set_guid(guid);
  SetProfileInfo(profile, first_name, middle_name, last_name, email, company,
                 address1, address2, city, state, zipcode, country, phone,
                 finalize, status);
}

void SetCreditCardInfo(CreditCard* credit_card,
                       const char* name_on_card,
                       const char* card_number,
                       const char* expiration_month,
                       const char* expiration_year,
                       const std::string& billing_address_id,
                       const std::u16string& cvc) {
  check_and_set(credit_card, CREDIT_CARD_NAME_FULL, name_on_card);
  check_and_set(credit_card, CREDIT_CARD_NUMBER, card_number);
  check_and_set(credit_card, CREDIT_CARD_EXP_MONTH, expiration_month);
  check_and_set(credit_card, CREDIT_CARD_EXP_4_DIGIT_YEAR, expiration_year);
  credit_card->set_cvc(cvc);
  credit_card->set_billing_address_id(billing_address_id);
}

CreditCard CreateCreditCardWithInfo(const char* name_on_card,
                                    const char* card_number,
                                    const char* expiration_month,
                                    const char* expiration_year,
                                    const std::string& billing_address_id,
                                    const std::u16string& cvc) {
  CreditCard credit_card;
  SetCreditCardInfo(&credit_card, name_on_card, card_number, expiration_month,
                    expiration_year, billing_address_id, cvc);
  return credit_card;
}

void SetServerCreditCards(PaymentsAutofillTable* table,
                          const std::vector<CreditCard>& cards) {
  for (const CreditCard& card : cards) {
    ASSERT_EQ(card.record_type(), CreditCard::RecordType::kMaskedServerCard);
    table->AddServerCvc({card.instrument_id(), card.cvc(),
                         /*last_updated_timestamp=*/AutofillClock::Now()});
  }
  table->SetServerCreditCards(cards);
}

void InitializePossibleTypes(std::vector<FieldTypeSet>& possible_field_types,
                             const std::vector<FieldType>& possible_types) {
  possible_field_types.emplace_back();
  for (const auto& possible_type : possible_types) {
    possible_field_types.back().insert(possible_type);
  }
}

void FillUploadField(AutofillUploadContents::Field* field,
                     unsigned signature,
                     unsigned autofill_type) {
  field->set_signature(signature);
  field->add_autofill_type(autofill_type);
}

void FillUploadField(AutofillUploadContents::Field* field,
                     unsigned signature,
                     const std::vector<unsigned>& autofill_types) {
  field->set_signature(signature);

  for (unsigned i = 0; i < autofill_types.size(); ++i) {
    field->add_autofill_type(autofill_types[i]);
  }
}

void GenerateTestAutofillPopup(
    AutofillExternalDelegate* autofill_external_delegate) {
  FormData form;
  FormFieldData field;
  form.set_host_frame(MakeLocalFrameToken());
  form.set_renderer_id(MakeFormRendererId());
  field.set_host_frame(MakeLocalFrameToken());
  field.set_renderer_id(MakeFieldRendererId());
  field.set_is_focusable(true);
  field.set_should_autocomplete(true);
  field.set_bounds(gfx::RectF(100.f, 100.f));
  autofill_external_delegate->OnQuery(
      form, field, /*caret_bounds=*/gfx::Rect(),
      AutofillSuggestionTriggerSource::kFormControlElementClicked);

  std::vector<Suggestion> suggestions;
  suggestions.push_back(Suggestion(u"Test suggestion"));
  autofill_metrics::SuggestionRankingContext context;
  autofill_external_delegate->OnSuggestionsReturned(
      field.global_id(), suggestions, std::move(context));
}

std::string ObfuscatedCardDigitsAsUTF8(const std::string& str,
                                       int obfuscation_length) {
  return base::UTF16ToUTF8(CreditCard::GetObfuscatedStringForCardDigits(
      obfuscation_length, base::ASCIIToUTF16(str)));
}

std::string NextMonth() {
  base::Time::Exploded now;
  AutofillClock::Now().LocalExplode(&now);
  return base::StringPrintf("%02d", now.month % 12 + 1);
}
std::string LastYear() {
  base::Time::Exploded now;
  AutofillClock::Now().LocalExplode(&now);
  return base::NumberToString(now.year - 1);
}
std::string NextYear() {
  base::Time::Exploded now;
  AutofillClock::Now().LocalExplode(&now);
  return base::NumberToString(now.year + 1);
}
std::string TenYearsFromNow() {
  base::Time::Exploded now;
  AutofillClock::Now().LocalExplode(&now);
  return base::NumberToString(now.year + 10);
}

std::vector<FormSignature> GetEncodedSignatures(const FormStructure& form) {
  std::vector<FormSignature> signatures;
  signatures.push_back(form.form_signature());
  return signatures;
}

std::vector<FormSignature> GetEncodedSignatures(
    const std::vector<raw_ptr<FormStructure, VectorExperimental>>& forms) {
  std::vector<FormSignature> all_signatures;
  for (const FormStructure* form : forms)
    all_signatures.push_back(form->form_signature());
  return all_signatures;
}

std::vector<FormSignature> GetEncodedAlternativeSignatures(
    const FormStructure& form) {
  return std::vector<FormSignature>{form.alternative_form_signature()};
}

std::vector<FormSignature> GetEncodedAlternativeSignatures(
    const std::vector<raw_ptr<FormStructure, VectorExperimental>>& forms) {
  std::vector<FormSignature> all_signatures;
  for (const FormStructure* form : forms) {
    all_signatures.push_back(form->alternative_form_signature());
  }
  return all_signatures;
}

FieldPrediction CreateFieldPrediction(FieldType type,
                                      FieldPrediction::Source source) {
  FieldPrediction field_prediction;
  field_prediction.set_type(type);
  field_prediction.set_source(source);
  if (source == FieldPrediction::SOURCE_OVERRIDE ||
      source == FieldPrediction::SOURCE_MANUAL_OVERRIDE) {
    field_prediction.set_override(true);
  }
  return field_prediction;
}

FieldPrediction CreateFieldPrediction(FieldType type, bool is_override) {
  if (is_override) {
    return CreateFieldPrediction(type, FieldPrediction::SOURCE_OVERRIDE);
  }
  if (type == NO_SERVER_DATA) {
    return CreateFieldPrediction(type, FieldPrediction::SOURCE_UNSPECIFIED);
  }
  return CreateFieldPrediction(
      type, GroupTypeOfFieldType(type) == FieldTypeGroup::kPasswordField
                ? FieldPrediction::SOURCE_PASSWORDS_DEFAULT
                : FieldPrediction::SOURCE_AUTOFILL_DEFAULT);
}

void AddFieldPredictionToForm(
    const FormFieldData& field_data,
    FieldType field_type,
    AutofillQueryResponse_FormSuggestion* form_suggestion,
    bool is_override) {
  auto* field_suggestion = form_suggestion->add_field_suggestions();
  field_suggestion->set_field_signature(
      CalculateFieldSignatureForField(field_data).value());
  *field_suggestion->add_predictions() =
      CreateFieldPrediction(field_type, is_override);
}

void AddFieldPredictionsToForm(
    const FormFieldData& field_data,
    const std::vector<FieldType>& field_types,
    AutofillQueryResponse_FormSuggestion* form_suggestion) {
  std::vector<FieldPrediction> field_predictions;
  field_predictions.reserve(field_types.size());
  base::ranges::transform(
      field_types, std::back_inserter(field_predictions),
      [](FieldType field_type) { return CreateFieldPrediction(field_type); });
  return AddFieldPredictionsToForm(field_data, field_predictions,
                                   form_suggestion);
}

void AddFieldPredictionsToForm(
    const FormFieldData& field_data,
    const std::vector<FieldPrediction>& field_predictions,
    AutofillQueryResponse_FormSuggestion* form_suggestion) {
  auto* field_suggestion = form_suggestion->add_field_suggestions();
  field_suggestion->set_field_signature(
      CalculateFieldSignatureForField(field_data).value());
  for (const auto& prediction : field_predictions) {
    *field_suggestion->add_predictions() = prediction;
  }
}

Suggestion CreateAutofillSuggestion(SuggestionType type,
                                    const std::u16string& main_text_value,
                                    const Suggestion::Payload& payload) {
  Suggestion suggestion;
  suggestion.type = type;
  suggestion.main_text.value = main_text_value;
  suggestion.payload = payload;
  return suggestion;
}

Suggestion CreateAutofillSuggestion(const std::u16string& main_text_value,
                                    const std::u16string& minor_text_value,
                                    bool apply_deactivated_style) {
  Suggestion suggestion;
  suggestion.main_text.value = main_text_value;
  suggestion.minor_text.value = minor_text_value;
  suggestion.apply_deactivated_style = apply_deactivated_style;
  return suggestion;
}

BankAccount CreatePixBankAccount(int64_t instrument_id) {
  BankAccount bank_account(
      instrument_id, u"nickname", GURL("http://www.example.com"), u"bank_name",
      u"account_number", BankAccount::AccountType::kChecking);
  return bank_account;
}

sync_pb::PaymentInstrument CreatePaymentInstrumentWithBankAccount(
    int64_t instrument_id) {
  sync_pb::PaymentInstrument payment_instrument;
  payment_instrument.set_instrument_id(instrument_id);
  sync_pb::BankAccountDetails* bank_account =
      payment_instrument.mutable_bank_account();
  bank_account->set_bank_name("bank_name");
  bank_account->set_account_number_suffix("1234");
  bank_account->set_account_type(
      sync_pb::BankAccountDetails_AccountType_CHECKING);
  return payment_instrument;
}

sync_pb::PaymentInstrument CreatePaymentInstrumentWithIban(
    int64_t instrument_id) {
  sync_pb::PaymentInstrument payment_instrument;
  payment_instrument.set_instrument_id(instrument_id);
  sync_pb::WalletMaskedIban* iban = payment_instrument.mutable_iban();
  iban->set_instrument_id("instrument_id");
  iban->set_prefix("FR76");
  iban->set_suffix("0189");
  iban->set_length(27);
  iban->set_nickname("nickname");
  return payment_instrument;
}

sync_pb::PaymentInstrument CreatePaymentInstrumentWithEwalletAccount(
    int64_t instrument_id) {
  sync_pb::PaymentInstrument payment_instrument;
  payment_instrument.set_instrument_id(instrument_id);
  sync_pb::DeviceDetails* device_details =
      payment_instrument.mutable_device_details();
  device_details->set_is_fido_enrolled(true);
  sync_pb::EwalletDetails* ewallet =
      payment_instrument.mutable_ewallet_details();
  ewallet->set_ewallet_name("ewallet_name");
  ewallet->set_account_display_name("account_display_name");
  ewallet->add_supported_payment_link_uris("supported_payment_link_uri_1");
  return payment_instrument;
}

}  // namespace test
}  // namespace autofill
