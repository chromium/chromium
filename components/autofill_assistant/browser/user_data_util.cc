// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/user_data_util.h"

#include <numeric>
#include "base/callback.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill_assistant/browser/action_value.pb.h"
#include "components/autofill_assistant/browser/cud_condition.pb.h"
#include "components/autofill_assistant/browser/field_formatter.h"
#include "components/autofill_assistant/browser/url_utils.h"
#include "components/autofill_assistant/browser/website_login_manager.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/libaddressinput/chromium/addressinput_util.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_assistant {
namespace {

constexpr char kDefaultLocale[] = "en-US";

// TODO: Share this helper function with use_address_action.
std::u16string GetProfileFullName(const autofill::AutofillProfile& profile) {
  return autofill::data_util::JoinNameParts(
      profile.GetRawInfo(autofill::NAME_FIRST),
      profile.GetRawInfo(autofill::NAME_MIDDLE),
      profile.GetRawInfo(autofill::NAME_LAST));
}

int CountCompletePaymentInstrumentFields(const CollectUserDataOptions& options,
                                         const PaymentInstrument& instrument) {
  int complete_fields = 0;
  if (!instrument.card->GetRawInfo(autofill::CREDIT_CARD_NAME_FULL).empty()) {
    ++complete_fields;
  }
  if (!instrument.card->GetRawInfo(autofill::CREDIT_CARD_NUMBER).empty()) {
    ++complete_fields;
  }
  if (!instrument.card->GetRawInfo(autofill::CREDIT_CARD_EXP_MONTH).empty()) {
    ++complete_fields;
  }
  if (!instrument.card->GetRawInfo(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR)
           .empty()) {
    ++complete_fields;
  }
  if (instrument.billing_address != nullptr) {
    ++complete_fields;

    if (options.require_billing_postal_code &&
        !instrument.billing_address->GetRawInfo(autofill::ADDRESS_HOME_ZIP)
             .empty()) {
      ++complete_fields;
    }
  }
  return complete_fields;
}

// Helper function that compares instances of PaymentInstrument by completeness
// in regards to the current options. Full payment instruments should be
// ordered before empty ones and fall back to compare the full name on the
// credit card in case of equality.
bool CompletenessComparePaymentInstruments(
    const CollectUserDataOptions& options,
    const PaymentInstrument& a,
    const PaymentInstrument& b) {
  int complete_fields_a = CountCompletePaymentInstrumentFields(options, a);
  int complete_fields_b = CountCompletePaymentInstrumentFields(options, b);
  if (complete_fields_a == complete_fields_b) {
    return base::i18n::ToLower(
               a.card->GetRawInfo(autofill::CREDIT_CARD_NAME_FULL))
               .compare(base::i18n::ToLower(
                   b.card->GetRawInfo(autofill::CREDIT_CARD_NAME_FULL))) < 0;
  }
  return complete_fields_a > complete_fields_b;
}

bool IsCompleteAddress(const autofill::AutofillProfile* profile,
                       bool require_postal_code) {
  if (!profile) {
    return false;
  }
  // We use a hard coded locale here since we are only interested in whether
  // fields are empty or not.
  auto address_data = autofill::i18n::CreateAddressDataFromAutofillProfile(
      *profile, kDefaultLocale);
  if (!autofill::addressinput::HasAllRequiredFields(*address_data)) {
    return false;
  }

  if (require_postal_code && address_data->postal_code.empty()) {
    return false;
  }

  return true;
}

ClientStatus ExtractProfileAndFormatAutofillValue(
    const AutofillProfile& profile,
    const ValueExpression& value_expression,
    const UserData* user_data,
    bool quote_meta,
    std::string* out_value) {
  if (profile.identifier().empty() || value_expression.chunk().empty()) {
    VLOG(1) << "|value_expression| with empty "
               "|profile.identifier| or |value_expression|";
    return ClientStatus(INVALID_ACTION);
  }

  const autofill::AutofillProfile* address =
      user_data->selected_address(profile.identifier());
  if (address == nullptr) {
    VLOG(1) << "Requested unknown address '" << profile.identifier() << "'";
    return ClientStatus(PRECONDITION_FAILED);
  }

  auto mappings =
      field_formatter::CreateAutofillMappings(*address, kDefaultLocale);
  ClientStatus format_status = field_formatter::FormatExpression(
      value_expression, mappings, quote_meta, out_value);
  if (!format_status.ok()) {
    return format_status;
  }

  return OkClientStatus();
}

void OnGetStoredPassword(
    base::OnceCallback<void(const ClientStatus&, const std::string&)> callback,
    bool success,
    std::string password) {
  if (!success) {
    std::move(callback).Run(ClientStatus(AUTOFILL_INFO_NOT_AVAILABLE),
                            std::string());
    return;
  }
  std::move(callback).Run(OkClientStatus(), password);
}

bool EvaluateCondition(const std::map<std::string, std::string>& data,
                       const RequiredDataPiece::Condition& condition) {
  auto it = data.find(base::NumberToString(condition.key()));
  if (it == data.end()) {
    return false;
  }
  auto value = it->second;
  switch (condition.condition_case()) {
    case RequiredDataPiece::Condition::kNotEmpty:
      return !value.empty();
    case RequiredDataPiece::Condition::kRegexp: {
      re2::RE2::Options options;
      options.set_case_sensitive(
          condition.regexp().text_filter().case_sensitive());
      re2::RE2 regexp(condition.regexp().text_filter().re2(), options);
      return RE2::PartialMatch(value, regexp);
    }
    case RequiredDataPiece::Condition::CONDITION_NOT_SET:
      return false;
  }
}

std::vector<std::string> GetValidationErrors(
    const std::map<std::string, std::string> data,
    const std::vector<RequiredDataPiece> required_data_pieces) {
  std::vector<std::string> errors;

  for (const auto& required_data_piece : required_data_pieces) {
    if (!EvaluateCondition(data, required_data_piece.condition())) {
      errors.push_back(required_data_piece.error_message());
    }
  }
  return errors;
}

// Helper function that compares instances of AutofillProfile by completeness
// in regards to the current options. Full profiles should be ordered before
// empty ones and fall back to compare the profile's name in case of equality.
bool CompletenessCompareContacts(const CollectUserDataOptions& options,
                                 const autofill::AutofillProfile& a,
                                 const autofill::AutofillProfile& b) {
  int incomplete_fields_a =
      GetValidationErrors(
          field_formatter::CreateAutofillMappings(a, kDefaultLocale),
          options.required_contact_data_pieces)
          .size();
  int incomplete_fields_b =
      GetValidationErrors(
          field_formatter::CreateAutofillMappings(b, kDefaultLocale),
          options.required_contact_data_pieces)
          .size();
  if (incomplete_fields_a != incomplete_fields_b) {
    return incomplete_fields_a <= incomplete_fields_b;
  }

  return base::i18n::ToLower(GetProfileFullName(a))
             .compare(base::i18n::ToLower(GetProfileFullName(b))) < 0;
}

int GetAddressEditorCompletenessRating(
    const autofill::AutofillProfile& profile) {
  auto address_data = autofill::i18n::CreateAddressDataFromAutofillProfile(
      profile, kDefaultLocale);
  std::multimap<i18n::addressinput::AddressField,
                i18n::addressinput::AddressProblem>
      problems;
  autofill::addressinput::ValidateRequiredFields(
      *address_data, /* filter= */ nullptr, &problems);
  return problems.size();
}

// Helper function that compares instances of AutofillProfile by completeness
// in regards to the current options. Full profiles should be ordered before
// empty ones and fall back to compare the profile's name in case of equality.
bool CompletenessCompareShippingAddresses(const CollectUserDataOptions& options,
                                          const autofill::AutofillProfile& a,
                                          const autofill::AutofillProfile& b) {
  // Compare by editor completeness first. This is done because the
  // AddressEditor only allows storing addresses it considers complete.
  int incomplete_fields_a = GetAddressEditorCompletenessRating(a);
  int incomplete_fields_b = GetAddressEditorCompletenessRating(b);
  if (incomplete_fields_a != incomplete_fields_b) {
    return incomplete_fields_a <= incomplete_fields_b;
  }

  incomplete_fields_a =
      GetValidationErrors(
          field_formatter::CreateAutofillMappings(a, kDefaultLocale),
          options.required_shipping_address_data_pieces)
          .size();
  incomplete_fields_b =
      GetValidationErrors(
          field_formatter::CreateAutofillMappings(b, kDefaultLocale),
          options.required_shipping_address_data_pieces)
          .size();
  if (incomplete_fields_a != incomplete_fields_b) {
    return incomplete_fields_a <= incomplete_fields_b;
  }

  return base::i18n::ToLower(GetProfileFullName(a))
             .compare(base::i18n::ToLower(GetProfileFullName(b))) < 0;
}

}  // namespace
namespace user_data {

std::vector<std::string> GetContactValidationErrors(
    const autofill::AutofillProfile* profile,
    const CollectUserDataOptions& collect_user_data_options) {
  if (collect_user_data_options.required_contact_data_pieces.empty()) {
    return std::vector<std::string>();
  }

  return GetValidationErrors(
      profile
          ? field_formatter::CreateAutofillMappings(*profile, kDefaultLocale)
          : std::map<std::string, std::string>(),
      collect_user_data_options.required_contact_data_pieces);
}

std::vector<int> SortContactsByCompleteness(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<autofill::AutofillProfile>>& profiles) {
  std::vector<int> profile_indices(profiles.size());
  std::iota(std::begin(profile_indices), std::end(profile_indices), 0);
  std::sort(profile_indices.begin(), profile_indices.end(),
            [&collect_user_data_options, &profiles](int i, int j) {
              return CompletenessCompareContacts(collect_user_data_options,
                                                 *profiles[i], *profiles[j]);
            });
  return profile_indices;
}

int GetDefaultContactProfile(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<autofill::AutofillProfile>>& profiles) {
  if (profiles.empty()) {
    return -1;
  }
  auto sorted_indices =
      SortContactsByCompleteness(collect_user_data_options, profiles);
  if (!collect_user_data_options.default_email.empty()) {
    for (int index : sorted_indices) {
      if (base::UTF16ToUTF8(
              profiles[index]->GetRawInfo(autofill::EMAIL_ADDRESS)) ==
          collect_user_data_options.default_email) {
        return index;
      }
    }
  }
  return sorted_indices[0];
}

std::vector<std::string> GetShippingAddressValidationErrors(
    const autofill::AutofillProfile* profile,
    const CollectUserDataOptions& collect_user_data_options) {
  std::vector<std::string> errors;
  if (!collect_user_data_options.request_shipping) {
    return errors;
  }

  if (!collect_user_data_options.required_shipping_address_data_pieces
           .empty()) {
    errors = GetValidationErrors(
        profile
            ? field_formatter::CreateAutofillMappings(*profile, kDefaultLocale)
            : std::map<std::string, std::string>(),
        collect_user_data_options.required_shipping_address_data_pieces);
  }

  // Require address editor completeness if Assistant validation succeeds. If
  // Assistant validation fails, the editor has to be opened and requires
  // completeness to save the change, do not append the (potentially duplicate)
  // error in this case.
  if (errors.empty() && (profile == nullptr ||
                         GetAddressEditorCompletenessRating(*profile) != 0)) {
    errors.push_back(l10n_util::GetStringUTF8(
        IDS_AUTOFILL_ASSISTANT_PAYMENT_INFORMATION_MISSING));
  }
  return errors;
}

std::vector<int> SortShippingAddressesByCompleteness(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<autofill::AutofillProfile>>& profiles) {
  std::vector<int> profile_indices(profiles.size());
  std::iota(std::begin(profile_indices), std::end(profile_indices), 0);
  std::sort(profile_indices.begin(), profile_indices.end(),
            [&collect_user_data_options, &profiles](int i, int j) {
              return CompletenessCompareShippingAddresses(
                  collect_user_data_options, *profiles[i], *profiles[j]);
            });
  return profile_indices;
}

int GetDefaultShippingAddressProfile(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<autofill::AutofillProfile>>& profiles) {
  if (profiles.empty()) {
    return -1;
  }
  auto sorted_indices =
      SortShippingAddressesByCompleteness(collect_user_data_options, profiles);
  return sorted_indices[0];
}

}  // namespace user_data

std::unique_ptr<autofill::AutofillProfile> MakeUniqueFromProfile(
    const autofill::AutofillProfile& profile) {
  auto unique_profile = std::make_unique<autofill::AutofillProfile>(profile);
  // Temporary workaround so that fields like first/last name a properly
  // populated.
  unique_profile->FinalizeAfterImport();
  return unique_profile;
}

std::vector<int> SortPaymentInstrumentsByCompleteness(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<PaymentInstrument>>&
        payment_instruments) {
  std::vector<int> payment_instrument_indices(payment_instruments.size());
  std::iota(std::begin(payment_instrument_indices),
            std::end(payment_instrument_indices), 0);
  std::sort(payment_instrument_indices.begin(),
            payment_instrument_indices.end(),
            [&collect_user_data_options, &payment_instruments](int a, int b) {
              return CompletenessComparePaymentInstruments(
                  collect_user_data_options, *payment_instruments[a],
                  *payment_instruments[b]);
            });
  return payment_instrument_indices;
}

int GetDefaultPaymentInstrument(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<PaymentInstrument>>&
        payment_instruments) {
  if (payment_instruments.empty()) {
    return -1;
  }
  auto sorted_indices = SortPaymentInstrumentsByCompleteness(
      collect_user_data_options, payment_instruments);
  return sorted_indices[0];
}

bool CompareContactDetails(
    const CollectUserDataOptions& collect_user_data_options,
    const autofill::AutofillProfile* a,
    const autofill::AutofillProfile* b) {
  std::vector<autofill::ServerFieldType> types;
  if (collect_user_data_options.request_payer_name) {
    types.emplace_back(autofill::NAME_FULL);
    types.emplace_back(autofill::NAME_FIRST);
    types.emplace_back(autofill::NAME_MIDDLE);
    types.emplace_back(autofill::NAME_LAST);
  }
  if (collect_user_data_options.request_payer_phone) {
    types.emplace_back(autofill::PHONE_HOME_WHOLE_NUMBER);
  }
  if (collect_user_data_options.request_payer_email) {
    types.emplace_back(autofill::EMAIL_ADDRESS);
  }
  if (types.empty()) {
    return a->guid() == b->guid();
  }

  for (auto type : types) {
    int comparison = a->GetRawInfo(type).compare(b->GetRawInfo(type));
    if (comparison != 0) {
      return false;
    }
  }

  return true;
}

bool IsCompleteCreditCard(
    const autofill::CreditCard* credit_card,
    const autofill::AutofillProfile* billing_profile,
    const CollectUserDataOptions& collect_user_data_options) {
  if (!collect_user_data_options.request_payment_method) {
    return true;
  }

  if (!credit_card || !billing_profile ||
      credit_card->billing_address_id().empty()) {
    return false;
  }

  if (!IsCompleteAddress(
          billing_profile,
          collect_user_data_options.require_billing_postal_code)) {
    return false;
  }

  if (credit_card->record_type() != autofill::CreditCard::MASKED_SERVER_CARD &&
      !credit_card->HasValidCardNumber()) {
    // Can't check validity of masked server card numbers because they are
    // incomplete until decrypted.
    return false;
  }

  if (!credit_card->HasValidExpirationDate()) {
    return false;
  }

  std::string basic_card_network =
      autofill::data_util::GetPaymentRequestData(credit_card->network())
          .basic_card_issuer_network;
  if (!collect_user_data_options.supported_basic_card_networks.empty() &&
      std::find(collect_user_data_options.supported_basic_card_networks.begin(),
                collect_user_data_options.supported_basic_card_networks.end(),
                basic_card_network) ==
          collect_user_data_options.supported_basic_card_networks.end()) {
    return false;
  }

  return true;
}

ClientStatus GetFormattedAutofillValue(const AutofillValue& autofill_value,
                                       const UserData* user_data,
                                       std::string* out_value) {
  return ExtractProfileAndFormatAutofillValue(
      autofill_value.profile(), autofill_value.value_expression(), user_data,
      /* quote_meta= */ false, out_value);
}

ClientStatus GetFormattedAutofillValue(
    const AutofillValueRegexp& autofill_value_regexp,
    const UserData* user_data,
    std::string* out_value) {
  return ExtractProfileAndFormatAutofillValue(
      autofill_value_regexp.profile(),
      autofill_value_regexp.value_expression_re2().value_expression(),
      user_data,
      /* quote_meta= */ true, out_value);
}

void GetPasswordManagerValue(
    const PasswordManagerValue& password_manager_value,
    const ElementFinder::Result& target_element,
    const UserData* user_data,
    WebsiteLoginManager* website_login_manager,
    base::OnceCallback<void(const ClientStatus&, const std::string&)>
        callback) {
  if (!user_data->selected_login_) {
    std::move(callback).Run(ClientStatus(PRECONDITION_FAILED), std::string());
    return;
  }
  if (!target_element.container_frame_host ||
      !url_utils::IsSamePublicSuffixDomain(
          target_element.container_frame_host->GetLastCommittedURL(),
          user_data->selected_login_->origin)) {
    std::move(callback).Run(ClientStatus(PASSWORD_ORIGIN_MISMATCH),
                            std::string());
    return;
  }

  switch (password_manager_value.credential_type()) {
    case PasswordManagerValue::PASSWORD:
      website_login_manager->GetPasswordForLogin(
          *user_data->selected_login_,
          base::BindOnce(&OnGetStoredPassword, std::move(callback)));
      return;
    case PasswordManagerValue::USERNAME:
      std::move(callback).Run(OkClientStatus(),
                              user_data->selected_login_->username);
      return;
    case PasswordManagerValue::NOT_SET:
      std::move(callback).Run(ClientStatus(INVALID_ACTION), std::string());
      return;
  }
}

ClientStatus GetClientMemoryStringValue(const std::string& client_memory_key,
                                        const UserData* user_data,
                                        std::string* out_value) {
  if (client_memory_key.empty()) {
    return ClientStatus(INVALID_ACTION);
  }
  if (!user_data->has_additional_value(client_memory_key) ||
      user_data->additional_value(client_memory_key)
              ->strings()
              .values()
              .size() != 1) {
    VLOG(1) << "Requested key '" << client_memory_key
            << "' not available in client memory";
    return ClientStatus(PRECONDITION_FAILED);
  }
  out_value->assign(
      user_data->additional_value(client_memory_key)->strings().values(0));
  return OkClientStatus();
}

void ResolveTextValue(const TextValue& text_value,
                      const ElementFinder::Result& target_element,
                      const ActionDelegate* action_delegate,
                      base::OnceCallback<void(const ClientStatus&,
                                              const std::string&)> callback) {
  std::string value;
  ClientStatus status = OkClientStatus();
  switch (text_value.value_case()) {
    case TextValue::kText:
      value = text_value.text();
      break;
    case TextValue::kAutofillValue: {
      status = GetFormattedAutofillValue(
          text_value.autofill_value(), action_delegate->GetUserData(), &value);
      break;
    }
    case TextValue::kPasswordManagerValue: {
      GetPasswordManagerValue(text_value.password_manager_value(),
                              target_element, action_delegate->GetUserData(),
                              action_delegate->GetWebsiteLoginManager(),
                              std::move(callback));
      return;
    }
    case TextValue::kClientMemoryKey: {
      status =
          GetClientMemoryStringValue(text_value.client_memory_key(),
                                     action_delegate->GetUserData(), &value);
      break;
    }
    case TextValue::VALUE_NOT_SET:
      status = ClientStatus(INVALID_ACTION);
  }

  std::move(callback).Run(status, value);
}

}  // namespace autofill_assistant
