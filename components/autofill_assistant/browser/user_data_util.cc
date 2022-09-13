// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/user_data_util.h"

#include <numeric>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/i18n/case_conversion.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill_assistant/browser/action_value.pb.h"
#include "components/autofill_assistant/browser/cud_condition.pb.h"
#include "components/autofill_assistant/browser/field_formatter.h"
#include "components/autofill_assistant/browser/model.pb.h"
#include "components/autofill_assistant/browser/public/password_change/website_login_manager.h"
#include "components/autofill_assistant/browser/url_utils.h"
#include "components/autofill_assistant/browser/web/element_finder_result.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/libaddressinput/chromium/addressinput_util.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_assistant {
namespace user_data {
namespace {

constexpr char kDefaultLocale[] = "en-US";

template <typename T>
ClientStatus ExtractDataAndFormatClientValue(
    const T& client_value,
    const ValueExpression& value_expression,
    const UserData& user_data,
    bool quote_meta,
    const std::string& locale,
    std::string* out_value) {
  if (value_expression.chunk().empty()) {
    VLOG(1) << "|value_expression| is empty";
    return ClientStatus(INVALID_ACTION);
  }

  base::flat_map<field_formatter::Key, std::string> data;
  std::string localeOrDefault = locale.empty() ? kDefaultLocale : locale;

  if (client_value.has_profile()) {
    const auto& profile = client_value.profile();
    if (profile.identifier().empty()) {
      VLOG(1) << "empty |profile.identifier|";
      return ClientStatus(INVALID_ACTION);
    }
    const autofill::AutofillProfile* address =
        user_data.selected_address(profile.identifier());
    if (address == nullptr) {
      VLOG(1) << "Requested unknown address '" << profile.identifier() << "'";
      return ClientStatus(PRECONDITION_FAILED);
    }

    auto address_map =
        field_formatter::CreateAutofillMappings(*address, localeOrDefault);
    data.insert(address_map.begin(), address_map.end());
  }

  const autofill::CreditCard* card = user_data.selected_card();
  if (card != nullptr) {
    auto card_map =
        field_formatter::CreateAutofillMappings(*card, localeOrDefault);
    data.insert(card_map.begin(), card_map.end());
  }

  for (const auto& chunk : value_expression.chunk()) {
    if (!chunk.has_memory_key() ||
        !user_data.HasAdditionalValue(chunk.memory_key())) {
      continue;
    }
    const ValueProto* value = user_data.GetAdditionalValue(chunk.memory_key());
    if (value->strings().values().size() == 1) {
      data.emplace(field_formatter::Key(chunk.memory_key()),
                   value->strings().values(0));
    }
  }

  const ClientStatus& format_status = field_formatter::FormatExpression(
      value_expression, data, quote_meta, out_value);
  if (!format_status.ok()) {
    return format_status;
  }
  if (out_value->empty()) {
    VLOG(1) << "|value_expression| result is empty";
    return ClientStatus(EMPTY_VALUE_EXPRESSION_RESULT);
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

bool EvaluateCondition(
    const base::flat_map<field_formatter::Key, std::string>& data,
    const RequiredDataPiece::Condition& condition) {
  std::string value;
  auto it = data.find(field_formatter::Key(condition.key()));
  if (it != data.end()) {
    value = it->second;
  }

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
    const base::flat_map<field_formatter::Key, std::string>& data,
    const std::vector<RequiredDataPiece>& required_data_pieces) {
  std::vector<std::string> errors;

  for (const auto& required_data_piece : required_data_pieces) {
    if (!EvaluateCondition(data, required_data_piece.condition())) {
      errors.push_back(required_data_piece.error_message());
    }
  }
  return errors;
}

std::vector<std::string> GetProfileValidationErrors(
    const autofill::AutofillProfile* profile,
    const std::vector<RequiredDataPiece>& required_data_pieces) {
  if (required_data_pieces.empty()) {
    return std::vector<std::string>();
  }

  return GetValidationErrors(
      profile
          ? field_formatter::CreateAutofillMappings(*profile, kDefaultLocale)
          : base::flat_map<field_formatter::Key, std::string>(),
      required_data_pieces);
}

// Helper function that compares instances of AutofillProfile by completeness
// in regards to the current options. Full profiles should be ordered before
// empty ones and fall back to compare the profile's last usage.
bool CompletenessCompareContacts(
    const std::vector<RequiredDataPiece>& required_data_pieces,
    const autofill::AutofillProfile& a,
    const base::flat_map<field_formatter::Key, std::string>& data_a,
    const autofill::AutofillProfile& b,
    const base::flat_map<field_formatter::Key, std::string>& data_b) {
  int incomplete_fields_a =
      GetValidationErrors(data_a, required_data_pieces).size();
  int incomplete_fields_b =
      GetValidationErrors(data_b, required_data_pieces).size();
  if (incomplete_fields_a != incomplete_fields_b) {
    return incomplete_fields_a <= incomplete_fields_b;
  }

  return a.use_date() > b.use_date();
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

int CompletenessCompareAddresses(
    const std::vector<RequiredDataPiece>& required_data_pieces,
    const autofill::AutofillProfile& a,
    const base::flat_map<field_formatter::Key, std::string>& data_a,
    const autofill::AutofillProfile& b,
    const base::flat_map<field_formatter::Key, std::string>& data_b) {
  // Compare by editor completeness first. This is done because the
  // AddressEditor only allows storing addresses it considers complete.
  int incomplete_fields_a = GetAddressEditorCompletenessRating(a);
  int incomplete_fields_b = GetAddressEditorCompletenessRating(b);
  if (incomplete_fields_a != incomplete_fields_b) {
    return incomplete_fields_b - incomplete_fields_a;
  }

  incomplete_fields_a =
      GetValidationErrors(data_a, required_data_pieces).size();
  incomplete_fields_b =
      GetValidationErrors(data_b, required_data_pieces).size();
  return incomplete_fields_b - incomplete_fields_a;
}

// Helper function that compares instances of AutofillProfile by completeness
// in regards to the current options. Full profiles should be ordered before
// empty ones and fall back to compare the profile's name in case of equality.
bool CompletenessCompareShippingAddresses(
    const CollectUserDataOptions& options,
    const autofill::AutofillProfile& a,
    const base::flat_map<field_formatter::Key, std::string>& data_a,
    const autofill::AutofillProfile& b,
    const base::flat_map<field_formatter::Key, std::string>& data_b) {
  int address_compare = CompletenessCompareAddresses(
      options.required_shipping_address_data_pieces, a, data_a, b, data_b);
  if (address_compare != 0) {
    return address_compare > 0;
  }

  return a.use_date() > b.use_date();
}

// Helper function that compares instances of PaymentInstrument by completeness
// in regards to the current options. Full payment instruments should be
// ordered before empty ones and fall back to compare the full name on the
// credit card in case of equality.
bool CompletenessComparePaymentInstruments(
    const CollectUserDataOptions& options,
    const PaymentInstrument& a,
    const base::flat_map<field_formatter::Key, std::string>& data_a,
    const PaymentInstrument& b,
    const base::flat_map<field_formatter::Key, std::string>& data_b) {
  DCHECK(a.card);
  DCHECK(b.card);
  int incomplete_fields_a =
      GetValidationErrors(data_a, options.required_credit_card_data_pieces)
          .size();
  int incomplete_fields_b =
      GetValidationErrors(data_b, options.required_credit_card_data_pieces)
          .size();
  if (incomplete_fields_a != incomplete_fields_b) {
    return incomplete_fields_a <= incomplete_fields_b;
  }

  bool a_has_valid_expiration = a.card->HasValidExpirationDate();
  bool b_has_valid_expiration = b.card->HasValidExpirationDate();
  if (a_has_valid_expiration != b_has_valid_expiration) {
    return !b_has_valid_expiration;
  }

  bool a_has_valid_number =
      (a.card->record_type() != autofill::CreditCard::MASKED_SERVER_CARD &&
       a.card->HasValidCardNumber()) ||
      (a.card->record_type() == autofill::CreditCard::MASKED_SERVER_CARD &&
       !a.card->GetRawInfo(autofill::CREDIT_CARD_NUMBER).empty());
  bool b_has_valid_number =
      (b.card->record_type() != autofill::CreditCard::MASKED_SERVER_CARD &&
       b.card->HasValidCardNumber()) ||
      (b.card->record_type() == autofill::CreditCard::MASKED_SERVER_CARD &&
       !b.card->GetRawInfo(autofill::CREDIT_CARD_NUMBER).empty());
  if (a_has_valid_number != b_has_valid_number) {
    return !b_has_valid_number;
  }

  bool a_has_address = a.billing_address != nullptr;
  bool b_has_address = b.billing_address != nullptr;
  if (a_has_address != b_has_address) {
    return !b_has_address;
  }
  if (a_has_address && b_has_address) {
    int address_compare = CompletenessCompareAddresses(
        options.required_billing_address_data_pieces, *a.billing_address,
        data_a, *b.billing_address, data_b);
    if (address_compare != 0) {
      return address_compare > 0;
    }
  }

  return a.card->use_date() > b.card->use_date();
}

bool EvaluateNotEmpty(
    const base::flat_map<field_formatter::Key, std::string>& mapping,
    autofill::ServerFieldType field_type) {
  auto it = mapping.find(field_formatter::Key(static_cast<int>(field_type)));
  return it != mapping.end() && !it->second.empty();
}

ClientStatus MoveAutofillValueRegexpToTextFilter(
    const UserData* user_data,
    SelectorProto::PropertyFilter* value) {
  if (!value->has_autofill_value_regexp()) {
    return OkClientStatus();
  }
  if (user_data == nullptr) {
    return ClientStatus(PRECONDITION_FAILED);
  }
  const AutofillValueRegexp& autofill_value_regexp =
      value->autofill_value_regexp();
  TextFilter text_filter;
  text_filter.set_case_sensitive(
      autofill_value_regexp.value_expression_re2().case_sensitive());
  std::string re2;
  ClientStatus re2_status =
      GetFormattedClientValue(autofill_value_regexp, *user_data, &re2);
  text_filter.set_re2(re2);
  // Assigning text_filter will clear autofill_value_regexp.
  *value->mutable_text_filter() = text_filter;
  return re2_status;
}

template <typename T>
void UpsertAutofillProfile(const autofill::AutofillProfile& profile,
                           std::vector<std::unique_ptr<T>>& list) {
  auto it =
      base::ranges::find_if(list, [&profile](const std::unique_ptr<T>& ptr) {
        return ptr->profile && ptr->profile->guid() == profile.guid();
      });

  auto new_profile = user_data::MakeUniqueFromProfile(profile);
  if (it == list.end()) {
    auto entry = std::make_unique<T>(std::move(new_profile));
    entry->identifier = profile.guid();
    list.emplace_back(std::move(entry));
    return;
  }

  (*it)->profile = std::move(new_profile);
}

}  // namespace

std::vector<std::string> GetContactValidationErrors(
    const autofill::AutofillProfile* profile,
    const CollectUserDataOptions& collect_user_data_options) {
  return GetProfileValidationErrors(
      profile, collect_user_data_options.required_contact_data_pieces);
}

std::vector<std::string> GetPhoneNumberValidationErrors(
    const autofill::AutofillProfile* profile,
    const CollectUserDataOptions& collect_user_data_options) {
  return GetProfileValidationErrors(
      profile, collect_user_data_options.required_phone_number_data_pieces);
}

std::vector<int> SortContactsByCompleteness(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<Contact>>& contacts) {
  std::vector<base::flat_map<field_formatter::Key, std::string>>
      mapped_contacts;
  for (const auto& contact : contacts) {
    mapped_contacts.push_back(field_formatter::CreateAutofillMappings(
        *contact->profile, kDefaultLocale));
  }
  std::vector<int> indices(contacts.size());
  std::iota(std::begin(indices), std::end(indices), 0);
  std::stable_sort(
      indices.begin(), indices.end(),
      [&collect_user_data_options, &contacts, &mapped_contacts](int i, int j) {
        return CompletenessCompareContacts(
            collect_user_data_options.required_contact_data_pieces,
            *contacts[i]->profile, mapped_contacts[i], *contacts[j]->profile,
            mapped_contacts[j]);
      });
  return indices;
}

std::vector<int> SortPhoneNumbersByCompleteness(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<PhoneNumber>>& phone_numbers) {
  std::vector<base::flat_map<field_formatter::Key, std::string>>
      mapped_phone_numbers;
  for (const auto& phone_number : phone_numbers) {
    mapped_phone_numbers.push_back(field_formatter::CreateAutofillMappings(
        *phone_number->profile, kDefaultLocale));
  }
  std::vector<int> indices(phone_numbers.size());
  std::iota(std::begin(indices), std::end(indices), 0);
  std::stable_sort(
      indices.begin(), indices.end(),
      [&collect_user_data_options, &phone_numbers, &mapped_phone_numbers](
          int i, int j) {
        return CompletenessCompareContacts(
            collect_user_data_options.required_phone_number_data_pieces,
            *phone_numbers[i]->profile, mapped_phone_numbers[i],
            *phone_numbers[j]->profile, mapped_phone_numbers[j]);
      });
  return indices;
}

int GetDefaultContact(const CollectUserDataOptions& collect_user_data_options,
                      const std::vector<std::unique_ptr<Contact>>& contacts) {
  if (contacts.empty()) {
    return -1;
  }
  auto sorted_indices =
      SortContactsByCompleteness(collect_user_data_options, contacts);
  if (!collect_user_data_options.default_email.empty()) {
    for (int index : sorted_indices) {
      if (base::UTF16ToUTF8(
              contacts[index]->profile->GetRawInfo(autofill::EMAIL_ADDRESS)) ==
          collect_user_data_options.default_email) {
        return index;
      }
    }
  }
  return sorted_indices[0];
}

int GetDefaultPhoneNumber(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<PhoneNumber>>& phone_numbers) {
  if (phone_numbers.empty()) {
    return -1;
  }
  auto sorted_indices =
      SortPhoneNumbersByCompleteness(collect_user_data_options, phone_numbers);
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
    errors = GetProfileValidationErrors(
        profile,
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
    const std::vector<std::unique_ptr<Address>>& addresses) {
  std::vector<base::flat_map<field_formatter::Key, std::string>>
      mapped_addresses;
  for (const auto& address : addresses) {
    mapped_addresses.push_back(field_formatter::CreateAutofillMappings(
        *address->profile, kDefaultLocale));
  }
  std::vector<int> indices(addresses.size());
  std::iota(std::begin(indices), std::end(indices), 0);
  std::stable_sort(indices.begin(), indices.end(),
                   [&collect_user_data_options, &addresses, &mapped_addresses](
                       int i, int j) {
                     return CompletenessCompareShippingAddresses(
                         collect_user_data_options, *addresses[i]->profile,
                         mapped_addresses[i], *addresses[j]->profile,
                         mapped_addresses[j]);
                   });
  return indices;
}

int GetDefaultShippingAddress(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<Address>>& addresses) {
  if (addresses.empty()) {
    return -1;
  }
  auto sorted_indices =
      SortShippingAddressesByCompleteness(collect_user_data_options, addresses);
  return sorted_indices[0];
}

std::vector<std::string> GetPaymentInstrumentValidationErrors(
    const autofill::CreditCard* credit_card,
    const autofill::AutofillProfile* billing_address,
    const CollectUserDataOptions& collect_user_data_options) {
  std::vector<std::string> errors;
  if (!collect_user_data_options.request_payment_method) {
    return errors;
  }

  if (!collect_user_data_options.required_credit_card_data_pieces.empty()) {
    const auto& card_errors = GetValidationErrors(
        credit_card ? field_formatter::CreateAutofillMappings(*credit_card,
                                                              kDefaultLocale)
                    : base::flat_map<field_formatter::Key, std::string>(),
        collect_user_data_options.required_credit_card_data_pieces);
    errors.insert(errors.end(), card_errors.begin(), card_errors.end());
  }
  if (credit_card && !credit_card->HasValidExpirationDate()) {
    errors.push_back(collect_user_data_options.credit_card_expired_text);
  }

  if (!collect_user_data_options.required_billing_address_data_pieces.empty()) {
    const auto& address_errors = GetProfileValidationErrors(
        billing_address,
        collect_user_data_options.required_billing_address_data_pieces);
    errors.insert(errors.end(), address_errors.begin(), address_errors.end());
  }

  // Require card editor completeness if Assistant validation succeeds. If
  // Assistant validation fails, the editor has to be opened and requires
  // completeness to save the change, do not append the (potentially duplicate)
  // error in this case.
  if (errors.empty()) {
    if (credit_card &&
        credit_card->record_type() !=
            autofill::CreditCard::MASKED_SERVER_CARD &&
        !credit_card->HasValidCardNumber()) {
      // Can't check validity of masked server card numbers, because they are
      // incomplete until decrypted.
      errors.push_back(l10n_util::GetStringUTF8(
          IDS_AUTOFILL_ASSISTANT_PAYMENT_INFORMATION_MISSING));
    } else if (!billing_address ||
               GetAddressEditorCompletenessRating(*billing_address) != 0) {
      errors.push_back(l10n_util::GetStringUTF8(
          IDS_AUTOFILL_ASSISTANT_PAYMENT_INFORMATION_MISSING));
    }
  }

  return errors;
}

std::vector<int> SortPaymentInstrumentsByCompleteness(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<PaymentInstrument>>&
        payment_instruments) {
  std::vector<base::flat_map<field_formatter::Key, std::string>>
      mapped_payment_instruments;
  for (const auto& payment_instrument : payment_instruments) {
    base::flat_map<field_formatter::Key, std::string>
        mapped_payment_instrument = field_formatter::CreateAutofillMappings(
            *payment_instrument->card, kDefaultLocale);
    if (payment_instrument->billing_address != nullptr) {
      base::flat_map<field_formatter::Key, std::string> mapped_address =
          field_formatter::CreateAutofillMappings(
              *payment_instrument->billing_address, kDefaultLocale);
      mapped_payment_instrument.insert(mapped_address.begin(),
                                       mapped_address.end());
    }
    mapped_payment_instruments.push_back(mapped_payment_instrument);
  }
  std::vector<int> payment_instrument_indices(payment_instruments.size());
  std::iota(std::begin(payment_instrument_indices),
            std::end(payment_instrument_indices), 0);
  std::stable_sort(payment_instrument_indices.begin(),
                   payment_instrument_indices.end(),
                   [&collect_user_data_options, &payment_instruments,
                    &mapped_payment_instruments](int a, int b) {
                     return CompletenessComparePaymentInstruments(
                         collect_user_data_options, *payment_instruments[a],
                         mapped_payment_instruments[a], *payment_instruments[b],
                         mapped_payment_instruments[b]);
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

std::unique_ptr<autofill::AutofillProfile> MakeUniqueFromProfile(
    const autofill::AutofillProfile& profile) {
  auto unique_profile = std::make_unique<autofill::AutofillProfile>(profile);
  // Temporary workaround so that fields like first/last name a properly
  // populated.
  unique_profile->FinalizeAfterImport();
  return unique_profile;
}

ClientStatus GetFormattedClientValue(const AutofillValue& autofill_value,
                                     const UserData& user_data,
                                     std::string* out_value) {
  return ExtractDataAndFormatClientValue(
      autofill_value, autofill_value.value_expression(), user_data,
      /* quote_meta= */ false, autofill_value.locale(), out_value);
}

ClientStatus GetFormattedClientValue(
    const AutofillValueRegexp& autofill_value_regexp,
    const UserData& user_data,
    std::string* out_value) {
  return ExtractDataAndFormatClientValue(
      autofill_value_regexp,
      autofill_value_regexp.value_expression_re2().value_expression(),
      user_data,
      /* quote_meta= */ true, autofill_value_regexp.locale(), out_value);
}

void GetPasswordManagerValue(
    const PasswordManagerValue& password_manager_value,
    const ElementFinderResult& target_element,
    const UserData* user_data,
    WebsiteLoginManager* website_login_manager,
    base::OnceCallback<void(const ClientStatus&, const std::string&)>
        callback) {
  if (!user_data->selected_login_) {
    std::move(callback).Run(ClientStatus(PRECONDITION_FAILED), std::string());
    return;
  }
  auto* target_render_frame_host = target_element.render_frame_host();
  if (!target_render_frame_host) {
    std::move(callback).Run(ClientStatus(PASSWORD_ORIGIN_MISMATCH),
                            std::string());
    return;
  }

  switch (password_manager_value.credential_type()) {
    case PasswordManagerValue::PASSWORD: {
      auto login = *user_data->selected_login_;
      // Origin check is done in PWM based on the
      // |target_render_frame_host->GetLastCommittedURL()|
      login.origin = target_render_frame_host->GetLastCommittedURL()
                         .DeprecatedGetOriginAsURL();
      website_login_manager->GetPasswordForLogin(
          login, base::BindOnce(&OnGetStoredPassword, std::move(callback)));
      return;
    }
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
                                        const UserModel* user_model,
                                        std::string* out_value) {
  if (client_memory_key.empty()) {
    return ClientStatus(INVALID_ACTION);
  }
  bool user_data_has_value = user_data->HasAdditionalValue(client_memory_key) &&
                             user_data->GetAdditionalValue(client_memory_key)
                                     ->strings()
                                     .values()
                                     .size() == 1;
  bool user_model_has_value =
      user_model->GetValue(client_memory_key).has_value() &&
      user_model->GetValue(client_memory_key)->strings().values_size() == 1;
  if (!user_data_has_value && !user_model_has_value) {
    VLOG(1) << "Requested key '" << client_memory_key
            << "' not present in user data and user model";
    return ClientStatus(PRECONDITION_FAILED);
  } else if (user_data_has_value && user_model_has_value &&
             user_data->GetAdditionalValue(client_memory_key)
                     ->strings()
                     .values(0) !=
                 user_model->GetValue(client_memory_key)->strings().values(0)) {
    VLOG(1) << "Requested key '" << client_memory_key
            << "' has different values in user data and user model";
    return ClientStatus(PRECONDITION_FAILED);
  }
  if (user_data_has_value) {
    out_value->assign(
        user_data->GetAdditionalValue(client_memory_key)->strings().values(0));
  } else {
    out_value->assign(
        user_model->GetValue(client_memory_key)->strings().values(0));
  }
  return OkClientStatus();
}

void ResolveTextValue(const TextValue& text_value,
                      const ElementFinderResult& target_element,
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
      status = GetFormattedClientValue(text_value.autofill_value(),
                                       *action_delegate->GetUserData(), &value);
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
      status = GetClientMemoryStringValue(
          text_value.client_memory_key(), action_delegate->GetUserData(),
          action_delegate->GetUserModel(), &value);
      break;
    }
    case TextValue::VALUE_NOT_SET:
      status = ClientStatus(INVALID_ACTION);
  }

  std::move(callback).Run(status, value);
}

Metrics::UserDataSelectionState GetNewSelectionState(
    Metrics::UserDataSelectionState old_state,
    UserDataEventType event_type) {
  switch (event_type) {
    case ENTRY_EDITED: {
      switch (old_state) {
        case Metrics::UserDataSelectionState::NO_CHANGE:
          return Metrics::UserDataSelectionState::EDIT_PRESELECTED;
        case Metrics::UserDataSelectionState::SELECTED_DIFFERENT_ENTRY:
          return Metrics::UserDataSelectionState::
              SELECTED_DIFFERENT_AND_MODIFIED_ENTRY;
        case Metrics::UserDataSelectionState::NEW_ENTRY:
        case Metrics::UserDataSelectionState::
            SELECTED_DIFFERENT_AND_MODIFIED_ENTRY:
        case Metrics::UserDataSelectionState::EDIT_PRESELECTED:
          return old_state;
      }
    }
    case SELECTION_CHANGED: {
      switch (old_state) {
        case Metrics::UserDataSelectionState::NO_CHANGE:
        case Metrics::UserDataSelectionState::EDIT_PRESELECTED:
          return Metrics::UserDataSelectionState::SELECTED_DIFFERENT_ENTRY;
        case Metrics::UserDataSelectionState::SELECTED_DIFFERENT_ENTRY:
        case Metrics::UserDataSelectionState::NEW_ENTRY:
        case Metrics::UserDataSelectionState::
            SELECTED_DIFFERENT_AND_MODIFIED_ENTRY:
          // We keep the state which represents the greater effort for the user.
          return old_state;
      }
    }
    case ENTRY_CREATED:
      return Metrics::UserDataSelectionState::NEW_ENTRY;
    case UNKNOWN:
    case NO_NOTIFICATION:
      return old_state;
  }
}

int GetFieldBitArrayForAddress(const autofill::AutofillProfile* profile) {
  return GetFieldBitArrayForAddressAndPhoneNumber(profile, profile);
}

int GetFieldBitArrayForAddressAndPhoneNumber(
    const autofill::AutofillProfile* profile,
    const autofill::AutofillProfile* phone_number_profile) {
  // Maps from the autofill field type to the respective position in the metrics
  // bitarray.
  static const base::NoDestructor<std::vector<std::pair<
      autofill::ServerFieldType, Metrics::AutofillAssistantProfileFields>>>
      fields_to_log(
          {{autofill::NAME_FIRST,
            Metrics::AutofillAssistantProfileFields::NAME_FIRST},
           {autofill::NAME_LAST,
            Metrics::AutofillAssistantProfileFields::NAME_LAST},
           {autofill::NAME_FULL,
            Metrics::AutofillAssistantProfileFields::NAME_FULL},
           {autofill::EMAIL_ADDRESS,
            Metrics::AutofillAssistantProfileFields::EMAIL_ADDRESS},
           {autofill::ADDRESS_HOME_COUNTRY,
            Metrics::AutofillAssistantProfileFields::ADDRESS_HOME_COUNTRY},
           {autofill::ADDRESS_HOME_STATE,
            Metrics::AutofillAssistantProfileFields::ADDRESS_HOME_STATE},
           {autofill::ADDRESS_HOME_CITY,
            Metrics::AutofillAssistantProfileFields::ADDRESS_HOME_CITY},
           {autofill::ADDRESS_HOME_ZIP,
            Metrics::AutofillAssistantProfileFields::ADDRESS_HOME_ZIP},
           {autofill::ADDRESS_HOME_STREET_ADDRESS,
            Metrics::AutofillAssistantProfileFields::ADDRESS_HOME_LINE1}});

  // Maps from the phone-related autofill field types to the respective position
  // in the metrics bitarray.
  static const base::NoDestructor<std::vector<std::pair<
      autofill::ServerFieldType, Metrics::AutofillAssistantProfileFields>>>
      phone_number_fields_to_log(
          {{autofill::PHONE_HOME_NUMBER,
            Metrics::AutofillAssistantProfileFields::PHONE_HOME_NUMBER},
           {autofill::PHONE_HOME_COUNTRY_CODE,
            Metrics::AutofillAssistantProfileFields::PHONE_HOME_COUNTRY_CODE},
           {autofill::PHONE_HOME_WHOLE_NUMBER,
            Metrics::AutofillAssistantProfileFields::PHONE_HOME_WHOLE_NUMBER}});

  int bit_array = 0;
  // Check the non-phone fields.
  if (profile) {
    auto mapping =
        field_formatter::CreateAutofillMappings(*profile, kDefaultLocale);
    for (auto fields_pair : *fields_to_log) {
      if (EvaluateNotEmpty(mapping, fields_pair.first)) {
        bit_array |= fields_pair.second;
      }
    }
  }
  // Check the phone fields.
  if (phone_number_profile) {
    auto mapping = field_formatter::CreateAutofillMappings(
        *phone_number_profile, kDefaultLocale);
    for (auto fields_pair : *phone_number_fields_to_log) {
      if (EvaluateNotEmpty(mapping, fields_pair.first)) {
        bit_array |= fields_pair.second;
      }
    }
  }
  return bit_array;
}

int GetFieldBitArrayForCreditCard(const autofill::CreditCard* card) {
  // If the card is nullptr, we consider all fields as missing.
  if (!card) {
    return 0;
  }

  auto mapping = field_formatter::CreateAutofillMappings(*card, kDefaultLocale);
  // Maps from the autofill field type to the respective position in the metrics
  // bitarray.
  static const base::NoDestructor<std::vector<std::pair<
      autofill::ServerFieldType, Metrics::AutofillAssistantCreditCardFields>>>
      fields_to_log(
          {{autofill::CREDIT_CARD_NAME_FULL,
            Metrics::AutofillAssistantCreditCardFields::CREDIT_CARD_NAME_FULL},
           {autofill::CREDIT_CARD_EXP_MONTH,
            Metrics::AutofillAssistantCreditCardFields::CREDIT_CARD_EXP_MONTH},
           {autofill::CREDIT_CARD_EXP_2_DIGIT_YEAR,
            Metrics::AutofillAssistantCreditCardFields::
                CREDIT_CARD_EXP_2_DIGIT_YEAR},
           {autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR,
            Metrics::AutofillAssistantCreditCardFields::
                CREDIT_CARD_EXP_4_DIGIT_YEAR}});

  int bit_array = 0;
  for (auto fields_pair : *fields_to_log) {
    if (EvaluateNotEmpty(mapping, fields_pair.first)) {
      bit_array |= fields_pair.second;
    }
  }

  if (card->record_type() == autofill::CreditCard::MASKED_SERVER_CARD) {
    bit_array |= Metrics::AutofillAssistantCreditCardFields::MASKED;
    // If the card is masked, we log the number as valid, to match what
    // CollectUserData considers complete for the purposes of enabling the
    // "Continue" button.
    bit_array |= Metrics::AutofillAssistantCreditCardFields::VALID_NUMBER;
  } else if (card->HasValidCardNumber()) {
    bit_array |= Metrics::AutofillAssistantCreditCardFields::VALID_NUMBER;
  }

  return bit_array;
}

ClientStatus ResolveSelectorUserData(SelectorProto* selector,
                                     const UserData* user_data) {
  for (auto& filter : *selector->mutable_filters()) {
    switch (filter.filter_case()) {
      case SelectorProto::Filter::kProperty: {
        ClientStatus filter_status = MoveAutofillValueRegexpToTextFilter(
            user_data, filter.mutable_property());
        if (!filter_status.ok()) {
          return filter_status;
        }
        break;
      }
      case SelectorProto::Filter::kInnerText:
      case SelectorProto::Filter::kValue:
      case SelectorProto::Filter::kPseudoElementContent:
      case SelectorProto::Filter::kCssStyle:
      case SelectorProto::Filter::kCssSelector:
      case SelectorProto::Filter::kEnterFrame:
      case SelectorProto::Filter::kPseudoType:
      case SelectorProto::Filter::kBoundingBox:
      case SelectorProto::Filter::kNthMatch:
      case SelectorProto::Filter::kLabelled:
      case SelectorProto::Filter::kMatchCssSelector:
      case SelectorProto::Filter::kOnTop:
      case SelectorProto::Filter::kParent:
      case SelectorProto::Filter::kSemantic:
      case SelectorProto::Filter::FILTER_NOT_SET:
        break;
        // Do not add default here. In case a new filter gets added (that may
        // contain a RegexpFilter) we want this to fail at compilation here.
    }
  }
  return OkClientStatus();
}

void UpsertContact(const autofill::AutofillProfile& profile,
                   std::vector<std::unique_ptr<Contact>>& list) {
  UpsertAutofillProfile(profile, list);
}

void UpsertPhoneNumber(const autofill::AutofillProfile& profile,
                       std::vector<std::unique_ptr<PhoneNumber>>& list) {
  UpsertAutofillProfile(profile, list);
}

bool ContactHasAtLeastOneRequiredField(
    const autofill::AutofillProfile& profile,
    const CollectUserDataOptions& collect_user_data_options) {
  autofill::ServerFieldTypeSet non_empty_fields;
  profile.GetNonEmptyTypes(kDefaultLocale, &non_empty_fields);

  if (collect_user_data_options.request_payer_name &&
      (non_empty_fields.contains(autofill::NAME_FULL) ||
       non_empty_fields.contains(autofill::NAME_FIRST) ||
       non_empty_fields.contains(autofill::NAME_LAST))) {
    return true;
  }

  if (collect_user_data_options.request_payer_email &&
      non_empty_fields.contains(autofill::EMAIL_ADDRESS)) {
    return true;
  }

  if (collect_user_data_options.request_payer_phone &&
      (non_empty_fields.contains(autofill::PHONE_HOME_NUMBER) ||
       non_empty_fields.contains(autofill::PHONE_HOME_COUNTRY_CODE) ||
       non_empty_fields.contains(autofill::PHONE_HOME_WHOLE_NUMBER))) {
    return true;
  }
  return false;
}

std::vector<autofill::AutofillProfile*> GetUniqueProfiles(
    const std::vector<autofill::AutofillProfile*> sorted_profiles,
    const std::string app_locale,
    const base::flat_set<autofill::ServerFieldType>& field_types) {
  std::vector<autofill::AutofillProfile*> unique_profiles;
  const autofill::AutofillProfileComparator comparator(app_locale);
  autofill::ServerFieldTypeSet types(field_types.begin(), field_types.end());
  for (size_t i = 0; i < sorted_profiles.size(); ++i) {
    bool include = true;

    autofill::AutofillProfile* profile_a = sorted_profiles[i];
    for (size_t j = 0; j < sorted_profiles.size(); ++j) {
      autofill::AutofillProfile* profile_b = sorted_profiles[j];
      // Check if profile A is a subset of profile B. If not, continue.
      if (i == j || !profile_a->IsSubsetOfForFieldSet(comparator, *profile_b,
                                                      app_locale, types)) {
        continue;
      }

      // Check if profile B is also a subset of profile A. If so, the
      // profiles are identical. Include the first one but not the second.
      if (i < j && profile_b->IsSubsetOfForFieldSet(comparator, *profile_a,
                                                    app_locale, types)) {
        continue;
      }

      // One-way subset. Don't include profile A.
      include = false;
      break;
    }
    if (include) {
      unique_profiles.push_back(sorted_profiles[i]);
    }
  }
  return unique_profiles;
}

}  // namespace user_data
}  // namespace autofill_assistant
