// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/hash/hash.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/crowdsourcing/mock_autofill_crowdsourcing_manager.h"
#include "components/autofill/core/browser/crowdsourcing/test_votes_uploader.h"
#include "components/autofill/core/browser/crowdsourcing/votes_uploader_test_api.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager_test_api.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager_test_api.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager_test_api.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_test_api.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/data_quality/addresses/profile_token_quality.h"
#include "components/autofill/core/browser/data_quality/addresses/profile_token_quality_test_api.h"
#include "components/autofill/core/browser/data_quality/validation.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/form_import/form_data_importer_test_api.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map_test_utils.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/browser/integrators/autofill_ai/mock_autofill_ai_delegate.h"
#include "components/autofill/core/browser/integrators/compose/autofill_compose_delegate.h"
#include "components/autofill/core/browser/integrators/compose/mock_autofill_compose_delegate.h"
#include "components/autofill/core/browser/integrators/identity_credential/identity_credential_delegate.h"
#include "components/autofill/core/browser/integrators/identity_credential/mock_identity_credential_delegate.h"
#include "components/autofill/core/browser/integrators/optimization_guide/mock_autofill_optimization_guide.h"
#include "components/autofill/core/browser/integrators/password_form_classification.h"
#include "components/autofill/core/browser/integrators/password_manager/mock_password_manager_delegate.h"
#include "components/autofill/core/browser/integrators/password_manager/password_manager_delegate.h"
#include "components/autofill/core/browser/integrators/plus_addresses/autofill_plus_address_delegate.h"
#include "components/autofill/core/browser/integrators/plus_addresses/mock_autofill_plus_address_delegate.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/ml_model/autofill_ai/mock_autofill_ai_model_cache.h"
#include "components/autofill/core/browser/ml_model/autofill_ai/mock_autofill_ai_model_executor.h"
#include "components/autofill/core/browser/payments/amount_extraction_manager.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/test/mock_bnpl_manager.h"
#include "components/autofill/core/browser/payments/test_credit_card_save_manager.h"
#include "components/autofill/core/browser/payments/test_payments_autofill_client.h"
#include "components/autofill/core/browser/payments/test_payments_network_interface.h"
#include "components/autofill/core/browser/single_field_fillers/mock_single_field_fill_router.h"
#include "components/autofill/core/browser/strike_databases/payments/test_credit_card_save_strike_database.h"
#include "components/autofill/core/browser/studies/autofill_experiments.h"
#include "components/autofill/core/browser/suggestions/addresses/address_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_test_helpers.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/test_utils/valuables_data_test_utils.h"
#include "components/autofill/core/browser/test_utils/vote_uploads_test_matchers.h"
#include "components/autofill/core/browser/ui/payments/bubble_show_options.h"
#include "components/autofill/core/browser/ui/test_autofill_external_delegate.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/signatures.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/core/pref_names.h"
#include "components/security_state/core/security_state.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/features.h"
#include "components/sync/test/test_sync_service.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/url_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"
#include "url/url_canon.h"

namespace autofill {
namespace {

using ::base::Bucket;
using ::base::BucketsAre;
using ::base::UTF8ToUTF16;
using ::base::test::RunCallback;
using ::base::test::RunOnceCallback;
using mojom::SubmissionIndicatorEvent;
using mojom::SubmissionSource;
using test::AddFieldPredictionsToForm;
using test::CreateFieldPrediction;
using test::CreateTestAddressFormData;
using test::CreateTestFormField;
using test::CreateTestIbanFormData;
using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyNumber;
using ::testing::AnyOf;
using ::testing::AtLeast;
using ::testing::Contains;
using ::testing::DoAll;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Property;
using ::testing::Ref;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::UnorderedElementsAre;
using ::testing::VariantWith;
using upload_contents_matchers::FieldAutofillTypeIs;
using upload_contents_matchers::FieldsAre;
using upload_contents_matchers::FormSignatureIs;
using upload_contents_matchers::ObservedSubmissionIs;

constexpr Suggestion::Icon kAddressEntryIcon = Suggestion::Icon::kAccount;
constexpr char kPlusAddress[] = "plus+remote@plus.plus";
constexpr char kEllipsisDotSeparator[] = "\u2022";

// Action `SaveArgElementsTo<k>(pointer)` saves the value pointed to by the
// `k`th (0-based) argument of the mock function by moving it to `*pointer`.
ACTION_TEMPLATE(SaveArgElementsTo,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(pointer)) {
  auto span = testing::get<k>(args);
  pointer->assign(span.begin(), span.end());
}

gfx::Rect GetFakeCaretBounds(const FormFieldData& focused_field) {
  gfx::PointF p = focused_field.bounds().origin();
  return gfx::Rect(gfx::Point(p.x(), p.y()), gfx::Size(0, 10));
}

bool ShouldSplitCardNameAndLastFourDigitsForMetadata() {
  // Splitting card name and last four logic does not apply to iOS because iOS
  // doesn't currently support it.
  return !BUILDFLAG(IS_IOS);
}

bool ShouldUseNewFopDisplay() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return false;
#else
  return base::FeatureList::IsEnabled(
      features::kAutofillEnableNewFopDisplayDesktop);
#endif
}

// The number of obfuscation dots we use as a prefix when showing a credit
// card's last four.
int ObfuscationLengthForCreditCardLastFourDigits() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return 2;
#else
  return ShouldUseNewFopDisplay() ? 2 : 4;
#endif
}

// Generates credit card suggestion labels. If metadata is enabled, we produce
// shortened labels regardless of whether there is card metadata or not.
std::vector<std::vector<Suggestion::Text>> GenerateLabelsFromCreditCard(
    CreditCard& card) {
  std::vector<std::vector<Suggestion::Text>> suggestion_labels;

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // Android and iOS do not adhere to card label splitting.
  suggestion_labels = {
      {Suggestion::Text(card.ObfuscatedNumberWithVisibleLastFourDigits(
          ObfuscationLengthForCreditCardLastFourDigits()))}};
#else
  if (ShouldUseNewFopDisplay()) {
    suggestion_labels = {
        {Suggestion::Text(card.NetworkAndLastFourDigits(
             ObfuscationLengthForCreditCardLastFourDigits())),
         Suggestion::Text(u"•"),
         Suggestion::Text(card.AbbreviatedExpirationDateForDisplay(false))}};
  } else if (ShouldSplitCardNameAndLastFourDigitsForMetadata()) {
    // First label contains card name details and second label contains
    // obfuscated last four.
    suggestion_labels = {
        {Suggestion::Text(card.CardNameForAutofillDisplay(),
                          Suggestion::Text::IsPrimary(false),
                          Suggestion::Text::ShouldTruncate(true)),
         Suggestion::Text(card.ObfuscatedNumberWithVisibleLastFourDigits(
             ObfuscationLengthForCreditCardLastFourDigits()))}};
  } else {
    // Desktop uses the descriptive label.
    suggestion_labels = {
        {Suggestion::Text(card.CardIdentifierStringAndDescriptiveExpiration(
            /*app_locale=*/"en-US"))}};
  }
#endif
  return suggestion_labels;
}

// TODO(crbug.com/342446796): Move suggestion related test coverage in
// to PaymentsSuggestionGeneratorTest.
Suggestion GenerateSuggestionFromCardDetails(
    const std::string& network,
    const Suggestion::Icon icon,
    const std::string& last_four,
    std::string expiration_date_label,
    const std::string& nickname = std::string(),
    FieldType type = CREDIT_CARD_NUMBER) {
  std::string network_or_nickname =
      nickname.empty()
          ? base::UTF16ToUTF8(CreditCard::NetworkForDisplay(network))
          : nickname;
  std::string obfuscated_card_digits = test::ObfuscatedCardDigitsAsUTF8(
      last_four, ObfuscationLengthForCreditCardLastFourDigits());
  std::string network_and_last_four =
      base::StrCat({base::UTF16ToUTF8(CreditCard::NetworkForDisplay(network)),
                    std::string("  "), obfuscated_card_digits});
  std::vector<std::vector<Suggestion::Text>> network_last_four_and_exp_labels =
      std::vector<std::vector<Suggestion::Text>>{
          {Suggestion::Text(base::UTF8ToUTF16(network_and_last_four)),
           Suggestion::Text(u"•"),
           Suggestion::Text(base::UTF8ToUTF16(expiration_date_label))}};
  if (type == CREDIT_CARD_NUMBER) {
    if (ShouldUseNewFopDisplay()) {
      if (!nickname.empty()) {
        return Suggestion(
            /*main_text=*/nickname,
            /*labels=*/network_last_four_and_exp_labels, icon,
            SuggestionType::kCreditCardEntry);
      } else {
        std::vector<std::string> minor_texts = {kEllipsisDotSeparator,
                                                expiration_date_label};
        return Suggestion(
            /*main_text=*/network_and_last_four,
            /*minor_text_labels=*/minor_texts,
            /*label=*/"", icon, SuggestionType::kCreditCardEntry);
      }
    }
    if (ShouldSplitCardNameAndLastFourDigitsForMetadata()) {
      std::vector<std::string> minor_text = {obfuscated_card_digits};
      return Suggestion(
          /*main_text=*/network_or_nickname,
          /*minor_text_labels=*/minor_text,
          /*label=*/expiration_date_label, icon,
          SuggestionType::kCreditCardEntry);
    } else {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
      if (!ShouldUseNewFopDisplay()) {
        // We use a longer label on desktop platforms.
        expiration_date_label =
            std::string("Expires on ") + expiration_date_label;
      }
#endif
      return Suggestion(
          /*main_text=*/base::StrCat(
              {network_or_nickname, std::string("  "), obfuscated_card_digits}),
          /*label=*/expiration_date_label, icon,
          SuggestionType::kCreditCardEntry);
    }
  } else if (type == CREDIT_CARD_NAME_FULL) {
    std::vector<std::vector<Suggestion::Text>> labels;
    std::u16string last_four_u16 = base::UTF8ToUTF16(last_four);
    if constexpr (BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)) {
      // The label is formatted as either "••••1234" or "••1234".
      labels.push_back(
          {Suggestion::Text(base::UTF8ToUTF16(obfuscated_card_digits))});
    } else if (ShouldUseNewFopDisplay()) {
      labels = network_last_four_and_exp_labels;
    } else if (ShouldSplitCardNameAndLastFourDigitsForMetadata()) {
      // The label is formatted as "Product Description/Nickname/Network
      // ••••1234".
      labels.push_back(
          {Suggestion::Text(base::UTF8ToUTF16(network_or_nickname),
                            Suggestion::Text::IsPrimary(false),
                            Suggestion::Text::ShouldTruncate(true)),
           Suggestion::Text(base::UTF8ToUTF16(obfuscated_card_digits))});
    } else {
      // The label is formatted as "Network/Nickname  ••••1234, expires on
      // 01/25".
      expiration_date_label =
          std::string("expires on ") + expiration_date_label;
      std::string descriptive_label = network_or_nickname + "  " +
                                      obfuscated_card_digits + ", " +
                                      expiration_date_label;
      labels.push_back(
          {Suggestion::Text(base::UTF8ToUTF16(descriptive_label))});
    }
    return Suggestion(/*main_text=*/"Elvis Presley", /*labels=*/labels, icon,
                      SuggestionType::kCreditCardEntry);
  }
  return Suggestion();
}

// Creates a virtual card suggestion for the associated FPAN `suggestion`.
Suggestion GenerateVirtualCardSuggestionFromCreditCardSuggestion(
    const Suggestion& suggestion,
    FieldType field_type = UNKNOWN_TYPE) {
  Suggestion virtual_card_suggestion = suggestion;
  virtual_card_suggestion.type = SuggestionType::kVirtualCreditCardEntry;
  const std::u16string& virtual_card_label = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_OPTION_VALUE);
#if BUILDFLAG(IS_IOS)
  virtual_card_suggestion.minor_texts = {};
  virtual_card_suggestion.minor_texts.emplace_back(
      virtual_card_suggestion.main_text.value);
  virtual_card_suggestion.main_text.value = virtual_card_label;
#elif BUILDFLAG(IS_ANDROID)
  if (field_type == CREDIT_CARD_NUMBER) {
    virtual_card_suggestion.labels.clear();
  }
  if (ShouldSplitCardNameAndLastFourDigitsForMetadata()) {
    virtual_card_suggestion.main_text.value = base::StrCat(
        {virtual_card_label, u"  ", virtual_card_suggestion.main_text.value});
  } else {
    virtual_card_suggestion.minor_texts = {};
    virtual_card_suggestion.minor_texts.emplace_back(
        virtual_card_suggestion.main_text.value);
    virtual_card_suggestion.main_text.value = virtual_card_label;
  }
#else
  if (field_type == CREDIT_CARD_NUMBER) {
    virtual_card_suggestion.labels.clear();
  }
  virtual_card_suggestion.labels.push_back(
      std::vector<Suggestion::Text>{Suggestion::Text(virtual_card_label)});
#endif
  return virtual_card_suggestion;
}

Suggestion GetCardSuggestion(const std::string& network,
                             const std::string& nickname = std::string(),
                             FieldType type = CREDIT_CARD_NUMBER) {
  Suggestion::Icon icon = Suggestion::Icon::kCardGeneric;
  std::string last_four;
  std::string expiration_date;
  if (network == kVisaCard) {
    icon = Suggestion::Icon::kCardVisa;
    last_four = "3456";
    expiration_date = "04/99";
  } else if (network == kMasterCard) {
    icon = Suggestion::Icon::kCardMasterCard;
    last_four = "8765";
    expiration_date = "10/98";
  } else if (network == kAmericanExpressCard) {
    icon = Suggestion::Icon::kCardAmericanExpress;
    last_four = "0005";
    expiration_date = "04/10";
  } else {
    NOTREACHED();
  }
  return GenerateSuggestionFromCardDetails(network, icon, last_four,
                                           expiration_date, nickname, type);
}

struct TestAddressFillData {
  TestAddressFillData(const char* first,
                      const char* middle,
                      const char* last,
                      const char* address1,
                      const char* address2,
                      const char* city,
                      const char* state,
                      const char* postal_code,
                      const char* country,
                      const char* country_short,
                      const char* phone,
                      const char* email,
                      const char* company)
      : first(first),
        middle(middle),
        last(last),
        address1(address1),
        address2(address2),
        city(city),
        state(state),
        postal_code(postal_code),
        country(country),
        country_short(country_short),
        phone(phone),
        email(email),
        company(company) {}

  const char* first;
  const char* middle;
  const char* last;
  const char* address1;
  const char* address2;
  const char* city;
  const char* state;
  const char* postal_code;
  const char* country;
  const char* country_short;
  const char* phone;
  const char* email;
  const char* company;
};

struct TestCardFillData {
  TestCardFillData(const char* name_on_card,
                   const char* card_number,
                   const char* expiration_month,
                   const char* expiration_year,
                   bool use_month_type)
      : name_on_card(name_on_card),
        card_number(card_number),
        expiration_month(expiration_month),
        expiration_year(expiration_year),
        use_month_type(use_month_type) {}
  const char* name_on_card;
  const char* card_number;
  const char* expiration_month;
  const char* expiration_year;
  bool use_month_type;
};

const TestAddressFillData
    kEmptyAddressFillData("", "", "", "", "", "", "", "", "", "", "", "", "");

const TestCardFillData kEmptyCardFillData("",
                                          "",
                                          "",
                                          "",
                                          /*use_month_type=*/false);

const TestCardFillData kElvisCardFillData("Elvis Presley",
                                          "4234567890123456",
                                          "04",
                                          "2999",
                                          /*use_month_type*/ false);

TestAddressFillData GetElvisAddressFillData() {
  return {"Elvis",
          "Aaron",
          "Presley",
          "3734 Elvis Presley Blvd.",
          "Apt. 10",
          "Memphis",
          "Tennessee",
          "38116",
          "United States",
          "US",
          "2345678901",
          "theking@gmail.com",
          "RCA"};
}

// Matches a AskForValuesToFillFieldLogEvent by equality of fields.
auto Equal(const AskForValuesToFillFieldLogEvent& expected) {
  return VariantWith<AskForValuesToFillFieldLogEvent>(AllOf(
      Field("has_suggestion", &AskForValuesToFillFieldLogEvent::has_suggestion,
            expected.has_suggestion),
      Field("suggestion_is_shown",
            &AskForValuesToFillFieldLogEvent::suggestion_is_shown,
            expected.suggestion_is_shown)));
}

// Matches a TriggerFillFieldLogEvent by equality of fields.
auto Equal(const TriggerFillFieldLogEvent& expected) {
  return VariantWith<TriggerFillFieldLogEvent>(
      AllOf(Field("fill_event_id", &TriggerFillFieldLogEvent::fill_event_id,
                  expected.fill_event_id),
            Field("data_type", &TriggerFillFieldLogEvent::data_type,
                  expected.data_type),
            Field("associated_country_code",
                  &TriggerFillFieldLogEvent::associated_country_code,
                  expected.associated_country_code),
            Field("timestamp", &TriggerFillFieldLogEvent::timestamp,
                  expected.timestamp)));
}

// Matches a FillFieldLogEvent by equality of fields. Use FillEventId(-1) if
// you want to ignore the fill_event_id.
auto Equal(const FillFieldLogEvent& expected) {
  return VariantWith<FillFieldLogEvent>(AllOf(
      testing::Conditional(
          expected.fill_event_id == FillEventId(-1), _,
          Field("fill_event_id", &FillFieldLogEvent::fill_event_id,
                expected.fill_event_id)),
      Field("had_value_before_filling",
            &FillFieldLogEvent::had_value_before_filling,
            expected.had_value_before_filling),
      Field("autofill_skipped_status",
            &FillFieldLogEvent::autofill_skipped_status,
            expected.autofill_skipped_status),
      Field("was_autofilled_before_security_policy",
            &FillFieldLogEvent::was_autofilled_before_security_policy,
            expected.was_autofilled_before_security_policy),
      Field("had_value_after_filling",
            &FillFieldLogEvent::had_value_after_filling,
            expected.had_value_after_filling),
      Field("filling_prevented_by_iframe_security_policy",
            &FillFieldLogEvent::filling_prevented_by_iframe_security_policy,
            expected.filling_prevented_by_iframe_security_policy)));
}

// Matches a TypingFieldLogEvent by equality of fields.
auto Equal(const TypingFieldLogEvent& expected) {
  return VariantWith<TypingFieldLogEvent>(Field(
      "has_value_after_typing", &TypingFieldLogEvent::has_value_after_typing,
      expected.has_value_after_typing));
}

// Matches a HeuristicPredictionFieldLogEvent by equality of fields.
auto Equal(const HeuristicPredictionFieldLogEvent& expected) {
  return VariantWith<HeuristicPredictionFieldLogEvent>(AllOf(
      Field("field_type", &HeuristicPredictionFieldLogEvent::field_type,
            expected.field_type),
      Field("heuristic_source",
            &HeuristicPredictionFieldLogEvent::heuristic_source,
            expected.heuristic_source),
      Field("is_active_heuristic_source",
            &HeuristicPredictionFieldLogEvent::is_active_heuristic_source,
            expected.is_active_heuristic_source),
      Field("rank_in_field_signature_group",
            &HeuristicPredictionFieldLogEvent::rank_in_field_signature_group,
            expected.rank_in_field_signature_group)));
}

// Matches a AutocompleteAttributeFieldLogEvent by equality of fields.
auto Equal(const AutocompleteAttributeFieldLogEvent& expected) {
  return VariantWith<AutocompleteAttributeFieldLogEvent>(AllOf(
      Field("html_type", &AutocompleteAttributeFieldLogEvent::html_type,
            expected.html_type),
      Field("html_mode", &AutocompleteAttributeFieldLogEvent::html_mode,
            expected.html_mode),
      Field("rank_in_field_signature_group",
            &AutocompleteAttributeFieldLogEvent::rank_in_field_signature_group,
            expected.rank_in_field_signature_group)));
}

// Matches a ServerPredictionFieldLogEvent by equality of fields.
auto Equal(const ServerPredictionFieldLogEvent& expected) {
  return VariantWith<ServerPredictionFieldLogEvent>(AllOf(
      Field("server_type1", &ServerPredictionFieldLogEvent::server_type1,
            expected.server_type1),
      Field("prediction_source1",
            &ServerPredictionFieldLogEvent::prediction_source1,
            expected.prediction_source1),
      Field("server_type2", &ServerPredictionFieldLogEvent::server_type2,
            expected.server_type2),
      Field("prediction_source2",
            &ServerPredictionFieldLogEvent::prediction_source2,
            expected.prediction_source2),
      Field("server_type_prediction_is_override",
            &ServerPredictionFieldLogEvent::server_type_prediction_is_override,
            expected.server_type_prediction_is_override),
      Field("rank_in_field_signature_group",
            &ServerPredictionFieldLogEvent::rank_in_field_signature_group,
            expected.rank_in_field_signature_group)));
}

// Matches a RationalizationFieldLogEvent by equality of fields.
auto Equal(const RationalizationFieldLogEvent& expected) {
  return VariantWith<RationalizationFieldLogEvent>(
      AllOf(Field("field_type", &RationalizationFieldLogEvent::field_type,
                  expected.field_type),
            Field("section_id", &RationalizationFieldLogEvent::section_id,
                  expected.section_id),
            Field("type_changed", &RationalizationFieldLogEvent::type_changed,
                  expected.type_changed)));
}

// Matches a AblationFieldLogEvent by equality of fields.
auto Equal(const AblationFieldLogEvent& expected) {
  return VariantWith<AblationFieldLogEvent>(
      AllOf(Field("ablation_group", &AblationFieldLogEvent::ablation_group,
                  expected.ablation_group),
            Field("conditional_ablation_group",
                  &AblationFieldLogEvent::conditional_ablation_group,
                  expected.conditional_ablation_group),
            Field("day_in_ablation_window",
                  &AblationFieldLogEvent::day_in_ablation_window,
                  expected.day_in_ablation_window)));
}

// Matches a vector of FieldLogEventType objects by equality of fields of each
// log event type.
auto ArrayEquals(
    const std::vector<AutofillField::FieldLogEventType>& expected) {
  static_assert(
      std::variant_size<AutofillField::FieldLogEventType>() == 10,
      "If you add a new field event type, you need to update this function");
  std::vector<Matcher<AutofillField::FieldLogEventType>> matchers;
  for (const auto& event : expected) {
    if (std::holds_alternative<AskForValuesToFillFieldLogEvent>(event)) {
      matchers.push_back(
          Equal(std::get<AskForValuesToFillFieldLogEvent>(event)));
    } else if (std::holds_alternative<TriggerFillFieldLogEvent>(event)) {
      matchers.push_back(Equal(std::get<TriggerFillFieldLogEvent>(event)));
    } else if (std::holds_alternative<FillFieldLogEvent>(event)) {
      matchers.push_back(Equal(std::get<FillFieldLogEvent>(event)));
    } else if (std::holds_alternative<TypingFieldLogEvent>(event)) {
      matchers.push_back(Equal(std::get<TypingFieldLogEvent>(event)));
    } else if (std::holds_alternative<HeuristicPredictionFieldLogEvent>(
                   event)) {
      matchers.push_back(
          Equal(std::get<HeuristicPredictionFieldLogEvent>(event)));
    } else if (std::holds_alternative<AutocompleteAttributeFieldLogEvent>(
                   event)) {
      matchers.push_back(
          Equal(std::get<AutocompleteAttributeFieldLogEvent>(event)));
    } else if (std::holds_alternative<ServerPredictionFieldLogEvent>(event)) {
      matchers.push_back(Equal(std::get<ServerPredictionFieldLogEvent>(event)));
    } else if (std::holds_alternative<RationalizationFieldLogEvent>(event)) {
      matchers.push_back(Equal(std::get<RationalizationFieldLogEvent>(event)));
    } else if (std::holds_alternative<AblationFieldLogEvent>(event)) {
      matchers.push_back(Equal(std::get<AblationFieldLogEvent>(event)));
    } else {
      NOTREACHED();
    }
  }
  return ElementsAreArray(matchers);
}

// Matches a FillFieldLogEvent by equality of fields. Use FillEventId(-1) if
// you want to ignore the fill_event_id.
auto EqualsFillFieldLogEvent(const FillFieldLogEvent& expected) {
  return AllOf(
      testing::Conditional(
          expected.fill_event_id == FillEventId(-1), _,
          Field("fill_event_id", &FillFieldLogEvent::fill_event_id,
                expected.fill_event_id)),
      Field("had_value_before_filling",
            &FillFieldLogEvent::had_value_before_filling,
            expected.had_value_before_filling),
      Field("autofill_skipped_status",
            &FillFieldLogEvent::autofill_skipped_status,
            expected.autofill_skipped_status),
      Field("was_autofilled_before_security_policy",
            &FillFieldLogEvent::was_autofilled_before_security_policy,
            expected.was_autofilled_before_security_policy),
      Field("had_value_after_filling",
            &FillFieldLogEvent::had_value_after_filling,
            expected.had_value_after_filling),
      Field("filling_prevented_by_iframe_security_policy",
            &FillFieldLogEvent::filling_prevented_by_iframe_security_policy,
            expected.filling_prevented_by_iframe_security_policy));
}

// Creates a GUID for testing. For example,
// MakeGuid(123) = "00000000-0000-0000-0000-000000000123";
std::string MakeGuid(size_t last_digit) {
  return base::StringPrintf("00000000-0000-0000-0000-%012zu", last_digit);
}

std::string kElvisProfileGuid = MakeGuid(1);

class MockCreditCardAccessManager : public CreditCardAccessManager {
 public:
  explicit MockCreditCardAccessManager(BrowserAutofillManager* bam)
      : CreditCardAccessManager(
            bam,
            test_api(*bam).credit_card_form_event_logger()) {
    ON_CALL(*this, FetchCreditCard)
        .WillByDefault(
            [this](const CreditCard* card, OnCreditCardFetchedCallback cb) {
              CreditCardAccessManager::FetchCreditCard(card, std::move(cb));
            });
  }

  MOCK_METHOD(void, PrepareToFetchCreditCard, (), (override));
  MOCK_METHOD(void,
              FetchCreditCard,
              (const CreditCard* card,
               OnCreditCardFetchedCallback on_credit_card_fetched),
              (override));
};

class MockPaymentsAutofillClient : public payments::TestPaymentsAutofillClient {
 public:
  explicit MockPaymentsAutofillClient(AutofillClient* client)
      : payments::TestPaymentsAutofillClient(client) {}
  ~MockPaymentsAutofillClient() override = default;

  MOCK_METHOD(bool, HasCreditCardScanFeature, (), (const override));
  MOCK_METHOD(void,
              OnCardDataAvailable,
              (const FilledCardInformationBubbleOptions&),
              (override));
};

class MockAutofillClient : public TestAutofillClient {
 public:
  static std::unique_ptr<MockAutofillClient> Create(
      syncer::TestSyncService* sync_service) {
    auto client = std::make_unique<NiceMock<MockAutofillClient>>();

    auto create_payments_autofill_client = [&client] {
      auto x = std::make_unique<MockPaymentsAutofillClient>(&*client);
      x->set_payments_network_interface(
          std::make_unique<payments::TestPaymentsNetworkInterface>(
              client->GetURLLoaderFactory(), client->GetIdentityManager(),
              &client->GetPersonalDataManager()));
      return x;
    };

    auto create_credit_card_save_manager = [&client] {
      auto x = std::make_unique<TestCreditCardSaveManager>(&*client);
      x->SetCreditCardUploadEnabled(true);
      return x;
    };

    client->set_payments_autofill_client(create_payments_autofill_client());
    client->SetPrefs(test::PrefServiceForTesting());
    client->GetPersonalDataManager().SetPrefService(client->GetPrefs());
    client->GetPersonalDataManager().SetSyncServiceForTest(sync_service);
    client->set_test_strike_database(std::make_unique<TestStrikeDatabase>());
    client->set_single_field_fill_router(
        std::make_unique<NiceMock<MockSingleFieldFillRouter>>(
            client->GetAutocompleteHistoryManager(),
            client->GetPaymentsAutofillClient()->GetIbanManager(),
            client->GetPaymentsAutofillClient()
                ->GetMerchantPromoCodeManager()));
    client->set_crowdsourcing_manager(
        std::make_unique<NiceMock<MockAutofillCrowdsourcingManager>>(&*client));
    test_api(client->GetPersonalDataManager().address_data_manager())
        .set_auto_accept_address_imports(true);
    test_api(*client->GetFormDataImporter())
        .set_credit_card_save_manager(create_credit_card_save_manager());
    test_api(*client->GetFormDataImporter())
        .set_iban_save_manager(std::make_unique<IbanSaveManager>(&*client));

    // By default, if we offer single field form fill, suggestions should be
    // returned because it is assumed `field.should_autocomplete` is set to
    // true. This should be overridden in tests where
    // `field.should_autocomplete` is set to false or other conditions for
    // different providers are not met.
    ON_CALL(*client->GetPaymentsAutofillClient()->GetMerchantPromoCodeManager(),
            OnGetSingleFieldSuggestions)
        .WillByDefault(Return(true));
    ON_CALL(*client->GetPaymentsAutofillClient()->GetIbanManager(),
            OnGetSingleFieldSuggestions)
        .WillByDefault(Return(true));
    ON_CALL(*client->GetAutocompleteHistoryManager(),
            OnGetSingleFieldSuggestions)
        .WillByDefault(Return(true));
    return client;
  }

  MockAutofillClient() = default;
  MockAutofillClient(const MockAutofillClient&) = delete;
  MockAutofillClient& operator=(const MockAutofillClient&) = delete;
  ~MockAutofillClient() override = default;

  MOCK_METHOD(void, GetAiPageContent, (GetAiPageContentCallback), (override));
  MOCK_METHOD(AutofillAiModelCache*, GetAutofillAiModelCache, (), (override));
  MOCK_METHOD(AutofillAiModelExecutor*,
              GetAutofillAiModelExecutor,
              (),
              (override));
  MOCK_METHOD(profile_metrics::BrowserProfileType,
              GetProfileType,
              (),
              (const override));
  MOCK_METHOD(void,
              HideAutofillSuggestions,
              (SuggestionHidingReason reason),
              (override));
  MOCK_METHOD(void,
              TriggerUserPerceptionOfAutofillSurvey,
              (FillingProduct, (const std::map<std::string, std::string>&)),
              (override));
  MOCK_METHOD(AutofillComposeDelegate*, GetComposeDelegate, (), (override));
  MOCK_METHOD(bool,
              ShowAutofillFieldIphForFeature,
              (const FormFieldData& field, AutofillClient::IphFeature feature),
              (override));
  MOCK_METHOD(void, HideAutofillFieldIph, (), (override));
  MOCK_METHOD(void,
              ShowPlusAddressEmailOverrideNotification,
              (const std::string&, AutofillClient::EmailOverrideUndoCallback),
              (override));
};

class MockTouchToFillDelegate : public TouchToFillDelegate {
 public:
  static std::unique_ptr<MockTouchToFillDelegate> Create(
      BrowserAutofillManager* manager) {
    auto delegate = std::make_unique<NiceMock<MockTouchToFillDelegate>>();
    ON_CALL(*delegate, GetManager()).WillByDefault(Return(manager));
    ON_CALL(*delegate, IsShowingTouchToFill()).WillByDefault(Return(false));
    return delegate;
  }

  MockTouchToFillDelegate() = default;
  MockTouchToFillDelegate(const MockTouchToFillDelegate&) = delete;
  MockTouchToFillDelegate& operator=(const MockTouchToFillDelegate&) = delete;
  ~MockTouchToFillDelegate() override = default;

  MOCK_METHOD(BrowserAutofillManager*, GetManager, (), (override));
  MOCK_METHOD(bool,
              IntendsToShowTouchToFill,
              (FormGlobalId, FieldGlobalId, const FormData&),
              (override));
  MOCK_METHOD(bool,
              TryToShowTouchToFill,
              (const FormData&, const FormFieldData&),
              (override));
  MOCK_METHOD(bool, IsShowingTouchToFill, (), (override));
  MOCK_METHOD(void, HideTouchToFill, (), (override));
  MOCK_METHOD(void, Reset, (), (override));
  MOCK_METHOD(bool, ShouldShowScanCreditCard, (), (override));
  MOCK_METHOD(void, ScanCreditCard, (), (override));
  MOCK_METHOD(void, OnCreditCardScanned, (const CreditCard& card), (override));
  MOCK_METHOD(void, ShowPaymentMethodSettings, (), (override));
  MOCK_METHOD(void,
              CreditCardSuggestionSelected,
              (std::string unique_id, bool is_virtual),
              (override));
  MOCK_METHOD(void,
              IbanSuggestionSelected,
              ((std::variant<Iban::Guid, Iban::InstrumentId>)),
              (override));
  MOCK_METHOD(void,
              LoyaltyCardSuggestionSelected,
              (const std::string& loyalty_card_number),
              (override));
  MOCK_METHOD(void, OnDismissed, (bool dismissed_by_user), (override));
  MOCK_METHOD(void,
              LogMetricsAfterSubmission,
              (const FormStructure&),
              (override));
};

class MockFastCheckoutDelegate : public FastCheckoutDelegate {
 public:
  static std::unique_ptr<MockFastCheckoutDelegate> Create() {
    auto delegate = std::make_unique<NiceMock<MockFastCheckoutDelegate>>();
    ON_CALL(*delegate, IsShowingFastCheckoutUI()).WillByDefault(Return(false));
    return delegate;
  }

  MockFastCheckoutDelegate() = default;
  MockFastCheckoutDelegate(const MockFastCheckoutDelegate&) = delete;
  MockFastCheckoutDelegate& operator=(const MockFastCheckoutDelegate&) = delete;
  ~MockFastCheckoutDelegate() override = default;

  MOCK_METHOD(bool,
              TryToShowFastCheckout,
              (const FormData&,
               const FormFieldData&,
               base::WeakPtr<AutofillManager>),
              (override));
  MOCK_METHOD(bool,
              IntendsToShowFastCheckout,
              (AutofillManager&, FormGlobalId, FieldGlobalId, const FormData&),
              (const, override));
  MOCK_METHOD(bool, IsShowingFastCheckoutUI, (), (const, override));
  MOCK_METHOD(void, HideFastCheckout, (bool), (override));
};

class MockAutofillDriver : public TestAutofillDriver {
 public:
  using TestAutofillDriver::TestAutofillDriver;
  MockAutofillDriver(const MockAutofillDriver&) = delete;
  MockAutofillDriver& operator=(const MockAutofillDriver&) = delete;

  // Mock methods to enable testability.
  MOCK_METHOD((base::flat_set<FieldGlobalId>),
              ApplyFormAction,
              (mojom::FormActionType action_type,
               mojom::ActionPersistence action_persistence,
               base::span<const FormFieldData> data,
               const url::Origin& triggered_origin,
               (const base::flat_map<FieldGlobalId, FieldType>&)),
              (override));
  MOCK_METHOD(void,
              ApplyFieldAction,
              (mojom::FieldActionType action_type,
               mojom::ActionPersistence action_persistence,
               const FieldGlobalId& field_id,
               const std::u16string& value),
              (override));
  MOCK_METHOD(void,
              SendTypePredictionsToRenderer,
              ((const FormStructure&)),
              (override));
};

class MockAmountExtractionManager : public payments::AmountExtractionManager {
 public:
  explicit MockAmountExtractionManager(TestBrowserAutofillManager* test_manager)
      : AmountExtractionManager(test_manager) {
    ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
                test_manager->client().GetAutofillOptimizationGuide()),
            IsUrlEligibleForBnplIssuer)
        .WillByDefault(Return(true));
  }

  MOCK_METHOD(void, TriggerCheckoutAmountExtraction, (), (override));
};

class TestBrowserAutofillManager : public autofill::TestBrowserAutofillManager {
 public:
  static std::unique_ptr<TestBrowserAutofillManager> Create(
      AutofillDriver* driver) {
    auto manager = std::make_unique<TestBrowserAutofillManager>(driver);
    manager->set_touch_to_fill_delegate(
        MockTouchToFillDelegate::Create(&*manager));
    manager->set_fast_checkout_delegate(MockFastCheckoutDelegate::Create());
    test_api(*manager).SetExternalDelegate(
        std::make_unique<TestAutofillExternalDelegate>(
            &*manager,
            /*call_parent_methods=*/true));
    test_api(*manager).set_credit_card_access_manager(
        std::make_unique<NiceMock<MockCreditCardAccessManager>>(&*manager));
    test_api(*manager).set_amount_extraction_manager(
        std::make_unique<NiceMock<MockAmountExtractionManager>>(&*manager));
    return manager;
  }

  using autofill::TestBrowserAutofillManager::TestBrowserAutofillManager;
};

AutofillProfile FillDataToAutofillProfile(const TestAddressFillData& data) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile, data.first, data.middle, data.last, data.email,
                       data.company, data.address1, data.address2, data.city,
                       data.state, data.postal_code, data.country_short,
                       data.phone);
  return profile;
}

void ExpectFilledField(const char* expected_label,
                       const char* expected_name,
                       const char* expected_value,
                       FormControlType expected_form_control_type,
                       const FormFieldData& field) {
  SCOPED_TRACE(expected_label);
  EXPECT_EQ(UTF8ToUTF16(expected_label), field.label());
  EXPECT_EQ(UTF8ToUTF16(expected_name), field.name());
  EXPECT_EQ(UTF8ToUTF16(expected_value), field.value());
  EXPECT_EQ(expected_form_control_type, field.form_control_type());
}

// Verifies that the |filled_form| has been filled with the given data.
// Verifies address fields if |has_address_fields| is true, and verifies
// credit card fields if |has_credit_card_fields| is true. Verifies both if both
// are true. |use_month_type| is used for credit card input month type.
void ExpectFilledForm(
    const FormData& filled_form,
    const std::optional<TestAddressFillData>& address_fill_data,
    const std::optional<TestCardFillData>& card_fill_data) {
  // The number of fields in the address and credit card forms created above.
  constexpr size_t kAddressFormSize = 11;
  constexpr size_t kCreditCardFormSizeMonthType = 4;
  constexpr size_t kCreditCardFormSizeNotMonthType = 5;

  EXPECT_EQ(u"MyForm", filled_form.name());
  EXPECT_EQ(GURL("https://myform.com/form.html"), filled_form.url());
  EXPECT_EQ(GURL("https://myform.com/submit.html"), filled_form.action());

  size_t form_size = 0;
  if (address_fill_data) {
    form_size += kAddressFormSize;
  }
  if (card_fill_data) {
    form_size += card_fill_data->use_month_type
                     ? kCreditCardFormSizeMonthType
                     : kCreditCardFormSizeNotMonthType;
  }
  ASSERT_EQ(form_size, filled_form.fields().size());

  if (address_fill_data) {
    ExpectFilledField("First Name", "firstname", address_fill_data->first,
                      FormControlType::kInputText, filled_form.fields()[0]);
    ExpectFilledField("Middle Name", "middlename", address_fill_data->middle,
                      FormControlType::kInputText, filled_form.fields()[1]);
    ExpectFilledField("Last Name", "lastname", address_fill_data->last,
                      FormControlType::kInputText, filled_form.fields()[2]);
    ExpectFilledField("Address Line 1", "addr1", address_fill_data->address1,
                      FormControlType::kInputText, filled_form.fields()[3]);
    ExpectFilledField("Address Line 2", "addr2", address_fill_data->address2,
                      FormControlType::kInputText, filled_form.fields()[4]);
    ExpectFilledField("City", "city", address_fill_data->city,
                      FormControlType::kInputText, filled_form.fields()[5]);
    ExpectFilledField("State", "state", address_fill_data->state,
                      FormControlType::kInputText, filled_form.fields()[6]);
    ExpectFilledField("Postal Code", "zipcode", address_fill_data->postal_code,
                      FormControlType::kInputText, filled_form.fields()[7]);
    ExpectFilledField("Country", "country", address_fill_data->country,
                      FormControlType::kInputText, filled_form.fields()[8]);
    ExpectFilledField("Phone Number", "phonenumber", address_fill_data->phone,
                      FormControlType::kInputTelephone,
                      filled_form.fields()[9]);
    ExpectFilledField("Email", "email", address_fill_data->email,
                      FormControlType::kInputEmail, filled_form.fields()[10]);
  }

  if (card_fill_data) {
    size_t offset = address_fill_data ? kAddressFormSize : 0;
    ExpectFilledField("Name on Card", "nameoncard",
                      card_fill_data->name_on_card, FormControlType::kInputText,
                      filled_form.fields()[offset + 0]);
    ExpectFilledField("Card Number", "cardnumber", card_fill_data->card_number,
                      FormControlType::kInputText,
                      filled_form.fields()[offset + 1]);
    if (card_fill_data->use_month_type) {
      std::string exp_year = card_fill_data->expiration_year;
      std::string exp_month = card_fill_data->expiration_month;
      std::string date;
      if (!exp_year.empty() && !exp_month.empty()) {
        date = exp_year + "-" + exp_month;
      }

      ExpectFilledField("Expiration Date", "ccmonth", date.c_str(),
                        FormControlType::kInputMonth,
                        filled_form.fields()[offset + 2]);
    } else {
      ExpectFilledField(
          "Expiration Date", "ccmonth", card_fill_data->expiration_month,
          FormControlType::kInputText, filled_form.fields()[offset + 2]);
      ExpectFilledField("", "ccyear", card_fill_data->expiration_year,
                        FormControlType::kInputText,
                        filled_form.fields()[offset + 3]);
    }
  }
}

void ExpectFilledAddressFormElvis(const FormData& filled_form,
                                  bool has_credit_card_fields) {
  std::optional<TestCardFillData> expected_card_fill_data;
  if (has_credit_card_fields) {
    expected_card_fill_data = kEmptyCardFillData;
  }
  ExpectFilledForm(filled_form, GetElvisAddressFillData(),
                   expected_card_fill_data);
}

void ExpectFilledCreditCardFormElvis(const FormData& filled_form,
                                     bool has_address_fields) {
  std::optional<TestAddressFillData> expected_address_fill_data;
  if (has_address_fields) {
    expected_address_fill_data = kEmptyAddressFillData;
  }
  ExpectFilledForm(filled_form, expected_address_fill_data, kElvisCardFillData);
}

// Returns a matcher that checks a `FormStructure`'s renderer id.
auto FormStructureHasRendererId(FormRendererId form_renderer_id) {
  return Pointee(Property(&FormStructure::global_id,
                          Field(&FormGlobalId::renderer_id, form_renderer_id)));
}

Suggestion CreateSeparator() {
  Suggestion suggestion;
  suggestion.type = SuggestionType::kSeparator;
  return suggestion;
}

Suggestion CreateUndoOrClearFormSuggestion() {
#if BUILDFLAG(IS_IOS)
  std::u16string value =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_CLEAR_FORM_MENU_ITEM);
  // TODO(crbug.com/40266549): iOS still uses Clear Form logic, replace with
  // Undo.
  Suggestion suggestion(value, SuggestionType::kUndoOrClear);
  suggestion.icon = Suggestion::Icon::kClear;
#else
  std::u16string value = l10n_util::GetStringUTF16(IDS_AUTOFILL_UNDO_MENU_ITEM);
  if constexpr (BUILDFLAG(IS_ANDROID)) {
    value = base::i18n::ToUpper(value);
  }
  Suggestion suggestion(value, SuggestionType::kUndoOrClear);
  suggestion.icon = Suggestion::Icon::kUndo;
#endif
  // TODO(crbug.com/40266549): update "Clear Form" a11y announcement to "Undo"
  suggestion.acceptance_a11y_announcement =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_A11Y_ANNOUNCE_CLEARED_FORM);
  return suggestion;
}

class BrowserAutofillManagerTest : public testing::Test {
 public:
  void SetUp() override {
    // Advance the mock clock to a fixed, arbitrary, somewhat recent date.
    base::Time year2020;
    ASSERT_TRUE(base::Time::FromString("01/01/20", &year2020));
    FastForwardBy(year2020 - base::Time::Now());

    client_ = CreateAutofillClient();
    driver_ = CreateAutofillDriver();
    driver_->set_autofill_manager(CreateAutofillManager());

    // Initialize the TestPersonalDataManager with some default data.
    CreateTestAutofillProfiles();
    CreateTestCreditCards();

    // Mandatory re-auth is required for credit card autofill on automotive, so
    // the authenticator response needs to be properly mocked.
#if BUILDFLAG(IS_ANDROID)
    payments_client().SetUpDeviceBiometricAuthenticatorSuccessOnAutomotive();
#endif
  }

  void TearDown() override {
    driver_.reset();
    client_.reset();
  }

  // Called by SetUp(). May be overridden by deriving fixtures.
  virtual std::unique_ptr<MockAutofillClient> CreateAutofillClient() {
    return MockAutofillClient::Create(&sync_service());
  }
  virtual std::unique_ptr<NiceMock<MockAutofillDriver>> CreateAutofillDriver() {
    return std::make_unique<NiceMock<MockAutofillDriver>>(&client());
  }
  virtual std::unique_ptr<TestBrowserAutofillManager> CreateAutofillManager() {
    return TestBrowserAutofillManager::Create(&driver());
  }

  void FastForwardBy(base::TimeDelta time_delta) {
    task_environment_.FastForwardBy(time_delta);
  }

  void OnAskForValuesToFill(
      const FormData& form,
      const FormFieldData& field,
      AutofillSuggestionTriggerSource trigger_source =
          AutofillSuggestionTriggerSource::kTextFieldValueChanged,
      base::optional_ref<const PasswordSuggestionRequest> password_request =
          std::nullopt) {
    manager().OnAskForValuesToFill(form, field.global_id(),
                                   GetFakeCaretBounds(field), trigger_source,
                                   password_request);
  }

  void DidShowSuggestions(const FormData& form,
                          size_t field_index = 0,
                          SuggestionType type = SuggestionType::kAddressEntry) {
    manager().DidShowSuggestions({Suggestion(type)}, form,
                                 form.fields()[field_index].global_id(), {});
  }

  void TryToShowTouchToFill(const FormData& form,
                            const FormFieldData& field,
                            bool form_element_was_clicked) {
    manager().OnAskForValuesToFill(
        form, field.global_id(), GetFakeCaretBounds(field),
        form_element_was_clicked
            ? AutofillSuggestionTriggerSource::kFormControlElementClicked
            : AutofillSuggestionTriggerSource::kTextFieldValueChanged,
        std::nullopt);
  }

  void FormsSeen(const std::vector<FormData>& forms) {
    manager().OnFormsSeen(/*updated_forms=*/forms,
                          /*removed_forms=*/{});
  }

  void FormSubmitted(const FormData& form) {
    manager().OnFormSubmitted(form, SubmissionSource::FORM_SUBMISSION);
  }

  // TODO(crbug.com/40227071): Have separate functions for profile and credit
  // card filling.
  void FillAutofillFormData(
      const FormData& form,
      const FormFieldData& field,
      std::string guid,
      AutofillTriggerSource trigger_source = AutofillTriggerSource::kPopup) {
    manager().OnAskForValuesToFill(
        form, field.global_id(), GetFakeCaretBounds(field),
        AutofillSuggestionTriggerSource::kTextFieldDidReceiveKeyDown,
        std::nullopt);
    if (const AutofillProfile* profile =
            personal_data().address_data_manager().GetProfileByGUID(guid)) {
      manager().FillOrPreviewForm(mojom::ActionPersistence::kFill, form,
                                  field.global_id(), profile, trigger_source);
    } else if (const CreditCard* card =
                   personal_data().payments_data_manager().GetCreditCardByGUID(
                       guid)) {
      manager().FillOrPreviewForm(mojom::ActionPersistence::kFill, form,
                                  field.global_id(), card, trigger_source);
    }
  }

  // Fakes an autofill of `input_form` and returns the `FormData` with the
  // filled values.
  FormData FillAutofillFormDataAndGetResults(
      const FormData& input_form,
      const FormFieldData& input_field,
      std::string guid,
      AutofillTriggerSource trigger_source = AutofillTriggerSource::kPopup) {
    std::vector<FormFieldData> filled_fields;
    EXPECT_CALL(driver(), ApplyFormAction)
        .WillOnce([&filled_fields](
                      mojom::FormActionType action_type,
                      mojom::ActionPersistence action_persistence,
                      base::span<const FormFieldData> data,
                      const url::Origin& triggered_origin,
                      const base::flat_map<FieldGlobalId, FieldType>&) {
          filled_fields = std::vector<FormFieldData>(data.begin(), data.end());
          return base::MakeFlatSet<FieldGlobalId>(data, {},
                                                  &FormFieldData::global_id);
        });
    FillAutofillFormData(input_form, input_field, guid, trigger_source);
    FormData result_form = input_form;
    // Copy the filled data into the form.
    for (FormFieldData& field : test_api(result_form).fields()) {
      if (auto it = std::ranges::find(filled_fields, field.global_id(),
                                      &FormFieldData::global_id);
          it != filled_fields.end()) {
        field = *it;
      }
    }
    return result_form;
  }

  FormData CreateTestCreditCardFormData(bool is_https, bool use_month_type) {
    FormData form;
    AppendTestCreditCardFormData(&form, is_https, use_month_type);
    return form;
  }

  // Populates |form| with data corresponding to a simple credit card form.
  // Note that this actually appends fields to the form data, which can be
  // useful for building up more complex test forms.
  void AppendTestCreditCardFormData(FormData* form,
                                    bool is_https,
                                    bool use_month_type) {
    form->set_name(u"MyForm");
    if (is_https) {
      form->set_url(GURL("https://myform.com/form.html"));
      form->set_action(GURL("https://myform.com/submit.html"));
    } else {
      form->set_url(GURL("http://myform.com/form.html"));
      form->set_action(GURL("http://myform.com/submit.html"));
    }
    client().set_last_committed_primary_main_frame_url(form->url());

    test_api(*form).Append(CreateTestFormField("Name on Card", "nameoncard", "",
                                               FormControlType::kInputText));
    test_api(*form).Append(CreateTestFormField("Card Number", "cardnumber", "",
                                               FormControlType::kInputText));
    if (use_month_type) {
      test_api(*form).Append(CreateTestFormField(
          "Expiration Date", "ccmonth", "", FormControlType::kInputMonth));
    } else {
      test_api(*form).Append(CreateTestFormField(
          "Expiration Date", "ccmonth", "", FormControlType::kInputText));
      test_api(*form).Append(
          CreateTestFormField("", "ccyear", "", FormControlType::kInputText));
    }
    test_api(*form).Append(
        CreateTestFormField("CVC", "cvc", "", FormControlType::kInputText));
  }

  void PrepareForRealPanResponse(FormData* form) {
    // This line silences the warning from PaymentsNetworkInterface about
    // matching sync and Payments server types.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        "sync-url", "https://google.com");

    AppendTestCreditCardFormData(form, /*is_https=*/true,
                                 /*use_month_type=*/false);
    FormsSeen({*form});
    CreditCard card =
        CreditCard(CreditCard::RecordType::kMaskedServerCard, "a123");
    test::SetCreditCardInfo(&card, "John Dillinger", "1881" /* Visa */, "01",
                            "2017", "1");
    card.SetNetworkForMaskedCard(kVisaCard);

    EXPECT_CALL(driver(), ApplyFormAction).Times(AtLeast(1));
    manager().FillOrPreviewForm(mojom::ActionPersistence::kFill, *form,
                                form->fields()[0].global_id(), &card,
                                AutofillTriggerSource::kPopup);
  }

  void OnDidGetRealPan(
      payments::PaymentsAutofillClient::PaymentsRpcResult result,
      const std::string& real_pan,
      bool is_virtual_card = false) {
    payments::FullCardRequest& full_card_request =
        manager()
            .client()
            .GetPaymentsAutofillClient()
            ->GetCvcAuthenticator()
            .full_card_request_for_testing();

    // Mock user response.
    payments::FullCardRequest::UserProvidedUnmaskDetails details;
    details.cvc = u"123";
    static_cast<CardUnmaskDelegate&>(full_card_request)
        .OnUnmaskPromptAccepted(details);

    // Mock payments response.
    payments::UnmaskResponseDetails response;
    response.card_type = is_virtual_card ? payments::PaymentsAutofillClient::
                                               PaymentsRpcCardType::kVirtualCard
                                         : payments::PaymentsAutofillClient::
                                               PaymentsRpcCardType::kServerCard;
    full_card_request.OnDidGetRealPan(result, response.with_real_pan(real_pan));
  }

  // Convenience method to cast the FullCardRequest into a CardUnmaskDelegate.
  CardUnmaskDelegate& full_card_unmask_delegate() {
    payments::FullCardRequest& full_card_request =
        manager()
            .client()
            .GetPaymentsAutofillClient()
            ->GetCvcAuthenticator()
            .full_card_request_for_testing();
    return static_cast<CardUnmaskDelegate&>(full_card_request);
  }

  void DisableAutofillViaAblation(
      base::test::ScopedFeatureList& scoped_feature_list,
      bool for_addresses,
      bool for_credit_cards) {
    base::FieldTrialParams feature_parameters{
        {features::kAutofillAblationStudyEnabledForAddressesParam.name,
         base::ToString(for_addresses)},
        {features::kAutofillAblationStudyEnabledForPaymentsParam.name,
         base::ToString(for_credit_cards)},
        {features::kAutofillAblationStudyAblationWeightPerMilleParam.name,
         "1000"},
    };
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        features::kAutofillEnableAblationStudy, feature_parameters);
  }

  syncer::TestSyncService& sync_service() { return sync_service_; }

  MockAutofillClient& client() { return *client_; }

  FormDataImporter& form_data_importer() {
    return *client().GetFormDataImporter();
  }

  MockSingleFieldFillRouter& single_field_fill_router() {
    return static_cast<MockSingleFieldFillRouter&>(
        client().GetSingleFieldFillRouter());
  }

  MockPaymentsAutofillClient& payments_client() {
    return static_cast<MockPaymentsAutofillClient&>(
        *client().GetPaymentsAutofillClient());
  }

  TestPersonalDataManager& personal_data() {
    return client().GetPersonalDataManager();
  }

  MockAutofillCrowdsourcingManager& crowdsourcing_manager() {
    return static_cast<MockAutofillCrowdsourcingManager&>(
        client().GetCrowdsourcingManager());
  }

  MockAutofillDriver& driver() { return *driver_; }

  TestBrowserAutofillManager& manager() {
    return static_cast<TestBrowserAutofillManager&>(
        driver_->GetAutofillManager());
  }

  MockTouchToFillDelegate& touch_to_fill_delegate() {
    return *static_cast<MockTouchToFillDelegate*>(
        manager().touch_to_fill_delegate());
  }

  MockFastCheckoutDelegate& fast_checkout_delegate() {
    return *static_cast<MockFastCheckoutDelegate*>(
        manager().fast_checkout_delegate());
  }

  MockCreditCardAccessManager& cc_access_manager() {
    return static_cast<MockCreditCardAccessManager&>(
        manager().GetCreditCardAccessManager());
  }

  MockAmountExtractionManager& amount_extraction_manager() {
    return static_cast<MockAmountExtractionManager&>(
        test_api(manager()).get_amount_extraction_manager_for_testing());
  }

  TestAutofillExternalDelegate* external_delegate() {
    return static_cast<TestAutofillExternalDelegate*>(
        test_api(manager()).external_delegate());
  }

  MockIbanManager& iban_manager() {
    return *static_cast<MockIbanManager*>(
        client().GetPaymentsAutofillClient()->GetIbanManager());
  }

  MockMerchantPromoCodeManager& merchant_promo_code_manager() {
    return *static_cast<MockMerchantPromoCodeManager*>(
        client().GetPaymentsAutofillClient()->GetMerchantPromoCodeManager());
  }

  MockAutocompleteHistoryManager& autocomplete_history_manager() {
    return *static_cast<MockAutocompleteHistoryManager*>(
        client().GetAutocompleteHistoryManager());
  }

 private:
  void CreateTestAutofillProfiles() {
    AutofillProfile profile1 =
        FillDataToAutofillProfile(GetElvisAddressFillData());
    profile1.set_guid(kElvisProfileGuid);
    profile1.usage_history().set_use_date(base::Time::Now() - base::Days(2));
    personal_data().address_data_manager().AddProfile(profile1);

    AutofillProfile profile2(
        i18n_model_definition::kLegacyHierarchyCountryCode);
    test::SetProfileInfo(&profile2, "Charles", "Hardin", "Holley",
                         "buddy@gmail.com", "Decca", "123 Apple St.", "unit 6",
                         "Lubbock", "Texas", "79401", "US", "23456789012");
    profile2.set_guid(MakeGuid(2));
    profile2.usage_history().set_use_date(base::Time::Now() - base::Days(1));
    personal_data().address_data_manager().AddProfile(profile2);

    AutofillProfile profile3(
        i18n_model_definition::kLegacyHierarchyCountryCode);
    test::SetProfileInfo(&profile3, "", "", "", "", "", "", "", "", "", "",
                         "US", "");
    profile3.set_guid(MakeGuid(3));
    profile3.usage_history().set_use_date(base::Time::Now());
    personal_data().address_data_manager().AddProfile(profile3);
  }

  void CreateTestCreditCards() {
    CreditCard credit_card1;
    test::SetCreditCardInfo(&credit_card1, "Elvis Presley",
                            "4234567890123456",  // Visa
                            "04", "2999", "1");
    credit_card1.set_guid(MakeGuid(4));
    credit_card1.usage_history().set_use_count(10);
    credit_card1.usage_history().set_use_date(base::Time::Now() -
                                              base::Days(5));
    personal_data().payments_data_manager().AddCreditCard(credit_card1);

    CreditCard credit_card2;
    test::SetCreditCardInfo(&credit_card2, "Buddy Holly",
                            "5187654321098765",  // Mastercard
                            "10", "2998", "1");
    credit_card2.set_guid(MakeGuid(5));
    credit_card2.usage_history().set_use_count(5);
    credit_card2.usage_history().set_use_date(base::Time::Now() -
                                              base::Days(4));
    personal_data().payments_data_manager().AddCreditCard(credit_card2);

    CreditCard credit_card3;
    test::SetCreditCardInfo(&credit_card3, "", "", "08", "2999", "");
    credit_card3.set_guid(MakeGuid(6));
    personal_data().payments_data_manager().AddCreditCard(credit_card3);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  syncer::TestSyncService sync_service_;
  std::unique_ptr<MockAutofillClient> client_;
  std::unique_ptr<MockAutofillDriver> driver_;
};

// Test that calling OnFormsSeen consecutively with a different set of forms
// will query for each separately.
TEST_F(BrowserAutofillManagerTest, OnFormsSeen_DifferentFormStructures) {
  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  FormData form2;
  form2.set_host_frame(test::MakeLocalFrameToken());
  form2.set_renderer_id(test::MakeFormRendererId());
  form2.set_name(u"MyForm");
  form2.set_url(GURL("https://myform.com/form.html"));
  form2.set_action(GURL("https://myform.com/submit.html"));
  form2.set_fields(
      {CreateTestFormField("First Name", "firstname", "",
                           FormControlType::kInputText),
       CreateTestFormField("Last Name", "lastname", "",
                           FormControlType::kInputText),
       CreateTestFormField("Email", "email", "", FormControlType::kInputText)});

  EXPECT_CALL(crowdsourcing_manager(), StartQueryRequest).Times(AnyNumber());
  EXPECT_CALL(
      crowdsourcing_manager(),
      StartQueryRequest(
          ElementsAre(FormStructureHasRendererId(form.renderer_id())), _, _));
  EXPECT_CALL(
      crowdsourcing_manager(),
      StartQueryRequest(
          ElementsAre(FormStructureHasRendererId(form2.renderer_id())), _, _));
  FormsSeen({form});
  FormsSeen({form2});
}

// Test that when forms are seen, the renderer is updated with the predicted
// field types
TEST_F(BrowserAutofillManagerTest, OnFormsSeen_SendTypePredictionsToRenderer) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::test::kAutofillShowTypePredictions);
  // Set up a queryable form.
  FormData form1 = CreateTestAddressFormData();

  // Set up a non-queryable form.
  FormData form2;
  form2.set_host_frame(test::MakeLocalFrameToken());
  form2.set_renderer_id(test::MakeFormRendererId());
  form2.set_name(u"NonQueryable");
  form2.set_url(form1.url());
  form2.set_action(GURL("https://myform.com/submit.html"));
  form2.set_fields({CreateTestFormField("Querty", "qwerty", "",
                                        FormControlType::kInputText)});

  // Package the forms for observation.

  // Setup expectations.
  EXPECT_CALL(driver(), SendTypePredictionsToRenderer).Times(2);
  FormsSeen({form1, form2});
}

// Test that no autofill suggestions are returned for a field with an
// unrecognized autocomplete attribute on desktop.
// On mobile, the keyboard accessory is shown unconditionally.
TEST_F(BrowserAutofillManagerTest,
       GetProfileSuggestions_UnrecognizedAttribute) {
  // Set up our form data.
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  form.set_fields(
      {// Set a valid autocomplete attribute for the first name.
       CreateTestFormField("First name", "firstname", "",
                           FormControlType::kInputText, "given-name"),
       // Set no autocomplete attribute for the middle name.
       CreateTestFormField("Middle name", "middle", "",
                           FormControlType::kInputText, ""),
       // Set an unrecognized autocomplete attribute for the last name.
       CreateTestFormField("Last Name", "lastname", "",
                           FormControlType::kInputText, "unrecognized")});
  FormsSeen({form});

  // Ensure that the single field suggestions are not considered for any
  // field.
  EXPECT_CALL(merchant_promo_code_manager(), OnGetSingleFieldSuggestions)
      .Times(0);
  EXPECT_CALL(iban_manager(), OnGetSingleFieldSuggestions).Times(0);
  EXPECT_CALL(autocomplete_history_manager(), OnGetSingleFieldSuggestions)
      .Times(0);

  // Suggestions should be returned for the first two fields.
  OnAskForValuesToFill(form, form.fields()[0]);
  external_delegate()->CheckSuggestionCount(form.fields()[0].global_id(), 4);
  OnAskForValuesToFill(form, form.fields()[1]);
  external_delegate()->CheckSuggestionCount(form.fields()[1].global_id(), 4);

  OnAskForValuesToFill(form, form.fields()[2]);
  // For the third field, suggestions should only be shown on mobile due to the
  // unrecognized autocomplete attribute.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  external_delegate()->CheckSuggestionCount(form.fields()[2].global_id(), 4);
#else
  external_delegate()->CheckNoSuggestions(form.fields()[2].global_id());
#endif
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Tests the behavior of address suggestion vis a vis the
// AddressSuggestionStrikeDatabase logic.
TEST_F(BrowserAutofillManagerTest,
       GetProfileSuggestions_BlockSuggestionsAfterStrikeLimit) {
  auto simulate_user_ignored_suggestions = [&](const FormData& form,
                                               const FormFieldData& field) {
    test_api(client().GetAutofillDriverFactory()).Reset(driver());
    manager().AddSeenForm(form, {NAME_FIRST, NAME_LAST});
    OnAskForValuesToFill(form, field);
    // This ensures that the field has `did_trigger_suggestion_` set.
    external_delegate()->OnSuggestionsShown(external_delegate()->suggestions());
    // Submit the form without calling  DidAcceptSuggestions, meaning the user
    // ignored the suggestions given by Autofill.
    FormSubmitted(form);
  };

  FormData form = test::GetFormData(
      {.fields = {
           {.role = NAME_FIRST, .autocomplete_attribute = "off"},
           {.role = NAME_LAST, .autocomplete_attribute = "family-name"}}});
  manager().AddSeenForm(form, {NAME_FIRST, NAME_LAST});

  base::HistogramTester histogram_tester;
  // Check that at first both first and last name fields have suggestions.
  ASSERT_FALSE(test_api(manager())
                   .GetProfileSuggestions(form, form.fields()[0])
                   .empty());
  ASSERT_FALSE(test_api(manager())
                   .GetProfileSuggestions(form, form.fields()[1])
                   .empty());

  // Ignore suggestions on the first field "strike limit" times.
  simulate_user_ignored_suggestions(form, form.fields()[0]);
  simulate_user_ignored_suggestions(form, form.fields()[0]);
  simulate_user_ignored_suggestions(form, form.fields()[0]);

  histogram_tester.ExpectBucketCount(
      "Autofill.Suggestion.StrikeSuppression.Address", 1, 0);
  // Check that no more suggestions are returned.
  EXPECT_TRUE(test_api(manager())
                  .GetProfileSuggestions(form, form.fields()[0])
                  .empty());
  histogram_tester.ExpectBucketCount(
      "Autofill.Suggestion.StrikeSuppression.Address", 1, 1);

  // Ignore suggestions on the second field "strike limit" times.
  simulate_user_ignored_suggestions(form, form.fields()[1]);
  simulate_user_ignored_suggestions(form, form.fields()[1]);
  simulate_user_ignored_suggestions(form, form.fields()[1]);
  // Check that suggestions are still returned, since this field does not have
  // autocomplete=off and hence is not part of the considered fields for the
  // N-strike model.
  EXPECT_FALSE(test_api(manager())
                   .GetProfileSuggestions(form, form.fields()[1])
                   .empty());

  // Check that after accepting a suggestion, suppression and strikes are reset.
  OnAskForValuesToFill(
      form, form.fields()[0],
      AutofillSuggestionTriggerSource::kFormControlElementClicked);
  external_delegate()->DidAcceptSuggestion(
      Suggestion(SuggestionType::kAddressEntry), {});
  EXPECT_FALSE(test_api(manager())
                   .GetProfileSuggestions(form, form.fields()[0])
                   .empty());
}
#endif

// Tests that ac=unrecognized fields only activate suggestions when triggered
// through manual fallbacks (even though the field has a type in both cases) on
// desktop.
// On mobile, suggestions are shown even for ac=unrecognized fields.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
TEST_F(BrowserAutofillManagerTest,
       GetProfileSuggestions_UnrecognizedAttribute_Predictions_Mobile) {
  // Create a form where the first field has ac=unrecognized.
  FormData form = CreateTestAddressFormData();
  test_api(form).field(0).set_parsed_autocomplete(
      AutocompleteParsingResult{.field_type = HtmlFieldType::kUnrecognized});
  FormsSeen({form});

  // Expect that two suggestions + footer are returned for all fields,
  // independently of the autocomplete attribute. Two, because the fixture
  // created three profiles during set up, one of which is empty and cannot be
  // suggested (see `CreateTestAutofillProfiles()`).
  for (const FormFieldData& field : form.fields()) {
    OnAskForValuesToFill(form, field);
    external_delegate()->CheckSuggestionCount(field.global_id(), 4);
  }
}
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

// Test that when small forms are disabled (min required fields enforced) no
// suggestions are returned when there are less than three fields and none of
// them have an autocomplete attribute.
TEST_F(BrowserAutofillManagerTest,
       GetProfileSuggestions_MinFieldsEnforced_NoAutocomplete) {
  // Set up our form data.
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  form.set_fields({CreateTestFormField("First Name", "firstname", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Last Name", "lastname", "",
                                       FormControlType::kInputText)});

  FormsSeen({form});

  // Ensure that the single field suggestions are considered for both fields.
  EXPECT_CALL(merchant_promo_code_manager(), OnGetSingleFieldSuggestions)
      .Times(2);

  OnAskForValuesToFill(form, form.fields()[0]);
  EXPECT_FALSE(external_delegate()->on_suggestions_returned_seen());

  OnAskForValuesToFill(form, form.fields()[1]);
  EXPECT_FALSE(external_delegate()->on_suggestions_returned_seen());
}

// Test that when small forms are disabled (min required fields enforced)
// for a form with two fields with one that has an autocomplete attribute,
// suggestions are only made for the one that has the attribute.
TEST_F(BrowserAutofillManagerTest,
       GetProfileSuggestions_MinFieldsEnforced_WithOneAutocomplete) {
  // Set up our form data.
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  form.set_fields(
      {CreateTestFormField("First Name", "firstname", "",
                           FormControlType::kInputText, "given-name"),
       CreateTestFormField("Last Name", "lastname", "",
                           FormControlType::kInputText, "")});

  FormsSeen({form});

  // Check that suggestions are made for the field that has the autocomplete
  // attribute.
  OnAskForValuesToFill(form, form.fields()[0]);
  external_delegate()->CheckSuggestions(
      form.fields()[0].global_id(),
      {Suggestion("Charles", std::vector<std::vector<Suggestion::Text>>{},
                  Suggestion::Icon::kAccount, SuggestionType::kAddressEntry),
       Suggestion("Elvis", std::vector<std::vector<Suggestion::Text>>{},
                  Suggestion::Icon::kAccount, SuggestionType::kAddressEntry),
       CreateSeparator(), CreateManageAddressesSuggestion()});

  // Check that there are no suggestions for the field without the autocomplete
  // attribute.
  OnAskForValuesToFill(form, form.fields()[1]);
  EXPECT_FALSE(external_delegate()->on_suggestions_returned_seen());
}

// Test that for a form with two fields with autocomplete attributes,
// suggestions are made for both fields. This is true even if a minimum number
// of fields is enforced.
TEST_F(BrowserAutofillManagerTest,
       GetProfileSuggestions_SmallFormWithTwoAutocomplete) {
  // Set up our form data.
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  form.set_fields(
      {CreateTestFormField("First Name", "firstname", "",
                           FormControlType::kInputText, "given-name"),
       CreateTestFormField("Last Name", "lastname", "",
                           FormControlType::kInputText, "family-name")});

  FormsSeen({form});

  OnAskForValuesToFill(form, form.fields()[0]);
  external_delegate()->CheckSuggestions(
      form.fields()[0].global_id(),
      {Suggestion("Charles", "Charles Hardin Holley", kAddressEntryIcon,
                  SuggestionType::kAddressEntry),
       Suggestion("Elvis", "Elvis Aaron Presley", kAddressEntryIcon,
                  SuggestionType::kAddressEntry),
       CreateSeparator(), CreateManageAddressesSuggestion()});

  OnAskForValuesToFill(form, form.fields()[1]);
  external_delegate()->CheckSuggestions(
      form.fields()[1].global_id(),
      {Suggestion("Holley", "Charles Hardin Holley", kAddressEntryIcon,
                  SuggestionType::kAddressEntry),
       Suggestion("Presley", "Elvis Aaron Presley", kAddressEntryIcon,
                  SuggestionType::kAddressEntry),
       CreateSeparator(), CreateManageAddressesSuggestion()});
}

// Test that we return all address profile suggestions when all form fields
// are empty.
TEST_F(BrowserAutofillManagerTest, GetProfileSuggestions_EmptyValue) {
  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});

  OnAskForValuesToFill(form, form.fields()[0]);
  // Test that we sent the right values to the external delegate.
  external_delegate()->CheckSuggestions(
      form.fields()[0].global_id(),
      {Suggestion("Charles", "123 Apple St.", kAddressEntryIcon,
                  SuggestionType::kAddressEntry),
       Suggestion("Elvis", "3734 Elvis Presley Blvd.", kAddressEntryIcon,
                  SuggestionType::kAddressEntry),
       CreateSeparator(), CreateManageAddressesSuggestion()});
}

// Test that we return only matching address profile suggestions when the
// selected form field has been partially filled out.
TEST_F(BrowserAutofillManagerTest, GetProfileSuggestions_MatchCharacter) {
  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  FormFieldData& firstname_field = test_api(form).field(0);
  ASSERT_EQ(firstname_field.name(), u"firstname");
  firstname_field.set_value(u"E");
  FormsSeen({form});

  OnAskForValuesToFill(form, firstname_field);
  // Test that we sent the right values to the external delegate.
  external_delegate()->CheckSuggestions(
      firstname_field.global_id(),
      {Suggestion("Elvis", "3734 Elvis Presley Blvd.", kAddressEntryIcon,
                  SuggestionType::kAddressEntry),
       CreateSeparator(), CreateManageAddressesSuggestion()});
}

// Tests that we return address profile suggestions values when the section
// is already autofilled, and that we merge identical values.
TEST_F(BrowserAutofillManagerTest,
       GetProfileSuggestions_AlreadyAutofilledMergeValues) {
  personal_data().test_address_data_manager().ClearProfiles();
  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});

  // First name is already autofilled which will make the section appear as
  // "already autofilled".
  test_api(form).field(0).set_is_autofilled(true);

  // Two profiles have the same last name, and the third shares the same first
  // letter for last name.
  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile1.set_guid(MakeGuid(103));
  profile1.usage_history().set_use_date(base::Time::Now() - base::Days(2));
  profile1.SetInfo(NAME_FIRST, u"Robin", "en-US");
  profile1.SetInfo(NAME_LAST, u"Grimes", "en-US");
  profile1.SetInfo(ADDRESS_HOME_LINE1, u"1234 Smith Blvd.", "en-US");
  personal_data().address_data_manager().AddProfile(profile1);

  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile2.set_guid(MakeGuid(124));
  profile2.usage_history().set_use_date(base::Time::Now() - base::Days(1));
  profile2.SetInfo(NAME_FIRST, u"Carl", "en-US");
  profile2.SetInfo(NAME_LAST, u"Grimes", "en-US");
  profile2.SetInfo(ADDRESS_HOME_LINE1, u"1234 Smith Blvd.", "en-US");
  personal_data().address_data_manager().AddProfile(profile2);

  AutofillProfile profile3(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile3.set_guid(MakeGuid(126));
  profile3.usage_history().set_use_date(base::Time::Now());
  profile3.SetInfo(NAME_FIRST, u"Aaron", "en-US");
  profile3.SetInfo(NAME_LAST, u"Googler", "en-US");
  profile3.SetInfo(ADDRESS_HOME_LINE1, u"1600 Amphitheater pkwy", "en-US");
  personal_data().address_data_manager().AddProfile(profile3);

  FormFieldData& lastname_field = test_api(form).field(2);
  ASSERT_EQ(lastname_field.name(), u"lastname");
  OnAskForValuesToFill(form, lastname_field);
  external_delegate()->CheckSuggestions(
      lastname_field.global_id(),
      {Suggestion("Googler", "1600 Amphitheater pkwy", kAddressEntryIcon,
                  SuggestionType::kAddressEntry),
       Suggestion("Grimes", "1234 Smith Blvd.", kAddressEntryIcon,
                  SuggestionType::kAddressEntry),
       CreateSeparator(), CreateManageAddressesSuggestion()});
}

// Tests that we return address field swapping suggestions when the field
// is already autofilled.
TEST_F(BrowserAutofillManagerTest,
       GetProfileSuggestions_AlreadyAutofilledNoLabels) {
  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  FormFieldData& firstname_field = test_api(form).field(0);
  ASSERT_EQ(firstname_field.name(), u"firstname");
  // First name is already autofilled which will make the section appear as
  // "already autofilled".
  firstname_field.set_is_autofilled(true);
  firstname_field.set_value(u"E");
  FormsSeen({form});

  FormStructure* form_structure =
      manager().FindCachedFormById(form.global_id());
  ASSERT_TRUE(form_structure);
  AutofillField* autofill_field = form_structure->field(0);
  ASSERT_TRUE(autofill_field);
  ASSERT_TRUE(firstname_field.global_id() == autofill_field->global_id());
  autofill_field->set_autofilled_type(autofill_field->Type().GetStorableType());

  OnAskForValuesToFill(form, firstname_field);
  // Test that we sent the right values to the external delegate.
  external_delegate()->CheckSuggestions(
      firstname_field.global_id(),
      {Suggestion("Charles", "", Suggestion::Icon::kAccount,
                  SuggestionType::kAddressFieldByFieldFilling),
       Suggestion("Elvis", "", Suggestion::Icon::kAccount,
                  SuggestionType::kAddressFieldByFieldFilling),
       CreateSeparator(), CreateUndoOrClearFormSuggestion(),
       CreateManageAddressesSuggestion()});
}

// Test that we return no suggestions when the form has no relevant fields.
TEST_F(BrowserAutofillManagerTest, GetProfileSuggestions_UnknownFields) {
  // Set up our form data.
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  form.set_fields(
      {CreateTestFormField("Username", "username", "",
                           FormControlType::kInputText),
       CreateTestFormField("Password", "password", "",
                           FormControlType::kInputPassword),
       CreateTestFormField("Quest", "quest", "", FormControlType::kInputText),
       CreateTestFormField("Color", "color", "", FormControlType::kInputText)});

  FormsSeen({form});

  OnAskForValuesToFill(form, form.fields().back());
  EXPECT_FALSE(external_delegate()->on_suggestions_returned_seen());
}

// Test that we call duplicate profile suggestions.
TEST_F(BrowserAutofillManagerTest, GetProfileSuggestions_WithDuplicates) {
  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});

  // Add a duplicate profile.
  AutofillProfile duplicate_profile =
      *personal_data().address_data_manager().GetProfileByGUID(MakeGuid(1));
  personal_data().address_data_manager().AddProfile(duplicate_profile);

  OnAskForValuesToFill(form, form.fields()[0]);

  // Test that we sent the right values to the external delegate.
  external_delegate()->CheckSuggestions(
      form.fields()[0].global_id(),
      {Suggestion("Charles", "123 Apple St.", kAddressEntryIcon,
                  SuggestionType::kAddressEntry),
       Suggestion("Elvis", "3734 Elvis Presley Blvd.", kAddressEntryIcon,
                  SuggestionType::kAddressEntry),
       CreateSeparator(), CreateManageAddressesSuggestion()});
}

// Test that we return no suggestions when autofill is disabled.
TEST_F(BrowserAutofillManagerTest,
       GetProfileSuggestions_AutofillDisabledByUser) {
  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});

  // Disable Autofill.
  client().SetAutofillProfileEnabled(false);
  client().SetAutofillPaymentMethodsEnabled(false);

  OnAskForValuesToFill(form, form.fields()[0]);
  EXPECT_FALSE(external_delegate()->on_suggestions_returned_seen());
}

TEST_F(BrowserAutofillManagerTest, TestParseFormUntilInteractionMetric) {
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});

  // Advance time to check interaction metric.
  base::TimeDelta time_delta = base::Seconds(42);
  FastForwardBy(time_delta);

  base::HistogramTester histogram_tester;
  OnAskForValuesToFill(form, form.fields()[0]);

  histogram_tester.ExpectUniqueTimeSample(
      "Autofill.Timing.ParseFormUntilInteraction2", time_delta,
      /*expected_bucket_count=*/1);
}

// Tests that `SingleFieldFillRouter` returns to the
// `AutofillExternalDelegate`, if it has any.
TEST_F(BrowserAutofillManagerTest,
       OnSuggestionsReturned_CallsExternalDelegate) {
  FormData form = CreateTestAddressFormData();
  form.set_fields({CreateTestFormField("Some Field", "somefield", "",
                                       FormControlType::kInputText)});
  FormsSeen({form});

  std::vector<Suggestion> suggestions = {Suggestion(u"one"),
                                         Suggestion(u"two")};

  // Mock returning some single field fill `suggestions`.
  EXPECT_CALL(merchant_promo_code_manager(), OnGetSingleFieldSuggestions)
      .WillRepeatedly([&](const FormStructure& form, const FormFieldData& field,
                          const AutofillField& autofill_field,
                          const AutofillClient& client,
                          SingleFieldFillRouter::OnSuggestionsReturnedCallback&
                              on_suggestions_returned) {
        std::move(on_suggestions_returned).Run(field.global_id(), suggestions);
        return true;
      });
  OnAskForValuesToFill(
      form, form.fields()[0],
      AutofillSuggestionTriggerSource::kFormControlElementClicked);

  EXPECT_EQ(external_delegate()->trigger_source(),
            AutofillSuggestionTriggerSource::kFormControlElementClicked);
  external_delegate()->CheckSuggestions(form.fields()[0].global_id(),
                                        {suggestions[0], suggestions[1]});
}

class BrowserAutofillManagerTestValuables : public BrowserAutofillManagerTest {
 public:
  void SetUp() override {
    BrowserAutofillManagerTest::SetUp();
    std::unique_ptr<ValuablesTable> valuables_table =
        std::make_unique<ValuablesTable>();
    valuables_table_ = valuables_table.get();

    web_data_service_helper_ =
        std::make_unique<AutofillWebDataServiceTestHelper>(
            std::move(valuables_table));
    client().set_valuables_data_manager(std::make_unique<ValuablesDataManager>(
        web_data_service_helper_->autofill_webdata_service(),
        /*image_fetcher=*/nullptr));
    web_data_service_helper_->WaitUntilIdle();
  }

  void SetLoyaltyCards(const std::vector<LoyaltyCard>& loyalty_cards) {
    valuables_table_.get()->SetLoyaltyCards(loyalty_cards);
    test_api(valuables_data()).LoadLoyaltyCards();
    web_data_service_helper_->WaitUntilIdle();
  }

  ValuablesDataManager& valuables_data() {
    return *client().GetValuablesDataManager();
  }

 private:
  std::unique_ptr<AutofillWebDataServiceTestHelper> web_data_service_helper_;
  // Owned by `web_data_service_helper_`.
  raw_ptr<ValuablesTable> valuables_table_;
};

TEST_F(BrowserAutofillManagerTestValuables, GetSuggestions_LoyaltyCards) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillEnableLoyaltyCardsFilling};

  SetLoyaltyCards({test::CreateLoyaltyCard()});

  FormData form =
      test::GetFormData({.fields = {{.role = LOYALTY_MEMBERSHIP_ID}}});

  FormsSeen({form});
  OnAskForValuesToFill(form, form.fields()[0]);

  external_delegate()->CheckSuggestions(
      form.fields()[0].global_id(),
      {Suggestion("1234", "Deutsche Bahn", Suggestion::Icon::kNoIcon,
                  SuggestionType::kLoyaltyCardEntry),
       CreateSeparator(),
       Suggestion(l10n_util::GetStringUTF8(IDS_AUTOFILL_MANAGE_LOYALTY_CARDS),
                  "", Suggestion::Icon::kSettings,
                  SuggestionType::kManageLoyaltyCard)});
}

// Tests that when both email and loyalty card suggestions are available, they
// are shown in the correct order.
TEST_F(BrowserAutofillManagerTestValuables,
       GetSuggestions_EmailAndLoyaltyCards) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kAutofillEnableLoyaltyCardsFilling,
       features::kAutofillEnableEmailOrLoyaltyCardsFilling},
      {});

  SetLoyaltyCards({test::CreateLoyaltyCard()});

  FormData form_data =
      test::GetFormData({.fields = {{.role = EMAIL_OR_LOYALTY_MEMBERSHIP_ID},
                                    {.role = PASSWORD}}});
  auto form_structure = std::make_unique<FormStructure>(form_data);
  form_structure->DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr);

  test_api(*form_structure)
      .SetFieldTypes({EMAIL_OR_LOYALTY_MEMBERSHIP_ID, PASSWORD},
                     {EMAIL_OR_LOYALTY_MEMBERSHIP_ID, PASSWORD});
  manager().AddSeenFormStructure(std::move(form_structure));

  FormsSeen({form_data});
  OnAskForValuesToFill(form_data, form_data.fields()[0]);

#if BUILDFLAG(IS_ANDROID)
  external_delegate()->CheckSuggestions(
      form_data.fields()[0].global_id(),
      {Suggestion("buddy@gmail.com", "", Suggestion::Icon::kEmail,
                  SuggestionType::kAddressEntry),
       Suggestion("theking@gmail.com", "", Suggestion::Icon::kEmail,
                  SuggestionType::kAddressEntry),
       CreateSeparator(), CreateManageAddressesSuggestion(),
       Suggestion("1234", "Deutsche Bahn", Suggestion::Icon::kNoIcon,
                  SuggestionType::kLoyaltyCardEntry)});
#else
  Suggestion loyalty_cards_submenu = Suggestion(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_LOYALTY_CARDS_SUBMENU_TITLE), "",
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      Suggestion::Icon::kGoogleWalletMonochrome,
#else
      Suggestion::Icon::kNoIcon,
#endif
      SuggestionType::kLoyaltyCardEntry);
  loyalty_cards_submenu.acceptability =
      Suggestion::Acceptability::kUnacceptable;
  loyalty_cards_submenu.children = {
      Suggestion("1234", "Deutsche Bahn", Suggestion::Icon::kNoIcon,
                 SuggestionType::kLoyaltyCardEntry),
      CreateSeparator(),
      Suggestion(l10n_util::GetStringUTF8(IDS_AUTOFILL_MANAGE_LOYALTY_CARDS),
                 "", Suggestion::Icon::kSettings,
                 SuggestionType::kManageLoyaltyCard),
  };

  external_delegate()->CheckSuggestions(
      form_data.fields()[0].global_id(),
      {Suggestion("buddy@gmail.com", "", Suggestion::Icon::kEmail,
                  SuggestionType::kAddressEntry),
       Suggestion("theking@gmail.com", "", Suggestion::Icon::kEmail,
                  SuggestionType::kAddressEntry),
       CreateSeparator(), loyalty_cards_submenu, CreateSeparator(),
       CreateManageAddressesSuggestion()});
#endif  // BUILDFLAG(IS_ANDROID)
}

// Tests that when only loyalty card suggestions are available, they are shown
// without a submenu.
TEST_F(BrowserAutofillManagerTestValuables,
       GetSuggestions_EmailAndLoyaltyCards_NoEmails) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kAutofillEnableLoyaltyCardsFilling,
       features::kAutofillEnableEmailOrLoyaltyCardsFilling},
      {});

  SetLoyaltyCards({test::CreateLoyaltyCard()});
  personal_data().test_address_data_manager().ClearProfiles();

  FormData form_data =
      test::GetFormData({.fields = {{.role = EMAIL_OR_LOYALTY_MEMBERSHIP_ID},
                                    {.role = PASSWORD}}});
  auto form_structure = std::make_unique<FormStructure>(form_data);
  form_structure->DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr);

  test_api(*form_structure)
      .SetFieldTypes({EMAIL_OR_LOYALTY_MEMBERSHIP_ID, PASSWORD},
                     {EMAIL_OR_LOYALTY_MEMBERSHIP_ID, PASSWORD});
  manager().AddSeenFormStructure(std::move(form_structure));

  FormsSeen({form_data});
  OnAskForValuesToFill(form_data, form_data.fields()[0]);

  external_delegate()->CheckSuggestions(
      form_data.fields()[0].global_id(),
      {Suggestion("1234", "Deutsche Bahn", Suggestion::Icon::kNoIcon,
                  SuggestionType::kLoyaltyCardEntry),
       CreateSeparator(),
       Suggestion(l10n_util::GetStringUTF8(IDS_AUTOFILL_MANAGE_LOYALTY_CARDS),
                  "", Suggestion::Icon::kSettings,
                  SuggestionType::kManageLoyaltyCard)});
}

class BrowserAutofillManagerTestForMetadataCardSuggestions
    : public BrowserAutofillManagerTest,
      public testing::WithParamInterface<bool> {
 public:
  BrowserAutofillManagerTestForMetadataCardSuggestions() {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    feature_flags_.InitAndEnableFeature(
        features::kAutofillEnableNewFopDisplayDesktop);
#endif
  }

 private:
  base::test::ScopedFeatureList feature_flags_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         BrowserAutofillManagerTestForMetadataCardSuggestions,
                         ::testing::Bool());

// Test that we return all credit card profile suggestions when all form fields
// are empty.
TEST_P(BrowserAutofillManagerTestForMetadataCardSuggestions,
       GetCreditCardSuggestions_EmptyValue) {
  // Set up our form data.
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form});

  OnAskForValuesToFill(form, form.fields()[1]);

  // Test that we sent the credit card suggestions to the external delegate.
  external_delegate()->CheckSuggestions(
      form.fields()[1].global_id(),
      {GetCardSuggestion(kVisaCard), GetCardSuggestion(kMasterCard),
       CreateSeparator(),
       CreateManageCreditCardsSuggestion(
           /*with_gpay_logo=*/false)});
}

// Test that we return all credit card profile suggestions when the triggering
// field has whitespace in it.
TEST_P(BrowserAutofillManagerTestForMetadataCardSuggestions,
       GetCreditCardSuggestions_Whitespace) {
  // Set up our form data.
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form});

  FormFieldData& field = test_api(form).field(1);
  field.set_value(u"       ");
  OnAskForValuesToFill(form, field);

  // Test that we sent the right values to the external delegate.
  external_delegate()->CheckSuggestions(
      field.global_id(), {GetCardSuggestion(kVisaCard),
                          GetCardSuggestion(kMasterCard), CreateSeparator(),
                          CreateManageCreditCardsSuggestion(
                              /*with_gpay_logo=*/false)});
}

// Test that we return all credit card profile suggestions when the triggering
// field has stop characters in it, which should be removed.
TEST_P(BrowserAutofillManagerTestForMetadataCardSuggestions,
       GetCreditCardSuggestions_StopCharsOnly) {
  // Set up our form data.
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form});

  FormFieldData& field = test_api(form).field(1);
  field.set_value(u"____-____-____-____");
  OnAskForValuesToFill(form, field);
  // Test that we sent the right values to the external delegate.
  external_delegate()->CheckSuggestions(
      field.global_id(), {GetCardSuggestion(kVisaCard),
                          GetCardSuggestion(kMasterCard), CreateSeparator(),
                          CreateManageCreditCardsSuggestion(
                              /*with_gpay_logo=*/false)});
}

// Test that we return all credit card profile suggestions when the triggering
// field has some invisible unicode characters in it.
TEST_P(BrowserAutofillManagerTestForMetadataCardSuggestions,
       GetCreditCardSuggestions_InvisibleUnicodeOnly) {
  // Set up our form data.
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form});

  FormFieldData& field = test_api(form).field(1);
  field.set_value(std::u16string({0x200E, 0x200F}));
  OnAskForValuesToFill(form, field);

  // Test that we sent the right values to the external delegate.
  external_delegate()->CheckSuggestions(
      field.global_id(), {GetCardSuggestion(kVisaCard),
                          GetCardSuggestion(kMasterCard), CreateSeparator(),
                          CreateManageCreditCardsSuggestion(
                              /*with_gpay_logo=*/false)});
}

// Test that we return all credit card profile suggestions when the triggering
// field has stop characters in it and some input.
TEST_P(BrowserAutofillManagerTestForMetadataCardSuggestions,
       GetCreditCardSuggestions_StopCharsWithInput) {
  // Add a credit card with particular numbers that we will attempt to recall.
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "John Smith",
                          "5255667890168765",  // Mastercard
                          "10", "2098", "1");
  credit_card.set_guid(MakeGuid(7));
  personal_data().payments_data_manager().AddCreditCard(credit_card);

  // Set up our form data.
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form});

  FormFieldData& field = test_api(form).field(1);
  field.set_value(u"5255-66__-____-____");
  OnAskForValuesToFill(form, field);

  external_delegate()->CheckSuggestions(
      field.global_id(),
      {GetCardSuggestion(kVisaCard), GetCardSuggestion(kMasterCard),
       GetCardSuggestion(kMasterCard), CreateSeparator(),
       CreateManageCreditCardsSuggestion(
           /*with_gpay_logo=*/false)});
}

// Test that we still return all credit card profile suggestions when the
// credit card form field has been partially filled out.
TEST_P(BrowserAutofillManagerTestForMetadataCardSuggestions,
       GetCreditCardSuggestions_MatchCharacter) {
  // Set up our form data.
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormFieldData& cc_number_field = test_api(form).field(1);
  ASSERT_EQ(cc_number_field.name(), u"cardnumber");
  cc_number_field.set_value(u"78");
  FormsSeen({form});

  OnAskForValuesToFill(form, cc_number_field);

  // Test that we sent the right values to the external delegate.
  external_delegate()->CheckSuggestions(
      cc_number_field.global_id(),
      {GetCardSuggestion(kVisaCard), GetCardSuggestion(kMasterCard),
       CreateSeparator(),
       CreateManageCreditCardsSuggestion(
           /*with_gpay_logo=*/false)});
}

// Test that we return a warning explaining that credit card profile suggestions
// are unavailable when the page is secure, but the form action URL is valid but
// not secure.
TEST_F(BrowserAutofillManagerTest,
       GetCreditCardSuggestions_SecureContext_FormActionNotHTTPS) {
  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(/*is_https=*/true, false);
  // However we set the action (target URL) to be HTTP after all.
  form.set_action(GURL("http://myform.com/submit.html"));
  FormsSeen({form});

  OnAskForValuesToFill(form, form.fields()[0]);

  // Test that we sent the right values to the external delegate.
  external_delegate()->CheckSuggestions(
      form.fields()[0].global_id(),
      {Suggestion(l10n_util::GetStringUTF8(IDS_AUTOFILL_WARNING_MIXED_FORM), "",
                  Suggestion::Icon::kNoIcon,
                  SuggestionType::kMixedFormMessage)});

  // Clear the test credit cards and try again -- we should still show the
  // mixed form warning.
  personal_data().test_payments_data_manager().ClearCreditCards();
  OnAskForValuesToFill(form, form.fields()[0]);
  external_delegate()->CheckSuggestions(
      form.fields()[0].global_id(),
      {Suggestion(l10n_util::GetStringUTF8(IDS_AUTOFILL_WARNING_MIXED_FORM), "",
                  Suggestion::Icon::kNoIcon,
                  SuggestionType::kMixedFormMessage)});
}

// Test that we return credit card suggestions for secure pages that have an
// empty form action target URL.
TEST_P(BrowserAutofillManagerTestForMetadataCardSuggestions,
       GetCreditCardSuggestions_SecureContext_EmptyFormAction) {
  // Set up our form data.
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  // Clear the form action.
  form.set_action(GURL());
  FormsSeen({form});

  OnAskForValuesToFill(form, form.fields()[1]);

  // Test that we sent the right values to the external delegate.
  external_delegate()->CheckSuggestions(
      form.fields()[1].global_id(),
      {GetCardSuggestion(kVisaCard), GetCardSuggestion(kMasterCard),
       CreateSeparator(),
       CreateManageCreditCardsSuggestion(
           /*with_gpay_logo=*/false)});
}

// Test that we return credit card suggestions for secure pages that have a
// form action set to "javascript:something".
TEST_P(BrowserAutofillManagerTestForMetadataCardSuggestions,
       GetCreditCardSuggestions_SecureContext_JavascriptFormAction) {
  // Set up our form data.
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  // Have the form action be a javascript function (which is a valid URL).
  form.set_action(GURL("javascript:alert('Hello');"));
  FormsSeen({form});

  OnAskForValuesToFill(form, form.fields()[1]);

  // Test that we sent the right values to the external delegate.
  external_delegate()->CheckSuggestions(
      form.fields()[1].global_id(),
      {GetCardSuggestion(kVisaCard), GetCardSuggestion(kMasterCard),
       CreateSeparator(),
       CreateManageCreditCardsSuggestion(
           /*with_gpay_logo=*/false)});
}

// Test that expired cards are ordered by their ranking score and are always
// suggested after non expired cards even if they have a higher ranking score.
TEST_P(BrowserAutofillManagerTestForMetadataCardSuggestions,
       GetCreditCardSuggestions_ExpiredCards) {
  personal_data().test_payments_data_manager().ClearCreditCards();

  // Add a never used non expired credit card.
  CreditCard credit_card0("002149C1-EE28-4213-A3B9-DA243FFF021B",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "Bonnie Parker",
                          "5105105105108765" /* Mastercard */, "10", "2098",
                          "1");
  credit_card0.set_guid(MakeGuid(1));
  personal_data().payments_data_manager().AddCreditCard(credit_card0);

  // Add an expired card with a higher ranking score.
  CreditCard credit_card1("287151C8-6AB1-487C-9095-28E80BE5DA15",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card1, "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2010", "1");
  credit_card1.set_guid(MakeGuid(2));
  credit_card1.usage_history().set_use_count(300);
  credit_card1.usage_history().set_use_date(base::Time::Now() - base::Days(10));
  personal_data().payments_data_manager().AddCreditCard(credit_card1);

  // Add an expired card with a lower ranking score.
  CreditCard credit_card2("1141084B-72D7-4B73-90CF-3D6AC154673B",
                          test::kEmptyOrigin);
  credit_card2.usage_history().set_use_count(3);
  credit_card2.usage_history().set_use_date(base::Time::Now() - base::Days(1));
  test::SetCreditCardInfo(&credit_card2, "John Dillinger",
                          "4234567890123456" /* Visa */, "04", "2011", "1");
  credit_card2.set_guid(MakeGuid(3));
  personal_data().payments_data_manager().AddCreditCard(credit_card2);

  ASSERT_EQ(3U,
            personal_data().payments_data_manager().GetCreditCards().size());

  // Set up our form data.
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form});
  OnAskForValuesToFill(form, form.fields()[1]);

  Suggestion visa_suggestion = GenerateSuggestionFromCardDetails(
      kVisaCard, Suggestion::Icon::kCardVisa, "3456", "04/11");
  Suggestion amex_suggestion = GetCardSuggestion(kAmericanExpressCard);
  Suggestion mastercard_suggestion = GetCardSuggestion(kMasterCard);

  // Test that we sent the credit card suggestions to the external delegate.
  external_delegate()->CheckSuggestions(form.fields()[1].global_id(),
                                        {mastercard_suggestion, amex_suggestion,
                                         visa_suggestion, CreateSeparator(),
                                         CreateManageCreditCardsSuggestion(
                                             /*with_gpay_logo=*/false)});
}

// Test cards that are expired AND disused are suppressed when suppression is
// enabled and the input field is empty.
TEST_P(BrowserAutofillManagerTestForMetadataCardSuggestions,
       GetCreditCardSuggestions_SuppressDisusedCreditCardsOnEmptyField) {
  personal_data().test_payments_data_manager().ClearCreditCards();
  ASSERT_EQ(0U,
            personal_data().payments_data_manager().GetCreditCards().size());

  // Add a never used non expired local credit card.
  CreditCard credit_card0(MakeGuid(0), test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "Bonnie Parker",
                          "5105105105105100" /* Mastercard */, "04", "2999",
                          "1");
  personal_data().payments_data_manager().AddCreditCard(credit_card0);

  auto now = base::Time::Now();

  // Add an expired local card last used 10 days ago
  CreditCard credit_card1(MakeGuid(1), test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card1, "Clyde Barrow",
                          "4234567890123456" /* Visa */, "04", "2010", "1");
  credit_card1.usage_history().set_use_date(now - base::Days(10));
  personal_data().payments_data_manager().AddCreditCard(credit_card1);

  // Add an expired local card last used 180 days ago.
  CreditCard credit_card2(MakeGuid(2), test::kEmptyOrigin);
  credit_card2.usage_history().set_use_date(now - base::Days(182));
  test::SetCreditCardInfo(&credit_card2, "John Dillinger",
                          "378282246310005" /* American Express */, "01",
                          "2010", "1");
  personal_data().payments_data_manager().AddCreditCard(credit_card2);

  ASSERT_EQ(3U,
            personal_data().payments_data_manager().GetCreditCards().size());

  // Set up our form data.
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form});

  // Query with empty string only returns card0 and card1. Note expired
  // masked card2 is not suggested on empty fields.
  {
    OnAskForValuesToFill(form, form.fields()[0]);

    external_delegate()->CheckSuggestions(
        form.fields()[0].global_id(),
        {Suggestion("Bonnie Parker", GenerateLabelsFromCreditCard(credit_card0),
                    Suggestion::Icon::kCardMasterCard,
                    SuggestionType::kCreditCardEntry),
         Suggestion("Clyde Barrow", GenerateLabelsFromCreditCard(credit_card1),
                    Suggestion::Icon::kCardVisa,
                    SuggestionType::kCreditCardEntry),
         CreateSeparator(),
         CreateManageCreditCardsSuggestion(
             /*with_gpay_logo=*/false)});
  }

  // Query with name prefix for card0 returns card0.
  {
    FormFieldData& field = test_api(form).field(0);
    field.set_value(u"B");
    OnAskForValuesToFill(form, field);

    external_delegate()->CheckSuggestions(
        form.fields()[0].global_id(),
        {Suggestion("Bonnie Parker", GenerateLabelsFromCreditCard(credit_card0),
                    Suggestion::Icon::kCardMasterCard,
                    SuggestionType::kCreditCardEntry),
         CreateSeparator(),
         CreateManageCreditCardsSuggestion(
             /*with_gpay_logo=*/false)});
  }

  // Query with name prefix for card1 returns card1.
  {
    FormFieldData& field = test_api(form).field(0);
    field.set_value(u"Cl");
    OnAskForValuesToFill(form, field);

    external_delegate()->CheckSuggestions(
        form.fields()[0].global_id(),
        {Suggestion("Clyde Barrow", GenerateLabelsFromCreditCard(credit_card1),
                    Suggestion::Icon::kCardVisa,
                    SuggestionType::kCreditCardEntry),
         CreateSeparator(),
         CreateManageCreditCardsSuggestion(
             /*with_gpay_logo=*/false)});
  }

  // Query with name prefix for card2 returns card2.
  {
    FormFieldData& field = test_api(form).field(0);
    field.set_value(u"Jo");
    OnAskForValuesToFill(form, field);

    external_delegate()->CheckSuggestions(
        form.fields()[0].global_id(),
        {Suggestion("John Dillinger",
                    GenerateLabelsFromCreditCard(credit_card2),
                    Suggestion::Icon::kCardAmericanExpress,
                    SuggestionType::kCreditCardEntry),
         CreateSeparator(),
         CreateManageCreditCardsSuggestion(
             /*with_gpay_logo=*/false)});
  }
}

// Test that a card that doesn't have a number is not shown in the
// suggestions when querying credit cards by their number, and is shown when
// querying other fields.
TEST_P(BrowserAutofillManagerTestForMetadataCardSuggestions,
       GetCreditCardSuggestions_NumberMissing) {
  // Create one normal credit card and one credit card with the number
  // missing.
  personal_data().test_payments_data_manager().ClearCreditCards();
  ASSERT_EQ(0U,
            personal_data().payments_data_manager().GetCreditCards().size());

  CreditCard credit_card0("287151C8-6AB1-487C-9095-28E80BE5DA15",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2910", "1");
  credit_card0.set_guid(MakeGuid(1));
  credit_card0.usage_history().set_use_date(base::Time::Now() - base::Days(1));
  personal_data().payments_data_manager().AddCreditCard(credit_card0);

  CreditCard credit_card1("1141084B-72D7-4B73-90CF-3D6AC154673B",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card1, "John Dillinger", "", "01", "2999",
                          "1");
  credit_card1.set_guid(MakeGuid(2));
  personal_data().payments_data_manager().AddCreditCard(credit_card1);

  ASSERT_EQ(2U,
            personal_data().payments_data_manager().GetCreditCards().size());

  // Set up our form data.
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form});

  // Query by card number field.
  OnAskForValuesToFill(form, form.fields()[1]);

  // Sublabel is expiration date when filling card number. The second card
  // doesn't have a number so it should not be included in the suggestions.
  external_delegate()->CheckSuggestions(
      form.fields()[1].global_id(),
      {GetCardSuggestion(kAmericanExpressCard), CreateSeparator(),
       CreateManageCreditCardsSuggestion(
           /*with_gpay_logo=*/false)});

  // Query by cardholder name field.
  OnAskForValuesToFill(form, form.fields()[0]);

  external_delegate()->CheckSuggestions(
      form.fields()[0].global_id(),
      {Suggestion("John Dillinger", "", Suggestion::Icon::kCardGeneric,
                  SuggestionType::kCreditCardEntry),
       Suggestion("Clyde Barrow", GenerateLabelsFromCreditCard(credit_card0),
                  Suggestion::Icon::kCardAmericanExpress,
                  SuggestionType::kCreditCardEntry),
       CreateSeparator(),
       CreateManageCreditCardsSuggestion(
           /*with_gpay_logo=*/false)});
}

// Test that we return profile and credit card suggestions for combined forms.
TEST_F(BrowserAutofillManagerTest, GetAddressAndCreditCardSuggestions) {
  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  const size_t first_credit_card_field = form.fields().size();
  AppendTestCreditCardFormData(&form, true, false);
  FormsSeen({form});

  OnAskForValuesToFill(form, form.fields()[0]);
  // Test that we sent the right values to the external delegate.
  external_delegate()->CheckSuggestions(
      form.fields()[0].global_id(),
      {Suggestion("Charles", "123 Apple St.", kAddressEntryIcon,
                  SuggestionType::kAddressEntry),
       Suggestion("Elvis", "3734 Elvis Presley Blvd.", kAddressEntryIcon,
                  SuggestionType::kAddressEntry),
       CreateSeparator(), CreateManageAddressesSuggestion()});

  FormFieldData& cc_number_field =
      test_api(form).field(first_credit_card_field + 1);
  ASSERT_EQ(cc_number_field.name(), u"cardnumber");
  OnAskForValuesToFill(form, cc_number_field);

  // Test that we sent the credit card suggestions to the external delegate.
  external_delegate()->CheckSuggestions(
      cc_number_field.global_id(),
      {GetCardSuggestion(kVisaCard), GetCardSuggestion(kMasterCard),
       CreateSeparator(),
       CreateManageCreditCardsSuggestion(
           /*with_gpay_logo=*/false)});
}

// Test that for non-https forms with both address and credit card fields, we
// only return address suggestions. Instead of credit card suggestions, we
// should return a warning explaining that credit card profile suggestions are
// unavailable when the form is not https.
TEST_F(BrowserAutofillManagerTest, GetAddressAndCreditCardSuggestionsNonHttps) {
  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  const size_t first_credit_card_field = form.fields().size();
  AppendTestCreditCardFormData(&form, /*is_https=*/false,
                               /*use_month_type=*/false);
  FormsSeen({form});

  OnAskForValuesToFill(form, form.fields()[0]);

  // Verify that suggestions are returned.
  EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());

  FormFieldData& cc_number_field =
      test_api(form).field(first_credit_card_field + 1);
  ASSERT_EQ(cc_number_field.name(), u"cardnumber");
  OnAskForValuesToFill(form, cc_number_field);

  // Test that we sent the right values to the external delegate.
  external_delegate()->CheckSuggestions(
      cc_number_field.global_id(),
      {Suggestion(
          l10n_util::GetStringUTF8(IDS_AUTOFILL_WARNING_INSECURE_CONNECTION),
          "", Suggestion::Icon::kNoIcon,
          SuggestionType::kInsecureContextPaymentDisabledMessage)});

  // Clear the test credit cards and try again -- we shouldn't return a warning.
  personal_data().test_payments_data_manager().ClearCreditCards();
  OnAskForValuesToFill(form, cc_number_field);
  external_delegate()->CheckNoSuggestions(cc_number_field.global_id());
}

TEST_F(BrowserAutofillManagerTest,
       ShouldShowAddressSuggestionsIfCreditCardAutofillDisabled) {
  base::test::ScopedFeatureList features;
  DisableAutofillViaAblation(features, /*for_addresses=*/false,
                             /*for_credit_cards=*/true);

  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});

  OnAskForValuesToFill(form, form.fields()[0]);
  // Verify that suggestions are returned.
  EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());
}

TEST_F(BrowserAutofillManagerTest,
       ShouldShowCreditCardSuggestionsIfAddressAutofillDisabled) {
  base::test::ScopedFeatureList features;
  DisableAutofillViaAblation(features, /*for_addresses=*/true,
                             /*for_credit_cards=*/false);

  // Set up our form data.
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form});

  OnAskForValuesToFill(form, form.fields()[0]);
  // Verify that suggestions are returned.
  EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());
}

TEST_F(BrowserAutofillManagerTest,
       ShouldNotShowCreditCardsSuggestionsIfCreditCardAutofillDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  DisableAutofillViaAblation(scoped_feature_list, /*for_addresses=*/false,
                             /*for_credit_cards=*/true);

  // Set up our form data.
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form});

  OnAskForValuesToFill(form, form.fields()[0]);

  // Check that credit card suggestions will not be available.
  external_delegate()->CheckNoSuggestions(form.fields()[0].global_id());
}

// Test that credit card suggestions are shown for expiry type field when
// credit card number field is empty.
TEST_F(BrowserAutofillManagerTest,
       ShowCreditCardSuggestions_ForExpirationTypeField_IfEmptyCCNumber) {
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form});

  // Expect that suggestions are returned for the expiry type field.
  const FormFieldData& expiry_type_field = form.fields()[2];
  OnAskForValuesToFill(form, expiry_type_field);
  EXPECT_FALSE(external_delegate()->suggestions().empty());
}

// Test that credit card suggestions are shown for expiry type field when
// credit card number field is not empty and has been autofilled.
TEST_F(
    BrowserAutofillManagerTest,
    ShowCreditCardSuggestions_ForExpirationTypeField_IfNonEmptyAutofilledCCNumber) {
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form});

  // Fill data in CC Number field and set it as autofilled.
  test_api(form).field(1).set_value(u"4444 4444 4444 4444");
  test_api(form).field(1).set_is_autofilled(true);

  // Expect that suggestions are returned for the expiry type field.
  const FormFieldData& expiry_type_field = form.fields()[2];
  OnAskForValuesToFill(form, expiry_type_field);
  EXPECT_FALSE(external_delegate()->suggestions().empty());
}

// Test that credit card suggestions are not shown for expiry type field when
// credit card number field is not empty and has not been autofilled.
TEST_F(
    BrowserAutofillManagerTest,
    DoNotShowCreditCardSuggestions_ForExpirationTypeField_IfNonEmptyNonAutofilledCCNumber) {
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form});

  // Fill data in CC Number field and set it as not autofilled.
  test_api(form).field(1).set_value(u"4444 4444 4444 4444");
  test_api(form).field(1).set_is_autofilled(false);

  // Expect no suggestions are returned for the expiry type field.
  const FormFieldData& expiry_type_field = form.fields()[2];
  OnAskForValuesToFill(form, expiry_type_field);
  external_delegate()->CheckNoSuggestions(expiry_type_field.global_id());
}

TEST_F(BrowserAutofillManagerTest,
       ShouldNotShowAddressSuggestionsIfAddressAutofillDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  DisableAutofillViaAblation(scoped_feature_list, /*for_addresses=*/true,
                             /*for_credit_cards=*/false);

  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});

  OnAskForValuesToFill(form, form.fields()[0]);

  // Check that credit card suggestions will not be available.
  external_delegate()->CheckNoSuggestions(form.fields()[0].global_id());
}

// Tests that if the ablation study runs in dry-run mode, suggestions are
// shown even though the ablation study is enabled.
// This is basically a copy of
// ShouldNotShowAddressSuggestionsIfAddressAutofillDisabled, except that the
// dryrun flag ist set.
TEST_F(BrowserAutofillManagerTest,
       ShouldShowAddressSuggestionsIfAblationIsInDryRunMode) {
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams feature_parameters{
      {features::kAutofillAblationStudyEnabledForAddressesParam.name, "true"},
      {features::kAutofillAblationStudyEnabledForPaymentsParam.name, "true"},
      {features::kAutofillAblationStudyAblationWeightPerMilleParam.name,
       "1000"},
      {features::kAutofillAblationStudyIsDryRun.name, "true"}};
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillEnableAblationStudy, feature_parameters);

  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});

  OnAskForValuesToFill(form, form.fields()[0]);

  // Verify that suggestions are returned.
  EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());
  EXPECT_GT(external_delegate()->suggestions().size(), 0u);
}

// Tests that `AmountExtractionManager` should trigger amount extraction if
// credit card form is clicked and `BnplManager` is notified about suggestion
// generation.
TEST_F(BrowserAutofillManagerTest,
       ShouldTriggerAmountExtraction_IfCreditCardFormIsClicked) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableAmountExtractionDesktop,
                            features::kAutofillEnableBuyNowPayLaterSyncing,
                            features::kAutofillEnableBuyNowPayLater},
      /*disabled_features=*/{});

  personal_data().test_payments_data_manager().AddBnplIssuer(
      test::GetTestUnlinkedBnplIssuer());
  // Set up our form data.
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form});

  // Test case for credit-card-number field.
  const FormFieldData& card_number_field = form.fields()[1];
  ASSERT_EQ(card_number_field.name(), u"cardnumber");

  // Verify that the amount extraction is triggered.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  EXPECT_CALL(amount_extraction_manager(), TriggerCheckoutAmountExtraction)
      .Times(1);
  EXPECT_CALL(*manager().GetPaymentsBnplManager(), NotifyOfSuggestionGeneration)
      .Times(1);
#else
  EXPECT_CALL(amount_extraction_manager(), TriggerCheckoutAmountExtraction)
      .Times(0);
#endif

  OnAskForValuesToFill(form, card_number_field);

  // Verify that suggestions are returned as normal.
  EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());
}

// Tests that `AmountExtractionManager` should not trigger amount extraction if
// a non-credit-card form is clicked and `BnplManager` is not notified about
// suggestion generation.
TEST_F(BrowserAutofillManagerTest,
       ShouldNotTriggerAmountExtraction_IfNonCreditCardFormIsClicked) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableAmountExtractionDesktop,
                            features::kAutofillEnableBuyNowPayLaterSyncing,
                            features::kAutofillEnableBuyNowPayLater},
      /*disabled_features=*/{});

  personal_data().test_payments_data_manager().AddBnplIssuer(
      test::GetTestUnlinkedBnplIssuer());
  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});

  // Verify that the amount extraction is not triggered.
  EXPECT_CALL(amount_extraction_manager(), TriggerCheckoutAmountExtraction)
      .Times(0);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  EXPECT_CALL(*manager().GetPaymentsBnplManager(), NotifyOfSuggestionGeneration)
      .Times(0);
#endif

  OnAskForValuesToFill(form, form.fields()[0]);

  // Verify that suggestions are returned as normal.
  EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());
}

// Tests that `AmountExtractionManager` should not trigger amount extraction if
// there is no credit card suggestion and `BnplManager` is not notified about
// suggestion generation.
TEST_F(BrowserAutofillManagerTest,
       ShouldNotTriggerAmountExtraction_IfNoSuggestion) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableAmountExtractionDesktop,
                            features::kAutofillEnableBuyNowPayLaterSyncing,
                            features::kAutofillEnableBuyNowPayLater},
      /*disabled_features=*/{});

  personal_data().test_payments_data_manager().AddBnplIssuer(
      test::GetTestUnlinkedBnplIssuer());
  // Set up our form data.
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form});

  // Remove all credit cards under testing profile so that there is no
  // suggestion is generated.
  personal_data().test_payments_data_manager().ClearAllLocalData();

  // Verify that the amount extraction is not triggered.
  EXPECT_CALL(amount_extraction_manager(), TriggerCheckoutAmountExtraction)
      .Times(0);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  EXPECT_CALL(*manager().GetPaymentsBnplManager(), NotifyOfSuggestionGeneration)
      .Times(0);
#endif

  OnAskForValuesToFill(form, form.fields()[0]);

  // Verify that no suggestion is returned.
  EXPECT_FALSE(external_delegate()->on_suggestions_returned_seen());
}

// Tests that `AmountExtractionManager` should not trigger amount extraction if
// Autofill is disabled and `BnplManager` is not notified about suggestion
// generation.
TEST_F(BrowserAutofillManagerTest,
       ShouldNotTriggerAmountExtraction_IfAutofillDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableAmountExtractionDesktop,
                            features::kAutofillEnableBuyNowPayLaterSyncing,
                            features::kAutofillEnableBuyNowPayLater},
      /*disabled_features=*/{});

  personal_data().test_payments_data_manager().AddBnplIssuer(
      test::GetTestUnlinkedBnplIssuer());
  // Set up our form data.
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form});

  // Disable Autofill.
  client().SetAutofillProfileEnabled(false);
  client().SetAutofillPaymentMethodsEnabled(false);

  // Verify that the amount extraction is not triggered.
  EXPECT_CALL(amount_extraction_manager(), TriggerCheckoutAmountExtraction)
      .Times(0);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  EXPECT_CALL(*manager().GetPaymentsBnplManager(), NotifyOfSuggestionGeneration)
      .Times(0);
#endif

  OnAskForValuesToFill(form, form.fields()[0]);

  // Verify at suggestions are not generated.
  EXPECT_FALSE(external_delegate()->on_suggestions_returned_seen());
}

struct LogAblationTestParams {
  const char* description;
  // Whether any autofillable data is stored.
  bool run_with_data_on_file = true;
  // If true, the credit card owner name field is filled with value that is not
  // a prefix of any stored credit card and then autofill suggestions are
  // queried a second time.
  bool second_query_for_suggestions_with_typed_prefix = false;
  // Whether the form should be submitted before validating the metrics.
  bool submit_form = true;
};

enum class LogAblationFormType {
  kAddress,
  kPayment,
  kMixed,  // address fields followed by payment fields
};

class BrowserAutofillManagerLogAblationTest
    : public BrowserAutofillManagerTest,
      public testing::WithParamInterface<
          std::tuple<LogAblationTestParams, LogAblationFormType>> {};

// Validate that UMA logging works correctly for ablation studies.
INSTANTIATE_TEST_SUITE_P(
    All,
    BrowserAutofillManagerLogAblationTest,
    testing::Combine(
        testing::Values(
            // Test that if autofillable data is stored and the ablation is
            // enabled, we record metrics as expected.
            LogAblationTestParams{.description = "Having data to fill"},
            // Test that if NO autofillable data is stored and the ablation is
            // enabled, we record "UnconditionalAblation" metrics but no
            // "ConditionalAblation" metrics. The latter only recoded on the
            // condition that we have data to fill.
            LogAblationTestParams{.description = "Having NO data to fill",
                                  .run_with_data_on_file = false},
            // In this test we trigger the OnAskForValuesToFill() call twice. By
            // the second time the user has typed a value that is not the prefix
            // of any existing autofill data. This means that autofill would not
            // create any suggestions. We still want to consider this a
            // conditional ablation (the condition to have fillable data on file
            // is met).
            LogAblationTestParams{
                .description = "Typed unknown prefix",
                .second_query_for_suggestions_with_typed_prefix = false},
            // Test that the right events are recorded in case the user
            // interacts with a form but does not submit it.
            LogAblationTestParams{.description = "No form submission",
                                  .submit_form = false}),
        testing::Values(LogAblationFormType::kAddress,
                        LogAblationFormType::kPayment,
                        LogAblationFormType::kMixed)));

TEST_P(BrowserAutofillManagerLogAblationTest, TestLogging) {
  const LogAblationTestParams& params = std::get<0>(GetParam());
  LogAblationFormType form_type = std::get<1>(GetParam());

  SCOPED_TRACE(testing::Message() << params.description << " Form type: "
                                  << static_cast<int>(form_type));

  if (!params.run_with_data_on_file) {
    personal_data().payments_data_manager().ClearAllServerDataForTesting();
    personal_data().test_payments_data_manager().ClearAllLocalData();
    personal_data().test_address_data_manager().ClearProfiles();
  }

  base::test::ScopedFeatureList scoped_feature_list;
  DisableAutofillViaAblation(scoped_feature_list, /*for_addresses=*/true,
                             /*for_credit_cards=*/true);
  base::HistogramTester histogram_tester;

  // Set up our form data. In the kMixed case the form will contain the fields
  // of an address form followed by the fields of fields of a payment form. The
  // triggering for autofill suggestions will happen on an address field in this
  // case.
  FormData form;
  if (form_type == LogAblationFormType::kAddress ||
      form_type == LogAblationFormType::kMixed) {
    form = CreateTestAddressFormData();
  }
  if (form_type == LogAblationFormType::kPayment ||
      form_type == LogAblationFormType::kMixed) {
    AppendTestCreditCardFormData(&form, /*is_https=*/true,
                                 /*use_month_type=*/false);
  }
  FormsSeen({form});

  // Simulate retrieving autofill suggestions with the first field as a trigger
  // script. This should emit signals that lead to recorded metrics later on.
  FormFieldData& field = test_api(form).field(0);
  OnAskForValuesToFill(form, field);

  // Simulate user typing into field (due to the ablation we would not fill).
  field.set_value(u"Unknown User");
  manager().OnTextFieldValueChanged(form, field.global_id(),
                                    base::TimeTicks::Now());

  if (params.second_query_for_suggestions_with_typed_prefix) {
    // Do another lookup. We won't have any suggestions because they would not
    // be compatible with the "Unknown User" username.
    OnAskForValuesToFill(form, field);
  }

  // Advance time and possibly submit the form.
  base::TimeDelta time_delta = base::Seconds(42);
  FastForwardBy(time_delta);
  if (params.submit_form) {
    FormSubmitted(form);
  }

  // Flush FormEventLoggers.
  test_api(client().GetAutofillDriverFactory()).Reset(driver());

  // Validate the recorded metrics.
  std::string form_type_str = (form_type == LogAblationFormType::kAddress ||
                               form_type == LogAblationFormType::kMixed)
                                  ? "Address"
                                  : "CreditCard";

  // If data was on file, we expect conditional ablation metrics.
  if (params.run_with_data_on_file) {
    histogram_tester.ExpectUniqueSample(
        "Autofill.Ablation.FormSubmissionAfterInteraction." + form_type_str +
            ".ConditionalAblation",
        /*sample=*/params.submit_form ? 1 : 0,
        /*expected_bucket_count=*/1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.Ablation.FormSubmissionAfterInteraction." + form_type_str +
            ".ConditionalAblation",
        /*expected_count=*/0);
  }
  // Only if data was on file an a submission happened, we can record the
  // duration from interaction to submission.
  if (params.run_with_data_on_file && params.submit_form) {
    histogram_tester.ExpectUniqueTimeSample(
        "Autofill.Ablation.FillDurationSinceInteraction." + form_type_str +
            ".ConditionalAblation",
        time_delta,
        /*expected_bucket_count=*/1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.Ablation.FillDurationSinceInteraction." + form_type_str +
            ".ConditionalAblation",
        /*expected_count=*/0);
  }
  // The unconditional ablation metrics should always be logged as this the
  // ablation study is always enabled.
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ablation.FormSubmissionAfterInteraction." + form_type_str +
          ".UnconditionalAblation",
      /*sample=*/params.submit_form ? 1 : 0,
      /*expected_bucket_count=*/1);
  // Expect a time from interaction to submission the form was submitted.
  if (params.submit_form) {
    histogram_tester.ExpectUniqueTimeSample(
        "Autofill.Ablation.FillDurationSinceInteraction." + form_type_str +
            ".UnconditionalAblation",
        time_delta,
        /*expected_bucket_count=*/1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.Ablation.FillDurationSinceInteraction." + form_type_str +
            ".UnconditionalAblation",
        /*expected_count=*/0);
  }

  // Ensure that no metrics are recorded for the complementary form type.
  // I.e. if the trigger element is of an address form, the credit card form
  // should have no metrics.
  std::string complementary_form_type_str =
      (form_type == LogAblationFormType::kAddress ||
       form_type == LogAblationFormType::kMixed)
          ? "CreditCard"
          : "Address";
  for (const char* metric :
       {"FormSubmissionAfterInteraction", "FillDurationSinceInteraction"}) {
    for (const char* ablation_type :
         {"UnconditionalAblation", "ConditionalAblation"}) {
      histogram_tester.ExpectTotalCount(
          base::StrCat({"Autofill.Ablation.", metric, ".",
                        complementary_form_type_str.c_str(), ".",
                        ablation_type}),
          /*expected_count=*/0);
    }
  }
}

// Ensures that if autofill is disabled but the password manager is enabled,
// Autofill still performs a lookup to the server.
TEST_F(BrowserAutofillManagerTest,
       OnFormsSeen_AutofillDisabledPasswordManagerEnabled) {
  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  client().SetAutofillPaymentMethodsEnabled(false);
  client().SetAutofillProfileEnabled(false);
  // If the password manager is enabled, that's enough to parse the form.
  EXPECT_CALL(crowdsourcing_manager(), StartQueryRequest).Times(AnyNumber());
  EXPECT_CALL(
      crowdsourcing_manager(),
      StartQueryRequest(
          ElementsAre(FormStructureHasRendererId(form.renderer_id())), _, _));
  FormsSeen({form});
}

// Test that we return normal Autofill suggestions when trying to autofill
// already filled forms.
TEST_F(BrowserAutofillManagerTest, GetFieldSuggestionsWhenFormIsAutofilled) {
  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});

  // Mark one of the fields as filled.
  test_api(form).field(2).set_is_autofilled(true);
  OnAskForValuesToFill(form, form.fields()[0]);
  // Test that we sent the right values to the external delegate.
  external_delegate()->CheckSuggestions(
      form.fields()[0].global_id(),
      {Suggestion("Charles", "123 Apple St.", kAddressEntryIcon,
                  SuggestionType::kAddressEntry),
       Suggestion("Elvis", "3734 Elvis Presley Blvd.", kAddressEntryIcon,
                  SuggestionType::kAddressEntry),
       CreateSeparator(), CreateManageAddressesSuggestion()});
}

// The method `GetPrefixMatchedProfiles` prevents
// that Android users see values that would override already filled fields
// due to the narrow surface and a missing preview.
#if !BUILDFLAG(IS_ANDROID)
// Test that we do not return duplicate values drawn from multiple profiles when
// filling an already filled field.
TEST_F(BrowserAutofillManagerTest, GetFieldSuggestionsWithDuplicateValues) {
  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});

  // |profile| will be owned by the mock PersonalDataManager.
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile, "Elvis", "", "", "", "", "", "", "", "", "",
                       "", "");
  profile.set_guid(MakeGuid(101));
  personal_data().address_data_manager().AddProfile(profile);

  FormStructure* form_structure =
      manager().FindCachedFormById(form.global_id());
  ASSERT_TRUE(form_structure);

  FormFieldData& field = test_api(form).field(0);
  AutofillField* autofill_field = form_structure->field(0);
  ASSERT_TRUE(autofill_field);
  ASSERT_TRUE(field.global_id() == autofill_field->global_id());
  field.set_is_autofilled(true);
  autofill_field->set_autofilled_type(autofill_field->Type().GetStorableType());
  field.set_value(u"Elvis");
  OnAskForValuesToFill(form, field);
  // Test that we sent the right values to the external delegate.
  external_delegate()->CheckSuggestions(
      field.global_id(),
      {Suggestion("Elvis", "", Suggestion::Icon::kAccount,
                  SuggestionType::kAddressFieldByFieldFilling),
       Suggestion("Charles", "", Suggestion::Icon::kAccount,
                  SuggestionType::kAddressFieldByFieldFilling),
       CreateSeparator(), CreateUndoOrClearFormSuggestion(),
       CreateManageAddressesSuggestion()});
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Tests that we return email profile suggestions values
// when the email field with username autocomplete attribute exist.
TEST_F(BrowserAutofillManagerTest,
       GetProfileSuggestions_ForEmailFieldWithUserNameAutocomplete) {
  // Set up our form data.
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));

  struct {
    const char* const label;
    const char* const name;
    size_t max_length;
    const char* const autocomplete_attribute;
  } test_fields[] = {{"First Name", "firstname", 30, "given-name"},
                     {"Last Name", "lastname", 30, "family-name"},
                     {"Email", "email", 30, "username"},
                     {"Password", "password", 30, "new-password"}};

  for (const auto& test_field : test_fields) {
    FormControlType field_type = strcmp(test_field.name, "password") == 0
                                     ? FormControlType::kInputPassword
                                     : FormControlType::kInputText;
    test_api(form).Append(CreateTestFormField(
        test_field.label, test_field.name, "", field_type,
        test_field.autocomplete_attribute, test_field.max_length));
  }

  FormsSeen({form});

  personal_data().test_address_data_manager().ClearProfiles();
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile.set_guid(MakeGuid(103));
  profile.SetRawInfo(NAME_FULL, u"Natty Bumppo");
  profile.SetRawInfo(EMAIL_ADDRESS, u"test@example.com");
  personal_data().address_data_manager().AddProfile(profile);

  OnAskForValuesToFill(form, form.fields()[2]);
  external_delegate()->CheckSuggestions(
      form.fields()[2].global_id(),
      {Suggestion("test@example.com", "Natty Bumppo", Suggestion::Icon::kEmail,
                  SuggestionType::kAddressEntry),
       CreateSeparator(), CreateManageAddressesSuggestion()});
}

// Tests that when focusing on an autofilled field, the user gets field-by-field
// filling suggestions without prefix matching.
TEST_F(BrowserAutofillManagerTest, GetProfileSuggestions_FieldSwapping) {
  FormData form =
      test::GetFormData({.fields = {{.role = NAME_FULL,
                                     .value = u"Full Name",
                                     .autocomplete_attribute = "name",
                                     .is_autofilled = true},
                                    {.role = ADDRESS_HOME_COUNTRY,
                                     .value = u"Country",
                                     .autocomplete_attribute = "country",
                                     .is_autofilled = true}}});
  FormsSeen({form});
  manager()
      .GetAutofillField(form.global_id(), form.fields()[0].global_id())
      ->set_autofilled_type(NAME_FULL);
  personal_data().test_address_data_manager().ClearProfiles();
  personal_data().test_address_data_manager().AddProfile(
      test::GetFullProfile());

  OnAskForValuesToFill(form, form.fields()[0]);
  external_delegate()->CheckSuggestions(
      form.fields()[0].global_id(),
      {Suggestion("John H. Doe", std::vector<std::vector<Suggestion::Text>>{},
                  Suggestion::Icon::kAccount,
                  SuggestionType::kAddressFieldByFieldFilling),
       CreateSeparator(), CreateUndoOrClearFormSuggestion(),
       CreateManageAddressesSuggestion()});
}

// Tests that fields with unrecognized autocomplete attribute don't contribute
// to key metrics.
TEST_F(BrowserAutofillManagerTest, AutocompleteUnrecognizedFields_KeyMetrics) {
  // Create an address form where field 1 has an unrecognized autocomplete
  // attribute.
  FormData form = CreateTestAddressFormData();
  ASSERT_GE(form.fields().size(), 2u);
  test_api(form).field(1).set_parsed_autocomplete(
      AutocompleteParsingResult{.field_type = HtmlFieldType::kUnrecognized});

  // Interact with an ac != unrecognized field: Expect key metrics to be
  // emitted. Note that "interacting" means querying suggestions, usually
  // caused by clicking into a field.
  {
    FormsSeen({form});
    OnAskForValuesToFill(form, form.fields()[0]);
    FormSubmitted(form);

    base::HistogramTester histogram_tester;
    test_api(client().GetAutofillDriverFactory()).Reset(driver());
    histogram_tester.ExpectTotalCount(
        "Autofill.KeyMetrics.FillingAssistance.Address", 1);
  }

  // Interact with an ac = unrecognized field: Expect no key metric to be
  // emitted.
  {
    FormsSeen({form});
    OnAskForValuesToFill(form, form.fields()[1]);
    FormSubmitted(form);

    base::HistogramTester histogram_tester;
    test_api(client().GetAutofillDriverFactory()).Reset(driver());
    histogram_tester.ExpectTotalCount(
        "Autofill.KeyMetrics.FillingAssistance.Address", 0);
  }
}

TEST_F(BrowserAutofillManagerTest,
       OnCreditCardFetchedSuccessfully_LocalCreditCard) {
  const CreditCard local_card = test::GetCreditCard();
  EXPECT_CALL(cc_access_manager(), FetchCreditCard)
      .WillOnce(base::test::RunOnceCallback<1>(local_card));
  EXPECT_CALL(payments_client(), OnCardDataAvailable).Times(0);

  FormData form = CreateTestCreditCardFormData(/*is_https=*/true,
                                               /*use_month_type=*/false);
  FormsSeen({form});
  manager().FillOrPreviewForm(mojom::ActionPersistence::kFill, form,
                              form.fields().front().global_id(), &local_card,
                              AutofillTriggerSource::kPopup);
  EXPECT_THAT(test_api(form_data_importer()).fetched_card_instrument_id(),
              testing::Optional(local_card.instrument_id()));
}

TEST_F(BrowserAutofillManagerTest,
       OnCreditCardFetchedSuccessfully_ServerCreditCard) {
  const CreditCard server_card = test::GetMaskedServerCard();
  EXPECT_CALL(cc_access_manager(), FetchCreditCard)
      .WillOnce(base::test::RunOnceCallback<1>(server_card));
  EXPECT_CALL(payments_client(), OnCardDataAvailable).Times(0);

  FormData form = CreateTestCreditCardFormData(/*is_https=*/true,
                                               /*use_month_type=*/false);
  FormsSeen({form});
  manager().FillOrPreviewForm(mojom::ActionPersistence::kFill, form,
                              form.fields().front().global_id(), &server_card,
                              AutofillTriggerSource::kPopup);
  EXPECT_THAT(test_api(form_data_importer()).fetched_card_instrument_id(),
              testing::Optional(server_card.instrument_id()));
}

TEST_F(BrowserAutofillManagerTest,
       OnCreditCardFetchedSuccessfully_VirtualCreditCard) {
  const CreditCard filled_card = test::WithCvc(test::GetVirtualCard());
  using Options = FilledCardInformationBubbleOptions;
  EXPECT_CALL(cc_access_manager(), FetchCreditCard)
      .WillOnce(base::test::RunOnceCallback<1>(filled_card));
  EXPECT_CALL(
      payments_client(),
      OnCardDataAvailable(
          AllOf(Field(&Options::masked_card_name,
                      filled_card.CardNameForAutofillDisplay()),
                Field(&Options::masked_card_number_last_four,
                      filled_card.ObfuscatedNumberWithVisibleLastFourDigits()),
                Field(&Options::cvc, filled_card.cvc()),
                Field(&Options::filled_card, filled_card))));

  FormData form = CreateTestCreditCardFormData(/*is_https=*/true,
                                               /*use_month_type=*/false);
  FormsSeen({form});
  manager().FillOrPreviewForm(mojom::ActionPersistence::kFill, form,
                              form.fields().front().global_id(), &filled_card,
                              AutofillTriggerSource::kPopup);
  EXPECT_THAT(test_api(form_data_importer()).fetched_card_instrument_id(),
              testing::Optional(filled_card.instrument_id()));
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
TEST_F(BrowserAutofillManagerTest, FillOrPreviewForm_CreditCard_Bnpl) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableBuyNowPayLaterSyncing);

  CreditCard bnpl_virtual_card = test::GetVirtualCard();
  bnpl_virtual_card.set_issuer_id(
      autofill::ConvertToBnplIssuerIdString(BnplIssuer::IssuerId::kBnplAffirm));
  bnpl_virtual_card.set_is_bnpl_card(/*is_bnpl_card=*/true);

  TestPaymentsDataManager& test_paydm = static_cast<TestPaymentsDataManager&>(
      payments_client().GetPaymentsDataManager());
  test_paydm.AddBnplIssuer(test::GetTestLinkedBnplIssuer());

  prefs::SetAutofillBnplEnabled(client().GetPrefs(), true);

  EXPECT_CALL(cc_access_manager(), FetchCreditCard).Times(0);

  using Options = FilledCardInformationBubbleOptions;
  EXPECT_CALL(
      payments_client(),
      OnCardDataAvailable(AllOf(
          Field(&Options::masked_card_name,
                bnpl_virtual_card.CardNameForAutofillDisplay()),
          Field(&Options::masked_card_number_last_four,
                bnpl_virtual_card.ObfuscatedNumberWithVisibleLastFourDigits()),
          Field(&Options::cvc, bnpl_virtual_card.cvc()),
          Field(&Options::filled_card, bnpl_virtual_card))));

  FormData form = CreateTestCreditCardFormData(/*is_https=*/true,
                                               /*use_month_type=*/false);
  FormsSeen({form});
  manager().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, form, form.fields().front().global_id(),
      &bnpl_virtual_card, AutofillTriggerSource::kPopup);
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

TEST_F(BrowserAutofillManagerTest,
       OnCreditCardFetchedSuccessfully_CardInfoRetrievalEnrolledCard) {
  CreditCard filled_card = test::WithCvc(test::GetFullServerCard());
  filled_card.set_card_info_retrieval_enrollment_state(
      CreditCard::CardInfoRetrievalEnrollmentState::kRetrievalEnrolled);
  using Options = FilledCardInformationBubbleOptions;
  EXPECT_CALL(cc_access_manager(), FetchCreditCard)
      .WillOnce(base::test::RunOnceCallback<1>(filled_card));
  EXPECT_CALL(
      payments_client(),
      OnCardDataAvailable(
          AllOf(Field(&Options::masked_card_name,
                      filled_card.CardNameForAutofillDisplay()),
                Field(&Options::masked_card_number_last_four,
                      filled_card.ObfuscatedNumberWithVisibleLastFourDigits()),
                Field(&Options::cvc, filled_card.cvc()),
                Field(&Options::filled_card, filled_card))));

  FormData form = CreateTestCreditCardFormData(/*is_https=*/true,
                                               /*use_month_type=*/false);
  FormsSeen({form});
  CreditCard card = test::GetMaskedServerCard();
  manager().FillOrPreviewForm(mojom::ActionPersistence::kFill, form,
                              form.fields().front().global_id(), &card,
                              AutofillTriggerSource::kPopup);
  EXPECT_THAT(test_api(form_data_importer()).fetched_card_instrument_id(),
              testing::Optional(filled_card.instrument_id()));
}

// BNPL suggestion is limited to Windows, macOS, Linux, and ChromeOS.
// Therefore, the system will only check for supported BNPL issuers on these
// platforms.
// TODO(crbug.com/401370610): Update when BNPL is launched to other platform.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
// Tests that, in the BNPL flow, a VCN is not fetched again due to prior
// authorization.
TEST_F(BrowserAutofillManagerTest,
       ShouldFetchCreditCard_VcnNotFetchedForSupportedBnplIssuer) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillEnableBuyNowPayLaterSyncing);
  BnplIssuer issuer = test::GetTestLinkedBnplIssuer();
  CreditCard credit_card = test::GetVirtualCard();
  credit_card.set_issuer_id(
      autofill::ConvertToBnplIssuerIdString(issuer.issuer_id()));
  credit_card.set_is_bnpl_card(/*is_bnpl_card=*/true);
  test_api(client().GetPersonalDataManager().payments_data_manager())
      .AddBnplIssuer(issuer);

  EXPECT_CALL(cc_access_manager(), FetchCreditCard).Times(0);

  FormData form = CreateTestCreditCardFormData(/*is_https=*/true,
                                               /*use_month_type=*/false);
  FormsSeen({form});
  manager().FillOrPreviewForm(mojom::ActionPersistence::kFill, form,
                              form.fields().front().global_id(), &credit_card,
                              AutofillTriggerSource::kPopup);
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

// Test that the importing logic is called on form submit.
TEST_F(BrowserAutofillManagerTest, FormSubmitted_FormDataImporter) {
  TestAddressDataManager& adm = personal_data().test_address_data_manager();
  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});

  // Fill the form.
  FormData response_data =
      FillAutofillFormDataAndGetResults(form, form.fields()[0], MakeGuid(1));
  ExpectFilledAddressFormElvis(response_data, false);
  AutofillProfile filled_profile = *adm.GetProfileByGUID(MakeGuid(1));

  // Remove the filled profile and simulate form submission. Since the
  // `personal_data()`'s auto accept imports for testing is enabled, expect
  // that the profile is imported again.
  adm.ClearProfiles();
  ASSERT_TRUE(adm.GetProfiles().empty());
  FormSubmitted(response_data);
  // Since the imported profile has a random GUID, AutofillProfile::operator==
  // cannot be used.
  ASSERT_EQ(adm.GetProfiles().size(), 1u);
  EXPECT_TRUE(adm.GetProfiles()[0]->Compare(filled_profile));
}

// Test that the user perception of autofill for address filling survey is
// triggered after a form submission.
TEST_F(BrowserAutofillManagerTest,
       UserPerceptionOfAddressAutofillSurvey_MinFormSizeReached_TriggerSurvey) {
  base::test::ScopedFeatureList enabled_features(
      features::kAutofillAddressUserPerceptionSurvey);
  // Set up a form with 4 fields (minimum form size to trigger a survey) and
  // fill them. The specific field types do not matter.
  const size_t n_fields = 4;
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FIRST, .autocomplete_attribute = "given-name"},
                  {.role = NAME_LAST, .autocomplete_attribute = "family-name"},
                  {.role = ADDRESS_HOME_LINE1,
                   .autocomplete_attribute = "address-line1"},
                  {.role = ADDRESS_HOME_LINE2,
                   .autocomplete_attribute = "address-line2"}}});
  FormsSeen({form});
  // Fill the form.
  FormData response_data =
      FillAutofillFormDataAndGetResults(form, form.fields()[0], MakeGuid(1));
  const std::map<std::string, std::string> expected_field_filling_stats_data = {
      {"Accepted fields", base::NumberToString(n_fields)},
      {"Corrected to same type", "0"},
      {"Corrected to a different type", "0"},
      {"Corrected to an unknown type", "0"},
      {"Corrected to empty", "0"},
      {"Manually filled to same type", "0"},
      {"Manually filled to a different type", "0"},
      {"Manually filled to an unknown type", "0"},
      {"Total corrected", "0"},
      {"Total filled", base::NumberToString(n_fields)},
      {"Total unfilled", "0"},
      {"Total manually filled", "0"},
      {"Total number of fields", base::NumberToString(n_fields)}};

  EXPECT_CALL(client(),
              TriggerUserPerceptionOfAutofillSurvey(
                  FillingProduct::kAddress, expected_field_filling_stats_data));
  EXPECT_CALL(client(), TriggerUserPerceptionOfAutofillSurvey(
                            FillingProduct::kCreditCard, _))
      .Times(0);

  // Simulate form submission.
  FormSubmitted(response_data);
}

TEST_F(
    BrowserAutofillManagerTest,
    UserPerceptionOfAutofillSurvey_MinFormSizeNotReached_DoNotTriggerSurvey) {
  base::test::ScopedFeatureList enabled_features(
      features::kAutofillAddressUserPerceptionSurvey);
  // Set up a form with only one field and fill it.
  FormData form =
      test::GetFormData({.fields = {{.role = NAME_FIRST,
                                     .autocomplete_attribute = "given-name"}}});
  FormsSeen({form});
  // Fill the form.
  FormData response_data =
      FillAutofillFormDataAndGetResults(form, form.fields()[0], MakeGuid(1));

  EXPECT_CALL(client(), TriggerUserPerceptionOfAutofillSurvey).Times(0);

  // Simulate form submission.
  FormSubmitted(response_data);
}

// Test that the user perception of autofill for credit card filling survey is
// triggered after a form submission.
TEST_F(BrowserAutofillManagerTest,
       UserPerceptionOfCreditCardAutofillSurvey_TriggerSurvey) {
  base::test::ScopedFeatureList enabled_features(
      features::kAutofillCreditCardUserPerceptionSurvey);
  constexpr size_t n_fields = 3;
  // Set up a CC form.
  FormData form;
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  form.set_fields({CreateTestFormField("Name on Card", "nameoncard", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Card Number", "cardnumber", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Expiration date", "exp_date", "",
                                       FormControlType::kInputText)});

  // Notify BrowserAutofillManager of the form.
  FormsSeen({form});

  // Fill the form.
  FormData response_data = FillAutofillFormDataAndGetResults(
      form, *form.fields().begin(), MakeGuid(4));

  const std::map<std::string, std::string> expected_field_filling_stats_data = {
      {"Accepted fields", base::NumberToString(n_fields)},
      {"Corrected to same type", "0"},
      {"Corrected to a different type", "0"},
      {"Corrected to an unknown type", "0"},
      {"Corrected to empty", "0"},
      {"Manually filled to same type", "0"},
      {"Manually filled to a different type", "0"},
      {"Manually filled to an unknown type", "0"},
      {"Total corrected", "0"},
      {"Total filled", base::NumberToString(n_fields)},
      {"Total unfilled", "0"},
      {"Total manually filled", "0"},
      {"Total number of fields", base::NumberToString(n_fields)}};

  EXPECT_CALL(client(), TriggerUserPerceptionOfAutofillSurvey(
                            FillingProduct::kCreditCard,
                            expected_field_filling_stats_data));
  EXPECT_CALL(client(),
              TriggerUserPerceptionOfAutofillSurvey(
                  FillingProduct::kAddress, expected_field_filling_stats_data))
      .Times(0);

  // Simulate form submission.
  FormSubmitted(response_data);
}

// Test the field log events at the form submission.
// TODO(crbug.com/40100455): Move those tests out of this file.
class BrowserAutofillManagerWithLogEventsTest
    : public BrowserAutofillManagerTest {
 protected:
  BrowserAutofillManagerWithLogEventsTest() {
    scoped_features_.InitAndEnableFeatureWithParameters(
        features::kAutofillLogUKMEventsWithSamplingOnSession,
        {{features::kAutofillLogUKMEventsWithSamplingOnSessionRate.name,
          "100"}});
  }

  std::vector<AutofillField::FieldLogEventType> ToFieldTypeEvents(
      const AutofillField& field,
      size_t field_signature_rank = 1) {
    std::vector<AutofillField::FieldLogEventType> expected_events;
    expected_events.push_back(HeuristicPredictionFieldLogEvent{
        .field_type = field.heuristic_type(),
        .heuristic_source = GetActiveHeuristicSource(),
        .is_active_heuristic_source = true,
        .rank_in_field_signature_group = field_signature_rank,
    });
    // Rationalization.
    expected_events.push_back(RationalizationFieldLogEvent{
        .field_type = field.heuristic_type(),
        .section_id = 1,
        .type_changed = false,
    });
    return expected_events;
  }

  // n = 1 means the first instance.
  template <class T>
  const T* FindNthEventOfType(
      const std::vector<AutofillField::FieldLogEventType>& events,
      size_t n) {
    // |count| represents the number of events of type T having been seen so
    // far.
    size_t count = 0;
    for (const auto& event : events) {
      if (const T* log_event = std::get_if<T>(&event)) {
        ++count;
        if (count == n) {
          return log_event;
        }
      }
    }
    return nullptr;
  }

  template <class T>
  size_t CountEventOfType(
      const std::vector<AutofillField::FieldLogEventType>& events) {
    return std::ranges::count_if(events, [](const auto& event) {
      return std::holds_alternative<T>(event);
    });
  }

  template <class T>
  const T* FindFirstEventOfType(
      const std::vector<AutofillField::FieldLogEventType>& events) {
    return FindNthEventOfType<T>(events, 1);
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
};

// Test that we record TriggerFillFieldLogEvent for the field we click to show
// the autofill suggestion and FillFieldLogEvent for every field in the form.
TEST_F(BrowserAutofillManagerWithLogEventsTest, LogEventsAtFormSubmitted) {
  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});

  // Fill the form.
  FormData response_data =
      FillAutofillFormDataAndGetResults(form, form.fields()[0], MakeGuid(1));
  ExpectFilledAddressFormElvis(response_data, false);

  // Simulate form submission.
  FormSubmitted(response_data);

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  ASSERT_TRUE(manager().GetCachedFormAndField(
      form.global_id(), form.fields().front().global_id(), &form_structure,
      &autofill_field));
  ASSERT_TRUE(form_structure);

  const std::vector<AutofillField::FieldLogEventType> focus_field_log_events =
      autofill_field->field_log_events();
  ASSERT_EQ(u"First Name", autofill_field->parseable_label());
  const TriggerFillFieldLogEvent* trigger_fill_field_log_event =
      FindFirstEventOfType<TriggerFillFieldLogEvent>(focus_field_log_events);
  ASSERT_TRUE(trigger_fill_field_log_event);

  for (const auto& autofill_field_ptr : *form_structure) {
    SCOPED_TRACE(autofill_field_ptr->parseable_label());
    std::vector<AutofillField::FieldLogEventType> expected_events =
        ToFieldTypeEvents(*autofill_field_ptr);

    if (autofill_field_ptr->parseable_label() == u"First Name") {
      // The "First Name" field is the trigger field, so it contains the
      // TriggerFillFieldLogEvent followed by a FillFieldLogEvent.
      expected_events.push_back(TriggerFillFieldLogEvent{
          .fill_event_id = trigger_fill_field_log_event->fill_event_id,
          .data_type = FillDataType::kAutofillProfile,
          .associated_country_code = "US",
          .timestamp = base::Time::Now()});
    }
    // All filled fields share the same expected FillFieldLogEvent.
    // The first TriggerFillFieldLogEvent determines the fill_event_id for
    // all following FillFieldLogEvents.
    expected_events.push_back(FillFieldLogEvent{
        .fill_event_id = trigger_fill_field_log_event->fill_event_id,
        .had_value_before_filling = OptionalBoolean::kFalse,
        .autofill_skipped_status = FieldFillingSkipReason::kNotSkipped,
        .was_autofilled_before_security_policy = OptionalBoolean::kTrue,
        .had_value_after_filling = OptionalBoolean::kTrue,
        .filling_prevented_by_iframe_security_policy = OptionalBoolean::kFalse,
    });
    EXPECT_THAT(autofill_field_ptr->field_log_events(),
                ArrayEquals(expected_events));
  }
}

// Test that we record FillFieldLogEvents correctly after autofill when the
// field has nothing to fill or the field contains a user typed value already.
TEST_F(BrowserAutofillManagerWithLogEventsTest,
       LogEventsFillPartlyManuallyFilledForm) {
  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});

  // Michael will be overridden with Elvis because Autofill is triggered from
  // the first field.
  test_api(form).field(0).set_value(u"Michael");
  test_api(form).field(0).set_properties_mask(
      form.fields()[0].properties_mask() | kUserTyped);

  // Jackson will be preserved, only override the first field.
  test_api(form).field(2).set_value(u"Jackson");
  test_api(form).field(2).set_properties_mask(
      form.fields()[2].properties_mask() | kUserTyped);

  // Fill the address data.
  TestAddressFillData address_fill_data(
      "Buddy", "Aaron", "Holly", "3734 Elvis Presley Blvd.", "Apt. 10",
      "Memphis", "Tennessee", "38116", "United States", "US", /*phone=*/"",
      /*email=*/"", "RCA");
  AutofillProfile profile1 = FillDataToAutofillProfile(address_fill_data);
  profile1.set_guid(MakeGuid(100));
  personal_data().address_data_manager().AddProfile(profile1);
  FormData response_data = FillAutofillFormDataAndGetResults(
      form, *form.fields().begin(), MakeGuid(100));

  TestAddressFillData expected_address_fill_data = address_fill_data;
  expected_address_fill_data.last = "Jackson";
  ExpectFilledForm(response_data, expected_address_fill_data,
                   /*card_fill_data=*/std::nullopt);

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  ASSERT_TRUE(manager().GetCachedFormAndField(
      form.global_id(), form.fields().front().global_id(), &form_structure,
      &autofill_field));
  ASSERT_TRUE(form_structure);

  const std::vector<AutofillField::FieldLogEventType>& focus_field_log_events =
      autofill_field->field_log_events();
  ASSERT_EQ(u"First Name", autofill_field->parseable_label());
  const TriggerFillFieldLogEvent* trigger_fill_field_log_event =
      FindFirstEventOfType<TriggerFillFieldLogEvent>(focus_field_log_events);
  ASSERT_TRUE(trigger_fill_field_log_event);

  // The first TriggerFillFieldLogEvent determines the fill_event_id for
  // all following FillFieldLogEvents.
  FillEventId fill_event_id = trigger_fill_field_log_event->fill_event_id;
  for (const auto& autofill_field_ptr : *form_structure) {
    SCOPED_TRACE(autofill_field_ptr->parseable_label());
    std::vector<AutofillField::FieldLogEventType> expected_events =
        ToFieldTypeEvents(*autofill_field_ptr);

    if (autofill_field_ptr->parseable_label() == u"First Name") {
      // The "First Name" field is the trigger field, so it contains the
      // TriggerFillFieldLogEvent followed by a FillFieldLogEvent.
      expected_events.push_back(
          TriggerFillFieldLogEvent{.fill_event_id = fill_event_id,
                                   .data_type = FillDataType::kAutofillProfile,
                                   .associated_country_code = "US",
                                   .timestamp = base::Time::Now()});
      expected_events.push_back(FillFieldLogEvent{
          .fill_event_id = fill_event_id,
          .had_value_before_filling = OptionalBoolean::kTrue,
          .autofill_skipped_status = FieldFillingSkipReason::kNotSkipped,
          .was_autofilled_before_security_policy = OptionalBoolean::kTrue,
          .had_value_after_filling = OptionalBoolean::kTrue,
          .filling_prevented_by_iframe_security_policy =
              OptionalBoolean::kFalse,
      });
    } else if (autofill_field_ptr->parseable_label() == u"Phone Number" ||
               autofill_field_ptr->parseable_label() == u"Email") {
      // Not filled because the address profile contained no data to fill.
      expected_events.push_back(FillFieldLogEvent{
          .fill_event_id = fill_event_id,
          .had_value_before_filling = OptionalBoolean::kFalse,
          .autofill_skipped_status = FieldFillingSkipReason::kNoValueToFill,
          .was_autofilled_before_security_policy = OptionalBoolean::kFalse,
          .had_value_after_filling = OptionalBoolean::kFalse,
          .filling_prevented_by_iframe_security_policy =
              OptionalBoolean::kUndefined,
      });
    } else if (autofill_field_ptr->parseable_label() == u"Last Name") {
      // Not filled because the field contained a user typed value already.
      expected_events.push_back(FillFieldLogEvent{
          .fill_event_id = fill_event_id,
          .had_value_before_filling = OptionalBoolean::kTrue,
          .autofill_skipped_status = FieldFillingSkipReason::kUserFilledFields,
          .was_autofilled_before_security_policy = OptionalBoolean::kFalse,
          .had_value_after_filling = OptionalBoolean::kTrue,
          .filling_prevented_by_iframe_security_policy =
              OptionalBoolean::kUndefined,
      });
    } else {
      expected_events.push_back(FillFieldLogEvent{
          .fill_event_id = fill_event_id,
          .had_value_before_filling = OptionalBoolean::kFalse,
          .autofill_skipped_status = FieldFillingSkipReason::kNotSkipped,
          .was_autofilled_before_security_policy = OptionalBoolean::kTrue,
          .had_value_after_filling = OptionalBoolean::kTrue,
          .filling_prevented_by_iframe_security_policy =
              OptionalBoolean::kFalse,
      });
    }
    EXPECT_THAT(autofill_field_ptr->field_log_events(),
                ArrayEquals(expected_events));
  }
}

// Test that we record FillFieldLogEvents after filling a form twice, the first
// time some field values are missing when autofilling.
TEST_F(BrowserAutofillManagerWithLogEventsTest, LogEventsAtRefillForm) {
  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // First fill the address data which does not have email and phone number.
  TestAddressFillData address_fill_data(
      "Buddy", "Aaron", "Holly", "3734 Elvis Presley Blvd.", "Apt. 10",
      "Memphis", "Tennessee", "38116", "United States", "US", /*phone=*/"",
      /*email=*/"", "RCA");
  AutofillProfile profile1 = FillDataToAutofillProfile(address_fill_data);
  profile1.set_guid(MakeGuid(100));
  personal_data().address_data_manager().AddProfile(profile1);
  FormData response_data = FillAutofillFormDataAndGetResults(
      form, *form.fields().begin(), MakeGuid(100));

  TestAddressFillData expected_address_fill_data = address_fill_data;
  ExpectFilledForm(response_data, expected_address_fill_data,
                   /*card_fill_data=*/std::nullopt);

  // Refill the address data with all the field values.
  response_data = FillAutofillFormDataAndGetResults(
      response_data, *response_data.fields().begin(), MakeGuid(1));

  expected_address_fill_data.first = "Elvis";
  expected_address_fill_data.phone = "2345678901";
  expected_address_fill_data.email = "theking@gmail.com";
  ExpectFilledForm(response_data, expected_address_fill_data,
                   /*card_fill_data=*/std::nullopt);

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  ASSERT_TRUE(manager().GetCachedFormAndField(
      form.global_id(), form.fields().front().global_id(), &form_structure,
      &autofill_field));
  ASSERT_TRUE(form_structure);

  const std::vector<AutofillField::FieldLogEventType>& focus_field_log_events =
      autofill_field->field_log_events();
  ASSERT_EQ(u"First Name", autofill_field->parseable_label());
  const TriggerFillFieldLogEvent* trigger_fill_field_log_event1 =
      FindNthEventOfType<TriggerFillFieldLogEvent>(focus_field_log_events, 1);
  ASSERT_TRUE(trigger_fill_field_log_event1);
  const TriggerFillFieldLogEvent* trigger_fill_field_log_event2 =
      FindNthEventOfType<TriggerFillFieldLogEvent>(focus_field_log_events, 2);
  ASSERT_TRUE(trigger_fill_field_log_event2);

  // The first TriggerFillFieldLogEvent determines the fill_event_id for
  // all following FillFieldLogEvents.
  FillFieldLogEvent expected_fill_field_log_event1{
      .fill_event_id = trigger_fill_field_log_event1->fill_event_id,
      .had_value_before_filling = OptionalBoolean::kFalse,
      .autofill_skipped_status = FieldFillingSkipReason::kNotSkipped,
      .was_autofilled_before_security_policy = OptionalBoolean::kTrue,
      .had_value_after_filling = OptionalBoolean::kTrue,
      .filling_prevented_by_iframe_security_policy = OptionalBoolean::kFalse,
  };

  for (const auto& autofill_field_ptr : *form_structure) {
    SCOPED_TRACE(autofill_field_ptr->parseable_label());
    std::vector<AutofillField::FieldLogEventType> expected_events =
        ToFieldTypeEvents(*autofill_field_ptr);

    if (autofill_field_ptr->parseable_label() == u"First Name") {
      // The "First Name" field is the trigger field, so it contains the
      // TriggerFillFieldLogEvent followed by a FillFieldLogEvent.
      expected_events.push_back(TriggerFillFieldLogEvent{
          .fill_event_id = trigger_fill_field_log_event1->fill_event_id,
          .data_type = FillDataType::kAutofillProfile,
          .associated_country_code = "US",
          .timestamp = base::Time::Now()});
      expected_events.push_back(expected_fill_field_log_event1);
      expected_events.push_back(TriggerFillFieldLogEvent{
          .fill_event_id = trigger_fill_field_log_event2->fill_event_id,
          .data_type = FillDataType::kAutofillProfile,
          .associated_country_code = "US",
          .timestamp = base::Time::Now()});
      expected_events.push_back(FillFieldLogEvent{
          .fill_event_id = trigger_fill_field_log_event2->fill_event_id,
          .had_value_before_filling = OptionalBoolean::kTrue,
          .autofill_skipped_status = FieldFillingSkipReason::kNotSkipped,
          .was_autofilled_before_security_policy = OptionalBoolean::kTrue,
          .had_value_after_filling = OptionalBoolean::kTrue,
          .filling_prevented_by_iframe_security_policy =
              OptionalBoolean::kFalse});
    } else if (autofill_field_ptr->parseable_label() == u"Phone Number" ||
               autofill_field_ptr->parseable_label() == u"Email") {
      FillFieldLogEvent expected_event = expected_fill_field_log_event1;
      expected_event.was_autofilled_before_security_policy =
          OptionalBoolean::kFalse;
      expected_event.had_value_after_filling = OptionalBoolean::kFalse;
      expected_event.filling_prevented_by_iframe_security_policy =
          OptionalBoolean::kUndefined;
      expected_event.autofill_skipped_status =
          FieldFillingSkipReason::kNoValueToFill;
      expected_events.push_back(expected_event);
      expected_events.push_back(FillFieldLogEvent{
          .fill_event_id = trigger_fill_field_log_event2->fill_event_id,
          .had_value_before_filling = OptionalBoolean::kFalse,
          .autofill_skipped_status = FieldFillingSkipReason::kNotSkipped,
          .was_autofilled_before_security_policy = OptionalBoolean::kTrue,
          .had_value_after_filling = OptionalBoolean::kTrue,
          .filling_prevented_by_iframe_security_policy =
              OptionalBoolean::kFalse});
    } else {
      expected_events.push_back(expected_fill_field_log_event1);
      expected_events.push_back(FillFieldLogEvent{
          .fill_event_id = trigger_fill_field_log_event2->fill_event_id,
          .had_value_before_filling = OptionalBoolean::kTrue,
          .autofill_skipped_status = FieldFillingSkipReason::kAlreadyAutofilled,
          .was_autofilled_before_security_policy = OptionalBoolean::kFalse,
          .had_value_after_filling = OptionalBoolean::kTrue,
          .filling_prevented_by_iframe_security_policy =
              OptionalBoolean::kUndefined});
    }
    EXPECT_THAT(autofill_field_ptr->field_log_events(),
                ArrayEquals(expected_events));
  }
}

// Test that we record user typing log event correctly after autofill.
TEST_F(BrowserAutofillManagerWithLogEventsTest, LogEventsAtUserTypingInField) {
  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Fill the form.
  FormData response_data =
      FillAutofillFormDataAndGetResults(form, form.fields()[0], MakeGuid(1));
  ExpectFilledAddressFormElvis(response_data, false);

  FormFieldData& field = test_api(form).field(0);
  // Simulate editing the first field.
  field.set_value(u"Michael");
  manager().OnTextFieldValueChanged(form, field.global_id(),
                                    base::TimeTicks::Now());

  // Simulate form submission.
  FormSubmitted(response_data);

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  ASSERT_TRUE(manager().GetCachedFormAndField(
      form.global_id(), form.fields().front().global_id(), &form_structure,
      &autofill_field));
  ASSERT_TRUE(form_structure);

  const std::vector<AutofillField::FieldLogEventType>& focus_field_log_events =
      autofill_field->field_log_events();
  ASSERT_EQ(u"First Name", autofill_field->parseable_label());
  const TriggerFillFieldLogEvent* trigger_fill_field_log_event =
      FindFirstEventOfType<TriggerFillFieldLogEvent>(focus_field_log_events);
  ASSERT_TRUE(trigger_fill_field_log_event);

  // All filled fields share the same expected FillFieldLogEvent.
  // The first TriggerFillFieldLogEvent determines the fill_event_id for
  // all following FillFieldLogEvents.
  FillFieldLogEvent expected_fill_field_log_event{
      .fill_event_id = trigger_fill_field_log_event->fill_event_id,
      .had_value_before_filling = OptionalBoolean::kFalse,
      .autofill_skipped_status = FieldFillingSkipReason::kNotSkipped,
      .was_autofilled_before_security_policy = OptionalBoolean::kTrue,
      .had_value_after_filling = OptionalBoolean::kTrue,
      .filling_prevented_by_iframe_security_policy = OptionalBoolean::kFalse,
  };

  for (const auto& autofill_field_ptr : *form_structure) {
    SCOPED_TRACE(autofill_field_ptr->parseable_label());
    std::vector<AutofillField::FieldLogEventType> expected_events =
        ToFieldTypeEvents(*autofill_field_ptr);

    if (autofill_field_ptr->parseable_label() == u"First Name") {
      // The "First Name" field is the trigger field, so it contains the
      // TriggerFillFieldLogEvent followed by a FillFieldLogEvent.
      expected_events.push_back(TriggerFillFieldLogEvent{
          .fill_event_id = trigger_fill_field_log_event->fill_event_id,
          .data_type = FillDataType::kAutofillProfile,
          .associated_country_code = "US",
          .timestamp = base::Time::Now()});
      expected_events.push_back(expected_fill_field_log_event);
      expected_events.push_back(TypingFieldLogEvent{
          .has_value_after_typing = OptionalBoolean::kTrue,
      });
    } else {
      expected_events.push_back(expected_fill_field_log_event);
    }
    EXPECT_THAT(autofill_field_ptr->field_log_events(),
                ArrayEquals(expected_events));
  }
}

// Test that we record field log events correctly when the user touches to fill
// and fills the credit card form with a suggestion.
TEST_F(BrowserAutofillManagerWithLogEventsTest,
       LogEventsAutofillSuggestionsOrTouchToFill) {
  FormData form = CreateTestCreditCardFormData(/*is_https=*/true,
                                               /*use_month_type=*/false);
  FormsSeen({form});
  const FormFieldData& field = form.fields()[0];

  // Touch the field of "Name on Card" and autofill suggestion is shown.
  EXPECT_CALL(touch_to_fill_delegate(), TryToShowTouchToFill)
      .WillOnce(Return(false));
  TryToShowTouchToFill(form, field, /*form_element_was_clicked=*/true);
  EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());

  // Fill the form by triggering the suggestion from "Name on Card" field.
  FormData response_data = FillAutofillFormDataAndGetResults(
      form, *form.fields().begin(), MakeGuid(4));
  ExpectFilledCreditCardFormElvis(response_data, /*has_address_fields=*/false);

  // Simulate form submission.
  FormSubmitted(response_data);

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  ASSERT_TRUE(manager().GetCachedFormAndField(
      form.global_id(), form.fields().front().global_id(), &form_structure,
      &autofill_field));
  ASSERT_TRUE(form_structure);

  const std::vector<AutofillField::FieldLogEventType>& focus_field_log_events =
      autofill_field->field_log_events();
  ASSERT_EQ(u"Name on Card", autofill_field->parseable_label());
  const TriggerFillFieldLogEvent* trigger_fill_field_log_event =
      FindFirstEventOfType<TriggerFillFieldLogEvent>(focus_field_log_events);
  ASSERT_TRUE(trigger_fill_field_log_event);

  // All filled fields share the same expected FillFieldLogEvent.
  // The first TriggerFillFieldLogEvent determines the fill_event_id for
  // all following FillFieldLogEvents.
  FillFieldLogEvent expected_fill_field_log_event{
      .fill_event_id = trigger_fill_field_log_event->fill_event_id,
      .had_value_before_filling = OptionalBoolean::kFalse,
      .autofill_skipped_status = FieldFillingSkipReason::kNotSkipped,
      .was_autofilled_before_security_policy = OptionalBoolean::kTrue,
      .had_value_after_filling = OptionalBoolean::kTrue,
      .filling_prevented_by_iframe_security_policy = OptionalBoolean::kFalse,
  };

  for (const auto& autofill_field_ptr : *form_structure) {
    SCOPED_TRACE(autofill_field_ptr->parseable_label());
    std::vector<AutofillField::FieldLogEventType> expected_events =
        ToFieldTypeEvents(*autofill_field_ptr);

    if (autofill_field_ptr->parseable_label() == u"Name on Card") {
      // The "Name on Card" field gets focus and shows a suggestion so it
      // contains the AskForValuesToFillFieldLogEvent.
      expected_events.push_back(AskForValuesToFillFieldLogEvent{
          .has_suggestion = OptionalBoolean::kTrue,
          .suggestion_is_shown = OptionalBoolean::kTrue,
      });
      expected_events.push_back(TriggerFillFieldLogEvent{
          .fill_event_id = trigger_fill_field_log_event->fill_event_id,
          .data_type = FillDataType::kCreditCard,
          .associated_country_code = "",
          .timestamp = base::Time::Now()});
      // The "Name on Card" field is the trigger field, so it contains the
      // TriggerFillFieldLogEvent followed by a FillFieldLogEvent.
      expected_events.push_back(expected_fill_field_log_event);
    } else if (autofill_field_ptr->parseable_label() == u"CVC") {
      // CVC field is not autofilled.
      expected_events.push_back(FillFieldLogEvent{
          .fill_event_id = trigger_fill_field_log_event->fill_event_id,
          .had_value_before_filling = OptionalBoolean::kFalse,
          .autofill_skipped_status = FieldFillingSkipReason::kNoValueToFill,
          .was_autofilled_before_security_policy = OptionalBoolean::kFalse,
          .had_value_after_filling = OptionalBoolean::kFalse,
          .filling_prevented_by_iframe_security_policy =
              OptionalBoolean::kUndefined,
      });
    } else {
      expected_events.push_back(expected_fill_field_log_event);
    }
    EXPECT_THAT(autofill_field_ptr->field_log_events(),
                ArrayEquals(expected_events));
  }
}

// Test that we record AutocompleteAttributeFieldLogEvent for the fields with
// autocomplete attributes in the form.
TEST_F(BrowserAutofillManagerWithLogEventsTest,
       LogEventsOnAutocompleteAttributeField) {
  // Set up our form data.
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  form.set_fields(
      {CreateTestFormField("First Name", "firstname", "",
                           FormControlType::kInputText, "given-name"),
       CreateTestFormField("Last Name", "lastname", "",
                           FormControlType::kInputText, "family-name"),
       // Set no autocomplete attribute for the middle name.
       CreateTestFormField("Middle name", "middle", "",
                           FormControlType::kInputText, ""),
       // Set an unrecognized autocomplete attribute for the last name.
       CreateTestFormField("Email", "email", "", FormControlType::kInputText,
                           "unrecognized")});

  // Simulate having seen this form on page load.
  auto form_structure_instance = std::make_unique<FormStructure>(form);
  FormStructure* form_structure = form_structure_instance.get();
  form_structure->DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr);
  manager().AddSeenFormStructure(std::move(form_structure_instance));

  // Simulate form submission.
  FormSubmitted(form);

  for (const auto& autofill_field_ptr : *form_structure) {
    SCOPED_TRACE(autofill_field_ptr->parseable_label());
    std::vector<AutofillField::FieldLogEventType> expected_events =
        ToFieldTypeEvents(*autofill_field_ptr);
    if (autofill_field_ptr->parseable_label() != u"Middle name") {
      expected_events.insert(expected_events.begin(),
                             AutocompleteAttributeFieldLogEvent{
                                 .html_type = autofill_field_ptr->html_type(),
                                 .html_mode = HtmlFieldMode::kNone,
                                 .rank_in_field_signature_group = 1,
                             });
    }
    EXPECT_THAT(autofill_field_ptr->field_log_events(),
                ArrayEquals(expected_events));
  }
}

// Test that we record field log events correctly for autofill crowdsourced
// server prediction.
TEST_F(BrowserAutofillManagerWithLogEventsTest,
       LogEventsParseQueryResponseServerPrediction) {
  // Set up our form data.
  FormData form;
  form.set_host_frame(test::MakeLocalFrameToken());
  form.set_renderer_id(test::MakeFormRendererId());
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  form.set_fields(
      {CreateTestFormField(/*label=*/"Name", /*name=*/"name", /*value=*/"",
                           /*type=*/FormControlType::kInputText),
       CreateTestFormField(/*label=*/"Street", /*name=*/"Street", /*value=*/"",
                           /*type=*/FormControlType::kInputText),
       CreateTestFormField(/*label=*/"City", /*name=*/"city", /*value=*/"",
                           /*type=*/FormControlType::kInputText),
       CreateTestFormField(/*label=*/"State", /*name=*/"state", /*value=*/"",
                           /*type=*/FormControlType::kInputText),
       CreateTestFormField(/*label=*/"Postal Code", /*name=*/"zipcode",
                           /*value=*/"",
                           /*type=*/FormControlType::kInputText)});

  // Simulate having seen this form on page load.
  auto form_structure_instance = std::make_unique<FormStructure>(form);
  FormStructure* form_structure = form_structure_instance.get();
  form_structure->DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr);
  manager().AddSeenFormStructure(std::move(form_structure_instance));

  // Make API response with suggestions.
  AutofillQueryResponse response;
  AutofillQueryResponse::FormSuggestion* form_suggestion;
  // Set suggestions for form.
  form_suggestion = response.add_form_suggestions();
  AddFieldPredictionsToForm(
      form.fields()[0],
      {CreateFieldPrediction(NAME_FIRST,
                             FieldPrediction::SOURCE_AUTOFILL_DEFAULT),
       CreateFieldPrediction(USERNAME,
                             FieldPrediction::SOURCE_PASSWORDS_DEFAULT)},
      form_suggestion);
  AddFieldPredictionsToForm(
      form.fields()[1],
      {CreateFieldPrediction(ADDRESS_HOME_LINE1,
                             FieldPrediction::SOURCE_OVERRIDE)},
      form_suggestion);
  autofill::test::AddFieldPredictionToForm(form.fields()[2], ADDRESS_HOME_CITY,
                                           form_suggestion);
  autofill::test::AddFieldPredictionToForm(form.fields()[3], ADDRESS_HOME_STATE,
                                           form_suggestion);
  autofill::test::AddFieldPredictionToForm(form.fields()[4], ADDRESS_HOME_ZIP,
                                           form_suggestion);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));
  // Query autofill server for the field type prediction.
  test_api(manager()).OnLoadedServerPredictions(
      base::Base64Encode(response_string),
      test::GetEncodedSignatures(*form_structure));
  std::vector<FieldType> types{NAME_FIRST, ADDRESS_HOME_LINE1,
                               ADDRESS_HOME_CITY, ADDRESS_HOME_STATE,
                               ADDRESS_HOME_ZIP};
  for (size_t i = 0; i < types.size(); ++i) {
    EXPECT_EQ(types[i], form_structure->field(i)->Type().GetStorableType());
  }

  // Simulate form submission.
  FormSubmitted(form);

  for (const auto& autofill_field_ptr : *form_structure) {
    SCOPED_TRACE(autofill_field_ptr->parseable_label());
    std::vector<AutofillField::FieldLogEventType> expected_events =
        ToFieldTypeEvents(*autofill_field_ptr);
    // The autofill server applies two predictions on the "Name" field.
    std::optional<FieldType> server_type2 =
        autofill_field_ptr->parseable_label() == u"Name"
            ? std::optional<FieldType>(USERNAME)
            : std::nullopt;
    FieldPrediction::Source prediction_source2 =
        autofill_field_ptr->parseable_label() == u"Name"
            ? FieldPrediction::SOURCE_PASSWORDS_DEFAULT
            : FieldPrediction::SOURCE_UNSPECIFIED;
    // The server prediction overrides the type predicted by local heuristic on
    // the field of label "Street".
    bool server_type_prediction_is_override =
        autofill_field_ptr->parseable_label() == u"Street" ? true : false;
    expected_events.push_back(ServerPredictionFieldLogEvent{
        .server_type1 = autofill_field_ptr->server_type(),
        .prediction_source1 =
            autofill_field_ptr->server_predictions()[0].source(),
        .server_type2 = server_type2,
        .prediction_source2 = prediction_source2,
        .server_type_prediction_is_override =
            server_type_prediction_is_override,
        .rank_in_field_signature_group = 1,
    });
    // Rationalization.
    expected_events.push_back(RationalizationFieldLogEvent{
        .field_type = autofill_field_ptr->server_type(),
        .section_id = 1,
        .type_changed = false,
    });
    EXPECT_THAT(autofill_field_ptr->field_log_events(),
                ArrayEquals(expected_events));
  }
}

// Test that we record field log events correctly for rationalization when
// there are two address fields.
TEST_F(BrowserAutofillManagerWithLogEventsTest,
       LogEventsRationalizationTwoAddresses) {
  // Set up our form data.
  FormData form;
  form.set_host_frame(test::MakeLocalFrameToken());
  form.set_renderer_id(test::MakeFormRendererId());
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  form.set_fields(
      {CreateTestFormField(/*label=*/"Full Name", /*name=*/"fullName",
                           /*value=*/"", /*type=*/FormControlType::kInputText),
       CreateTestFormField(/*label=*/"Address", /*name=*/"address",
                           /*value=*/"",
                           /*type=*/FormControlType::kInputText),
       CreateTestFormField(/*label=*/"Address", /*name=*/"address",
                           /*value=*/"",
                           /*type=*/FormControlType::kInputText),
       CreateTestFormField(/*label=*/"City", /*name=*/"city", /*value=*/"",
                           /*type=*/FormControlType::kInputText)});

  // Simulate having seen this form on page load.
  auto form_structure_instance = std::make_unique<FormStructure>(form);
  FormStructure* form_structure = form_structure_instance.get();
  form_structure->DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr);
  manager().AddSeenFormStructure(std::move(form_structure_instance));

  // Make API response with suggestions.
  AutofillQueryResponse response;
  AutofillQueryResponse::FormSuggestion* form_suggestion;
  // Set suggestions for form.
  form_suggestion = response.add_form_suggestions();
  std::vector<FieldType> server_types{NAME_FULL, ADDRESS_HOME_STREET_ADDRESS,
                                      ADDRESS_HOME_STREET_ADDRESS,
                                      ADDRESS_HOME_CITY};
  for (size_t i = 0; i < server_types.size(); ++i) {
    autofill::test::AddFieldPredictionToForm(form.fields()[i], server_types[i],
                                             form_suggestion);
  }

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));
  // Query autofill server for the field type prediction.
  test_api(manager()).OnLoadedServerPredictions(
      base::Base64Encode(response_string),
      test::GetEncodedSignatures(*form_structure));
  std::vector<FieldType> overall_types{NAME_FULL, ADDRESS_HOME_LINE1,
                                       ADDRESS_HOME_LINE2, ADDRESS_HOME_CITY};
  for (size_t i = 0; i < server_types.size(); ++i) {
    EXPECT_EQ(overall_types[i],
              form_structure->field(i)->Type().GetStorableType());
  }

  // Simulate form submission.
  FormSubmitted(form);

  for (const auto& autofill_field_ptr : *form_structure) {
    SCOPED_TRACE(autofill_field_ptr->parseable_label());
    size_t field_signature_rank =
        autofill_field_ptr->heuristic_type() == ADDRESS_HOME_LINE2 ? 2 : 1;
    std::vector<AutofillField::FieldLogEventType> expected_events =
        ToFieldTypeEvents(*autofill_field_ptr, field_signature_rank);
    expected_events.push_back(ServerPredictionFieldLogEvent{
        .server_type1 = autofill_field_ptr->server_type(),
        .prediction_source1 =
            autofill_field_ptr->server_predictions()[0].source(),
        .server_type2 = std::nullopt,
        .prediction_source2 = FieldPrediction::SOURCE_UNSPECIFIED,
        .server_type_prediction_is_override = false,
        .rank_in_field_signature_group = field_signature_rank,
    });
    // Rationalization.
    bool type_changed =
        autofill_field_ptr->parseable_label() == u"Address" ? true : false;
    expected_events.push_back(RationalizationFieldLogEvent{
        .field_type = autofill_field_ptr->Type().GetStorableType(),
        .section_id = 1,
        .type_changed = type_changed,
    });
    EXPECT_THAT(autofill_field_ptr->field_log_events(),
                ArrayEquals(expected_events));
  }
}

// Test that we record field log events correctly for the single field form
// with the IBAN field.
TEST_F(BrowserAutofillManagerWithLogEventsTest, LogIBANField) {
  FormData form = CreateTestIbanFormData();
  FormsSeen({form});

  manager().FillOrPreviewField(
      mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
      form, form.fields().front(), u"CH93 0076 2011 6238 5295 7",
      SuggestionType::kIbanEntry, IBAN_VALUE);
  FormSubmitted(form);

  const std::vector<AutofillField::FieldLogEventType>& fill_field_log_events =
      manager()
          .GetAutofillField(form.global_id(), form.fields()[0].global_id())
          ->field_log_events();
  ASSERT_EQ(CountEventOfType<FillFieldLogEvent>(fill_field_log_events), 1u);
  EXPECT_THAT(
      *FindFirstEventOfType<FillFieldLogEvent>(fill_field_log_events),
      EqualsFillFieldLogEvent(FillFieldLogEvent{
          .fill_event_id = FillEventId(-1),
          .had_value_before_filling = OptionalBoolean::kTrue,
          .autofill_skipped_status = FieldFillingSkipReason::kNotSkipped,
          .was_autofilled_before_security_policy = OptionalBoolean::kTrue,
          .had_value_after_filling = OptionalBoolean::kTrue,
          .filling_prevented_by_iframe_security_policy =
              OptionalBoolean::kUndefined,
      }));
}

// Test that when Autocomplete is enabled and Autofill is disabled, form
// submissions are still received by the SingleFieldFillRouter.
TEST_F(BrowserAutofillManagerTest, FormSubmittedAutocompleteEnabled) {
  client().SetAutofillProfileEnabled(false);
  client().SetAutofillPaymentMethodsEnabled(false);

  // Set up our form data.
  FormData form = CreateTestAddressFormData();

  EXPECT_CALL(single_field_fill_router(), OnWillSubmitForm(_, _, true));
  FormSubmitted(form);
}

// Test that the value patterns metric is reported.
TEST_F(BrowserAutofillManagerTest, ValuePatternsMetric) {
  struct ValuePatternTestCase {
    const char* value;
    ValuePatternsMetric pattern;
  } kTestCases[] = {
      {"user@okaxis", ValuePatternsMetric::kUpiVpa},
      {"IT60X0542811101000000123456", ValuePatternsMetric::kIban}};
  for (const ValuePatternTestCase test_case : kTestCases) {
    // Set up our form data.
    FormData form;
    form.set_name(u"my-form");
    form.set_url(GURL("https://myform.com/form.html"));
    form.set_action(GURL("https://myform.com/submit.html"));
    form.set_fields(
        {CreateTestFormField("Some label", "my-field", test_case.value,
                             FormControlType::kInputText)});
    FormsSeen({form});

    base::HistogramTester histogram_tester;
    FormSubmitted(form);
    histogram_tester.ExpectUniqueSample("Autofill.SubmittedValuePatterns",
                                        test_case.pattern, 1);
  }
}

// Test that when Autofill is disabled, single field form fill suggestions are
// still queried as a fallback.
TEST_F(BrowserAutofillManagerTest,
       SingleFieldFillSuggestions_SomeWhenAutofillDisabled) {
  client().SetAutofillProfileEnabled(false);
  client().SetAutofillPaymentMethodsEnabled(false);

  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});

  // Expect the single field suggestion generator to be called for suggestions.
  EXPECT_CALL(merchant_promo_code_manager(), OnGetSingleFieldSuggestions);

  OnAskForValuesToFill(form, form.fields()[0]);

  // Single field form fill suggestions were returned, so we should not go
  // through the normal autofill flow.
  EXPECT_FALSE(external_delegate()->on_suggestions_returned_seen());
}

// Test that we do not query for single field form fill suggestions when there
// are Autofill suggestions available.
TEST_F(BrowserAutofillManagerTest,
       SingleFieldFillSuggestions_NoneWhenAutofillPresent) {
  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});

  // Ensure that the single field suggestions are not considered for any
  // field.
  EXPECT_CALL(merchant_promo_code_manager(), OnGetSingleFieldSuggestions)
      .Times(0);
  EXPECT_CALL(iban_manager(), OnGetSingleFieldSuggestions).Times(0);
  EXPECT_CALL(autocomplete_history_manager(), OnGetSingleFieldSuggestions)
      .Times(0);

  OnAskForValuesToFill(form, form.fields()[0]);
  // Verify that suggestions are returned.
  EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());
}

// Test that we query for single field form fill suggestions when there are no
// Autofill suggestions available.
TEST_F(BrowserAutofillManagerTest,
       SingleFieldFillSuggestions_SomeWhenAutofillEmpty) {
  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  FormFieldData& email_field = test_api(form).field(-1);
  ASSERT_EQ(email_field.name(), u"email");
  // No suggestions match "donkey".
  email_field.set_value(u"donkey");
  FormsSeen({form});

  // Single field managers are called for suggestions because Autofill
  // is empty.
  EXPECT_CALL(merchant_promo_code_manager(), OnGetSingleFieldSuggestions);

  OnAskForValuesToFill(form, email_field);
}

// Test fixture with Autofill for addresses and payment methods disabled.
class BrowserAutofillManagerTest_AutofillDisabled
    : public BrowserAutofillManagerTest {
 public:
  std::unique_ptr<MockAutofillClient> CreateAutofillClient() override {
    auto client = BrowserAutofillManagerTest::CreateAutofillClient();
    // Since we are testing a form that submits over HTTP, we also need to set
    // the main frame to HTTP in the client, otherwise mixed form warnings will
    // trigger and autofill will be disabled.
    client->set_last_committed_primary_main_frame_url(
        GURL("http://example.test"));
    client->SetAutofillProfileEnabled(false);
    client->SetAutofillPaymentMethodsEnabled(false);
    return client;
  }
};

// Test that when Autofill is disabled and the field is a credit card name
// field, single field form fill suggestions are queried.
TEST_F(BrowserAutofillManagerTest_AutofillDisabled,
       SingleFieldFillSuggestions_CreditCardNameFieldShouldAutocomplete) {
  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(/*is_https=*/false,
                                               /*use_month_type=*/false);
  FormsSeen({form});
  // The first field is "Name on card", which should autocomplete.
  FormFieldData& field = test_api(form).field(0);
  field.set_should_autocomplete(true);

  // Single field managers are called for suggestions because Autofill
  // is empty.
  EXPECT_CALL(merchant_promo_code_manager(), OnGetSingleFieldSuggestions);

  OnAskForValuesToFill(form, field);
}

// Test that when Autofill is disabled and the field is a credit card number
// field, single field form fill suggestions are not queried.
TEST_F(BrowserAutofillManagerTest_AutofillDisabled,
       SingleFieldFillSuggestions_CreditCardNumberShouldNotAutocomplete) {
  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(/*is_https=*/false,
                                               /*use_month_type=*/false);
  FormsSeen({form});
  // The second field is "Card Number", which should not autocomplete.
  FormFieldData& field = test_api(form).field(1);
  field.set_should_autocomplete(true);

  // Ensure that the single field suggestions are not considered for any
  // field.
  EXPECT_CALL(merchant_promo_code_manager(), OnGetSingleFieldSuggestions)
      .Times(0);
  EXPECT_CALL(iban_manager(), OnGetSingleFieldSuggestions).Times(0);
  EXPECT_CALL(autocomplete_history_manager(), OnGetSingleFieldSuggestions)
      .Times(0);

  OnAskForValuesToFill(form, field);
}

// Test that the situation where there are no Autofill suggestions available,
// and no single field form fill conditions were met is correctly handled. The
// single field form fill conditions were not met because autocomplete is set to
// off and the field is not recognized as a promo code field.
TEST_F(
    BrowserAutofillManagerTest,
    SingleFieldFillSuggestions_NoneWhenAutofillEmptyAndSingleFieldFillConditionsNotMet) {
  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  FormFieldData& email_field = test_api(form).field(-1);
  ASSERT_EQ(email_field.name(), u"email");
  // No suggestions match "donkey".
  email_field.set_value(u"donkey");
  email_field.set_should_autocomplete(false);
  FormsSeen({form});

  EXPECT_CALL(merchant_promo_code_manager(), OnGetSingleFieldSuggestions)
      .WillRepeatedly(Return(false));
  EXPECT_CALL(iban_manager(), OnGetSingleFieldSuggestions)
      .WillRepeatedly(Return(false));
  EXPECT_CALL(autocomplete_history_manager(), OnGetSingleFieldSuggestions)
      .WillRepeatedly(Return(false));

  OnAskForValuesToFill(form, email_field);

  // Single field form fill was not triggered, so go through the normal autofill
  // flow.
  EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());
}

// Test that the situation where no single field form fill conditions were met
// is handled correctly. The single field form fill conditions were not met
// because autocomplete is set to off and the field is not recognized as a promo
// code field.
TEST_F(BrowserAutofillManagerTest,
       SingleFieldFillSuggestions_NoneWhenSingleFieldFillConditionsNotMet) {
  client().SetAutofillProfileEnabled(false);
  client().SetAutofillPaymentMethodsEnabled(false);

  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});
  FormFieldData& field = test_api(form).field(0);
  field.set_should_autocomplete(false);

  EXPECT_CALL(merchant_promo_code_manager(), OnGetSingleFieldSuggestions)
      .WillRepeatedly(Return(false));
  EXPECT_CALL(iban_manager(), OnGetSingleFieldSuggestions)
      .WillRepeatedly(Return(false));
  EXPECT_CALL(autocomplete_history_manager(), OnGetSingleFieldSuggestions)
      .WillRepeatedly(Return(false));

  OnAskForValuesToFill(form, field);

  // Single field form fill was not triggered, so go through the normal autofill
  // flow.
  EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());
}

TEST_F(BrowserAutofillManagerTest, DestructorCancelsSingleFieldFillQueries) {
  EXPECT_CALL(single_field_fill_router(), CancelPendingQueries);
}

// Test that OnLoadedServerPredictions can obtain the FormStructure with the
// signature of the queried form from the API and apply type predictions.
// What we test here:
//  * The API response parser is used.
//  * The query can be processed with a response from the API.
TEST_F(BrowserAutofillManagerTest, OnLoadedServerPredictionsFromApi) {
  // First form on the page.
  FormData form;
  form.set_host_frame(test::MakeLocalFrameToken());
  form.set_renderer_id(test::MakeFormRendererId());
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  form.set_fields(
      {CreateTestFormField(/*label=*/"City", /*name=*/"city", /*value=*/"",
                           /*type=*/FormControlType::kInputText),
       CreateTestFormField(/*label=*/"State", /*name=*/"state", /*value=*/"",
                           /*type=*/FormControlType::kInputText),
       CreateTestFormField(/*label=*/"Postal Code", /*name=*/"zipcode",
                           /*value=*/"",
                           /*type=*/FormControlType::kInputText)});
  // Simulate having seen this form on page load.
  auto form_structure_instance = std::make_unique<FormStructure>(form);
  FormStructure* form_structure = form_structure_instance.get();
  form_structure->DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr);
  manager().AddSeenFormStructure(std::move(form_structure_instance));

  // Second form on the page.
  FormData form2;
  form2.set_host_frame(test::MakeLocalFrameToken());
  form2.set_renderer_id(test::MakeFormRendererId());
  form2.set_name(u"MyForm2");
  form2.set_url(GURL("https://myform.com/form.html"));
  form2.set_action(GURL("https://myform.com/submit.html"));
  form2.set_fields({CreateTestFormField("Last Name", "lastname", "",
                                        FormControlType::kInputText),
                    CreateTestFormField("Middle Name", "middlename", "",
                                        FormControlType::kInputText),
                    CreateTestFormField("Postal Code", "zipcode", "",
                                        FormControlType::kInputText)});
  auto form_structure_instance2 = std::make_unique<FormStructure>(form2);
  // This pointer is valid as long as autofill manager lives.
  FormStructure* form_structure2 = form_structure_instance2.get();
  form_structure2->DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr);
  manager().AddSeenFormStructure(std::move(form_structure_instance2));

  // Make API response with suggestions.
  AutofillQueryResponse response;
  AutofillQueryResponse::FormSuggestion* form_suggestion;
  // Set suggestions for form 1.
  form_suggestion = response.add_form_suggestions();
  autofill::test::AddFieldPredictionToForm(form.fields()[0], ADDRESS_HOME_CITY,
                                           form_suggestion);
  autofill::test::AddFieldPredictionToForm(form.fields()[1], ADDRESS_HOME_STATE,
                                           form_suggestion);
  autofill::test::AddFieldPredictionToForm(form.fields()[2], ADDRESS_HOME_ZIP,
                                           form_suggestion);
  // Set suggestions for form 2.
  form_suggestion = response.add_form_suggestions();
  autofill::test::AddFieldPredictionToForm(form2.fields()[0], NAME_LAST,
                                           form_suggestion);
  autofill::test::AddFieldPredictionToForm(form2.fields()[1], NAME_MIDDLE,
                                           form_suggestion);
  autofill::test::AddFieldPredictionToForm(form2.fields()[2], ADDRESS_HOME_ZIP,
                                           form_suggestion);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));
  std::vector<FormSignature> signatures =
      test::GetEncodedSignatures({form_structure, form_structure2});

  // Run method under test.
  base::HistogramTester histogram_tester;
  test_api(manager()).OnLoadedServerPredictions(
      base::Base64Encode(response_string), signatures);

  // Verify whether the relevant histograms were updated.
  histogram_tester.ExpectBucketCount("Autofill.ServerQueryResponse",
                                     AutofillMetrics::QUERY_RESPONSE_RECEIVED,
                                     1);
  histogram_tester.ExpectBucketCount("Autofill.ServerQueryResponse",
                                     AutofillMetrics::QUERY_RESPONSE_PARSED, 1);

  // We expect the server suggestions to have been applied to the first field of
  // the first form.
  EXPECT_EQ(ADDRESS_HOME_CITY,
            form_structure->field(0)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STATE,
            form_structure->field(1)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_ZIP,
            form_structure->field(2)->Type().GetStorableType());
  // We expect the server suggestions to have been applied to the second form as
  // well.
  EXPECT_EQ(NAME_LAST, form_structure2->field(0)->Type().GetStorableType());
  EXPECT_EQ(NAME_MIDDLE, form_structure2->field(1)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_ZIP,
            form_structure2->field(2)->Type().GetStorableType());
}

// Test that OnLoadedServerPredictions does not call ParseQueryResponse if the
// BrowserAutofillManager has been reset between the time the query was sent and
// the response received.
TEST_F(BrowserAutofillManagerTest, OnLoadedServerPredictions_ResetManager) {
  // Set up our form data.
  FormData form = CreateTestAddressFormData();

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |manager()|.
  auto form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr);
  std::vector<FormSignature> signatures =
      test::GetEncodedSignatures(*form_structure);
  manager().AddSeenFormStructure(std::move(form_structure));

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  form_suggestion->add_field_suggestions()->add_predictions()->set_type(3);
  for (int i = 0; i < 7; ++i) {
    form_suggestion->add_field_suggestions()->add_predictions()->set_type(0);
  }
  form_suggestion->add_field_suggestions()->add_predictions()->set_type(3);
  form_suggestion->add_field_suggestions()->add_predictions()->set_type(2);
  form_suggestion->add_field_suggestions()->add_predictions()->set_type(61);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));
  // Reset the manager (such as during a navigation).
  test_api(client().GetAutofillDriverFactory()).Reset(driver());

  base::HistogramTester histogram_tester;
  test_api(manager()).OnLoadedServerPredictions(
      base::Base64Encode(response_string), signatures);

  // Verify that FormStructure::ParseQueryResponse was NOT called.
  histogram_tester.ExpectTotalCount("Autofill.ServerQueryResponse", 0);
}

// Test that when server predictions disagree with the heuristic ones, the
// overall types and sections would be set based on the server one.
TEST_F(BrowserAutofillManagerTest, DetermineHeuristicsWithOverallPrediction) {
  // Set up our form data.
  FormData form;
  form.set_url(GURL("https://www.myform.com"));
  form.set_fields({CreateTestFormField("First Name", "firstname", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Last Name", "lastname", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Card Number", "cardnumber", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Expiration Year", "exp_year", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Expiration Month", "exp_month", "",
                                       FormControlType::kInputText)});

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |manager()|.
  FormStructure* form_structure = [&] {
    auto form_structure = std::make_unique<FormStructure>(form);
    FormStructure* ptr = form_structure.get();
    form_structure->DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr);
    manager().AddSeenFormStructure(std::move(form_structure));
    return ptr;
  }();

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  autofill::test::AddFieldPredictionToForm(
      form.fields()[0], CREDIT_CARD_NAME_FIRST, form_suggestion);
  autofill::test::AddFieldPredictionToForm(
      form.fields()[1], CREDIT_CARD_NAME_LAST, form_suggestion);
  autofill::test::AddFieldPredictionToForm(form.fields()[2], CREDIT_CARD_NUMBER,
                                           form_suggestion);
  autofill::test::AddFieldPredictionToForm(
      form.fields()[3], CREDIT_CARD_EXP_MONTH, form_suggestion);
  autofill::test::AddFieldPredictionToForm(
      form.fields()[4], CREDIT_CARD_EXP_4_DIGIT_YEAR, form_suggestion);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));
  base::HistogramTester histogram_tester;
  test_api(manager()).OnLoadedServerPredictions(
      base::Base64Encode(response_string),
      test::GetEncodedSignatures(*form_structure));
  // Verify that FormStructure::ParseQueryResponse was called (here and below).
  histogram_tester.ExpectBucketCount("Autofill.ServerQueryResponse",
                                     AutofillMetrics::QUERY_RESPONSE_RECEIVED,
                                     1);
  histogram_tester.ExpectBucketCount("Autofill.ServerQueryResponse",
                                     AutofillMetrics::QUERY_RESPONSE_PARSED, 1);

  // Since the card holder name appears as the first name + last name (rather
  // than the full name), and since they appears as the first fields of the
  // section, the heuristics detect them as the address first/last name.
  EXPECT_EQ(NAME_FIRST, form_structure->field(0)->heuristic_type());
  EXPECT_EQ(NAME_LAST, form_structure->field(1)->heuristic_type());
  EXPECT_EQ(CREDIT_CARD_NUMBER, form_structure->field(2)->heuristic_type());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, form_structure->field(3)->heuristic_type());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            form_structure->field(4)->heuristic_type());

  // We expect to see the server type as the overall type.
  EXPECT_EQ(CREDIT_CARD_NAME_FIRST,
            form_structure->field(0)->Type().GetStorableType());
  EXPECT_EQ(CREDIT_CARD_NAME_LAST,
            form_structure->field(1)->Type().GetStorableType());
  EXPECT_EQ(CREDIT_CARD_NUMBER, form_structure->field(2)->heuristic_type());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, form_structure->field(3)->heuristic_type());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            form_structure->field(4)->heuristic_type());

  // Although the heuristic types of the first two fields belongs to the address
  // section, the final fields' section should be based on the overall
  // prediction, therefore they should be grouped in one section.
  const auto section = form_structure->field(0)->section();
  EXPECT_EQ(section, form_structure->field(1)->section());
  EXPECT_EQ(section, form_structure->field(2)->section());
  EXPECT_EQ(section, form_structure->field(3)->section());
  EXPECT_EQ(section, form_structure->field(4)->section());
}

// Test that the form signature for an uploaded form always matches the form
// signature from the query.
TEST_F(BrowserAutofillManagerTest, FormSubmittedWithDifferentFields) {
  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});

  // Cache the expected form signature.
  std::string signature = FormStructure(form).FormSignatureAsStr();

  // Change the structure of the form prior to submission.
  // Websites would typically invoke JavaScript either on page load or on form
  // submit to achieve this.
  test_api(form).Remove(-1);
  FormFieldData& field = test_api(form).field(3);
  test_api(form).field(3) = form.fields()[7];
  test_api(form).field(7) = field;

  // Simulate form submission.
  FormSubmitted(form);
  EXPECT_EQ(signature, client().GetVotesUploader().submitted_form_signature());
}

// Test that we do not save form data when submitted fields contain default
// values.
TEST_F(BrowserAutofillManagerTest, FormSubmittedWithDefaultValues) {
  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  FormFieldData* addr1_field = test_api(form).FindFieldByNameForTest(u"addr1");
  ASSERT_TRUE(addr1_field != nullptr);
  addr1_field->set_value(u"Enter your address");

  FormsSeen({form});

  // Fill the form.
  FormData response_data =
      FillAutofillFormDataAndGetResults(form, *addr1_field, kElvisProfileGuid);
  // Set the address field's value back to the default value.
  test_api(response_data).field(3).set_value(u"Enter your address");

  // Simulate form submission. The profile should not be updated with the
  // meaningless default value of the street address field.
  AutofillProfile profile =
      *personal_data().address_data_manager().GetProfileByGUID(
          kElvisProfileGuid);
  profile.ClearFields({ADDRESS_HOME_STREET_ADDRESS});
  personal_data().address_data_manager().UpdateProfile(profile);
  FormSubmitted(response_data);
  EXPECT_FALSE(personal_data()
                   .address_data_manager()
                   .GetProfileByGUID(kElvisProfileGuid)
                   ->HasInfo(ADDRESS_HOME_STREET_ADDRESS));
}

void DoTestFormSubmittedControlWithDefaultValue(
    BrowserAutofillManagerTest* test,
    FormControlType form_control_type) {
  // Set up our form data.
  FormData form = CreateTestAddressFormData();

  // Convert the state field to a <select> popup, to make sure that we only
  // reject default values for text fields.
  FormFieldData* state_field = test_api(form).FindFieldByNameForTest(u"state");
  ASSERT_TRUE(state_field != nullptr);
  state_field->set_form_control_type(form_control_type);
  state_field->set_value(base::UTF8ToUTF16(GetElvisAddressFillData().state));

  test->FormsSeen({form});

  // Fill the form.
  FormData response_data = test->FillAutofillFormDataAndGetResults(
      form, form.fields()[3], kElvisProfileGuid);

  AutofillProfile profile =
      *test->personal_data().address_data_manager().GetProfileByGUID(
          kElvisProfileGuid);
  profile.ClearFields({ADDRESS_HOME_STATE});
  test->personal_data().address_data_manager().UpdateProfile(profile);
  test->FormSubmitted(response_data);
  // Expect that the profile was updated with the value of the state select.
  EXPECT_EQ(state_field->value(), test->personal_data()
                                      .address_data_manager()
                                      .GetProfileByGUID(kElvisProfileGuid)
                                      ->GetRawInfo(ADDRESS_HOME_STATE));
}

// Test that we save form data when a <select> in the form contains the
// default value.
TEST_F(BrowserAutofillManagerTest, FormSubmittedSelectWithDefaultValue) {
  DoTestFormSubmittedControlWithDefaultValue(this, FormControlType::kSelectOne);
}

void DoTestFormSubmittedNonAddressControlWithDefaultValue(
    BrowserAutofillManagerTest* test,
    FormControlType form_control_type) {
  // Set up our form data.
  FormData form = CreateTestAddressFormData();

  // Remove phone number field.
  auto phonenumber_it =
      std::ranges::find(form.fields(), u"phonenumber", &FormFieldData::name);
  ASSERT_TRUE(phonenumber_it != form.fields().end());
  test_api(form).fields().erase(phonenumber_it);

  // Insert country code and national phone number fields.
  test_api(form).Append(CreateTestFormField("Country Code", "countrycode", "1",
                                            form_control_type,
                                            "tel-country-code"));
  test_api(form).Append(CreateTestFormField("Phone Number", "phonenumber", "",
                                            FormControlType::kInputText,
                                            "tel-national"));

  test->FormsSeen({form});

  // Fill the form.
  FormData response_data = test->FillAutofillFormDataAndGetResults(
      form, form.fields()[3], kElvisProfileGuid);

  // Value of country code field should have been saved.
  AutofillProfile profile =
      *test->personal_data().address_data_manager().GetProfileByGUID(
          kElvisProfileGuid);
  profile.ClearFields({PHONE_HOME_WHOLE_NUMBER});
  test->personal_data().address_data_manager().UpdateProfile(profile);
  test->FormSubmitted(response_data);
  std::u16string formatted_phone_number =
      test->personal_data()
          .address_data_manager()
          .GetProfileByGUID(kElvisProfileGuid)
          ->GetRawInfo(PHONE_HOME_WHOLE_NUMBER);
  std::u16string phone_number_numbers_only;
  base::RemoveChars(formatted_phone_number, u"+- ", &phone_number_numbers_only);
  EXPECT_TRUE(phone_number_numbers_only.starts_with(u"1"));
}

// Test that we save form data when a non-country, non-state <select> in the
// form contains the default value.
TEST_F(BrowserAutofillManagerTest,
       FormSubmittedNonAddressSelectWithDefaultValue) {
  DoTestFormSubmittedNonAddressControlWithDefaultValue(
      this, FormControlType::kSelectOne);
}

// Tests that DeterminePossibleFieldTypesForUpload is called when a form is
// submitted.
TEST_F(BrowserAutofillManagerTest,
       DeterminePossibleFieldTypesForUpload_IsTriggered) {
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));

  std::vector<FieldTypeSet> expected_types;
  std::vector<std::u16string> expected_values;

  // These fields should all match.
  FieldTypeSet types;

  expected_values.push_back(u"Elvis");
  types.clear();
  types.insert(NAME_FIRST);
  test_api(form).Append(
      CreateTestFormField("", "1", "", FormControlType::kInputText));
  expected_types.push_back(types);

  expected_values.push_back(u"Aaron");
  types.clear();
  types.insert(NAME_MIDDLE);
  test_api(form).Append(
      CreateTestFormField("", "2", "", FormControlType::kInputText));
  expected_types.push_back(types);

  expected_values.push_back(u"A");
  types.clear();
  types.insert(NAME_MIDDLE_INITIAL);
  test_api(form).Append(
      CreateTestFormField("", "3", "", FormControlType::kInputText));
  expected_types.push_back(types);

  // Make sure the form is in the cache so that it is processed for Autofill
  // upload.
  FormsSeen({form});

  // Once the form is cached, fill the values.
  EXPECT_EQ(form.fields().size(), expected_values.size());
  for (size_t i = 0; i < expected_values.size(); i++) {
    test_api(form).field(i).set_value(expected_values[i]);
  }

  client().GetVotesUploader().set_expected_submitted_field_types(
      expected_types);
  FormSubmitted(form);
}

// Test our external delegate is called at the right time.
TEST_F(BrowserAutofillManagerTest, TestExternalDelegate) {
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});
  // Should call the delegate's OnQuery().
  OnAskForValuesToFill(form, form.fields()[0]);

  EXPECT_TRUE(external_delegate()->on_query_seen());
}

// Test that unfocusing a filled form sends an upload with types matching the
// fields.
TEST_F(BrowserAutofillManagerTest, OnTextFieldValueChangedAndUnfocus_Upload) {
  // Set up our form data (it's already filled out with user data).
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));

  std::vector<FieldTypeSet> expected_types;
  FieldTypeSet types;

  test_api(form).Append(CreateTestFormField("First Name", "firstname", "",
                                            FormControlType::kInputText));
  types.insert(NAME_FIRST);
  expected_types.push_back(types);

  test_api(form).Append(CreateTestFormField("Last Name", "lastname", "",
                                            FormControlType::kInputText));
  types.clear();
  types.insert(NAME_LAST);
  types.insert(NAME_LAST_SECOND);
  expected_types.push_back(types);

  test_api(form).Append(
      CreateTestFormField("Email", "email", "", FormControlType::kInputText));
  types.clear();
  types.insert(EMAIL_ADDRESS);
  expected_types.push_back(types);

  FormsSeen({form});

  // We will expect these types in the upload and no observed submission (the
  // callback initiated by WaitForAsyncUploadProcess checks these expectations.)
  client().GetVotesUploader().set_expected_submitted_field_types(
      expected_types);
  client().GetVotesUploader().set_expected_observed_submission(false);

  // The fields are edited after calling FormsSeen on them. This is because
  // default values are not used for upload comparisons.
  test_api(form).field(0).set_value(u"Elvis");
  test_api(form).field(1).set_value(u"Presley");
  test_api(form).field(2).set_value(u"theking@gmail.com");
  // Simulate editing a field.
  manager().OnTextFieldValueChanged(form, form.fields().front().global_id(),
                                    base::TimeTicks::Now());

  // Simulate lost of focus on the form.
  manager().OnFocusOnNonFormField();
}

// Test that navigating with a filled form sends an upload with types matching
// the fields.
TEST_F(BrowserAutofillManagerTest,
       OnTextFieldValueChangedAndNavigation_Upload) {
  // Set up our form data (it's already filled out with user data).
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));

  std::vector<FieldTypeSet> expected_types;
  FieldTypeSet types;

  test_api(form).Append(CreateTestFormField("First Name", "firstname", "",
                                            FormControlType::kInputText));
  types.insert(NAME_FIRST);
  expected_types.push_back(types);

  test_api(form).Append(CreateTestFormField("Last Name", "lastname", "",
                                            FormControlType::kInputText));
  types.clear();
  types.insert(NAME_LAST_SECOND);
  types.insert(NAME_LAST);
  expected_types.push_back(types);

  test_api(form).Append(
      CreateTestFormField("Email", "email", "", FormControlType::kInputText));
  types.clear();
  types.insert(EMAIL_ADDRESS);
  expected_types.push_back(types);

  FormsSeen({form});

  // We will expect these types in the upload and no observed submission. (the
  // callback initiated by WaitForAsyncUploadProcess checks these expectations.)
  client().GetVotesUploader().set_expected_submitted_field_types(
      expected_types);
  client().GetVotesUploader().set_expected_observed_submission(false);

  // The fields are edited after calling FormsSeen on them. This is because
  // default values are not used for upload comparisons.
  test_api(form).field(0).set_value(u"Elvis");
  test_api(form).field(1).set_value(u"Presley");
  test_api(form).field(2).set_value(u"theking@gmail.com");
  // Simulate editing a field.
  manager().OnTextFieldValueChanged(form, form.fields().front().global_id(),
                                    base::TimeTicks::Now());

  // Simulate a navigation so that the pending form is uploaded.
  test_api(client().GetAutofillDriverFactory()).Reset(driver());
}

// Test that unfocusing a filled form sends an upload with types matching the
// fields.
TEST_F(BrowserAutofillManagerTest, OnDidFillAutofillFormDataAndUnfocus_Upload) {
  // Set up our form data (empty).
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));

  std::vector<FieldTypeSet> expected_types;

  // These fields should all match.
  FieldTypeSet types;
  test_api(form).Append(CreateTestFormField("First Name", "firstname", "",
                                            FormControlType::kInputText));
  types.insert(NAME_FIRST);
  expected_types.push_back(types);

  test_api(form).Append(CreateTestFormField("Last Name", "lastname", "",
                                            FormControlType::kInputText));
  types.clear();
  types.insert(NAME_LAST_SECOND);
  types.insert(NAME_LAST);
  expected_types.push_back(types);

  test_api(form).Append(
      CreateTestFormField("Email", "email", "", FormControlType::kInputText));
  types.clear();
  types.insert(EMAIL_ADDRESS);
  expected_types.push_back(types);

  FormsSeen({form});

  // We will expect these types in the upload and no observed submission. (the
  // callback initiated by WaitForAsyncUploadProcess checks these expectations.)
  client().GetVotesUploader().set_expected_submitted_field_types(
      expected_types);
  client().GetVotesUploader().set_expected_observed_submission(false);

  // Form was autofilled with user data.
  test_api(form).field(0).set_value(u"Elvis");
  test_api(form).field(1).set_value(u"Presley");
  test_api(form).field(2).set_value(u"theking@gmail.com");
  manager().OnDidFillAutofillFormData(form, base::TimeTicks::Now());

  // Simulate lost of focus on the form.
  manager().OnFocusOnNonFormField();
}

// Test that suggestions are returned for credit card fields with an
// unrecognized autocomplete attribute.
TEST_F(BrowserAutofillManagerTest,
       GetCreditCardSuggestions_UnrecognizedAttribute) {
  // Set up the form data.
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  form.set_fields(
      {// Set a valid autocomplete attribute on the card name.
       CreateTestFormField("Name on Card", "nameoncard", "",
                           FormControlType::kInputText, "cc-name"),
       // Set no autocomplete attribute on the card number.
       CreateTestFormField("Card Number", "cardnumber", "",
                           FormControlType::kInputText),
       // Set an unrecognized autocomplete attribute on the expiration month.
       CreateTestFormField("Expiration Date", "ccmonth", "",
                           FormControlType::kInputText, "unrecognized")});
  FormsSeen({form});

  // Suggestions should be returned for the first two fields
  OnAskForValuesToFill(form, form.fields()[0]);
  EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());
  OnAskForValuesToFill(form, form.fields()[1]);
  EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());

  // Suggestions should still be returned for the third field because it is a
  // credit card field.
  OnAskForValuesToFill(form, form.fields()[2]);
  EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());
}

// Test to verify suggestions appears for forms having credit card number split
// across fields. No prefix matching is applied.
TEST_P(BrowserAutofillManagerTestForMetadataCardSuggestions,
       GetCreditCardSuggestions_ForNumberSplitAcrossFields) {
  // Set up our form data with credit card number split across fields.
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  form.set_fields({CreateTestFormField("Name on Card", "nameoncard", "",
                                       FormControlType::kInputText)});

  // Add new 4 |card_number_field|s to the |form|.
  constexpr uint64_t kMaxLength = 4;
  test_api(form).Append(CreateTestFormField("Card Number", "cardnumber_1", "",
                                            FormControlType::kInputText, "",
                                            kMaxLength));
  test_api(form).Append(CreateTestFormField(
      "", "cardnumber_2", "", FormControlType::kInputText, "", kMaxLength));
  test_api(form).Append(CreateTestFormField(
      "", "cardnumber_3", "", FormControlType::kInputText, "", kMaxLength));
  test_api(form).Append(CreateTestFormField(
      "", "cardnumber_4", "", FormControlType::kInputText, "", kMaxLength));

  test_api(form).Append(CreateTestFormField("Expiration Date", "ccmonth", "",
                                            FormControlType::kInputText));
  test_api(form).Append(
      CreateTestFormField("", "ccyear", "", FormControlType::kInputText));

  FormsSeen({form});

  // Verify whether suggestions are populated correctly for one of the middle
  // credit card number fields when filled partially.
  FormFieldData& number_field = test_api(form).field(3);
  number_field.set_value(u"901");

  // Get the suggestions for already filled credit card |number_field|.
  OnAskForValuesToFill(form, number_field);

  external_delegate()->CheckSuggestions(
      form.fields()[3].global_id(),
      {GetCardSuggestion(kVisaCard), GetCardSuggestion(kMasterCard),
       CreateSeparator(),
       CreateManageCreditCardsSuggestion(
           /*with_gpay_logo=*/false)});
}

// Test that inputs detected to be CVC inputs are forced to
// !should_autocomplete for SingleFieldFillRouter::OnWillSubmitForm.
TEST_F(BrowserAutofillManagerTest, DontSaveCvcInAutocompleteHistory) {
  FormData form_seen_by_ahm;
  EXPECT_CALL(single_field_fill_router(), OnWillSubmitForm(_, _, true))
      .WillOnce(SaveArg<0>(&form_seen_by_ahm));

  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));

  struct TestField {
    const char* label;
    const char* name;
    const char* value;
    FieldType expected_field_type;
  };
  constexpr auto test_fields = std::to_array<TestField>({
      TestField{"Card number", "1", "4234-5678-9012-3456", CREDIT_CARD_NUMBER},
      TestField{"Card verification code", "2", "123",
                CREDIT_CARD_VERIFICATION_CODE},
      TestField{"expiration date", "3", "04/2020",
                CREDIT_CARD_EXP_4_DIGIT_YEAR},
  });

  for (const auto& test_field : test_fields) {
    test_api(form).Append(CreateTestFormField(test_field.label, test_field.name,
                                              test_field.value,
                                              FormControlType::kInputText));
  }

  FormsSeen({form});
  FormSubmitted(form);

  EXPECT_EQ(form.fields().size(), form_seen_by_ahm.fields().size());
  ASSERT_EQ(test_fields.size(), form_seen_by_ahm.fields().size());
  for (size_t i = 0; i < test_fields.size(); ++i) {
    EXPECT_EQ(
        form_seen_by_ahm.fields()[i].should_autocomplete(),
        test_fields[i].expected_field_type != CREDIT_CARD_VERIFICATION_CODE);
  }
}
// Test that autofilled loyalty card fields are forced to !should_autocomplete.
TEST_F(BrowserAutofillManagerTest,
       DontSaveAutofilledLoyaltyCardsInAutocompleteHistory) {
  FormData form_seen_by_ahm;
  EXPECT_CALL(single_field_fill_router(), OnWillSubmitForm(_, _, true))
      .WillOnce(SaveArg<0>(&form_seen_by_ahm));

  // Set up form.
  FormData form = test::GetFormData({.fields = {
                                         {.role = LOYALTY_MEMBERSHIP_ID},
                                     }});
  manager().AddSeenForm(form, {LOYALTY_MEMBERSHIP_ID});
  // Mark the loyalty card field as autofilled.
  test_api(form).field(0).set_is_autofilled(true);
  test_api(form).field(0).set_value(u"LOYALTYCARDNUMBER");

  FormSubmitted(form);
  ASSERT_EQ(form.fields().size(), form_seen_by_ahm.fields().size());
  EXPECT_FALSE(test_api(form_seen_by_ahm).field(0).should_autocomplete());
}

TEST_F(BrowserAutofillManagerTest, DontOfferToSavePaymentsCard) {
  FormData form;
  PrepareForRealPanResponse(&form);

  // Manually fill out |form| so we can use it in OnFormSubmitted.
  for (auto& field : test_api(form).fields()) {
    if (field.name() == u"cardnumber") {
      field.set_value(u"4012888888881881");
    } else if (field.name() == u"nameoncard") {
      field.set_value(u"John H Dillinger");
    } else if (field.name() == u"ccmonth") {
      field.set_value(u"01");
    } else if (field.name() == u"ccyear") {
      field.set_value(u"2017");
    }
  }

  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  full_card_unmask_delegate().OnUnmaskPromptAccepted(details);
  OnDidGetRealPan(payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
                  "4012888888881881");
  FormSubmitted(form);
}

TEST_F(BrowserAutofillManagerTest, ProfileDisabledDoesNotSuggest) {
  client().SetAutofillProfileEnabled(false);

  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});

  OnAskForValuesToFill(form, form.fields().back());
  // Expect no suggestions as autofill and autocomplete are disabled for
  // addresses.
  EXPECT_FALSE(external_delegate()->on_suggestions_returned_seen());
}

TEST_F(BrowserAutofillManagerTest, CreditCardDisabledDoesNotSuggest) {
  client().SetAutofillPaymentMethodsEnabled(false);

  // Set up our form data.
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form});

  OnAskForValuesToFill(form, form.fields()[0]);
  // Expect no suggestions as autofill and autocomplete are disabled for credit
  // cards.
  EXPECT_FALSE(external_delegate()->on_suggestions_returned_seen());
}

TEST_F(BrowserAutofillManagerTest, ShouldUploadForm) {
  // Note: The enforcement of a minimum number of required fields for upload
  // is disabled by default. This tests validates both the disabled and enabled
  // scenarios.
  FormData form;
  form.set_name(u"TestForm");
  form.set_url(GURL("https://example.com/form.html"));
  form.set_action(GURL("https://example.com/submit.html"));

  // Empty Form.
  EXPECT_FALSE(manager().ShouldUploadForm(FormStructure(form)));

  // Add a field to the form.
  test_api(form).Append(
      CreateTestFormField("Name", "name", "", FormControlType::kInputText));

  EXPECT_TRUE(manager().ShouldUploadForm(FormStructure(form)));

  // Add a second field to the form.
  test_api(form).Append(
      CreateTestFormField("Email", "email", "", FormControlType::kInputText));

  EXPECT_TRUE(manager().ShouldUploadForm(FormStructure(form)));

  // Has less than 3 fields but has autocomplete attribute.
  constexpr char autocomplete[] = "given-name";
  test_api(form).field(0).set_autocomplete_attribute(autocomplete);
  test_api(form).field(0).set_parsed_autocomplete(
      ParseAutocompleteAttribute(autocomplete));

  EXPECT_TRUE(manager().ShouldUploadForm(FormStructure(form)));

  // Has more than 3 fields, no autocomplete attribute.
  test_api(form).Append(CreateTestFormField("Country", "country", "",
                                            FormControlType::kInputText, ""));
  FormStructure form_structure_3(form);
  EXPECT_TRUE(manager().ShouldUploadForm(FormStructure(form)));

  // Has more than 3 fields and at least one autocomplete attribute.
  test_api(form).field(0).set_autocomplete_attribute(autocomplete);
  test_api(form).field(0).set_parsed_autocomplete(
      ParseAutocompleteAttribute(autocomplete));
  EXPECT_TRUE(manager().ShouldUploadForm(FormStructure(form)));

  // Is off the record.
  client().set_is_off_the_record(true);
  EXPECT_FALSE(manager().ShouldUploadForm(FormStructure(form)));

  // Make sure it's reset for the next test case.
  client().set_is_off_the_record(false);
  EXPECT_TRUE(manager().ShouldUploadForm(FormStructure(form)));

  // Has one field which is appears to be a password field.
  form.set_fields({CreateTestFormField("Password", "password", "",
                                       FormControlType::kInputPassword)});

  // With min required fields disabled.
  EXPECT_TRUE(manager().ShouldUploadForm(FormStructure(form)));

  // Autofill disabled.
  client().SetAutofillProfileEnabled(false);
  client().SetAutofillPaymentMethodsEnabled(false);
  EXPECT_FALSE(manager().ShouldUploadForm(FormStructure(form)));
}

// Verify that no suggestions are shown on desktop for non credit card related
// fields if the initiating field has the "autocomplete" attribute set to off.
TEST_F(BrowserAutofillManagerTest,
       DisplaySuggestions_AutocompleteOffNotRespected_AddressField) {
  // Set up an address form.
  FormData mixed_form;
  mixed_form.set_name(u"MyForm");
  mixed_form.set_url(GURL("https://myform.com/form.html"));
  mixed_form.set_action(GURL("https://myform.com/submit.html"));
  test_api(mixed_form)
      .Append(CreateTestFormField("First name", "firstname", "",
                                  FormControlType::kInputText));
  test_api(mixed_form).field(-1).set_should_autocomplete(false);
  test_api(mixed_form)
      .Append(CreateTestFormField("Last name", "lastname", "",
                                  FormControlType::kInputText));
  test_api(mixed_form)
      .Append(CreateTestFormField("Address", "address", "",
                                  FormControlType::kInputText));
  FormsSeen({mixed_form});

  // Suggestions should be displayed on desktop for this field in all
  // circumstances.
  OnAskForValuesToFill(mixed_form, mixed_form.fields()[0]);
  EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());

  // Suggestions should always be displayed for all the other fields.
  for (size_t i = 1U; i < mixed_form.fields().size(); ++i) {
    OnAskForValuesToFill(mixed_form, mixed_form.fields()[i]);
    EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());
  }
}

// Verify that suggestions are shown on desktop for credit card related fields
// even if the initiating field has the "autocomplete" attribute set to off.
TEST_F(BrowserAutofillManagerTest,
       DisplaySuggestions_AutocompleteOff_CreditCardField) {
  // Set up a credit card form.
  FormData mixed_form;
  mixed_form.set_name(u"MyForm");
  mixed_form.set_url(GURL("https://myform.com/form.html"));
  mixed_form.set_action(GURL("https://myform.com/submit.html"));
  mixed_form.set_fields({CreateTestFormField("Name on Card", "nameoncard", "",
                                             FormControlType::kInputText)});
  test_api(mixed_form).field(-1).set_should_autocomplete(false);
  test_api(mixed_form)
      .Append(CreateTestFormField("Card Number", "cardnumber", "",
                                  FormControlType::kInputText));
  test_api(mixed_form)
      .Append(CreateTestFormField("Expiration Month", "ccexpiresmonth", "",
                                  FormControlType::kInputText));
  test_api(mixed_form).field(-1).set_should_autocomplete(false);
  FormsSeen({mixed_form});

  // Suggestions should always be displayed.
  for (const FormFieldData& mixed_form_field : mixed_form.fields()) {
    ON_CALL(merchant_promo_code_manager(), OnGetSingleFieldSuggestions)
        .WillByDefault(Return(false));
    ON_CALL(iban_manager(), OnGetSingleFieldSuggestions)
        .WillByDefault(Return(false));
    // Single field form fill suggestions being returned are directly correlated
    // to whether or not the field has autocomplete set to true or false. We
    // know autocomplete must be the single field form filler in this case due
    // to the field not having a type that would route to any of the other
    // single field form fillers.
    ON_CALL(autocomplete_history_manager(), OnGetSingleFieldSuggestions)
        .WillByDefault(Return(mixed_form_field.should_autocomplete()));
    OnAskForValuesToFill(mixed_form, mixed_form_field);

    EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());
  }
}

// Tests that a form with server only types is still autofillable if the form
// gets updated in cache.
TEST_F(BrowserAutofillManagerTest,
       DisplaySuggestionsForUpdatedServerTypedForm) {
  // Create a form with unknown heuristic fields.
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  form.set_fields({CreateTestFormField("Field 1", "field1", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Field 2", "field2", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Field 3", "field3", "",
                                       FormControlType::kInputText)});

  auto form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr);
  // Make sure the form can not be autofilled now.
  ASSERT_FALSE(std::ranges::any_of(
      form_structure->fields(),
      [](const auto& field) { return field->IsFieldFillable(); }));

  for (size_t idx = 0; idx < form_structure->field_count(); ++idx) {
    ASSERT_EQ(UNKNOWN_TYPE, form_structure->field(idx)->heuristic_type());
  }

  // Prepare and set known server fields.
  const std::vector<FieldType> heuristic_types(form.fields().size(),
                                               UNKNOWN_TYPE);
  const std::vector<FieldType> server_types{NAME_FIRST, NAME_MIDDLE, NAME_LAST};
  test_api(*form_structure).SetFieldTypes(heuristic_types, server_types);
  manager().AddSeenFormStructure(std::move(form_structure));

  // Make sure the form can be autofilled.
  for (const FormFieldData& form_field : form.fields()) {
    OnAskForValuesToFill(form, form_field);
    ASSERT_TRUE(external_delegate()->on_suggestions_returned_seen());
  }

  // Modify one of the fields in the original form.
  test_api(form).field(0).set_css_classes(form.fields()[0].css_classes() +
                                          u"a");

  // Expect the form still can be autofilled.
  for (const FormFieldData& form_field : form.fields()) {
    OnAskForValuesToFill(form, form_field);
    EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());
  }

  // Modify form action URL. This can happen on in-page navigation if the form
  // doesn't have an actual action (attribute is empty).
  form.set_action(net::AppendQueryParameter(form.action(), "arg", "value"));

  // Expect the form still can be autofilled.
  for (const FormFieldData& form_field : form.fields()) {
    OnAskForValuesToFill(form, form_field);
    EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());
  }
}

TEST_F(BrowserAutofillManagerTest, GetCreditCardSuggestions_VirtualCard) {
  personal_data().test_payments_data_manager().ClearCreditCards();
  CreditCard masked_server_card(CreditCard::RecordType::kMaskedServerCard,
                                /*server_id=*/"a123");
  test::SetCreditCardInfo(&masked_server_card, "Elvis Presley",
                          "4234567890123456",  // Visa
                          "04", "2999", "1");
  masked_server_card.SetNetworkForMaskedCard(kVisaCard);
  masked_server_card.set_guid(MakeGuid(7));
  masked_server_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  masked_server_card.SetNickname(u"nickname");
  personal_data().test_payments_data_manager().AddServerCreditCard(
      masked_server_card);

  // Set up our form data.
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form});

  // Card number field.
  OnAskForValuesToFill(form, form.fields()[1]);

  Suggestion expected_credit_card_number_suggestion =
      GetCardSuggestion(kVisaCard, /*nickname=*/"nickname");
  Suggestion expected_virtual_card_number_suggestion =
      GenerateVirtualCardSuggestionFromCreditCardSuggestion(
          expected_credit_card_number_suggestion, CREDIT_CARD_NUMBER);
  external_delegate()->CheckSuggestions(
      form.fields()[1].global_id(),
      {expected_virtual_card_number_suggestion,
       expected_credit_card_number_suggestion, CreateSeparator(),
       CreateManageCreditCardsSuggestion(
           /*with_gpay_logo=*/true)});

  // Non card number field (cardholder name field).
  OnAskForValuesToFill(form, form.fields()[0]);

  Suggestion expected_credit_card_name_suggestion = GetCardSuggestion(
      kVisaCard, /*nickname=*/"nickname", CREDIT_CARD_NAME_FULL);
  Suggestion expected_virtual_card_name_suggestion =
      GenerateVirtualCardSuggestionFromCreditCardSuggestion(
          expected_credit_card_name_suggestion, CREDIT_CARD_NAME_FULL);

  external_delegate()->CheckSuggestions(
      form.fields()[0].global_id(),
      {expected_virtual_card_name_suggestion,
       expected_credit_card_name_suggestion, CreateSeparator(),
       CreateManageCreditCardsSuggestion(
           /*with_gpay_logo=*/true)});
}

TEST_F(BrowserAutofillManagerTest,
       GetCreditCardSuggestions_VirtualCard_MetadataEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  personal_data().test_payments_data_manager().ClearCreditCards();
  CreditCard masked_server_card(CreditCard::RecordType::kMaskedServerCard,
                                /*server_id=*/"a123");
  test::SetCreditCardInfo(&masked_server_card, "Elvis Presley",
                          "4234567890123456",  // Visa
                          "04", "2999", "1");
  masked_server_card.SetNetworkForMaskedCard(kVisaCard);
  masked_server_card.set_guid(MakeGuid(7));
  masked_server_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  masked_server_card.SetNickname(u"nickname");
  personal_data().test_payments_data_manager().AddServerCreditCard(
      masked_server_card);

  // Set up our form data.
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form});

  // Card number field.
  OnAskForValuesToFill(form, form.fields()[1]);

  Suggestion credit_card_number_suggestion =
      GetCardSuggestion(kVisaCard, /*nickname=*/"nickname");
  Suggestion virtual_card_number_suggestion =
      GenerateVirtualCardSuggestionFromCreditCardSuggestion(
          credit_card_number_suggestion, CREDIT_CARD_NUMBER);
  external_delegate()->CheckSuggestions(
      form.fields()[1].global_id(),
      {virtual_card_number_suggestion, credit_card_number_suggestion,
       CreateSeparator(),
       CreateManageCreditCardsSuggestion(
           /*with_gpay_logo=*/true)});

  // Non card number field (cardholder name field).
  OnAskForValuesToFill(form, form.fields()[0]);

  Suggestion credit_card_name_suggestion = GetCardSuggestion(
      kVisaCard, /*nickname=*/"nickname", CREDIT_CARD_NAME_FULL);
  Suggestion virtual_card_name_suggestion =
      GenerateVirtualCardSuggestionFromCreditCardSuggestion(
          credit_card_name_suggestion, CREDIT_CARD_NAME_FULL);

  external_delegate()->CheckSuggestions(
      form.fields()[0].global_id(),
      {virtual_card_name_suggestion, credit_card_name_suggestion,
       CreateSeparator(),
       CreateManageCreditCardsSuggestion(
           /*with_gpay_logo=*/true)});
}

TEST_F(BrowserAutofillManagerTest,
       IbanFormProcessed_AutofillOptimizationGuidePresent) {
  FormData form_data = CreateTestIbanFormData();
  FormStructure form_structure{form_data};
  test_api(form_structure).SetFieldTypes({IBAN_VALUE}, {IBAN_VALUE});

  EXPECT_CALL(*client().GetAutofillOptimizationGuide(), OnDidParseForm)
      .Times(1);

  test_api(manager()).OnFormProcessed(form_data, form_structure);
}

TEST_F(BrowserAutofillManagerTest,
       IbanFormProcessed_AutofillOptimizationGuideNotPresent) {
  FormData form_data = CreateTestIbanFormData();
  FormStructure form_structure{form_data};
  test_api(form_structure).SetFieldTypes({IBAN_VALUE}, {IBAN_VALUE});

  // Test that form processing doesn't crash when we have an IBAN form but no
  // AutofillOptimizationGuide present.
  test_api(manager()).OnFormProcessed(form_data, form_structure);
}

TEST_F(BrowserAutofillManagerTest,
       DidShowSuggestions_LogAutocomplete_NoHappinessMetricsEmitted) {
  FormData form;
  form.set_name(u"NothingSpecial");
  form.set_fields({CreateTestFormField("Something", "something", "",
                                       FormControlType::kInputText)});
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  manager().DidShowSuggestions({Suggestion(SuggestionType::kAutocompleteEntry)},
                               form, form.fields().back().global_id(), {});
  // No Autofill logs.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(histograms,
              Not(AnyOf(HasSubstr("Autofill.FormEvents.Address"),
                        HasSubstr("Autofill.FormEvents.CreditCard"))));
}

TEST_F(BrowserAutofillManagerTest,
       DidShowSuggestions_LogAutofillAddressShownMetric) {
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  DidShowSuggestions(form);

  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // No Autocomplete or credit cards logs.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(histograms,
              Not(AnyOf(HasSubstr("Autocomplete.Events3"),
                        HasSubstr("Autofill.FormEvents.CreditCard"))));
}

TEST_F(BrowserAutofillManagerTest,
       DidShowSuggestions_LogIbanSuggestionsShownMetric) {
  FormData form = CreateTestIbanFormData();
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  manager().DidShowSuggestions({Suggestion(SuggestionType::kIbanEntry)}, form,
                               form.fields().back().global_id(), {});
  manager().DidShowSuggestions({Suggestion(SuggestionType::kIbanEntry)}, form,
                               form.fields().back().global_id(), {});

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.Iban.Suggestions"),
      BucketsAre(
          base::Bucket(
              autofill_metrics::IbanSuggestionsEvent::kIbanSuggestionsShown, 2),
          base::Bucket(
              autofill_metrics::IbanSuggestionsEvent::kIbanSuggestionsShownOnce,
              1)));
}

TEST_F(BrowserAutofillManagerTest, DidShowSuggestions_LogByType_AddressOnly) {
  // Create a form with name and address fields.
  FormData form;
  form.set_name(u"MyForm");
  form.set_button_titles({std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)});
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  form.set_main_frame_origin(
      url::Origin::Create(GURL("https://myform_root.com/form.html")));
  form.set_submission_event(SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION);
  form.set_fields({CreateTestFormField("First Name", "firstname", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Last Name", "lastname", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Address Line 1", "addr1", "",
                                       FormControlType::kInputText)});
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  DidShowSuggestions(form);

  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressOnly",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressOnly",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.FormEvents.Address.AddressPlusContact"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmail "),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.ContactOnly"),
          HasSubstr("Autofill.FormEvents.Address.PhoneOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_F(BrowserAutofillManagerTest,
       DidShowSuggestions_LogByType_AddressOnlyWithoutName) {
  // Create a form with address fields.
  FormData form;
  form.set_name(u"MyForm");
  form.set_button_titles({std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)});
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  form.set_main_frame_origin(
      url::Origin::Create(GURL("https://myform_root.com/form.html")));
  form.set_submission_event(SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION);
  form.set_fields({CreateTestFormField("Address Line 1", "addr1", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Address Line 2", "addr2", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Postal Code", "zipcode", "",
                                       FormControlType::kInputText)});
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  DidShowSuggestions(form);

  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressOnly",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressOnly",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.FormEvents.Address.AddressPlusContact"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmail "),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.ContactOnly"),
          HasSubstr("Autofill.FormEvents.Address.PhoneOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_F(BrowserAutofillManagerTest, DidShowSuggestions_LogByType_ContactOnly) {
  // Create a form with name and contact fields.
  FormData form;
  form.set_name(u"MyForm");
  form.set_button_titles({std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)});
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  form.set_main_frame_origin(
      url::Origin::Create(GURL("https://myform_root.com/form.html")));
  form.set_submission_event(SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION);
  form.set_fields({CreateTestFormField("First Name", "firstname", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Last Name", "lastname", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Email", "email", "",
                                       FormControlType::kInputEmail)});
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  DidShowSuggestions(form);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.ContactOnly",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.ContactOnly",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.FormEvents.Address.AddressPlusContact"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmail "),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressOnly"),
          HasSubstr("Autofill.FormEvents.Address.PhoneOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_F(BrowserAutofillManagerTest,
       DidShowSuggestions_LogByType_ContactOnlyWithoutName) {
  // Create a form with contact fields.
  FormData form;
  form.set_name(u"MyForm");
  form.set_button_titles({std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)});
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  form.set_main_frame_origin(
      url::Origin::Create(GURL("https://myform_root.com/form.html")));
  form.set_submission_event(SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION);
  form.set_fields(
      {CreateTestFormField("Phone Number", "phonenumber1", "",
                           FormControlType::kInputTelephone,
                           "tel-country-code"),
       CreateTestFormField("Phone Number", "phonenumber2", "",
                           FormControlType::kInputTelephone, "tel-national"),
       CreateTestFormField("Email", "email", "", FormControlType::kInputEmail,
                           "email")});
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  DidShowSuggestions(form);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.ContactOnly",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.ContactOnly",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.FormEvents.Address.AddressPlusContact"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmail "),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressOnly"),
          HasSubstr("Autofill.FormEvents.Address.PhoneOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_F(BrowserAutofillManagerTest, DidShowSuggestions_LogByType_PhoneOnly) {
  // Create a form with phone field.
  FormData form;
  form.set_name(u"MyForm");
  form.set_button_titles({std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)});
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  form.set_main_frame_origin(
      url::Origin::Create(GURL("https://myform_root.com/form.html")));
  form.set_submission_event(SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION);
  form.set_fields(
      {CreateTestFormField("Phone Number", "phonenumber", "",
                           FormControlType::kInputTelephone,
                           "tel-country-code"),
       CreateTestFormField("Phone Number", "phonenumber", "",
                           FormControlType::kInputTelephone, "tel-area-code"),
       CreateTestFormField("Phone Number", "phonenumber", "",
                           FormControlType::kInputTelephone, "tel-local")});
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  DidShowSuggestions(form);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.PhoneOnly",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.PhoneOnly",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmail"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusContact"),
          HasSubstr("Autofill.FormEvents.Address.AddressOnly"),
          HasSubstr("Autofill.FormEvents.Address.ContactOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_F(BrowserAutofillManagerTest, DidShowSuggestions_LogByType_Other) {
  // Create a form with name fields.
  FormData form;
  form.set_name(u"MyForm");
  form.set_button_titles({std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)});
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  form.set_main_frame_origin(
      url::Origin::Create(GURL("https://myform_root.com/form.html")));
  form.set_submission_event(SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION);
  form.set_fields({CreateTestFormField("First Name", "firstname", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Middle Name", "middlename", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Last Name", "lastname", "",
                                       FormControlType::kInputText)});
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  DidShowSuggestions(form);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.Other",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.Other",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.FormEvents.Address.AddressPlusContact"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmail "),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressOnly"),
          HasSubstr("Autofill.FormEvents.Address.PhoneOnly"),
          HasSubstr("Autofill.FormEvents.Address.ContactOnly"))));
}

TEST_F(BrowserAutofillManagerTest,
       DidShowSuggestions_LogByType_AddressPlusEmail) {
  // Create a form with name, address, and email fields.
  FormData form;
  form.set_name(u"MyForm");
  form.set_button_titles({std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)});
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  form.set_main_frame_origin(
      url::Origin::Create(GURL("https://myform_root.com/form.html")));
  form.set_submission_event(SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION);
  form.set_fields({CreateTestFormField("First Name", "firstname", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Last Name", "lastname", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Address Line 1", "addr1", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Email", "email", "",
                                       FormControlType::kInputEmail)});
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  DidShowSuggestions(form);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusEmail",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusEmail",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressOnly"),
          HasSubstr("Autofill.FormEvents.Address.ContactOnly"),
          HasSubstr("Autofill.FormEvents.Address.PhoneOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_F(BrowserAutofillManagerTest,
       DidShowSuggestions_LogByType_AddressPlusEmailWithoutName) {
  // Create a form with address and email fields.
  FormData form;
  form.set_name(u"MyForm");
  form.set_button_titles({std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)});
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  form.set_main_frame_origin(
      url::Origin::Create(GURL("https://myform_root.com/form.html")));
  form.set_submission_event(SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION);
  form.set_fields({CreateTestFormField("Address Line 1", "addr1", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Address Line 2", "addr2", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Email", "email", "",
                                       FormControlType::kInputEmail)});
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  DidShowSuggestions(form);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusEmail",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusEmail",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressOnly"),
          HasSubstr("Autofill.FormEvents.Address.ContactOnly"),
          HasSubstr("Autofill.FormEvents.Address.PhoneOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_F(BrowserAutofillManagerTest,
       DidShowSuggestions_LogByType_AddressPlusPhone) {
  // Create a form with name fields.
  FormData form;
  form.set_name(u"MyForm");
  form.set_button_titles({std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)});
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  form.set_main_frame_origin(
      url::Origin::Create(GURL("https://myform_root.com/form.html")));
  form.set_submission_event(SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION);
  form.set_fields({CreateTestFormField("First Name", "firstname", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Last Name", "lastname", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Address Line 1", "addr1", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Phone Number", "phonenumber", "",
                                       FormControlType::kInputTelephone)});
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  DidShowSuggestions(form);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusPhone",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusPhone",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmail "),
          HasSubstr("Autofill.FormEvents.Address.AddressOnly"),
          HasSubstr("Autofill.FormEvents.Address.ContactOnly"),
          HasSubstr("Autofill.FormEvents.Address.PhoneOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_F(BrowserAutofillManagerTest,
       DidShowSuggestions_LogByType_AddressPlusPhoneWithoutName) {
  // Create a form with name, address, and phone fields.
  FormData form;
  form.set_name(u"MyForm");
  form.set_button_titles({std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)});
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  form.set_main_frame_origin(
      url::Origin::Create(GURL("https://myform_root.com/form.html")));
  form.set_submission_event(SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION);
  form.set_fields({CreateTestFormField("Address Line 1", "addr1", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Address Line 2", "addr2", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Phone Number", "phonenumber", "",
                                       FormControlType::kInputTelephone)});
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  DidShowSuggestions(form);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusPhone",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusPhone",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmail "),
          HasSubstr("Autofill.FormEvents.Address.AddressOnly"),
          HasSubstr("Autofill.FormEvents.Address.ContactOnly"),
          HasSubstr("Autofill.FormEvents.Address.PhoneOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_F(BrowserAutofillManagerTest,
       DidShowSuggestions_LogByType_AddressPlusEmailPlusPhone) {
  // Create a form with name, address, phone, and email fields.
  FormData form;
  form.set_name(u"MyForm");
  form.set_button_titles({std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)});
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  form.set_main_frame_origin(
      url::Origin::Create(GURL("https://myform_root.com/form.html")));
  form.set_submission_event(SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION);
  form.set_fields({CreateTestFormField("First Name", "firstname", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Last Name", "lastname", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Address Line 1", "addr1", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Phone Number", "phonenumber", "",
                                       FormControlType::kInputTelephone),
                   CreateTestFormField("Email", "email", "",
                                       FormControlType::kInputEmail)});
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  DidShowSuggestions(form);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusEmailPlusPhone",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusEmailPlusPhone",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(HasSubstr("Autofill.FormEvents.Address.AddressPlusPhone"),
                HasSubstr("Autofill.FormEvents.Address.AddressPlusEmail "),
                HasSubstr("Autofill.FormEvents.Address.AddressOnly"),
                HasSubstr("Autofill.FormEvents.Address.ContactOnly"),
                HasSubstr("Autofill.FormEvents.Address.PhoneOnly"),
                HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_F(BrowserAutofillManagerTest,
       DidShowSuggestions_LogByType_AddressPlusEmailPlusPhoneWithoutName) {
  // Create a form with address, phone, and email fields.
  FormData form;
  form.set_name(u"MyForm");
  form.set_button_titles({std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)});
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  form.set_main_frame_origin(
      url::Origin::Create(GURL("https://myform_root.com/form.html")));
  form.set_submission_event(SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION);
  form.set_fields({CreateTestFormField("Address Line 1", "addr1", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Phone Number", "phonenumber", "",
                                       FormControlType::kInputTelephone),
                   CreateTestFormField("Email", "email", "",
                                       FormControlType::kInputEmail)});
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  DidShowSuggestions(form);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusEmailPlusPhone",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusEmailPlusPhone",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(HasSubstr("Autofill.FormEvents.Address.AddressPlusPhone"),
                HasSubstr("Autofill.FormEvents.Address.AddressPlusEmail "),
                HasSubstr("Autofill.FormEvents.Address.AddressOnly"),
                HasSubstr("Autofill.FormEvents.Address.ContactOnly"),
                HasSubstr("Autofill.FormEvents.Address.PhoneOnly"),
                HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_F(BrowserAutofillManagerTest,
       DidShowSuggestions_LogAutofillCreditCardShownMetric) {
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  DidShowSuggestions(form, /*field_index=*/0, SuggestionType::kCreditCardEntry);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // No Autocomplete or address logs.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(histograms, Not(AnyOf(HasSubstr("Autocomplete.Events3"),
                                    HasSubstr("Autofill.FormEvents.Address"))));
}

TEST_F(BrowserAutofillManagerTest,
       DidShowSuggestions_CreditCard_PreflightFetchingCall) {
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form});

  EXPECT_CALL(cc_access_manager(), PrepareToFetchCreditCard)
      .Times(IsCreditCardFidoAuthenticationEnabled() ? 1 : 0);
  manager().DidShowSuggestions({Suggestion(SuggestionType::kCreditCardEntry)},
                               form, form.fields()[0].global_id(), {});
}

TEST_F(BrowserAutofillManagerTest,
       DidShowSuggestions_AddessSuggestion_NoPreflightFetchingCall) {
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});

  EXPECT_CALL(cc_access_manager(), PrepareToFetchCreditCard).Times(0);
  manager().DidShowSuggestions({Suggestion(SuggestionType::kAddressEntry)},
                               form, form.fields()[0].global_id(), {});
}

TEST_F(BrowserAutofillManagerTest, PageLanguageGetsCorrectlySet) {
  FormData form = CreateTestAddressFormData();

  manager().OnFormsSeen({form}, {});
  FormStructure* parsed_form = manager().FindCachedFormById(form.global_id());

  ASSERT_TRUE(parsed_form);
  ASSERT_EQ(LanguageCode(), parsed_form->current_page_language());

  client().GetLanguageState()->SetCurrentLanguage("zh");

  manager().OnFormsSeen({form}, {});
  parsed_form = manager().FindCachedFormById(form.global_id());

  ASSERT_EQ(LanguageCode("zh"), parsed_form->current_page_language());
}

// Test language detection on frames depending on whether the frame is active or
// not.
class BrowserAutofillManagerTestPageLanguageDetection
    : public BrowserAutofillManagerTest,
      public testing::WithParamInterface<bool> {
 public:
  BrowserAutofillManagerTestPageLanguageDetection() {
    scoped_features_.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillPageLanguageDetection},
        /*disabled_features=*/{});
  }

  bool is_active() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_features_;
};

TEST_P(BrowserAutofillManagerTestPageLanguageDetection, GetsCorrectlyDetected) {
  FormData form = CreateTestAddressFormData();

  manager().OnFormsSeen({form}, {});
  FormStructure* parsed_form = manager().FindCachedFormById(form.global_id());

  ASSERT_TRUE(parsed_form);
  ASSERT_EQ(LanguageCode(), parsed_form->current_page_language());

  translate::LanguageDetectionDetails language_detection_details;
  language_detection_details.adopted_language = "hu";
  client().GetLanguageState()->SetCurrentLanguage("hu");

  driver().SetIsActive(is_active());
  manager().OnLanguageDetermined(language_detection_details);

  parsed_form = manager().FindCachedFormById(form.global_id());

  // Language detection is used only for active frames.
  auto expected_language_code =
      is_active() ? LanguageCode("hu") : LanguageCode();

  ASSERT_EQ(*expected_language_code, *parsed_form->current_page_language());
}

INSTANTIATE_TEST_SUITE_P(All,
                         BrowserAutofillManagerTestPageLanguageDetection,
                         testing::Bool());

// BrowserAutofillManagerTest with different browser profile types.
class BrowserAutofillManagerProfileMetricsTest
    : public BrowserAutofillManagerTest,
      public testing::WithParamInterface<profile_metrics::BrowserProfileType> {
 public:
  profile_metrics::BrowserProfileType profile_type() { return GetParam(); }

  void SetUp() override {
    BrowserAutofillManagerTest::SetUp();
    ON_CALL(client(), GetProfileType()).WillByDefault(Return(profile_type()));
  }
};

// Tests if submitting a form in different browser profile types records correct
// |Autofill.FormSubmission.PerProfileType| metric.
TEST_P(BrowserAutofillManagerProfileMetricsTest,
       FormSubmissionPerProfileTypeMetrics) {
  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});
  OnAskForValuesToFill(form, form.fields()[0]);

  base::HistogramTester histogram_tester;

  FormSubmitted(form);
  histogram_tester.ExpectBucketCount("Autofill.FormSubmission.PerProfileType",
                                     profile_type(), 1);
  histogram_tester.ExpectTotalCount("Autofill.FormSubmission.PerProfileType",
                                    1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    BrowserAutofillManagerProfileMetricsTest,
    testing::ValuesIn({profile_metrics::BrowserProfileType::kRegular,
                       profile_metrics::BrowserProfileType::kIncognito,
                       profile_metrics::BrowserProfileType::kGuest}));

// Tests that autocomplete-related metrics are emitted correctly on form
// submission.
TEST_F(BrowserAutofillManagerTest, AutocompleteMetrics) {
  // `kAutocompleteValues` corresponds to empty, valid, garbage and off.
  constexpr auto kAutocompleteValues =
      std::to_array<const char*>({"", "name", "asdf", "off"});
  // The 4 possible combinations of heuristic and server type status:
  // - Neither a fillable heuristic type nor a fillable server type.
  // - Only a fillable server type.
  // - Only a fillable heuristic type.
  // - Both a fillable heuristic type and a fillable server type.
  // NO_SERVER_DATA and UNKNOWN_TYPE are both unfillable types, but
  // NO_SERVER_DATA is ignored in the PredictionCollisionType metric.
  constexpr auto kTypeClasses = std::to_array<std::array<FieldType, 2>>(
      {{UNKNOWN_TYPE, NO_SERVER_DATA},
       {UNKNOWN_TYPE, EMAIL_ADDRESS},
       {ADDRESS_HOME_COUNTRY, UNKNOWN_TYPE},
       {ADDRESS_HOME_COUNTRY, EMAIL_ADDRESS}});

  // Create a form with one field per kAutofillValue x kTypeClass combination.
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  std::vector<FieldType> heuristic_types, server_types;
  for (const char* autocomplete : kAutocompleteValues) {
    for (const auto& types : kTypeClasses) {
      test_api(form).Append(CreateTestFormField(
          "", "", "", FormControlType::kInputText, autocomplete));
      heuristic_types.push_back(types[0]);
      server_types.push_back(types[1]);
    }
  }
  // Override the types and simulate seeing the form on page load.
  auto form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr);
  test_api(*form_structure).SetFieldTypes(heuristic_types, server_types);
  manager().AddSeenFormStructure(std::move(form_structure));

  // Submit the form and verify that all metrics are collected correctly.
  base::HistogramTester histogram_tester;
  FormSubmitted(form);

  // Expect one entry for each possible PredictionStateAutocompleteStatePair.
  // Fields without type predictions and autocomplete attributes are ignored.
  histogram_tester.ExpectTotalCount(
      "Autofill.Autocomplete.PredictionCollisionState",
      form.fields().size() - 1);
  for (int i = 0; i < 15; i++) {
    histogram_tester.ExpectBucketCount(
        "Autofill.Autocomplete.PredictionCollisionState", i, 1);
  }

  // A separate PredictionCollisionType metric exists for every prediction-type
  // autocomplete status combination. Above, we created four fields per
  // `kAutocompleteValues` - so we expect 4 samples in every bucket.
  // The exception is the .Server metric, which is not emitted for
  // NO_SERVER_DATA and hence expects only three samples.
  const std::string kTypeHistogram =
      "Autofill.Autocomplete.PredictionCollisionType2.";
  for (const char* suffix : {"Garbage", "None", "Off", "Valid"}) {
    histogram_tester.ExpectTotalCount(kTypeHistogram + "Heuristics." + suffix,
                                      4);
    histogram_tester.ExpectTotalCount(kTypeHistogram + "Server." + suffix, 3);
    histogram_tester.ExpectTotalCount(
        kTypeHistogram + "ServerOrHeuristics." + suffix, 4);
  }
}

// Test that if a form is mixed content we show a warning instead of any
// suggestions.
TEST_F(BrowserAutofillManagerTest, GetSuggestions_MixedForm) {
  // Set up our form data.
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("http://myform.com/submit.html"));
  form.set_fields({CreateTestFormField("Name on Card", "nameoncard", "",
                                       FormControlType::kInputText)});

  OnAskForValuesToFill(form, form.fields()[0]);

  // Test that we sent the right values to the external delegate.
  external_delegate()->CheckSuggestions(
      form.fields().back().global_id(),
      {Suggestion(l10n_util::GetStringUTF8(IDS_AUTOFILL_WARNING_MIXED_FORM), "",
                  Suggestion::Icon::kNoIcon,
                  SuggestionType::kMixedFormMessage)});
}

// Test that if a form is mixed content we do not show a warning if the opt out
// policy is set.
TEST_F(BrowserAutofillManagerTest, GetSuggestions_MixedFormOptOutPolicy) {
  // Set pref to disabled.
  client().GetPrefs()->SetBoolean(::prefs::kMixedFormsWarningsEnabled, false);

  // Set up our form data.
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("http://myform.com/submit.html"));
  form.set_fields({CreateTestFormField("Name on Card", "nameoncard", "",
                                       FormControlType::kInputText)});
  OnAskForValuesToFill(form, form.fields()[0]);

  // Check there is no warning.
  EXPECT_FALSE(external_delegate()->on_suggestions_returned_seen());
}

// Test that we dismiss the mixed form warning if user starts typing.
TEST_F(BrowserAutofillManagerTest, GetSuggestions_MixedFormUserTyped) {
  // Set up our form data.
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("http://myform.com/submit.html"));
  form.set_fields({CreateTestFormField("Name on Card", "nameoncard", "",
                                       FormControlType::kInputText)});

  OnAskForValuesToFill(form, form.fields()[0]);

  // Test that we sent the right values to the external delegate.
  external_delegate()->CheckSuggestions(
      form.fields().back().global_id(),
      {Suggestion(l10n_util::GetStringUTF8(IDS_AUTOFILL_WARNING_MIXED_FORM), "",
                  Suggestion::Icon::kNoIcon,
                  SuggestionType::kMixedFormMessage)});

  // Pretend user started typing and make sure we no longer set suggestions.
  test_api(form).field(0).set_value(u"Michael");
  test_api(form).field(0).set_properties_mask(
      form.fields()[0].properties_mask() | kUserTyped);
  OnAskForValuesToFill(form, form.fields()[0]);
  external_delegate()->CheckNoSuggestions(form.fields()[0].global_id());
}

// Test that we don't treat javascript scheme target URLs as mixed forms.
// Regression test for crbug.com/1135173
TEST_F(BrowserAutofillManagerTest, GetSuggestions_JavascriptUrlTarget) {
  // Set up our form data, using a javascript scheme target URL.
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("javascript:alert('hello');"));
  form.set_fields({CreateTestFormField("Name on Card", "nameoncard", "",
                                       FormControlType::kInputText)});
  OnAskForValuesToFill(form, form.fields()[0]);

  // Check there is no warning.
  EXPECT_FALSE(external_delegate()->on_suggestions_returned_seen());
}

// Test that we don't treat about:blank target URLs as mixed forms.
TEST_F(BrowserAutofillManagerTest, GetSuggestions_AboutBlankTarget) {
  // Set up our form data, using a javascript scheme target URL.
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("about:blank"));
  form.set_fields({CreateTestFormField("Name on Card", "nameoncard", "",
                                       FormControlType::kInputText)});
  OnAskForValuesToFill(form, form.fields()[0]);

  // Check there is no warning.
  EXPECT_FALSE(external_delegate()->on_suggestions_returned_seen());
}

// Tests that both Autofill popup and TTF are hidden on renderer event.
TEST_F(BrowserAutofillManagerTest, HideAutofillSuggestionsAndOtherPopups) {
  EXPECT_CALL(client(),
              HideAutofillSuggestions(SuggestionHidingReason::kRendererEvent));
  EXPECT_CALL(client(), HideAutofillFieldIph);
  EXPECT_CALL(touch_to_fill_delegate(), HideTouchToFill);
  EXPECT_CALL(fast_checkout_delegate(),
              HideFastCheckout(/*allow_further_runs=*/false));
  manager().OnHidePopup();
}

// Tests that only Autofill popup is hidden on editing end, but not TTF or FC.
TEST_F(BrowserAutofillManagerTest, OnDidEndTextFieldEditing) {
  EXPECT_CALL(client(),
              HideAutofillSuggestions(SuggestionHidingReason::kEndEditing));
  EXPECT_CALL(touch_to_fill_delegate(), HideTouchToFill).Times(0);
  EXPECT_CALL(fast_checkout_delegate(),
              HideFastCheckout(/*allow_further_runs=*/false))
      .Times(0);
  manager().OnDidEndTextFieldEditing();
}

// Tests that keyboard accessory is not shown if TTF is eligible.
TEST_F(BrowserAutofillManagerTest, TouchToFillSuggestionForIban) {
  FormData form = CreateTestIbanFormData();
  FormsSeen({form});

  // A form element click and TTF available, Autofill suggestions not shown.
  EXPECT_CALL(touch_to_fill_delegate(), TryToShowTouchToFill)
      .WillOnce(Return(true));
  TryToShowTouchToFill(form, form.fields()[0],
                       /*form_element_was_clicked=*/true);
  EXPECT_FALSE(external_delegate()->on_suggestions_returned_seen());
}

// Tests that Autofill suggestions are not shown if TTF is eligible and shown.
TEST_F(BrowserAutofillManagerTest, AutofillSuggestionsOrTouchToFillForCards) {
  FormData form = CreateTestCreditCardFormData(/*is_https=*/true,
                                               /*use_month_type=*/false);
  FormsSeen({form});
  const FormFieldData& field = form.fields()[1];

  // Not a form element click, Autofill suggestions shown.
  EXPECT_CALL(touch_to_fill_delegate(), TryToShowTouchToFill).Times(0);
  TryToShowTouchToFill(form, field, /*form_element_was_clicked=*/false);
  EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());

  // TTF not available, Autofill suggestions shown.
  EXPECT_CALL(touch_to_fill_delegate(), TryToShowTouchToFill)
      .WillOnce(Return(false));
  TryToShowTouchToFill(form, field, /*form_element_was_clicked=*/true);
  EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());

  // A form element click and TTF available, Autofill suggestions not shown.
  EXPECT_CALL(touch_to_fill_delegate(), TryToShowTouchToFill)
      .WillOnce(Return(true));
  TryToShowTouchToFill(form, field, /*form_element_was_clicked=*/true);
  EXPECT_FALSE(external_delegate()->on_suggestions_returned_seen());
}

// Tests that neither Autofill suggestions nor TTF is triggered if TTF is
// already shown.
TEST_F(BrowserAutofillManagerTest, ShowNothingIfTouchToFillAlreadyShown) {
  FormData form = CreateTestCreditCardFormData(/*is_https=*/true,
                                               /*use_month_type=*/false);
  FormsSeen({form});
  const FormFieldData& field = form.fields()[1];

  EXPECT_CALL(touch_to_fill_delegate(), IsShowingTouchToFill)
      .WillOnce(Return(true));
  EXPECT_CALL(touch_to_fill_delegate(), TryToShowTouchToFill).Times(0);
  TryToShowTouchToFill(form, field, /*form_element_was_clicked=*/true);
  EXPECT_FALSE(external_delegate()->on_suggestions_returned_seen());
}

// Test that 'Scan New Card' suggestion is shown based on whether autofill
// credit card is enabled or disabled.
TEST_F(BrowserAutofillManagerTest, ScanCreditCardBasedOnAutofillPreference) {
  ON_CALL(payments_client(), HasCreditCardScanFeature())
      .WillByDefault(Return(true));

  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form});

  const FormFieldData& card_number_field = form.fields()[1];
  ASSERT_EQ(card_number_field.name(), u"cardnumber");

  // Test case where autofill is enabled.
  client().SetAutofillPaymentMethodsEnabled(true);
  EXPECT_TRUE(manager().ShouldShowScanCreditCard(form, card_number_field));

  // Test case where autofill is disabled.
  client().SetAutofillPaymentMethodsEnabled(false);
  EXPECT_FALSE(manager().ShouldShowScanCreditCard(form, card_number_field));
}

// Test that 'Scan New Card' suggestion is shown based on whether platform
// supports card scanning.
TEST_F(BrowserAutofillManagerTest, ScanCreditCardBasedOnPlatformSupport) {
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form});

  const FormFieldData& card_number_field = form.fields()[1];
  ASSERT_EQ(card_number_field.name(), u"cardnumber");

  // Test case where device and platform support scanning credit cards.
  ON_CALL(payments_client(), HasCreditCardScanFeature())
      .WillByDefault(Return(true));
  EXPECT_TRUE(manager().ShouldShowScanCreditCard(form, card_number_field));

  // Test case where device and platform do not support scanning credit cards.
  ON_CALL(payments_client(), HasCreditCardScanFeature())
      .WillByDefault(Return(false));
  EXPECT_FALSE(manager().ShouldShowScanCreditCard(form, card_number_field));
}

// Test that 'Scan New Card' suggestion is shown based on whether form field
// chosen is a credit card number field.
TEST_F(BrowserAutofillManagerTest, ScanCreditCardBasedOnCreditCardNumberField) {
  ON_CALL(payments_client(), HasCreditCardScanFeature())
      .WillByDefault(Return(true));

  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form});

  // Test case for credit-card-number field.
  const FormFieldData& card_number_field = form.fields()[1];
  ASSERT_EQ(card_number_field.name(), u"cardnumber");
  EXPECT_TRUE(manager().ShouldShowScanCreditCard(form, card_number_field));

  // Test case for non-credit-card-number field.
  const FormFieldData& cvc_field = form.fields()[4];
  ASSERT_EQ(cvc_field.name(), u"cvc");
  EXPECT_FALSE(manager().ShouldShowScanCreditCard(form, cvc_field));
}

// Test that 'Scan New Card' suggestion is shown based on whether the form is
// secure.
TEST_F(BrowserAutofillManagerTest, ScanCreditCardBasedOnIsFormSecure) {
  ON_CALL(payments_client(), HasCreditCardScanFeature())
      .WillByDefault(Return(true));

  // Test case for HTTP form.
  FormData form_http = CreateTestCreditCardFormData(/*is_https=*/false,
                                                    /*use_month_type=*/false);
  FormsSeen({form_http});

  const FormFieldData& card_number_field_http = form_http.fields()[1];
  ASSERT_EQ(card_number_field_http.name(), u"cardnumber");
  EXPECT_FALSE(
      manager().ShouldShowScanCreditCard(form_http, card_number_field_http));

  // Test case for HTTPS form.
  FormData form_https =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form_https});

  const FormFieldData& card_number_field_https = form_https.fields()[1];
  ASSERT_EQ(card_number_field_https.name(), u"cardnumber");
  EXPECT_TRUE(
      manager().ShouldShowScanCreditCard(form_https, card_number_field_https));
}

// Tests that compose suggestions are not queried if Autofill has suggestions
// itself.
TEST_F(BrowserAutofillManagerTest, NoComposeSuggestionsByDefault) {
  MockAutofillComposeDelegate compose_delegate;
  ON_CALL(client(), GetComposeDelegate)
      .WillByDefault(Return(&compose_delegate));

  FormData form = CreateTestAddressFormData();
  test_api(form).field(3).set_form_control_type(FormControlType::kTextArea);
  FormsSeen({form});

  // The third field is meant to correspond to address line 1. For that (unlike
  // for first and last name), parsing also derives that type if it is a
  // textarea.
  EXPECT_CALL(compose_delegate, GetSuggestion).Times(0);
  OnAskForValuesToFill(form, form.fields()[3]);
  external_delegate()->CheckSuggestions(
      form.fields()[3].global_id(),
      {Suggestion("123 Apple St., unit 6", "123 Apple St.", kAddressEntryIcon,
                  SuggestionType::kAddressEntry),
       Suggestion("3734 Elvis Presley Blvd., Apt. 10",
                  "3734 Elvis Presley Blvd.", kAddressEntryIcon,
                  SuggestionType::kAddressEntry),
       CreateSeparator(), CreateManageAddressesSuggestion()});
}

// Tests that Compose suggestions are queried if the trigger source indicates
// that the focus change happened without click/tap interaction. It also
// verifies that neither Autofill nor single form fill suggestions are queried.
TEST_F(BrowserAutofillManagerTest, ComposeSuggestionsOnFocusWithoutClick) {
  MockAutofillComposeDelegate compose_delegate;
  ON_CALL(client(), GetComposeDelegate)
      .WillByDefault(Return(&compose_delegate));

  FormData form = CreateTestAddressFormData();
  test_api(form).field(3).set_form_control_type(FormControlType::kTextArea);
  FormsSeen({form});

  EXPECT_CALL(merchant_promo_code_manager(), OnGetSingleFieldSuggestions)
      .Times(0);
  EXPECT_CALL(iban_manager(), OnGetSingleFieldSuggestions).Times(0);
  EXPECT_CALL(autocomplete_history_manager(), OnGetSingleFieldSuggestions)
      .Times(0);
  EXPECT_CALL(
      compose_delegate,
      GetSuggestion(
          _,
          Property(&FormFieldData::global_id, Eq(form.fields()[3].global_id())),
          AutofillSuggestionTriggerSource::kTextareaFocusedWithoutClick))
      .WillOnce(Return(
          Suggestion(u"Help me write", SuggestionType::kComposeResumeNudge)));
  OnAskForValuesToFill(
      form, form.fields()[3],
      AutofillSuggestionTriggerSource::kTextareaFocusedWithoutClick);
  external_delegate()->CheckSuggestionCount(form.fields()[3].global_id(), 1);
}

// Tests that compose suggestions are queried and shown for textareas if
// Autofill does not have suggestions of its own and the OS is not Android, iOS
// or ChromeOS.
TEST_F(BrowserAutofillManagerTest, ComposeSuggestionsAreQueriedForTextareas) {
  MockAutofillComposeDelegate compose_delegate;
  ON_CALL(client(), GetComposeDelegate)
      .WillByDefault(Return(&compose_delegate));

  FormData form = test::GetFormData(
      {.fields = {{.form_control_type = FormControlType::kTextArea}}});
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  FormsSeen({form});

  EXPECT_CALL(merchant_promo_code_manager(), OnGetSingleFieldSuggestions)
      .Times(0);
  EXPECT_CALL(iban_manager(), OnGetSingleFieldSuggestions).Times(0);
  EXPECT_CALL(autocomplete_history_manager(), OnGetSingleFieldSuggestions)
      .Times(0);
  EXPECT_CALL(
      compose_delegate,
      GetSuggestion(
          _,
          Property(&FormFieldData::global_id, Eq(form.fields()[0].global_id())),
          AutofillSuggestionTriggerSource::kTextFieldValueChanged))
      .WillOnce(Return(
          Suggestion(u"Help me write", SuggestionType::kComposeResumeNudge)));
  OnAskForValuesToFill(form, form.fields()[0]);
  external_delegate()->CheckSuggestionCount(form.fields()[0].global_id(), 1);
}

class BrowserAutofillManagerTest_AutofillAi
    : public BrowserAutofillManagerTest {
 public:
  void SetUp() override {
    BrowserAutofillManagerTest::SetUp();
    client().set_entity_data_manager(std::make_unique<EntityDataManager>(
        webdata_helper_.autofill_webdata_service(), /*history_service=*/nullptr,
        /*strike_database=*/nullptr));
    client().GetEntityDataManager()->AddOrUpdateEntityInstance(
        test::GetPassportEntityInstance());
    client().GetEntityDataManager()->AddOrUpdateEntityInstance(
        test::GetDriversLicenseEntityInstance());
    webdata_helper_.WaitUntilIdle();
    client().SetUpPrefsAndIdentityForAutofillAi();

    GenerateNewPassportForm(/*autocomplete_unrecognized=*/false);
  }

  const FormData& passport_form() const { return passport_form_; }

  FormGlobalId SeeForm(bool may_run_model,
                       bool add_autofill_ai_predictions = true) {
    LOG_IF(ERROR, may_run_model && !base::FeatureList::IsEnabled(
                                       features::kAutofillAiServerModel))
        << __func__ << "(/*may_run_model=*/true, ...)"
        << " cannot run the model because AutofillAiServerModel is disabled";

    FormData form = passport_form();
    FormStructure* form_structure = nullptr;
    {
      auto fs = std::make_unique<FormStructure>(form);
      form_structure = fs.get();
      fs->DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr);
      manager().AddSeenFormStructure(std::move(fs));
    }

    // Make API response with suggestions.
    std::string server_response;
    {
      AutofillQueryResponse response;
      AutofillQueryResponse::FormSuggestion* form_suggestion =
          response.add_form_suggestions();

      auto add_predictions = [&](const FormFieldData& field,
                                 std::vector<FieldPrediction> predictions) {
        if (!add_autofill_ai_predictions) {
          std::erase_if(predictions, [](const auto& prediction) {
            return prediction.source() == FieldPrediction::SOURCE_AUTOFILL_AI;
          });
        }
        AddFieldPredictionsToForm(field, predictions, form_suggestion);
      };

      add_predictions(
          form.fields()[0],
          {CreateFieldPrediction(NAME_FIRST,
                                 FieldPrediction::SOURCE_AUTOFILL_DEFAULT),
           CreateFieldPrediction(PASSPORT_NAME_TAG,
                                 FieldPrediction::SOURCE_AUTOFILL_AI)});
      add_predictions(
          form.fields()[1],
          {CreateFieldPrediction(NAME_LAST,
                                 FieldPrediction::SOURCE_AUTOFILL_DEFAULT),
           CreateFieldPrediction(PASSPORT_NAME_TAG,
                                 FieldPrediction::SOURCE_AUTOFILL_AI)});
      add_predictions(
          form.fields()[2],
          {CreateFieldPrediction(PASSPORT_NUMBER,
                                 FieldPrediction::SOURCE_AUTOFILL_AI)});
      form_suggestion->set_run_autofill_ai_model(may_run_model);
      response.SerializeToString(&server_response);
    }

    test_api(manager()).OnLoadedServerPredictions(
        base::Base64Encode(server_response),
        test::GetEncodedSignatures(*form_structure));
    return form.global_id();
  }

  void GenerateNewPassportForm(bool autocomplete_unrecognized) {
    passport_form_ = test::GetFormData(
        {.fields = {{.name = u"First name",
                     .autocomplete_attribute =
                         autocomplete_unrecognized ? "nonsense" : ""},
                    {.name = u"Last name"},
                    {.name = u"Passport number"},
                    {.name = u"Passport issue date"}}});
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      features::kAutofillAiWithDataSchema};
  AutofillWebDataServiceTestHelper webdata_helper_{
      std::make_unique<EntityTable>()};
  FormData passport_form_;
};

// Tests that Autofill AI suggestions are shown.
TEST_F(BrowserAutofillManagerTest_AutofillAi, ShowAutofillAiSuggestions) {
  SeeForm(/*may_run_model=*/false);

  MockAutofillAiDelegate& delegate = *client().GetAutofillAiDelegate();
  std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kFillAutofillAi)};
  EXPECT_CALL(delegate, GetSuggestions).WillOnce(Return(suggestions));

  OnAskForValuesToFill(passport_form(), passport_form().fields().front(),
                       AutofillSuggestionTriggerSource::kAutofillAi);
  EXPECT_THAT(external_delegate()->suggestions(),
              ElementsAre(Field(&Suggestion::type,
                                Eq(SuggestionType::kFillAutofillAi))));
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Tests that no Autofill AI suggestions are shown if the autocomplete attribute
// is unrecognized.
TEST_F(BrowserAutofillManagerTest_AutofillAi,
       NoAutofillAiSuggestionsForAutocompleteUnrecognized) {
  GenerateNewPassportForm(/*autocomplete_unrecognized=*/true);
  SeeForm(/*may_run_model=*/false);

  EXPECT_CALL(*client().GetAutofillAiDelegate(), GetSuggestions).Times(0);
  OnAskForValuesToFill(passport_form(), passport_form().fields().front(),
                       AutofillSuggestionTriggerSource::kAutofillAi);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Tests that if a field has an Autofill AI TAG type and the EntityDataManager
// can serve that TAG type (e.g., PASSPORT_NAME_TAG) but not the specific type
// of the field (e.g., NAME_MIDDLE), there are no suggestions shown on that
// specific field. In particular, even if there are matching address
// suggestions, these are ignored. See crbug.com/402397312 for a concrete
// example.
TEST_F(BrowserAutofillManagerTest_AutofillAi, ShowNoSuggestionsIfCollision) {
  SeeForm(/*may_run_model=*/false);

  EXPECT_CALL(*client().GetAutofillAiDelegate(), GetSuggestions)
      .WillOnce(Return(std::vector<Suggestion>{}));

  OnAskForValuesToFill(passport_form(), passport_form().fields().front(),
                       AutofillSuggestionTriggerSource::kAutofillAi);
  EXPECT_THAT(external_delegate()->suggestions(), IsEmpty());
}

// Tests that the Autofill AI IPH is attempted to be shown if there are no
// Autofill suggestions and the delegate returns that IPH should show.
TEST_F(BrowserAutofillManagerTest_AutofillAi, AutofillAiIph) {
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});
  ON_CALL(*client().GetAutofillAiDelegate(), ShouldDisplayIph)
      .WillByDefault(Return(true));
  personal_data().test_address_data_manager().ClearProfiles();

  EXPECT_CALL(client(), ShowAutofillFieldIphForFeature(
                            _, AutofillClient::IphFeature::kAutofillAi));
  OnAskForValuesToFill(form, form.fields().front(),
                       AutofillSuggestionTriggerSource::kAutofillAi);
}

// Tests that the Autofill AI IPH is not shown if there are Autofill
// suggestions.
TEST_F(BrowserAutofillManagerTest_AutofillAi,
       NoAutofillAiIphWhenThereAreAutofillSuggestions) {
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});
  ON_CALL(*client().GetAutofillAiDelegate(), ShouldDisplayIph)
      .WillByDefault(Return(false));
  ASSERT_THAT(personal_data().test_address_data_manager().GetProfiles(),
              Not(IsEmpty()));

  EXPECT_CALL(client(), ShowAutofillFieldIphForFeature(
                            _, AutofillClient::IphFeature::kAutofillAi))
      .Times(0);
  OnAskForValuesToFill(form, form.fields().front(),
                       AutofillSuggestionTriggerSource::kAutofillAi);
}

// Tests that an Autofill profile is not imported into the address data manager
// when the submitted form was imported by AutofillAI.
TEST_F(BrowserAutofillManagerTest_AutofillAi,
       ProfileNotImportedOnSuccessfulAutofillAiImport) {
  TestAddressDataManager& adm = personal_data().test_address_data_manager();
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});

  // Fill the form.
  FormData response_data =
      FillAutofillFormDataAndGetResults(form, form.fields()[0], MakeGuid(1));
  ExpectFilledAddressFormElvis(response_data, false);
  AutofillProfile filled_profile = *adm.GetProfileByGUID(MakeGuid(1));

  // Remove the filled profile and simulate form submission. Since the
  // `personal_data()`'s auto accept imports for testing is enabled, expect
  // that the profile should be imported again which it's not because of
  // prediction improvements.
  adm.ClearProfiles();
  ASSERT_TRUE(adm.GetProfiles().empty());
  EXPECT_CALL(*client().GetAutofillAiDelegate(), OnFormSubmitted)
      .WillOnce(Return(true));
  FormSubmitted(response_data);
  EXPECT_TRUE(adm.GetProfiles().empty());
}

// Tests that an Autofill profile is imported into the address data manager when
// the submitted form was not imported by AutofillAI.
TEST_F(BrowserAutofillManagerTest_AutofillAi,
       ProfileImportedOnFailedAutofillAiImport) {
  TestAddressDataManager& adm = personal_data().test_address_data_manager();
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});

  // Fill the form.
  FormData response_data =
      FillAutofillFormDataAndGetResults(form, form.fields()[0], MakeGuid(1));
  ExpectFilledAddressFormElvis(response_data, false);
  AutofillProfile filled_profile = *adm.GetProfileByGUID(MakeGuid(1));

  // Remove the filled profile and simulate form submission. Since the
  // `personal_data()`'s auto accept imports for testing is enabled, expect
  // that the profile is imported again.
  adm.ClearProfiles();
  ASSERT_TRUE(adm.GetProfiles().empty());
  EXPECT_CALL(*client().GetAutofillAiDelegate(), OnFormSubmitted)
      .WillOnce(Return(false));
  FormSubmitted(response_data);
  EXPECT_FALSE(adm.GetProfiles().empty());
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
class BrowserAutofillManagerTest_AutofillAi_WithModel
    : public BrowserAutofillManagerTest_AutofillAi {
 public:
  void SetUp() override {
    BrowserAutofillManagerTest_AutofillAi::SetUp();
    ON_CALL(client(), GetAutofillAiModelCache).WillByDefault(Return(&cache_));
    ON_CALL(client(), GetAutofillAiModelExecutor)
        .WillByDefault(Return(&executor_));
  }

  MockAutofillAiModelCache& cache() { return cache_; }
  MockAutofillAiModelExecutor& executor() { return executor_; }

 private:
  base::test::ScopedFeatureList feature_list_{features::kAutofillAiServerModel};
  NiceMock<MockAutofillAiModelCache> cache_;
  NiceMock<MockAutofillAiModelExecutor> executor_;
};

// Tests that the Autofill AI server model is run if cache and model are
// available and the form is not contained in the cache.
TEST_F(BrowserAutofillManagerTest_AutofillAi_WithModel,
       AutofillAiServerModelRun) {
  ON_CALL(cache(), Contains).WillByDefault(Return(false));
  EXPECT_CALL(client(), GetAiPageContent).Times(0);
  EXPECT_CALL(executor(), GetPredictions);
  SeeForm(/*may_run_model=*/true);
}

// Tests that BAM requests annotated page content and passes it on to the
// model executor if kAutofillAiServerModelSendPageContent is true.
TEST_F(BrowserAutofillManagerTest_AutofillAi_WithModel,
       AutofillAiServerModelReceivesAnnotatedPageContent) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillAiServerModel,
      {{"autofill_ai_model_send_apc", "true"}});

  ON_CALL(cache(), Contains).WillByDefault(Return(false));
  EXPECT_CALL(client(), GetAiPageContent)
      .WillOnce(RunOnceCallback<0>(
          optimization_guide::proto::AnnotatedPageContent()));
  EXPECT_CALL(executor(), GetPredictions(_, _, Not(Eq(std::nullopt))));
  SeeForm(/*may_run_model=*/true);
}

// Tests that the Autofill AI server model is not run if the form is already
// contained in the cache.
TEST_F(BrowserAutofillManagerTest_AutofillAi_WithModel,
       AutofillAiServerModelNotRunWhenInCache) {
  ON_CALL(cache(), Contains).WillByDefault(Return(true));
  EXPECT_CALL(executor(), GetPredictions).Times(0);
  SeeForm(/*may_run_model=*/true);
}

// Tests that the Autofill AI server model is not run if the form is not
// eligible.
TEST_F(BrowserAutofillManagerTest_AutofillAi_WithModel,
       AutofillAiServerModelNotRunWhenNotFormIneligible) {
  ON_CALL(cache(), Contains).WillByDefault(Return(false));
  EXPECT_CALL(executor(), GetPredictions).Times(0);
  SeeForm(/*may_run_model=*/false);
}

// Tests that the Autofill AI server model is not run if the user does not have
// model permissions.
TEST_F(BrowserAutofillManagerTest_AutofillAi_WithModel,
       AutofillAiServerModelNotRunWhenNotUserIneligible) {
  client().SetCanUseModelExecutionFeatures(false);
  ON_CALL(cache(), Contains).WillByDefault(Return(false));
  EXPECT_CALL(executor(), GetPredictions).Times(0);
  SeeForm(/*may_run_model=*/true);
}

// Tests that cache results are used to populate field server types.
TEST_F(BrowserAutofillManagerTest_AutofillAi_WithModel, CacheResultUsed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillAiServerModel,
      {{"autofill_ai_model_use_cache_results", "true"}});

  // Get the same form that will be seen to calculate the field signatures.
  const FormData form = passport_form();
  const FieldSignature field_signature1 =
      CalculateFieldSignatureForField(form.fields()[2]);
  const FieldSignature field_signature2 =
      CalculateFieldSignatureForField(form.fields()[3]);
  const FormSignature form_signature = CalculateFormSignature(form);

  using FieldIdentifier = AutofillAiModelCache::FieldIdentifier;
  using ModelFieldPrediction = AutofillAiModelCache::FieldPrediction;
  auto predictions = base::flat_map<FieldIdentifier, ModelFieldPrediction>(
      {std::pair{FieldIdentifier{.signature = field_signature1},
                 ModelFieldPrediction{.field_type = PASSPORT_NUMBER}},
       std::pair{FieldIdentifier{.signature = field_signature2},
                 ModelFieldPrediction{.field_type = PASSPORT_ISSUE_DATE,
                                      .format_string = u"D.M.YYYY"}}});

  EXPECT_CALL(cache(), Contains(form_signature)).WillOnce(Return(true));
  EXPECT_CALL(cache(), GetFieldPredictions(form_signature))
      .WillOnce(Return(predictions));
  EXPECT_CALL(executor(), GetPredictions).Times(0);
  const FormGlobalId form_id =
      SeeForm(/*may_run_model=*/true, /*add_autofill_ai_predictions=*/false);

  // Check that we set predictions on the form.
  const FormStructure* const fs = manager().FindCachedFormById(form_id);
  ASSERT_TRUE(fs);
  EXPECT_EQ(fs->field(0)->GetAutofillAiServerTypePredictions(), std::nullopt);
  EXPECT_EQ(fs->field(1)->GetAutofillAiServerTypePredictions(), std::nullopt);
  EXPECT_EQ(fs->field(2)->GetAutofillAiServerTypePredictions(),
            PASSPORT_NUMBER);
  EXPECT_EQ(fs->field(3)->GetAutofillAiServerTypePredictions(),
            PASSPORT_ISSUE_DATE);
  ASSERT_TRUE(fs->field(3)->format_string().has_value());
  EXPECT_EQ(fs->field(3)->format_string().value(), u"D.M.YYYY");
  EXPECT_EQ(fs->field(3)->format_string_source(),
            AutofillField::FormatStringSource::kModelResult);
}

// Tests that if the form has at least one existing AutofillAI prediction, then
// the cache is not used for populating predictions.
TEST_F(BrowserAutofillManagerTest_AutofillAi_WithModel,
       CacheResultsDoNotOverrideAiServerPredictions) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillAiServerModel,
      {{"autofill_ai_model_use_cache_results", "true"}});

  // Get the same form that will be seen to calculate the field signatures.
  const FormData form = passport_form();
  const FieldSignature field_signature1 =
      CalculateFieldSignatureForField(form.fields()[2]);
  const FieldSignature field_signature2 =
      CalculateFieldSignatureForField(form.fields()[3]);
  const FormSignature form_signature = CalculateFormSignature(form);

  using FieldIdentifier = AutofillAiModelCache::FieldIdentifier;
  using ModelFieldPrediction = AutofillAiModelCache::FieldPrediction;
  auto predictions = base::flat_map<FieldIdentifier, ModelFieldPrediction>(
      {std::pair{FieldIdentifier{.signature = field_signature1},
                 ModelFieldPrediction{.field_type = PASSPORT_NUMBER}},
       std::pair{FieldIdentifier{.signature = field_signature2},
                 ModelFieldPrediction{.field_type = PASSPORT_ISSUE_DATE,
                                      .format_string = u"D.M.YYYY"}}});

  ON_CALL(cache(), Contains(form_signature)).WillByDefault(Return(true));
  EXPECT_CALL(cache(), GetFieldPredictions(form_signature)).Times(0);
  EXPECT_CALL(executor(), GetPredictions).Times(0);
  SeeForm(/*may_run_model=*/false);
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

// Test param indicates if there is an active screen reader.
class OnFocusOnFormFieldTest : public BrowserAutofillManagerTest,
                               public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    BrowserAutofillManagerTest::SetUp();

    has_active_screen_reader_ = GetParam();
    external_delegate()->set_has_active_screen_reader(
        has_active_screen_reader_);
  }

  void TearDown() override {
    external_delegate()->set_has_active_screen_reader(false);
    BrowserAutofillManagerTest::TearDown();
  }

  void CheckSuggestionsAvailableIfScreenReaderRunning() {
    EXPECT_EQ(has_active_screen_reader_,
              external_delegate()->has_suggestions_available_on_field_focus());
  }

  void CheckNoSuggestionsAvailableOnFieldFocus() {
    EXPECT_FALSE(
        external_delegate()->has_suggestions_available_on_field_focus());
  }

  bool has_active_screen_reader_;
};

TEST_P(OnFocusOnFormFieldTest, AddressSuggestions) {
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  form.set_fields(
      {// Set a valid autocomplete attribute for the first name.
       CreateTestFormField("First name", "firstname", "",
                           FormControlType::kInputText, "given-name"),
       // Set an unrecognized autocomplete attribute for the last name.
       CreateTestFormField("Last Name", "lastname", "",
                           FormControlType::kInputText, "unrecognized")});
  FormsSeen({form});

  // Suggestions should be returned for the first field.
  manager().OnFocusOnFormFieldImpl(form, form.fields()[0].global_id());
  CheckSuggestionsAvailableIfScreenReaderRunning();

  // No suggestions should be provided for the second field because of its
  // unrecognized autocomplete attribute.
  manager().OnFocusOnFormFieldImpl(form, form.fields()[1].global_id());
  CheckNoSuggestionsAvailableOnFieldFocus();
}

TEST_P(OnFocusOnFormFieldTest, AddressSuggestions_AutocompleteOffNotRespected) {
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  form.set_fields(
      {// Set a valid autocomplete attribute for the first name.
       CreateTestFormField("First name", "firstname", "",
                           FormControlType::kInputText, "given-name"),
       // Set an autocomplete=off attribute for the last name.
       CreateTestFormField("Last Name", "lastname", "",
                           FormControlType::kInputText, "given-name")});
  test_api(form).field(-1).set_should_autocomplete(false);
  FormsSeen({form});

  manager().OnFocusOnFormFieldImpl(form, form.fields()[1].global_id());
  CheckSuggestionsAvailableIfScreenReaderRunning();
}

TEST_P(OnFocusOnFormFieldTest, AddressSuggestions_Ablation) {
  base::test::ScopedFeatureList scoped_feature_list;
  DisableAutofillViaAblation(scoped_feature_list, /*for_addresses=*/true,
                             /*for_credit_cards=*/false);

  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  // Clear the form action.
  form.set_action(GURL());
  FormsSeen({form});

  manager().OnFocusOnFormFieldImpl(form, form.fields()[1].global_id());
  CheckNoSuggestionsAvailableOnFieldFocus();
}

TEST_P(OnFocusOnFormFieldTest, CreditCardSuggestions_SecureContext) {
  // Set up our form data.
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  // Clear the form action.
  form.set_action(GURL());
  FormsSeen({form});

  manager().OnFocusOnFormFieldImpl(form, form.fields()[1].global_id());
  CheckSuggestionsAvailableIfScreenReaderRunning();
}

TEST_P(OnFocusOnFormFieldTest, CreditCardSuggestions_NonSecureContext) {
  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(/*is_https=*/false,
                                               /*use_month_type=*/false);
  // Clear the form action.
  form.set_action(GURL());
  FormsSeen({form});

  manager().OnFocusOnFormFieldImpl(form, form.fields()[1].global_id());
  // In a non-HTTPS context, there will be a warning indicating the page is
  // insecure.
  CheckSuggestionsAvailableIfScreenReaderRunning();
}

TEST_P(OnFocusOnFormFieldTest, CreditCardSuggestions_Ablation) {
  base::test::ScopedFeatureList scoped_feature_list;
  DisableAutofillViaAblation(scoped_feature_list, /*for_addresses=*/false,
                             /*for_credit_cards=*/true);

  // Set up our form data.
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  // Clear the form action.
  form.set_action(GURL());
  FormsSeen({form});

  manager().OnFocusOnFormFieldImpl(form, form.fields()[1].global_id());
  CheckNoSuggestionsAvailableOnFieldFocus();
}

// Ensure that focus events are properly reported to the AutofillFields.
TEST_P(OnFocusOnFormFieldTest, FocusReporting) {
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  form.set_fields(
      {CreateTestFormField("First name", "firstname", "",
                           FormControlType::kInputText, "given-name"),
       CreateTestFormField("Last Name", "lastname", "",
                           FormControlType::kInputText, "unrecognized")});

  // Observe form and retrieve pointers.
  FormsSeen({form});
  FormStructure* parsed_form = manager().FindCachedFormById(form.global_id());
  ASSERT_TRUE(parsed_form);
  const AutofillField* field0 =
      parsed_form->GetFieldById(form.fields()[0].global_id());
  const AutofillField* field1 =
      parsed_form->GetFieldById(form.fields()[1].global_id());
  ASSERT_TRUE(field0 && field1);

  // On page load nothing should be labeled as `was_focused`
  EXPECT_FALSE(field0->was_focused());
  EXPECT_FALSE(field1->was_focused());

  // Focus field0 and verify expectations.
  manager().OnFocusOnFormFieldImpl(form, form.fields()[0].global_id());
  EXPECT_TRUE(field0->was_focused());
  EXPECT_FALSE(field1->was_focused());

  // Focus field1 and verify expectations.
  manager().OnFocusOnFormFieldImpl(form, form.fields()[1].global_id());
  EXPECT_TRUE(field0->was_focused());
  EXPECT_TRUE(field1->was_focused());

  // Simulate that the forms were parsed again.
  FormsSeen({form});

  // Verify that the focus states carry over when the form is parsed again.
  parsed_form = manager().FindCachedFormById(form.global_id());
  ASSERT_TRUE(parsed_form);
  field0 = parsed_form->GetFieldById(form.fields()[0].global_id());
  field1 = parsed_form->GetFieldById(form.fields()[1].global_id());
  ASSERT_TRUE(field0 && field1);
  EXPECT_TRUE(field0->was_focused());
  EXPECT_TRUE(field1->was_focused());
}

INSTANTIATE_TEST_SUITE_P(All, OnFocusOnFormFieldTest, testing::Bool());

struct ShareNicknameTestParam {
  std::string local_nickname;
  std::string server_nickname;
  std::string expected_nickname;
};

const ShareNicknameTestParam kShareNicknameTestParam[] = {
    {"", "", ""},
    {"", "server nickname", "server nickname"},
    {"local nickname", "", "local nickname"},
    {"local nickname", "server nickname", "local nickname"},
    {"local nickname", "server nickname", "local nickname"},
};

class BrowserAutofillManagerTestForSharingNickname
    : public BrowserAutofillManagerTest,
      public testing::WithParamInterface<ShareNicknameTestParam> {
 public:
  BrowserAutofillManagerTestForSharingNickname()
      : local_nickname_(GetParam().local_nickname),
        server_nickname_(GetParam().server_nickname),
        expected_nickname_(GetParam().expected_nickname) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    feature_flags_.InitAndEnableFeature(
        features::kAutofillEnableNewFopDisplayDesktop);
#endif
  }

  CreditCard GetLocalCard() {
    CreditCard local_card("287151C8-6AB1-487C-9095-28E80BE5DA15",
                          test::kEmptyOrigin);
    test::SetCreditCardInfo(&local_card, "Clyde Barrow",
                            "378282246310005" /* American Express */, "04",
                            "2910", "1");
    local_card.usage_history().set_use_count(3);
    local_card.usage_history().set_use_date(base::Time::Now() - base::Days(1));
    local_card.SetNickname(base::UTF8ToUTF16(local_nickname_));
    local_card.set_guid(MakeGuid(1));
    return local_card;
  }

  CreditCard GetServerCard(const std::string& network) {
    CHECK(network == kVisaCard || network == kAmericanExpressCard);

    const std::string last_four = network == kVisaCard ? "3456" : "0005";
    const std::string expiry_year = network == kVisaCard ? "2999" : "2910";

    CreditCard masked_server_card(CreditCard::RecordType::kMaskedServerCard,
                                  "c789");
    test::SetCreditCardInfo(&masked_server_card, "Clyde Barrow",
                            last_four.c_str(), /*expiration_month=*/"04",
                            expiry_year.c_str(), /*billing_address_id=*/"1");
    masked_server_card.SetNetworkForMaskedCard(network);
    masked_server_card.SetNickname(base::UTF8ToUTF16(server_nickname_));
    masked_server_card.set_guid(MakeGuid(2));
    return masked_server_card;
  }

  base::test::ScopedFeatureList feature_flags_;
  std::string local_nickname_;
  std::string server_nickname_;
  std::string expected_nickname_;
};

INSTANTIATE_TEST_SUITE_P(,
                         BrowserAutofillManagerTestForSharingNickname,
                         testing::ValuesIn(kShareNicknameTestParam));

// Tests that when there is a duplicate local and server card that will be
// combined into a single suggestion, the merged suggestion inherits the correct
// expected nickname.
TEST_P(BrowserAutofillManagerTestForSharingNickname,
       VerifySuggestion_DuplicateCards) {
  personal_data().test_payments_data_manager().ClearCreditCards();
  ASSERT_EQ(0U,
            personal_data().payments_data_manager().GetCreditCards().size());
  CreditCard local_card = GetLocalCard();
  personal_data().payments_data_manager().AddCreditCard(local_card);
  personal_data().test_payments_data_manager().AddServerCreditCard(
      GetServerCard(kAmericanExpressCard));
  ASSERT_EQ(2U,
            personal_data().payments_data_manager().GetCreditCards().size());

  // Set up our form data.
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form});

  // Query by card number field.
  OnAskForValuesToFill(form, form.fields()[1]);

  external_delegate()->CheckSuggestions(
      form.fields()[1].global_id(),
      {GetCardSuggestion(kAmericanExpressCard, expected_nickname_),
       CreateSeparator(),
       CreateManageCreditCardsSuggestion(
           /*with_gpay_logo=*/true)});
}

// Tests that when there are two unrelated local and server cards, they are
// shown separately and each displays their own nickname.
TEST_P(BrowserAutofillManagerTestForSharingNickname,
       VerifySuggestion_UnrelatedCards) {
  personal_data().test_payments_data_manager().ClearCreditCards();
  ASSERT_EQ(0U,
            personal_data().payments_data_manager().GetCreditCards().size());
  CreditCard local_card = GetLocalCard();
  personal_data().payments_data_manager().AddCreditCard(local_card);

  std::vector<CreditCard> server_cards;
  CreditCard server_card = GetServerCard(kVisaCard);
  personal_data().test_payments_data_manager().AddServerCreditCard(server_card);

  ASSERT_EQ(2U,
            personal_data().payments_data_manager().GetCreditCards().size());

  // Set up our form data.
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form});

  // Query by card number field.
  OnAskForValuesToFill(form, form.fields()[1]);

  external_delegate()->CheckSuggestions(
      form.fields()[1].global_id(),
      {GetCardSuggestion(kAmericanExpressCard, local_nickname_),
       GetCardSuggestion(kVisaCard, server_nickname_), CreateSeparator(),
       CreateManageCreditCardsSuggestion(
           /*with_gpay_logo=*/false)});
}

// Tests that analyze metrics logging in case JavaScript clears a field
// immediately after it was filled.
class BrowserAutofillManagerClearFieldTest : public BrowserAutofillManagerTest {
 public:
  void SetUp() override {
    BrowserAutofillManagerTest::SetUp();

    // Set up a CC form.
    FormData form;
    form.set_url(GURL("https://myform.com/form.html"));
    form.set_action(GURL("https://myform.com/submit.html"));
    form.set_fields({CreateTestFormField("Name on Card", "nameoncard", "",
                                         FormControlType::kInputText),
                     CreateTestFormField("Card Number", "cardnumber", "",
                                         FormControlType::kInputText),
                     CreateTestFormField("Expiration date", "exp_date", "",
                                         FormControlType::kInputText)});

    // Notify BrowserAutofillManager of the form.
    FormsSeen({form});

    // Simulate filling and store the data to be filled in `fill_data_`.
    fill_data_ = FillAutofillFormDataAndGetResults(form, *form.fields().begin(),
                                                   MakeGuid(4));
    ASSERT_EQ(3u, fill_data_.fields().size());
    ExpectFilledField("Name on Card", "nameoncard", "Elvis Presley",
                      FormControlType::kInputText, fill_data_.fields()[0]);
    ExpectFilledField("Card Number", "cardnumber", "4234567890123456",
                      FormControlType::kInputText, fill_data_.fields()[1]);
    ExpectFilledField("Expiration date", "exp_date", "04/2999",
                      FormControlType::kInputText, fill_data_.fields()[2]);
  }

  void SimulateOverrideFieldByJavaScript(size_t field_index,
                                         const std::u16string& new_value) {
    std::u16string old_value = fill_data_.fields()[field_index].value();
    test_api(fill_data_).field(field_index).set_value(new_value);
    manager().OnJavaScriptChangedAutofilledValue(
        fill_data_, fill_data_.fields()[field_index].global_id(), old_value);
  }

  // Content of the form.
  FormData fill_data_;

  base::HistogramTester histogram_tester_;

  // Shorter alias of the Autofill.FormEvents we are interested in.
  const autofill_metrics::FormEvent kEvent = autofill_metrics::
      FORM_EVENT_AUTOFILLED_FIELD_CLEARED_BY_JAVASCRIPT_AFTER_FILL_ONCE;
};

// Ensure that we log the appropriate Autofill.FormEvents event if an autofilled
// field is cleared by JavaScript immediately after the filling.
TEST_F(BrowserAutofillManagerClearFieldTest, OneClearedField) {
  // Simulate that JavaScript clears the first field.
  SimulateOverrideFieldByJavaScript(0, u"");

  EXPECT_THAT(histogram_tester_.GetAllSamples("Autofill.FormEvents.CreditCard"),
              base::BucketsInclude(base::Bucket(kEvent, 1)));
}

// Ensure that we log a single Autofill.FormEvents event even if *two*
// autofilled fields are cleared by JavaScript immediately after the filling.
TEST_F(BrowserAutofillManagerClearFieldTest, TwoClearedFields) {
  // Simulate that JavaScript clears the first two field.
  SimulateOverrideFieldByJavaScript(0, u"");
  SimulateOverrideFieldByJavaScript(1, u"");

  EXPECT_THAT(histogram_tester_.GetAllSamples("Autofill.FormEvents.CreditCard"),
              base::BucketsInclude(base::Bucket(kEvent, 1)));
}

// Ensure that we do not log an Autofill.FormEvents event for the case that
// JavaScript modifies an autofilled field but does not clear it.
TEST_F(BrowserAutofillManagerClearFieldTest, ModifiedButDidNotClearField) {
  // Simulate that JavaScript modifies the value of the field but does not clear
  // the field.
  SimulateOverrideFieldByJavaScript(0, u"Elvis Aaron Presley");

  EXPECT_THAT(histogram_tester_.GetAllSamples("Autofill.FormEvents.CreditCard"),
              base::BucketsInclude(base::Bucket(kEvent, 0)));
}

// Ensure that we do not log an appropriate Autofill.FormEvents event if an
// autofilled field is cleared by JavaScript too long after it was filled.
TEST_F(BrowserAutofillManagerClearFieldTest, NoLoggingAfterDelay) {
  FastForwardBy(base::Seconds(5));

  // Simulate that JavaScript clears the first field.
  SimulateOverrideFieldByJavaScript(0, u"");

  EXPECT_THAT(histogram_tester_.GetAllSamples("Autofill.FormEvents.CreditCard"),
              base::BucketsInclude(base::Bucket(kEvent, 0)));
}

class BrowserAutofillManagerVotingTest : public BrowserAutofillManagerTest {
 public:
  void SetUp() override {
    BrowserAutofillManagerTest::SetUp();

    // All uploads should be expected explicitly.
    EXPECT_CALL(crowdsourcing_manager(), StartUploadRequest).Times(0);

    form_.set_name(u"MyForm");
    form_.set_url(GURL("https://myform.com/form.html"));
    form_.set_action(GURL("https://myform.com/submit.html"));
    form_.set_fields(
        {CreateTestFormField("First Name", "firstname", "",
                             FormControlType::kInputText, "given-name"),
         CreateTestFormField("Last Name", "lastname", "",
                             FormControlType::kInputText, "family-name")});

    // Set up our form data.
    FormsSeen({form_});
  }

  void SimulateTypingFirstNameIntoFirstField() {
    test_api(form_).field(0).set_value(u"Elvis");
    manager().OnTextFieldValueChanged(form_, form_.fields()[0].global_id(),
                                      base::TimeTicks::Now());
  }

 protected:
  FormData form_;
};

// Ensure that a vote is submitted after a regular form submission.
TEST_F(BrowserAutofillManagerVotingTest, Submission) {
  SimulateTypingFirstNameIntoFirstField();
  EXPECT_CALL(crowdsourcing_manager(),
              StartUploadRequest(
                  FirstElementIs(AllOf(
                      FormSignatureIs(CalculateFormSignature(form_)),
                      FieldsAre(FieldAutofillTypeIs(
                                    {FieldType::NAME_FIRST,
                                     FieldType::CREDIT_CARD_NAME_FIRST}),
                                FieldAutofillTypeIs({FieldType::EMPTY_TYPE})),
                      ObservedSubmissionIs(true))),
                  _, _));
  FormSubmitted(form_);
}

// Test that when modifying the form, a blur vote can be sent for the early
// version and a submission vote can be sent for the final version.
TEST_F(BrowserAutofillManagerVotingTest, DynamicFormSubmission) {
  // 1. Simulate typing.
  SimulateTypingFirstNameIntoFirstField();

  // 2. Simulate removing focus from the form, which triggers a blur vote.
  FormSignature first_form_signature = CalculateFormSignature(form_);
  manager().OnFocusOnNonFormField();

  // 3. Simulate typing into second field
  test_api(form_).field(1).set_value(u"Presley");
  manager().OnTextFieldValueChanged(form_, form_.fields()[1].global_id(),
                                    base::TimeTicks::Now());

  // 4. Simulate removing the focus from the form, which generates a second blur
  // vote which should be sent.
  EXPECT_CALL(
      crowdsourcing_manager(),
      StartUploadRequest(
          FirstElementIs(AllOf(
              FormSignatureIs(first_form_signature),
              FieldsAre(
                  FieldAutofillTypeIs({FieldType::NAME_FIRST,
                                       FieldType::CREDIT_CARD_NAME_FIRST}),
                  FieldAutofillTypeIs({FieldType::NAME_LAST,
                                       FieldType::CREDIT_CARD_NAME_LAST,
                                       FieldType::NAME_LAST_SECOND})),
              ObservedSubmissionIs(false))),
          _, _));
  manager().OnFocusOnNonFormField();

  // 5. Grow the form by one field, which changes the form signature.
  test_api(form_).Append(CreateTestFormField(
      "Zip code", "zip", "", FormControlType::kInputText, "postal-code"));
  FormsSeen({form_});

  // 6. Ensure that a form submission triggers votes for the new form.
  // Adding a field should have changed the form signature.
  FormSignature second_form_signature = CalculateFormSignature(form_);
  EXPECT_NE(first_form_signature, second_form_signature);
  // Because the next field after the two names is not a credit card field,
  // field disambiguation removes the credit card name votes.
  EXPECT_CALL(
      crowdsourcing_manager(),
      StartUploadRequest(
          FirstElementIs(AllOf(
              FormSignatureIs(second_form_signature),
              FieldsAre(FieldAutofillTypeIs({FieldType::NAME_FIRST}),
                        FieldAutofillTypeIs({FieldType::NAME_LAST,
                                             FieldType::NAME_LAST_SECOND}),
                        FieldAutofillTypeIs({FieldType::EMPTY_TYPE})),
              ObservedSubmissionIs(true))),
          _, _));
  FormSubmitted(form_);
}

// Ensure that a blur votes is sent after a navigation.
TEST_F(BrowserAutofillManagerVotingTest, BlurVoteOnNavigation) {
  SimulateTypingFirstNameIntoFirstField();

  // Simulate removing focus from form, which triggers a blur vote.
  EXPECT_CALL(crowdsourcing_manager(),
              StartUploadRequest(
                  FirstElementIs(AllOf(
                      FormSignatureIs(CalculateFormSignature(form_)),
                      FieldsAre(FieldAutofillTypeIs(
                                    {FieldType::NAME_FIRST,
                                     FieldType::CREDIT_CARD_NAME_FIRST}),
                                FieldAutofillTypeIs({FieldType::EMPTY_TYPE})),
                      ObservedSubmissionIs(false))),
                  _, _));
  manager().OnFocusOnNonFormField();

  // Simulate a navigation. This is when the vote is sent.
  test_api(client().GetAutofillDriverFactory()).Reset(driver());
}

// Ensure that a submission vote blocks sending a blur vote for the same form
// signature.
TEST_F(BrowserAutofillManagerVotingTest, NoBlurVoteOnSubmission) {
  SimulateTypingFirstNameIntoFirstField();

  // Simulate removing focus from form, which enqueues a blur vote. The blur
  // vote will be ignored and only the submission will be sent.
  manager().OnFocusOnNonFormField();
  EXPECT_CALL(crowdsourcing_manager(),
              StartUploadRequest(
                  FirstElementIs(AllOf(
                      FormSignatureIs(CalculateFormSignature(form_)),
                      FieldsAre(FieldAutofillTypeIs(
                                    {FieldType::NAME_FIRST,
                                     FieldType::CREDIT_CARD_NAME_FIRST}),
                                FieldAutofillTypeIs({FieldType::EMPTY_TYPE})),
                      ObservedSubmissionIs(true))),
                  _, _));
  FormSubmitted(form_);
}

// Test that the call is properly forwarded to its SingleFieldFillRouter.
TEST_F(BrowserAutofillManagerTest, OnSingleFieldSuggestionSelected) {
  std::u16string test_value = u"TestValue";
  FormData form = test::CreateTestAddressFormData();
  FormFieldData& field = test_api(form).field(0);

  Suggestion autocomplete_suggestion(test_value,
                                     SuggestionType::kAutocompleteEntry);
  EXPECT_CALL(single_field_fill_router(),
              OnSingleFieldSuggestionSelected(autocomplete_suggestion));

  manager().OnSingleFieldSuggestionSelected(
      autocomplete_suggestion, form.global_id(), field.global_id());

  EXPECT_CALL(single_field_fill_router(),
              OnSingleFieldSuggestionSelected(autocomplete_suggestion));

  manager().OnSingleFieldSuggestionSelected(
      autocomplete_suggestion, form.global_id(), field.global_id());

  Suggestion iban_suggestion(test_value, SuggestionType::kIbanEntry);
  EXPECT_CALL(single_field_fill_router(),
              OnSingleFieldSuggestionSelected(iban_suggestion));

  manager().OnSingleFieldSuggestionSelected(iban_suggestion, form.global_id(),
                                            field.global_id());

  Suggestion merchant_promo_suggestion(test_value,
                                       SuggestionType::kMerchantPromoCodeEntry);
  EXPECT_CALL(single_field_fill_router(),
              OnSingleFieldSuggestionSelected(merchant_promo_suggestion));

  manager().OnSingleFieldSuggestionSelected(
      merchant_promo_suggestion, form.global_id(), field.global_id());
}

// Test that we correctly fill an address form and update the used profile.
TEST_F(BrowserAutofillManagerTest, FillAddressForm_UpdateProfile) {
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FULL, .autocomplete_attribute = "name"},
                  {.role = ADDRESS_HOME_LINE1,
                   .autocomplete_attribute = "address-line1"}}});
  FormsSeen({form});

  // Create a profile and add it to the PDM.
  personal_data().test_address_data_manager().ClearProfiles();
  AutofillProfile profile = test::GetFullProfile();
  profile.usage_history().set_use_date(base::Time::Now());
  profile.usage_history().set_use_count(1u);
  personal_data().address_data_manager().AddProfile(profile);
  const AutofillProfile* pdm_profile =
      personal_data().address_data_manager().GetProfileByGUID(profile.guid());
  ASSERT_TRUE(pdm_profile);

  FastForwardBy(base::Hours(1));
  const base::Time hour_later = base::Time::Now();

  FillAutofillFormData(form, form.fields()[0], pdm_profile->guid());
  EXPECT_EQ(2U, pdm_profile->usage_history().use_count());
  EXPECT_LE(hour_later, pdm_profile->usage_history().use_date());
}

// Tests that `ProfileTokenQuality` is correctly integrated into
// `AutofillProfile` and that on form submit, observations are collected.
TEST_F(BrowserAutofillManagerTest, FillAddressForm_CollectObservations) {
  personal_data().test_address_data_manager().ClearProfiles();
  AutofillProfile profile = test::GetFullProfile();
  // This is needed to not get an update prompt that would compromise the test.
  test_api(profile).set_record_type(AutofillProfile::RecordType::kAccount);
  test_api(profile.token_quality()).disable_randomization();
  personal_data().address_data_manager().AddProfile(profile);
  const AutofillProfile* pdm_profile =
      personal_data().address_data_manager().GetProfileByGUID(profile.guid());
  ASSERT_TRUE(pdm_profile);

  // Create and fill an address form with the `profile`.
  FormData form =
      test::GetFormData({.fields = {{.role = NAME_FULL},
                                    {.role = ADDRESS_HOME_LINE1},
                                    {.role = ADDRESS_HOME_COUNTRY}}});
  FormsSeen({form});
  FormData filled_form = FillAutofillFormDataAndGetResults(
      form, form.fields()[0], pdm_profile->guid());

  // Expect that no observations for any of the form's types were collected yet.
  FormStructure* form_structure =
      manager().FindCachedFormById(form.global_id());
  EXPECT_TRUE(std::ranges::all_of(
      *form_structure,
      [&pdm_profile](const std::unique_ptr<AutofillField>& field) {
        return pdm_profile->token_quality()
            .GetObservationTypesForFieldType(field->Type().GetStorableType())
            .empty();
      }));
  // Submit the form and expect observations for all of the form's types. This
  // updates the `profile` in `personal_data()`, invalidating the pointer.
  FormSubmitted(filled_form);
  pdm_profile =
      personal_data().address_data_manager().GetProfileByGUID(profile.guid());
  ASSERT_TRUE(pdm_profile);
  EXPECT_TRUE(std::ranges::none_of(
      *form_structure,
      [&pdm_profile](const std::unique_ptr<AutofillField>& field) {
        return pdm_profile->token_quality()
            .GetObservationTypesForFieldType(field->Type().GetStorableType())
            .empty();
      }));
}

TEST_F(BrowserAutofillManagerTest,
       AddressSuggestionOnTyping_ShownWhenNoOtherSuggestionExists) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillAddressSuggestionsOnTyping};
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  const std::u16string address_home_line1 = u"sherman wallaby 42 sidney";
  profile.SetInfo(ADDRESS_HOME_LINE1, address_home_line1, "en-US");
  personal_data().test_address_data_manager().ClearProfiles();
  personal_data().test_address_data_manager().AddProfile(profile);

  FormData form;
  form.set_name(u"NothingSpecial");
  // Note the value is the first 3 characters of the `ADDRESS_HOME_LINE1` value.
  form.set_fields({CreateTestFormField("Something", "something", "she",
                                       FormControlType::kInputText)});
  // Autocomplete suggestions (and all others for that matter) should be empty
  // in order to `SuggestionType::kAddressEntryOnTyping` to exist.
  EXPECT_CALL(merchant_promo_code_manager(), OnGetSingleFieldSuggestions)
      .WillRepeatedly(Return(false));
  EXPECT_CALL(iban_manager(), OnGetSingleFieldSuggestions)
      .WillRepeatedly(Return(false));
  EXPECT_CALL(autocomplete_history_manager(), OnGetSingleFieldSuggestions)
      .WillRepeatedly(Return(false));
  FormsSeen({form});
  OnAskForValuesToFill(form, form.fields()[0]);

  EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());
  const std::vector<Suggestion>& suggestions =
      external_delegate()->suggestions();
  ASSERT_TRUE(suggestions.size() > 0);
  // Assert that the first suggestion is of type
  // `SuggestionType::kAddressEntryOnTyping` and its text is the profile value
  // for `ADDRESS_HOME_LINE1`.
  EXPECT_THAT(suggestions[0],
              EqualsSuggestion(SuggestionType::kAddressEntryOnTyping,
                               address_home_line1));
}

// Tests that the form signatures of import-worthy forms submitted shortly after
// another (within a 5 minutes time window) are set accordingly in their form
// associations. Regression test for crbug.com/395812863.
TEST_F(BrowserAutofillManagerTest,
       FormAssociationSetOnFilledAndSubmittedForms) {
  // The `url` of the forms created in this test. Forms need to share the same
  // origin to be associated with each other.
  const GURL url = GURL("https://myform.com/form.html");
  // Used for setting `unique_id` in `CreateTestAddressFormData()` that will
  // make the form's signature unique.
  size_t address_form_unique_id = 0;
  // Creates an address or credit card form, fills and submits it. Lastly,
  // returns a pointer to the `FormStructure`.
  auto create_fill_submit_and_find_cached_form =
      [this, &url,
       &address_form_unique_id](bool is_credit_card_form) -> FormStructure* {
    FormData form =
        is_credit_card_form
            ? CreateTestCreditCardFormData(/*is_https=*/true,
                                           /*use_month_type=*/false)
            : CreateTestAddressFormData(
                  base::NumberToString(++address_form_unique_id).c_str());
    form.set_url(url);
    form.set_main_frame_origin(url::Origin::Create(url));

    FormsSeen({form});

    FormData filled_form = FillAutofillFormDataAndGetResults(
        form, form.fields()[0],
        is_credit_card_form ? MakeGuid(4) : kElvisProfileGuid);

    FormSubmitted(filled_form);

    return manager().FindCachedFormById(filled_form.global_id());
  };

  const FormStructure::FormAssociations& last_uploaded_form_associations =
      client().GetVotesUploader().get_last_uploaded_form_associations();

  // After the `first_address_form` was submitted, expect that its form
  // signature is set to the `last_address_form_submitted` on its form
  // associations.
  FormStructure* first_address_form =
      create_fill_submit_and_find_cached_form(/*is_credit_card_form=*/false);
  ASSERT_TRUE(first_address_form);
  EXPECT_THAT(last_uploaded_form_associations.last_address_form_submitted,
              Optional(first_address_form->form_signature()));
  EXPECT_EQ(last_uploaded_form_associations.second_last_address_form_submitted,
            std::nullopt);
  EXPECT_EQ(last_uploaded_form_associations.last_credit_card_form_submitted,
            std::nullopt);

  // After the `second_address_form` was submitted, expect that its form
  // signature is set to the `last_address_form_submitted` on its form
  // associations. The signature of the `first_address_form` is expected to be
  // the `second_last_address_form_signature` now.
  FormStructure* second_address_form =
      create_fill_submit_and_find_cached_form(/*is_credit_card_form=*/false);
  ASSERT_TRUE(second_address_form);
  EXPECT_THAT(last_uploaded_form_associations.last_address_form_submitted,
              Optional(second_address_form->form_signature()));
  EXPECT_THAT(
      last_uploaded_form_associations.second_last_address_form_submitted,
      Optional(first_address_form->form_signature()));
  EXPECT_EQ(last_uploaded_form_associations.last_credit_card_form_submitted,
            std::nullopt);

  // Expect that `last_credit_card_form_submitted` is also set with the form
  // submission of the `credit_card_form`. The address form signatures are
  // expected to be set before.
  FormStructure* credit_card_form =
      create_fill_submit_and_find_cached_form(/*is_credit_card_form=*/true);
  ASSERT_TRUE(credit_card_form);
  EXPECT_THAT(last_uploaded_form_associations.last_address_form_submitted,
              Optional(second_address_form->form_signature()));
  EXPECT_THAT(
      last_uploaded_form_associations.second_last_address_form_submitted,
      Optional(first_address_form->form_signature()));
  EXPECT_THAT(last_uploaded_form_associations.last_credit_card_form_submitted,
              Optional(credit_card_form->form_signature()));
}

class BrowserAutofillManagerPlusAddressTest
    : public BrowserAutofillManagerTest {
 protected:
  void SetUp() override {
    BrowserAutofillManagerTest::SetUp();
    auto plus_address_delegate =
        std::make_unique<NiceMock<MockAutofillPlusAddressDelegate>>();
    ON_CALL(*plus_address_delegate, GetManagePlusAddressSuggestion)
        .WillByDefault(Return(Suggestion(SuggestionType::kManagePlusAddress)));
    ON_CALL(*plus_address_delegate, IsPlusAddressFillingEnabled)
        .WillByDefault(Return(true));
    ON_CALL(*plus_address_delegate, IsPlusAddressFullFormFillingEnabled)
        .WillByDefault(Return(true));
    client().set_plus_address_delegate(std::move(plus_address_delegate));
  }

  MockAutofillPlusAddressDelegate& plus_address_delegate() {
    return static_cast<MockAutofillPlusAddressDelegate&>(
        *client().GetPlusAddressDelegate());
  }
};

// Ensure that plus address options aren't queried for non-email fields.
TEST_F(BrowserAutofillManagerPlusAddressTest, NoPlusAddressesWithNameFields) {
  const std::vector<std::string> plus_addresses = {kPlusAddress};
  EXPECT_CALL(plus_address_delegate(), IsFieldEligibleForPlusAddress)
      .WillRepeatedly([&](const AutofillField& f) {
        return autofill::GetFillingProductFromFieldTypeGroup(
                   f.Type().group()) == FillingProduct::kAddress;
      });
  EXPECT_CALL(plus_address_delegate(), GetAffiliatedPlusAddresses)
      .WillOnce(RunOnceCallback<1>(plus_addresses));
  EXPECT_CALL(plus_address_delegate(), GetSuggestionsFromPlusAddresses(
                                           plus_addresses, _, _, _, _, _, _, _))
      .Times(0);
  // Set up our form data.
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FIRST, .autocomplete_attribute = "given-name"},
                  {.role = NAME_LAST}}});
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  FormsSeen({form});

  // Check that suggestions are made for the field that has the autocomplete
  // attribute. Ensure that there is no plus address option shown.
  OnAskForValuesToFill(form, form.fields()[0]);
  EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());

  // Also check that there are no suggestions for the field without the
  // autocomplete attribute, ensuring that unrecognized fields don't get plus
  // address options.
  OnAskForValuesToFill(form, form.fields()[1]);
  EXPECT_FALSE(external_delegate()->on_suggestions_returned_seen());
}

// Tests that plus address suggestions are queried and shown for email fields
// when address suggestions are available. In this case, the option to manage
// plus addresses is not offered.
TEST_F(BrowserAutofillManagerPlusAddressTest,
       CreatePlusAddressSuggestionShownWithAddressSuggestions) {
  using enum AutofillPlusAddressDelegate::SuggestionContext;
  using enum PasswordFormClassification::Type;

  // Plus address suggestions request.
  const std::vector<std::string> plus_addresses = {kPlusAddress};
  EXPECT_CALL(plus_address_delegate(), IsFieldEligibleForPlusAddress)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(plus_address_delegate(), GetAffiliatedPlusAddresses)
      .WillOnce(RunOnceCallback<1>(plus_addresses));
  EXPECT_CALL(plus_address_delegate(), GetSuggestionsFromPlusAddresses(
                                           plus_addresses, _, _, _, _, _, _, _))
      .WillOnce(Return(std::vector<Suggestion>{
          Suggestion(SuggestionType::kFillExistingPlusAddress)}));
  // No single field form fill suggestions requests.
  EXPECT_CALL(merchant_promo_code_manager(), OnGetSingleFieldSuggestions)
      .Times(0);
  EXPECT_CALL(iban_manager(), OnGetSingleFieldSuggestions).Times(0);
  EXPECT_CALL(autocomplete_history_manager(), OnGetSingleFieldSuggestions)
      .Times(0);

  EXPECT_CALL(plus_address_delegate(),
              OnPlusAddressSuggestionShown(
                  Ref(manager()), _, _, kAutofillProfileOnEmailField,
                  kNoPasswordForm, SuggestionType::kFillExistingPlusAddress));

  // Set up our form data. Notably, the first field is an email address.
  FormData form = test::GetFormData(
      {.fields = {{.role = EMAIL_ADDRESS, .autocomplete_attribute = "email"}}});
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));

  FormsSeen({form});

  // Check that the plus address suggestion is offered together with address
  // suggestions.
  OnAskForValuesToFill(form, form.fields()[0]);
  EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());
  EXPECT_THAT(
      external_delegate()->suggestions(),
      ElementsAre(EqualsSuggestion(SuggestionType::kFillExistingPlusAddress),
                  EqualsSuggestion(SuggestionType::kAddressEntry),
                  EqualsSuggestion(SuggestionType::kAddressEntry),
                  EqualsSuggestion(SuggestionType::kSeparator),
                  EqualsSuggestion(SuggestionType::kManageAddress)));
}

class BrowserAutofillManagerIdentityCredentialTest
    : public BrowserAutofillManagerTest {
 protected:
  void SetUp() override {
    BrowserAutofillManagerTest::SetUp();
    auto identity_credential_delegate =
        std::make_unique<NiceMock<MockIdentityCredentialDelegate>>();
    client().set_identity_credential_delegate(
        std::move(identity_credential_delegate));
  }

  MockIdentityCredentialDelegate& identity_credential_delegate() {
    return static_cast<MockIdentityCredentialDelegate&>(
        *client().GetIdentityCredentialDelegate());
  }
};

// Tests that verified fields are shown above unverified fields.
TEST_F(BrowserAutofillManagerIdentityCredentialTest,
       CreateVerifiedEmailSuggestionShownBeforeAddressSuggestions) {
  EXPECT_CALL(identity_credential_delegate(), GetVerifiedAutofillSuggestions)
      .WillOnce(testing::WithArg<0>([](const FieldType& field_type) {
        std::vector<Suggestion> suggestions = {
            Suggestion(SuggestionType::kIdentityCredential)};
        return suggestions;
      }));

  // Set up our form data. Notably, the first field is an email address
  // with webidentity.
  FormData form = test::GetFormData(
      {.fields = {{.role = EMAIL_ADDRESS,
                   .autocomplete_attribute = "email webidentity"}}});
  FormsSeen({form});
  // Check that the plus address suggestion is offered together with address
  // suggestions.
  OnAskForValuesToFill(form, form.fields()[0]);
  EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());
  EXPECT_THAT(external_delegate()->suggestions(),
              ElementsAre(EqualsSuggestion(SuggestionType::kIdentityCredential),
                          EqualsSuggestion(SuggestionType::kAddressEntry),
                          EqualsSuggestion(SuggestionType::kAddressEntry),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          EqualsSuggestion(SuggestionType::kManageAddress)));
}

// Tests that verified fields are not shown with the email field alone.
TEST_F(BrowserAutofillManagerIdentityCredentialTest,
       EmailFieldAloneDoesNotTriggerIdentityCredentialSuggestion) {
  EXPECT_CALL(identity_credential_delegate(), GetVerifiedAutofillSuggestions)
      .Times(0);

  // Set up our form data. Notably, the first field is an email address
  // without webidentity.
  FormData form = test::GetFormData(
      {.fields = {{.role = EMAIL_ADDRESS, .autocomplete_attribute = "email"}}});
  FormsSeen({form});
  OnAskForValuesToFill(form, form.fields()[0]);
  EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());
  EXPECT_THAT(
      external_delegate()->suggestions(),
      Not(Contains(EqualsSuggestion(SuggestionType::kIdentityCredential))));
}

// Tests that plus address suggestions are queried and shown for email fields
// when no single field form suggestions are available. In this case, a
// ManagePlusAddress suggestion is also offered.
TEST_F(BrowserAutofillManagerPlusAddressTest,
       CreatePlusAddressSuggestionShown) {
  using enum AutofillPlusAddressDelegate::SuggestionContext;
  using enum PasswordFormClassification::Type;
  personal_data().test_address_data_manager().ClearProfiles();

  // Plus address suggestions request.
  EXPECT_CALL(plus_address_delegate(), IsFieldEligibleForPlusAddress)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(plus_address_delegate(), GetAffiliatedPlusAddresses)
      .WillOnce(RunOnceCallback<1>(std::vector<std::string>{}));
  EXPECT_CALL(plus_address_delegate(), GetSuggestionsFromPlusAddresses)
      .WillOnce(Return(std::vector<Suggestion>{
          Suggestion(SuggestionType::kCreateNewPlusAddress)}));
  // Single field form fill suggestions request - No results.
  EXPECT_CALL(merchant_promo_code_manager(), OnGetSingleFieldSuggestions)
      .WillRepeatedly(Return(false));
  EXPECT_CALL(iban_manager(), OnGetSingleFieldSuggestions)
      .WillRepeatedly(Return(false));
  EXPECT_CALL(autocomplete_history_manager(), OnGetSingleFieldSuggestions)
      .WillRepeatedly(Return(false));

  EXPECT_CALL(plus_address_delegate(),
              OnPlusAddressSuggestionShown(
                  Ref(manager()), _, _, kAutocomplete, kNoPasswordForm,
                  SuggestionType::kCreateNewPlusAddress));

  // Set up our form data. Notably, the first field is an email address.
  FormData form = test::GetFormData(
      {.fields = {{.role = EMAIL_ADDRESS, .autocomplete_attribute = "email"}}});
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));

  FormsSeen({form});

  // Check that the plus address suggestion is offered.
  OnAskForValuesToFill(form, form.fields()[0]);
  EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());
  EXPECT_THAT(
      external_delegate()->suggestions(),
      ElementsAre(EqualsSuggestion(SuggestionType::kCreateNewPlusAddress),
                  EqualsSuggestion(SuggestionType::kSeparator),
                  EqualsSuggestion(SuggestionType::kManagePlusAddress)));
}

// Tests that single field form suggestions (IBANs in this case) are shown
// normally if plus address suggestions are not available for the field.
TEST_F(BrowserAutofillManagerPlusAddressTest,
       NoPlusAddressOnlyIBANsSuggestions) {
  using enum AutofillPlusAddressDelegate::SuggestionContext;
  using enum PasswordFormClassification::Type;
  personal_data().test_address_data_manager().ClearProfiles();

  // No plus address suggestions request.
  EXPECT_CALL(plus_address_delegate(), GetAffiliatedPlusAddresses).Times(0);
  EXPECT_CALL(plus_address_delegate(), GetSuggestionsFromPlusAddresses)
      .Times(0);
  // Single field form fill suggestions request.
  EXPECT_CALL(merchant_promo_code_manager(), OnGetSingleFieldSuggestions)
      .WillRepeatedly(Return(false));
  EXPECT_CALL(iban_manager(), OnGetSingleFieldSuggestions)
      .WillRepeatedly([&](const FormStructure&, const FormFieldData& field,
                          const AutofillField&, const AutofillClient&,
                          SingleFieldFillRouter::OnSuggestionsReturnedCallback&
                              on_suggestions_returned) {
        std::move(on_suggestions_returned)
            .Run(field.global_id(),
                 std::vector<Suggestion>{
                     Suggestion(SuggestionType::kIbanEntry),
                     Suggestion(SuggestionType::kIbanEntry),
                     Suggestion(SuggestionType::kSeparator),
                     Suggestion(SuggestionType::kManageIban)});
        return true;
      });

  EXPECT_CALL(plus_address_delegate(), OnPlusAddressSuggestionShown).Times(0);

  // Set up our form data. Notably, the first field is an IBAN field.
  FormData form = CreateTestIbanFormData();
  FormsSeen({form});

  // Check that only IBAN related suggestions are offered.
  OnAskForValuesToFill(form, form.fields()[0]);
  EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());
  EXPECT_THAT(external_delegate()->suggestions(),
              ElementsAre(EqualsSuggestion(SuggestionType::kIbanEntry),
                          EqualsSuggestion(SuggestionType::kIbanEntry),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          EqualsSuggestion(SuggestionType::kManageIban)));
}

// Tests that single field form suggestions (Merchant promo code in this case)
// are shown normally if plus address suggestions are not available for the
// field.
TEST_F(BrowserAutofillManagerPlusAddressTest,
       NoPlusAddressOnlyPromoCodesSuggestions) {
  using enum AutofillPlusAddressDelegate::SuggestionContext;
  using enum PasswordFormClassification::Type;
  personal_data().test_address_data_manager().ClearProfiles();

  // No plus address suggestions request.
  EXPECT_CALL(plus_address_delegate(), GetAffiliatedPlusAddresses).Times(0);
  EXPECT_CALL(plus_address_delegate(), GetSuggestionsFromPlusAddresses)
      .Times(0);
  // Single field form fill suggestions request.
  EXPECT_CALL(merchant_promo_code_manager(), OnGetSingleFieldSuggestions)
      .WillRepeatedly([&](const FormStructure&, const FormFieldData& field,
                          const AutofillField&, const AutofillClient&,
                          SingleFieldFillRouter::OnSuggestionsReturnedCallback&
                              on_suggestions_returned) {
        std::move(on_suggestions_returned)
            .Run(field.global_id(),
                 std::vector<Suggestion>{
                     Suggestion(SuggestionType::kMerchantPromoCodeEntry),
                     Suggestion(SuggestionType::kMerchantPromoCodeEntry),
                     Suggestion(SuggestionType::kSeparator),
                     Suggestion(SuggestionType::kSeePromoCodeDetails)});
        return true;
      });

  EXPECT_CALL(plus_address_delegate(), OnPlusAddressSuggestionShown).Times(0);

  // Set up our form data. Notably, the first field is a promo code field.
  FormData form = test::CreateTestMerchantPromoCodeFormData();
  FormsSeen({form});

  // Check that only promo code related suggestions are offered.
  OnAskForValuesToFill(form, form.fields()[0]);
  EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());
  EXPECT_THAT(
      external_delegate()->suggestions(),
      ElementsAre(EqualsSuggestion(SuggestionType::kMerchantPromoCodeEntry),
                  EqualsSuggestion(SuggestionType::kMerchantPromoCodeEntry),
                  EqualsSuggestion(SuggestionType::kSeparator),
                  EqualsSuggestion(SuggestionType::kSeePromoCodeDetails)));
}

// Tests that plus address suggestions are queried and shown for email fields
// when single field form suggestions are available. Tests also that plus
// address suggestions are prioritized over single field form fill suggestions.
TEST_F(BrowserAutofillManagerPlusAddressTest,
       CreatePlusAddressSuggestionShownWithSingleFieldFillSuggestions) {
  using enum AutofillPlusAddressDelegate::SuggestionContext;
  using enum PasswordFormClassification::Type;
  personal_data().test_address_data_manager().ClearProfiles();

  // Plus address suggestions request.
  const std::vector<std::string> plus_addresses = {kPlusAddress};
  EXPECT_CALL(plus_address_delegate(), IsFieldEligibleForPlusAddress)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(plus_address_delegate(), GetAffiliatedPlusAddresses)
      .WillOnce(RunOnceCallback<1>(plus_addresses));
  EXPECT_CALL(plus_address_delegate(), GetSuggestionsFromPlusAddresses(
                                           plus_addresses, _, _, _, _, _, _, _))
      .WillOnce(Return(std::vector<Suggestion>{
          Suggestion(SuggestionType::kFillExistingPlusAddress)}));
  // Single field form fill suggestions request.
  EXPECT_CALL(merchant_promo_code_manager(), OnGetSingleFieldSuggestions)
      .WillRepeatedly(Return(false));
  EXPECT_CALL(iban_manager(), OnGetSingleFieldSuggestions)
      .WillRepeatedly(Return(false));
  EXPECT_CALL(autocomplete_history_manager(), OnGetSingleFieldSuggestions)
      .WillRepeatedly([&](const FormFieldData& field, const AutofillClient&,
                          SingleFieldFillRouter::OnSuggestionsReturnedCallback&
                              on_suggestions_returned) {
        std::move(on_suggestions_returned)
            .Run(field.global_id(),
                 std::vector<Suggestion>{
                     Suggestion(SuggestionType::kAutocompleteEntry),
                     Suggestion(SuggestionType::kAutocompleteEntry)});
        return true;
      });

  EXPECT_CALL(plus_address_delegate(),
              OnPlusAddressSuggestionShown(
                  Ref(manager()), _, _, kAutocomplete, kNoPasswordForm,
                  SuggestionType::kFillExistingPlusAddress));

  // Set up our form data. Notably, the first field is an email address.
  FormData form = test::GetFormData(
      {.fields = {{.role = EMAIL_ADDRESS, .autocomplete_attribute = "email"}}});
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));

  FormsSeen({form});

  // Check that the plus address suggestion is offered.
  OnAskForValuesToFill(form, form.fields()[0]);
  EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());
  EXPECT_THAT(
      external_delegate()->suggestions(),
      ElementsAre(EqualsSuggestion(SuggestionType::kFillExistingPlusAddress),
                  EqualsSuggestion(SuggestionType::kAutocompleteEntry),
                  EqualsSuggestion(SuggestionType::kAutocompleteEntry),
                  EqualsSuggestion(SuggestionType::kSeparator),
                  EqualsSuggestion(SuggestionType::kManagePlusAddress)));
}

// Tests that a manage plus address suggestion is not added if there are no plus
// address suggestions.
TEST_F(BrowserAutofillManagerPlusAddressTest,
       NoStandaloneManagePlusAddressSuggestion) {
  using enum AutofillPlusAddressDelegate::SuggestionContext;
  using enum PasswordFormClassification::Type;
  personal_data().test_address_data_manager().ClearProfiles();
  EXPECT_CALL(plus_address_delegate(), IsFieldEligibleForPlusAddress)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(plus_address_delegate(), GetAffiliatedPlusAddresses)
      .WillOnce(RunOnceCallback<1>(std::vector<std::string>{}));
  EXPECT_CALL(plus_address_delegate(), GetSuggestionsFromPlusAddresses)
      .WillOnce(Return(std::vector<Suggestion>{}));
  EXPECT_CALL(plus_address_delegate(), GetManagePlusAddressSuggestion).Times(0);
  EXPECT_CALL(plus_address_delegate(), OnPlusAddressSuggestionShown).Times(0);
  // Single field form fill suggestions request - No results.
  EXPECT_CALL(merchant_promo_code_manager(), OnGetSingleFieldSuggestions)
      .WillRepeatedly(Return(false));
  EXPECT_CALL(iban_manager(), OnGetSingleFieldSuggestions)
      .WillRepeatedly(Return(false));
  EXPECT_CALL(autocomplete_history_manager(), OnGetSingleFieldSuggestions)
      .WillRepeatedly(Return(false));

  // Set up our form data. Notably, the first field is an email address.
  FormData form = test::GetFormData(
      {.fields = {{.role = EMAIL_ADDRESS, .autocomplete_attribute = "email"}}});
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));

  FormsSeen({form});

  // Check that no suggestions are offered.
  OnAskForValuesToFill(form, form.fields()[0]);
  EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());
  EXPECT_THAT(external_delegate()->suggestions(), IsEmpty());
}

// Tests that only Plus Address suggestions are shown when the trigger source is
// a manual fallback for plus addresses.
TEST_F(BrowserAutofillManagerPlusAddressTest, ManualFallbackPlusAddress) {
  using enum AutofillPlusAddressDelegate::SuggestionContext;
  using enum PasswordFormClassification::Type;
  EXPECT_CALL(plus_address_delegate(), GetAffiliatedPlusAddresses)
      .WillOnce(RunOnceCallback<1>(std::vector<std::string>{}));
  EXPECT_CALL(
      plus_address_delegate(),
      GetSuggestionsFromPlusAddresses(
          _, _, _, _, _, _, _,
          AutofillSuggestionTriggerSource::kManualFallbackPlusAddresses))
      .WillOnce(Return(std::vector<Suggestion>{
          Suggestion(SuggestionType::kCreateNewPlusAddress)}));
  EXPECT_CALL(plus_address_delegate(),
              OnPlusAddressSuggestionShown(
                  Ref(manager()), _, _, kManualFallback, kNoPasswordForm,
                  SuggestionType::kCreateNewPlusAddress));
  EXPECT_CALL(merchant_promo_code_manager(), OnGetSingleFieldSuggestions)
      .Times(0);
  EXPECT_CALL(iban_manager(), OnGetSingleFieldSuggestions).Times(0);
  EXPECT_CALL(autocomplete_history_manager(), OnGetSingleFieldSuggestions)
      .Times(0);

  FormData form = CreateTestAddressFormData();
  FormsSeen({form});

  // Check that only the plus address suggestion is offered.
  OnAskForValuesToFill(
      form, form.fields()[0],
      AutofillSuggestionTriggerSource::kManualFallbackPlusAddresses);
  EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());
  EXPECT_THAT(
      external_delegate()->suggestions(),
      ElementsAre(EqualsSuggestion(SuggestionType::kCreateNewPlusAddress),
                  EqualsSuggestion(SuggestionType::kSeparator),
                  EqualsSuggestion(SuggestionType::kManagePlusAddress)));
}

// Test that plus address inputs are forced to !should_autocomplete
// for `SingleFieldFillRouter::OnWillSubmitForm()`.
TEST_F(BrowserAutofillManagerPlusAddressTest,
       DontSaveActivePlusAddressInAutocompleteHistory) {
  const std::string kDummyPlusAddress = "plus+plus@plus.plus";
  ON_CALL(plus_address_delegate(), IsPlusAddress)
      .WillByDefault([&](const std::string& address) {
        return address == kDummyPlusAddress;
      });
  FormData form_seen_by_autocomplete;
  EXPECT_CALL(single_field_fill_router(),
              OnWillSubmitForm(_, _, /*is_autocomplete_enabled=*/true))
      .WillOnce(SaveArg<0>(&form_seen_by_autocomplete));

  FormData form = test::GetFormData(
      {.fields = {{.role = EMAIL_ADDRESS, .name = u"email"},
                  {.role = EMAIL_ADDRESS, .name = u"unfilled-email"}}});

  // First, note the field with the empty value.
  FormsSeen({form});
  // Then fill in the dummy plus address.
  test_api(form).field(0).set_value(base::UTF8ToUTF16(kDummyPlusAddress));

  // Submit the form, capturing it as it is passed to the autocomplete history
  // manager. The first field should not be autocomplete eligible.
  FormSubmitted(form);

  EXPECT_EQ(form.fields().size(), form_seen_by_autocomplete.fields().size());
  EXPECT_FALSE(form_seen_by_autocomplete.fields()[0].should_autocomplete());
  EXPECT_TRUE(form_seen_by_autocomplete.fields()[1].should_autocomplete());
}

// Test that inputs matching the plus address format are forced to
// !should_autocomplete for `SingleFieldFillRouter::OnWillSubmitForm()`.
TEST_F(BrowserAutofillManagerPlusAddressTest,
       DontSaveMatchedPlusAddressInAutocompleteHistory) {
  const std::u16string kInvalidPlusAddressFormat = u"plus+plus@plus.plus";
  const std::u16string kValidPlusAddressFormat = u"plus+plus@grelay.com";
  ON_CALL(plus_address_delegate(), IsPlusAddress).WillByDefault(Return(false));
  ON_CALL(plus_address_delegate(), MatchesPlusAddressFormat)
      .WillByDefault([&](const std::u16string& address) {
        return address.ends_with(u"@grelay.com");
      });
  FormData form_seen_by_autocomplete;
  EXPECT_CALL(single_field_fill_router(),
              OnWillSubmitForm(_, _, /*is_autocomplete_enabled=*/true))
      .WillOnce(SaveArg<0>(&form_seen_by_autocomplete));

  FormData form = test::GetFormData(
      {.fields = {{.role = EMAIL_ADDRESS, .name = u"email"},
                  {.role = EMAIL_ADDRESS, .name = u"unfilled-email"}}});

  // First, note the field with the empty value.
  FormsSeen({form});

  test_api(form).field(0).set_value(kInvalidPlusAddressFormat);
  test_api(form).field(1).set_value(kValidPlusAddressFormat);

  // Submit the form, capturing it as it is passed to the autocomplete history
  // manager. The first field should not be autocomplete eligible.
  FormSubmitted(form);

  EXPECT_EQ(form.fields().size(), form_seen_by_autocomplete.fields().size());
  EXPECT_TRUE(form_seen_by_autocomplete.fields()[0].should_autocomplete());
  EXPECT_FALSE(form_seen_by_autocomplete.fields()[1].should_autocomplete());
}

// Test that plus address inputs are forced to !should_autocomplete
// for `SingleFieldFillRouter::OnWillSubmitForm()`.
TEST_F(BrowserAutofillManagerPlusAddressTest,
       TestPlusAddressEmailOverridesAppliedOnAddresses) {
  using enum AutofillPlusAddressDelegate::SuggestionContext;
  using enum PasswordFormClassification::Type;

  const std::string gaia_email = "foo@mail.com";
  client().identity_test_environment().MakePrimaryAccountAvailable(
      gaia_email, signin::ConsentLevel::kSignin);

  personal_data().test_address_data_manager().ClearProfiles();
  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile1.set_guid(MakeGuid(1));
  profile1.SetRawInfo(EMAIL_ADDRESS, u"test@example.com");
  personal_data().test_address_data_manager().AddProfile(profile1);

  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile2.set_guid(MakeGuid(2));
  profile2.SetRawInfo(EMAIL_ADDRESS, base::UTF8ToUTF16(gaia_email));
  personal_data().test_address_data_manager().AddProfile(profile2);

  // Plus address suggestions request.
  const std::vector<std::string> plus_addresses = {"plus+remote@plus.plus"};
  EXPECT_CALL(plus_address_delegate(), GetAffiliatedPlusAddresses)
      .WillOnce(RunOnceCallback<1>(plus_addresses));
  // No plus address suggestions are built as the plus address replaced the
  // address profile email.
  EXPECT_CALL(plus_address_delegate(), GetSuggestionsFromPlusAddresses(
                                           plus_addresses, _, _, _, _, _, _, _))
      .Times(0);
  // No single field form fill suggestions requests.
  EXPECT_CALL(plus_address_delegate(), IsFieldEligibleForPlusAddress)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(merchant_promo_code_manager(), OnGetSingleFieldSuggestions)
      .Times(0);
  EXPECT_CALL(iban_manager(), OnGetSingleFieldSuggestions).Times(0);
  EXPECT_CALL(autocomplete_history_manager(), OnGetSingleFieldSuggestions)
      .Times(0);
  EXPECT_CALL(plus_address_delegate(), OnPlusAddressSuggestionShown).Times(0);

  // Set up our form data. Notably, the first field is an email address.
  FormData form = test::GetFormData(
      {.fields = {{.role = EMAIL_ADDRESS, .autocomplete_attribute = "email"}}});
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));

  FormsSeen({form});

  // Check that the plus address suggestion are embedded in the address
  // suggestions and not offered separately.
  OnAskForValuesToFill(form, form.fields()[0]);
  EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());
  EXPECT_THAT(
      external_delegate()->suggestions(),
      ElementsAre(EqualsSuggestion(SuggestionType::kAddressEntry,
                                   Suggestion::AutofillProfilePayload(
                                       Suggestion::Guid(profile1.guid()))),
                  // The second address suggestion with a plus address email.
                  EqualsSuggestion(SuggestionType::kAddressEntry,
                                   Suggestion::AutofillProfilePayload(
                                       Suggestion::Guid(profile2.guid()),
                                       u"plus+remote@plus.plus")),
                  EqualsSuggestion(SuggestionType::kSeparator),
                  EqualsSuggestion(SuggestionType::kManageAddress)));
}

// Tests that an address suggestion with an email override is filled when
// selected. In addition, if the undo operation is called, the original profile
// without the email override is filled.
TEST_F(BrowserAutofillManagerPlusAddressTest,
       FillsEmailOverrideAndShowsUserNotification) {
  base::UserActionTester user_action_tester;
  // Set up gaia email.
  const std::string gaia_email = "theking@gmail.com";
  client().identity_test_environment().MakePrimaryAccountAvailable(
      gaia_email, signin::ConsentLevel::kSignin);

  // Set up profile.
  personal_data().test_address_data_manager().ClearProfiles();
  AutofillProfile profile =
      FillDataToAutofillProfile(GetElvisAddressFillData());
  profile.set_guid(kElvisProfileGuid);
  personal_data().address_data_manager().AddProfile(profile);

  // Plus address suggestions request.
  const std::string kDummyPlusAddress = "plus+plus@plus.plus";
  const std::vector<std::string> plus_addresses = {kDummyPlusAddress};
  EXPECT_CALL(plus_address_delegate(), IsFieldEligibleForPlusAddress)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(plus_address_delegate(), GetAffiliatedPlusAddresses)
      .WillOnce(RunOnceCallback<1>(plus_addresses));
  ON_CALL(plus_address_delegate(), IsPlusAddress)
      .WillByDefault([&](const std::string& address) {
        return address == kDummyPlusAddress;
      });

  // Set up form.
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FIRST, .autocomplete_attribute = "given-name"},
                  {.role = NAME_LAST, .autocomplete_attribute = "family-name"},
                  {.role = EMAIL_ADDRESS, .autocomplete_attribute = "email"}}});

  manager().AddSeenForm(form, {NAME_FIRST, NAME_LAST, EMAIL_ADDRESS});

  std::vector<FieldGlobalId> global_ids;
  for (const auto& field : form.fields()) {
    global_ids.push_back(field.global_id());
  }

  AutofillClient::EmailOverrideUndoCallback undo_callback;
  {
    InSequence s;
    EXPECT_CALL(driver(), ApplyFormAction).WillOnce(Return(global_ids));
    EXPECT_CALL(plus_address_delegate(), DidFillPlusAddress);
    EXPECT_CALL(client(),
                ShowPlusAddressEmailOverrideNotification(gaia_email, _))
        .WillOnce(MoveArg<1>(&undo_callback));
    EXPECT_CALL(client(), HideAutofillSuggestions(
                              SuggestionHidingReason::kAcceptSuggestion));
    EXPECT_CALL(driver(),
                ApplyFieldAction(mojom::FieldActionType::kReplaceAll,
                                 mojom::ActionPersistence::kFill, global_ids[2],
                                 base::UTF8ToUTF16(gaia_email)));
  }

  OnAskForValuesToFill(form, form.fields()[2]);
  EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());
  // Select an option to trigger Autofill. This should trigger a call to hide
  // the popup since we've selected an option.
  external_delegate()->DidAcceptSuggestion(
      external_delegate()->suggestions().front(), {});
  EXPECT_EQ(user_action_tester.GetActionCount(
                "PlusAddresses.FillAddressSuggestionAccepted"),
            1);
  // Call the undo operation.
  ASSERT_TRUE(undo_callback);
  std::move(undo_callback).Run();
  EXPECT_EQ(user_action_tester.GetActionCount(
                "PlusAddresses.FillAddressSuggestionUndone"),
            1);
}

// Tests that when an address suggestion with an email override is filled, a
// user notification is not shown if an email field was never filled.
TEST_F(BrowserAutofillManagerPlusAddressTest,
       FillsEmailOverrideNoNotificationIfNoEmailIsFilled) {
  // Set up gaia email.
  const std::string gaia_email = "theking@gmail.com";
  client().identity_test_environment().MakePrimaryAccountAvailable(
      gaia_email, signin::ConsentLevel::kSignin);

  // Set up profile.
  personal_data().test_address_data_manager().ClearProfiles();
  AutofillProfile profile =
      FillDataToAutofillProfile(GetElvisAddressFillData());
  profile.set_guid(kElvisProfileGuid);
  personal_data().address_data_manager().AddProfile(profile);

  // Plus address suggestions request.
  const std::string kDummyPlusAddress = "plus+plus@plus.plus";
  const std::vector<std::string> plus_addresses = {kDummyPlusAddress};
  EXPECT_CALL(plus_address_delegate(), IsFieldEligibleForPlusAddress)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(plus_address_delegate(), GetAffiliatedPlusAddresses)
      .WillOnce(RunOnceCallback<1>(plus_addresses));
  ON_CALL(plus_address_delegate(), IsPlusAddress)
      .WillByDefault([&](const std::string& address) {
        return address == kDummyPlusAddress;
      });

  // Set up form.
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FIRST, .autocomplete_attribute = "given-name"},
                  {.role = NAME_LAST, .autocomplete_attribute = "family-name"},
                  {.role = ADDRESS_HOME_CITY}}});

  manager().AddSeenForm(form, {NAME_FIRST, NAME_LAST, ADDRESS_HOME_CITY});

  std::vector<FieldGlobalId> global_ids;
  for (const auto& field : form.fields()) {
    global_ids.push_back(field.global_id());
  }

  AutofillClient::EmailOverrideUndoCallback undo_callback;
  {
    InSequence s;
    EXPECT_CALL(driver(), ApplyFormAction).WillOnce(Return(global_ids));
    EXPECT_CALL(plus_address_delegate(), DidFillPlusAddress).Times(0);
    EXPECT_CALL(client(), ShowPlusAddressEmailOverrideNotification).Times(0);
    EXPECT_CALL(client(), HideAutofillSuggestions(
                              SuggestionHidingReason::kAcceptSuggestion));
  }

  OnAskForValuesToFill(form, form.fields()[0]);
  EXPECT_TRUE(external_delegate()->on_suggestions_returned_seen());
  // Select an option to trigger Autofill. This should trigger a call to hide
  // the popup since we've selected an option.
  external_delegate()->DidAcceptSuggestion(
      external_delegate()->suggestions().front(), {});
}

// Fixture setting the BAM's `PasswordManagerDelegate` to a mock instead
// of the default `nullptr`. Enables any features if necessary.
class BrowserAutofillManagerUsingPasswordDelegateTest
    : public BrowserAutofillManagerTest {
 protected:
  void SetUp() override {
    BrowserAutofillManagerTest::SetUp();
    client().set_password_manager_delegate(CreatePasswordDelegate());
  }

  void TearDown() override {
    delegate_ = nullptr;
    BrowserAutofillManagerTest::TearDown();
  }

  MockPasswordManagerDelegate& password_delegate() { return *delegate_; }

 private:
  std::unique_ptr<PasswordManagerDelegate> CreatePasswordDelegate() {
    auto password_delegate = std::make_unique<MockPasswordManagerDelegate>();
    delegate_ = password_delegate.get();
    return password_delegate;
  }

  raw_ptr<MockPasswordManagerDelegate> delegate_ = nullptr;
};

// Test that the BAM queries the password delegate as soon as it's present.
TEST_F(BrowserAutofillManagerUsingPasswordDelegateTest,
       QueriesDelegateWhenGeneratingSuggestions) {
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});

#if !BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(password_delegate(), ShowSuggestions);
#else   // BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(password_delegate(), ShowKeyboardReplacingSurface);
#endif  // !BUILDFLAG(IS_ANDROID)

  OnAskForValuesToFill(form, form.fields()[0],
                       AutofillSuggestionTriggerSource::kTextFieldValueChanged,
                       PasswordSuggestionRequest({}, form,
                                                 /*username_field_index=*/0,
                                                 /*password_field_index=*/0));
}

}  // namespace
}  // namespace autofill
