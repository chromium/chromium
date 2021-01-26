// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/user_data_util.h"

#include <numeric>
#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill_assistant/browser/field_formatter.h"
#include "third_party/libaddressinput/chromium/addressinput_util.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/re2/src/re2/re2.h"

namespace autofill_assistant {
namespace {

// TODO: Share this helper function with use_address_action.
base::string16 GetProfileFullName(const autofill::AutofillProfile& profile) {
  return autofill::data_util::JoinNameParts(
      profile.GetRawInfo(autofill::NAME_FIRST),
      profile.GetRawInfo(autofill::NAME_MIDDLE),
      profile.GetRawInfo(autofill::NAME_LAST));
}

int CountCompleteContactFields(const CollectUserDataOptions& options,
                               const autofill::AutofillProfile& profile) {
  int completed_fields = 0;
  if (options.request_payer_name && !GetProfileFullName(profile).empty()) {
    ++completed_fields;
  }
  if (options.request_shipping &&
      !profile.GetRawInfo(autofill::ADDRESS_HOME_STREET_ADDRESS).empty()) {
    ++completed_fields;
  }
  if (options.request_payer_email &&
      !profile.GetRawInfo(autofill::EMAIL_ADDRESS).empty()) {
    ++completed_fields;
  }
  if (options.request_payer_phone &&
      !profile.GetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER).empty()) {
    ++completed_fields;
  }
  return completed_fields;
}

// Helper function that compares instances of AutofillProfile by completeness
// in regards to the current options. Full profiles should be ordered before
// empty ones and fall back to compare the profile's name in case of equality.
bool CompletenessCompareContacts(const CollectUserDataOptions& options,
                                 const autofill::AutofillProfile& a,
                                 const autofill::AutofillProfile& b) {
  int complete_fields_a = CountCompleteContactFields(options, a);
  int complete_fields_b = CountCompleteContactFields(options, b);
  if (complete_fields_a == complete_fields_b) {
    return base::i18n::ToLower(GetProfileFullName(a))
               .compare(base::i18n::ToLower(GetProfileFullName(b))) < 0;
  }
  return complete_fields_a > complete_fields_b;
}

int GetAddressCompletenessRating(const CollectUserDataOptions& options,
                                 const autofill::AutofillProfile& profile) {
  auto address_data =
      autofill::i18n::CreateAddressDataFromAutofillProfile(profile, "en-US");
  std::multimap<i18n::addressinput::AddressField,
                i18n::addressinput::AddressProblem>
      problems;
  autofill::addressinput::ValidateRequiredFields(
      *address_data, /* filter= */ nullptr, &problems);
  return -problems.size();
}

// Helper function that compares instances of AutofillProfile by completeness
// in regards to the current options. Full profiles should be ordered before
// empty ones and fall back to compare the profile's name in case of equality.
bool CompletenessCompareAddresses(const CollectUserDataOptions& options,
                                  const autofill::AutofillProfile& a,
                                  const autofill::AutofillProfile& b) {
  int complete_fields_a = GetAddressCompletenessRating(options, a);
  int complete_fields_b = GetAddressCompletenessRating(options, b);
  if (complete_fields_a == complete_fields_b) {
    return base::i18n::ToLower(GetProfileFullName(a))
               .compare(base::i18n::ToLower(GetProfileFullName(b))) < 0;
  }
  return complete_fields_a > complete_fields_b;
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
  auto address_data =
      autofill::i18n::CreateAddressDataFromAutofillProfile(*profile, "en-US");
  if (!autofill::addressinput::HasAllRequiredFields(*address_data)) {
    return false;
  }

  if (require_postal_code && address_data->postal_code.empty()) {
    return false;
  }

  return true;
}

template <typename T>
ClientStatus ExtractProfileAndFormatAutofillValue(
    const T& profile,
    const std::string& value_expression,
    const UserData* user_data,
    bool quote_meta,
    std::string* out_value) {
  if (profile.identifier().empty() || value_expression.empty()) {
    VLOG(1) << "|autofill_value| with empty "
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
      field_formatter::CreateAutofillMappings(*address,
                                              /* locale= */ "en-US");
  if (quote_meta) {
    for (const auto& it : mappings) {
      mappings[it.first] = re2::RE2::QuoteMeta(it.second);
    }
  }
  auto value = field_formatter::FormatString(value_expression, mappings,
                                             /* strict= */ true);
  if (!value.has_value()) {
    return ClientStatus(AUTOFILL_INFO_NOT_AVAILABLE);
  }

  out_value->assign(*value);
  return OkClientStatus();
}

}  // namespace

std::unique_ptr<autofill::AutofillProfile> MakeUniqueFromProfile(
    const autofill::AutofillProfile& profile) {
  auto unique_profile = std::make_unique<autofill::AutofillProfile>(profile);
  // Temporary workaround so that fields like first/last name a properly
  // populated.
  unique_profile->FinalizeAfterImport();
  return unique_profile;
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

std::vector<int> SortAddressesByCompleteness(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<autofill::AutofillProfile>>& profiles) {
  std::vector<int> profile_indices(profiles.size());
  std::iota(std::begin(profile_indices), std::end(profile_indices), 0);
  std::sort(profile_indices.begin(), profile_indices.end(),
            [&collect_user_data_options, &profiles](int i, int j) {
              return CompletenessCompareAddresses(collect_user_data_options,
                                                  *profiles[i], *profiles[j]);
            });
  return profile_indices;
}

int GetDefaultAddressProfile(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<autofill::AutofillProfile>>& profiles) {
  if (profiles.empty()) {
    return -1;
  }
  auto sorted_indices =
      SortContactsByCompleteness(collect_user_data_options, profiles);
  return sorted_indices[0];
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

bool IsCompleteContact(
    const autofill::AutofillProfile* profile,
    const CollectUserDataOptions& collect_user_data_options) {
  if (!collect_user_data_options.request_payer_name &&
      !collect_user_data_options.request_payer_email &&
      !collect_user_data_options.request_payer_phone) {
    return true;
  }

  if (!profile) {
    return false;
  }

  if (collect_user_data_options.request_payer_name &&
      !profile->HasInfo(autofill::NAME_FULL)) {
    return false;
  }

  if (collect_user_data_options.request_payer_email &&
      !profile->HasInfo(autofill::EMAIL_ADDRESS)) {
    return false;
  }

  if (collect_user_data_options.request_payer_phone &&
      !profile->HasInfo(autofill::PHONE_HOME_WHOLE_NUMBER)) {
    return false;
  }
  return true;
}

bool IsCompleteShippingAddress(
    const autofill::AutofillProfile* profile,
    const CollectUserDataOptions& collect_user_data_options) {
  return !collect_user_data_options.request_shipping ||
         IsCompleteAddress(profile, /* require_postal_code = */ false);
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
  return ExtractProfileAndFormatAutofillValue<AutofillValue::Profile>(
      autofill_value.profile(), autofill_value.value_expression(), user_data,
      /* quote_meta= */ false, out_value);
}

ClientStatus GetFormattedAutofillValue(
    const AutofillValueRegexp& autofill_value,
    const UserData* user_data,
    std::string* out_value) {
  return ExtractProfileAndFormatAutofillValue<AutofillValueRegexp::Profile>(
      autofill_value.profile(), autofill_value.value_expression().re2(),
      user_data, /* quote_meta= */ true, out_value);
}

}  // namespace autofill_assistant
