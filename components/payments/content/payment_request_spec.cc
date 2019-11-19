// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request_spec.h"

#include <utility>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/payments/content/payment_request_converter.h"
#include "components/payments/core/features.h"
#include "components/payments/core/method_strings.h"
#include "components/payments/core/payment_app.h"
#include "components/payments/core/payment_method_data.h"
#include "components/payments/core/payment_request_data_util.h"
#include "components/payments/core/payments_experimental_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace payments {

namespace {

// Validates the |method_data| and fills the output parameters.
void PopulateValidatedMethodData(
    const std::vector<PaymentMethodData>& method_data_vector,
    std::vector<std::string>* supported_card_networks,
    std::set<std::string>* basic_card_specified_networks,
    std::set<std::string>* supported_card_networks_set,
    std::set<autofill::CreditCard::CardType>* supported_card_types_set,
    std::vector<GURL>* url_payment_method_identifiers,
    std::set<std::string>* payment_method_identifiers_set,
    std::map<std::string, std::set<std::string>>* stringified_method_data) {
  data_util::ParseSupportedMethods(method_data_vector, supported_card_networks,
                                   basic_card_specified_networks,
                                   url_payment_method_identifiers,
                                   payment_method_identifiers_set);
  supported_card_networks_set->insert(supported_card_networks->begin(),
                                      supported_card_networks->end());

  data_util::ParseSupportedCardTypes(method_data_vector,
                                     supported_card_types_set);
}

void PopulateValidatedMethodData(
    const std::vector<mojom::PaymentMethodDataPtr>& method_data_mojom,
    std::vector<std::string>* supported_card_networks,
    std::set<std::string>* basic_card_specified_networks,
    std::set<std::string>* supported_card_networks_set,
    std::set<autofill::CreditCard::CardType>* supported_card_types_set,
    std::vector<GURL>* url_payment_method_identifiers,
    std::set<std::string>* payment_method_identifiers_set,
    std::map<std::string, std::set<std::string>>* stringified_method_data) {
  std::vector<PaymentMethodData> method_data_vector;
  method_data_vector.reserve(method_data_mojom.size());
  for (const mojom::PaymentMethodDataPtr& method_data_entry :
       method_data_mojom) {
    (*stringified_method_data)[method_data_entry->supported_method].insert(
        method_data_entry->stringified_data);

    method_data_vector.push_back(ConvertPaymentMethodData(method_data_entry));
  }

  PopulateValidatedMethodData(
      method_data_vector, supported_card_networks,
      basic_card_specified_networks, supported_card_networks_set,
      supported_card_types_set, url_payment_method_identifiers,
      payment_method_identifiers_set, stringified_method_data);
}

std::string ToString(bool value) {
  return value ? "true" : "false";
}

}  // namespace

PaymentRequestSpec::PaymentRequestSpec(
    mojom::PaymentOptionsPtr options,
    mojom::PaymentDetailsPtr details,
    std::vector<mojom::PaymentMethodDataPtr> method_data,
    Observer* observer,
    const std::string& app_locale)
    : options_(std::move(options)),
      details_(std::move(details)),
      method_data_(std::move(method_data)),
      app_locale_(app_locale),
      selected_shipping_option_(nullptr),
      current_update_reason_(UpdateReason::NONE) {
  if (observer)
    AddObserver(observer);
  if (!details_->display_items)
    details_->display_items = std::vector<mojom::PaymentItemPtr>();
  if (!details_->shipping_options)
    details_->shipping_options = std::vector<mojom::PaymentShippingOptionPtr>();
  if (!details_->modifiers)
    details_->modifiers = std::vector<mojom::PaymentDetailsModifierPtr>();
  UpdateSelectedShippingOption(/*after_update=*/false);
  PopulateValidatedMethodData(
      method_data_, &supported_card_networks_, &basic_card_specified_networks_,
      &supported_card_networks_set_, &supported_card_types_set_,
      &url_payment_method_identifiers_, &payment_method_identifiers_set_,
      &stringified_method_data_);

  query_for_quota_ = stringified_method_data_;
  if (base::Contains(payment_method_identifiers_set_, methods::kBasicCard) &&
      PaymentsExperimentalFeatures::IsEnabled(
          features::kStrictHasEnrolledAutofillInstrument)) {
    query_for_quota_["basic-card-payment-options"] = {
        base::ReplaceStringPlaceholders(
            "{payerEmail:$1,payerName:$2,payerPhone:$3,shipping:$4}",
            {ToString(request_payer_email()), ToString(request_payer_name()),
             ToString(request_payer_phone()), ToString(request_shipping())},
            nullptr)};
  }
}
PaymentRequestSpec::~PaymentRequestSpec() {}

void PaymentRequestSpec::UpdateWith(mojom::PaymentDetailsPtr details) {
  DCHECK(details_);
  DCHECK(details_->total || details->total);
  if (details->total)
    details_->total = std::move(details->total);
  if (details->display_items)
    details_->display_items = std::move(details->display_items);
  if (details->shipping_options)
    details_->shipping_options = std::move(details->shipping_options);
  if (details->modifiers)
    details_->modifiers = std::move(details->modifiers);
  details_->error = std::move(details->error);
  if (details->shipping_address_errors)
    details_->shipping_address_errors =
        std::move(details->shipping_address_errors);
  if (details->id)
    details_->id = std::move(details->id);
  DCHECK(details_->total);
  DCHECK(details_->display_items);
  DCHECK(details_->shipping_options);
  DCHECK(details_->modifiers);
  RecomputeSpecForDetails();
}

void PaymentRequestSpec::Retry(
    mojom::PaymentValidationErrorsPtr validation_errors) {
  if (!validation_errors)
    return;

  retry_error_message_ =
      validation_errors->error.empty()
          ? l10n_util::GetStringUTF16(IDS_PAYMENTS_ERROR_MESSAGE)
          : base::UTF8ToUTF16(std::move(validation_errors->error));
  details_->shipping_address_errors =
      std::move(validation_errors->shipping_address);
  payer_errors_ = std::move(validation_errors->payer);
  current_update_reason_ = UpdateReason::RETRY;
  NotifyOnSpecUpdated();
  current_update_reason_ = UpdateReason::NONE;
}

base::string16 PaymentRequestSpec::GetShippingAddressError(
    autofill::ServerFieldType type) {
  if (!details_->shipping_address_errors)
    return base::string16();

  if (type == autofill::ADDRESS_HOME_STREET_ADDRESS)
    return base::UTF8ToUTF16(details_->shipping_address_errors->address_line);

  if (type == autofill::ADDRESS_HOME_CITY)
    return base::UTF8ToUTF16(details_->shipping_address_errors->city);

  if (type == autofill::ADDRESS_HOME_COUNTRY)
    return base::UTF8ToUTF16(details_->shipping_address_errors->country);

  if (type == autofill::ADDRESS_HOME_DEPENDENT_LOCALITY)
    return base::UTF8ToUTF16(
        details_->shipping_address_errors->dependent_locality);

  if (type == autofill::COMPANY_NAME)
    return base::UTF8ToUTF16(details_->shipping_address_errors->organization);

  if (type == autofill::PHONE_HOME_WHOLE_NUMBER)
    return base::UTF8ToUTF16(details_->shipping_address_errors->phone);

  if (type == autofill::ADDRESS_HOME_ZIP)
    return base::UTF8ToUTF16(details_->shipping_address_errors->postal_code);

  if (type == autofill::NAME_FULL)
    return base::UTF8ToUTF16(details_->shipping_address_errors->recipient);

  if (type == autofill::ADDRESS_HOME_STATE)
    return base::UTF8ToUTF16(details_->shipping_address_errors->region);

  if (type == autofill::ADDRESS_HOME_SORTING_CODE)
    return base::UTF8ToUTF16(details_->shipping_address_errors->sorting_code);

  return base::string16();
}

base::string16 PaymentRequestSpec::GetPayerError(
    autofill::ServerFieldType type) {
  if (!payer_errors_)
    return base::string16();

  if (type == autofill::EMAIL_ADDRESS)
    return base::UTF8ToUTF16(payer_errors_->email);

  if (type == autofill::NAME_FULL)
    return base::UTF8ToUTF16(payer_errors_->name);

  if (type == autofill::PHONE_HOME_WHOLE_NUMBER)
    return base::UTF8ToUTF16(payer_errors_->phone);

  return base::string16();
}

bool PaymentRequestSpec::has_shipping_address_error() const {
  return details_->shipping_address_errors && request_shipping() &&
         !(details_->shipping_address_errors->address_line.empty() &&
           details_->shipping_address_errors->city.empty() &&
           details_->shipping_address_errors->country.empty() &&
           details_->shipping_address_errors->dependent_locality.empty() &&
           details_->shipping_address_errors->organization.empty() &&
           details_->shipping_address_errors->phone.empty() &&
           details_->shipping_address_errors->postal_code.empty() &&
           details_->shipping_address_errors->recipient.empty() &&
           details_->shipping_address_errors->region.empty() &&
           details_->shipping_address_errors->sorting_code.empty());
}

bool PaymentRequestSpec::has_payer_error() const {
  return payer_errors_ &&
         (request_payer_email() || request_payer_name() ||
          request_payer_phone()) &&
         !(payer_errors_->email.empty() && payer_errors_->name.empty() &&
           payer_errors_->phone.empty());
}

void PaymentRequestSpec::RecomputeSpecForDetails() {
  // Reparse the |details_| and update the observers.
  bool is_initialization =
      current_update_reason_ == UpdateReason::INITIAL_PAYMENT_DETAILS;
  UpdateSelectedShippingOption(/*after_update=*/!is_initialization);

  NotifyOnSpecUpdated();

  if (is_initialization)
    NotifyInitialized();

  current_update_reason_ = UpdateReason::NONE;
}

void PaymentRequestSpec::AddObserver(Observer* observer) {
  CHECK(observer);
  observers_.AddObserver(observer);
}

void PaymentRequestSpec::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool PaymentRequestSpec::IsInitialized() const {
  return current_update_reason_ != UpdateReason::INITIAL_PAYMENT_DETAILS;
}

bool PaymentRequestSpec::request_shipping() const {
  return options_->request_shipping;
}
bool PaymentRequestSpec::request_payer_name() const {
  return options_->request_payer_name;
}
bool PaymentRequestSpec::request_payer_phone() const {
  return options_->request_payer_phone;
}
bool PaymentRequestSpec::request_payer_email() const {
  return options_->request_payer_email;
}

PaymentShippingType PaymentRequestSpec::shipping_type() const {
  // Transform Mojo-specific enum into platform-agnostic equivalent.
  switch (options_->shipping_type) {
    case mojom::PaymentShippingType::DELIVERY:
      return PaymentShippingType::DELIVERY;
    case payments::mojom::PaymentShippingType::PICKUP:
      return PaymentShippingType::PICKUP;
    case payments::mojom::PaymentShippingType::SHIPPING:
      return PaymentShippingType::SHIPPING;
    default:
      NOTREACHED();
  }
  // Needed for compilation on some platforms.
  return PaymentShippingType::SHIPPING;
}

bool PaymentRequestSpec::IsMethodSupportedThroughBasicCard(
    const std::string& method_name) {
  return basic_card_specified_networks_.count(method_name) > 0;
}

base::string16 PaymentRequestSpec::GetFormattedCurrencyAmount(
    const mojom::PaymentCurrencyAmountPtr& currency_amount) {
  CurrencyFormatter* formatter =
      GetOrCreateCurrencyFormatter(currency_amount->currency, app_locale_);
  return formatter->Format(currency_amount->value);
}

std::string PaymentRequestSpec::GetFormattedCurrencyCode(
    const mojom::PaymentCurrencyAmountPtr& currency_amount) {
  CurrencyFormatter* formatter =
      GetOrCreateCurrencyFormatter(currency_amount->currency, app_locale_);

  return formatter->formatted_currency_code();
}

void PaymentRequestSpec::StartWaitingForUpdateWith(
    PaymentRequestSpec::UpdateReason reason) {
  current_update_reason_ = reason;
  for (auto& observer : observers_) {
    observer.OnStartUpdating(reason);
  }
}

bool PaymentRequestSpec::IsMixedCurrency() const {
  DCHECK(details_->display_items);
  const std::string& total_currency = details_->total->amount->currency;
  return std::any_of(details_->display_items->begin(),
                     details_->display_items->end(),
                     [&total_currency](const mojom::PaymentItemPtr& item) {
                       return item->amount->currency != total_currency;
                     });
}

const mojom::PaymentItemPtr& PaymentRequestSpec::GetTotal(
    PaymentApp* selected_app) const {
  const mojom::PaymentDetailsModifierPtr* modifier =
      GetApplicableModifier(selected_app);
  return modifier && (*modifier)->total ? (*modifier)->total : details_->total;
}

std::vector<const mojom::PaymentItemPtr*> PaymentRequestSpec::GetDisplayItems(
    PaymentApp* selected_app) const {
  std::vector<const mojom::PaymentItemPtr*> display_items;
  const mojom::PaymentDetailsModifierPtr* modifier =
      GetApplicableModifier(selected_app);
  DCHECK(details_->display_items);
  for (const auto& item : *details_->display_items) {
    display_items.push_back(&item);
  }

  if (modifier) {
    for (const auto& additional_item : (*modifier)->additional_display_items) {
      display_items.push_back(&additional_item);
    }
  }
  return display_items;
}

const std::vector<mojom::PaymentShippingOptionPtr>&
PaymentRequestSpec::GetShippingOptions() const {
  DCHECK(details_->shipping_options);
  return *details_->shipping_options;
}

const mojom::PaymentDetailsModifierPtr*
PaymentRequestSpec::GetApplicableModifier(PaymentApp* selected_app) const {
  if (!selected_app ||
      !base::FeatureList::IsEnabled(features::kWebPaymentsModifiers))
    return nullptr;

  DCHECK(details_->modifiers);
  for (const auto& modifier : *details_->modifiers) {
    std::set<std::string> supported_card_networks_set;
    std::set<autofill::CreditCard::CardType> supported_types;
    // The following 4 are unused but required by PopulateValidatedMethodData.
    std::set<std::string> basic_card_specified_networks;
    std::vector<std::string> supported_networks;
    std::vector<GURL> url_payment_method_identifiers;
    std::set<std::string> payment_method_identifiers_set;
    std::map<std::string, std::set<std::string>> stringified_method_data;
    PopulateValidatedMethodData(
        {ConvertPaymentMethodData(modifier->method_data)}, &supported_networks,
        &basic_card_specified_networks, &supported_card_networks_set,
        &supported_types, &url_payment_method_identifiers,
        &payment_method_identifiers_set, &stringified_method_data);

    if (selected_app->IsValidForModifier(
            modifier->method_data->supported_method,
            !modifier->method_data->supported_networks.empty(),
            supported_card_networks_set,
            !modifier->method_data->supported_types.empty(), supported_types)) {
      return &modifier;
    }
  }
  return nullptr;
}

void PaymentRequestSpec::UpdateSelectedShippingOption(bool after_update) {
  if (!request_shipping() || !details_->shipping_options)
    return;

  selected_shipping_option_ = nullptr;
  selected_shipping_option_error_.clear();
  if (details_->shipping_options->empty() || !details_->error.empty()) {
    // The merchant provided either no shipping options or an error message.
    if (after_update) {
      // This is after an update, which means that the selected address is not
      // supported. The merchant may have customized the error string, or a
      // generic one is used.
      if (!details_->error.empty()) {
        selected_shipping_option_error_ = base::UTF8ToUTF16(details_->error);
      } else {
        // The generic error string depends on the shipping type.
        switch (shipping_type()) {
          case PaymentShippingType::DELIVERY:
            selected_shipping_option_error_ = l10n_util::GetStringUTF16(
                IDS_PAYMENTS_UNSUPPORTED_DELIVERY_ADDRESS);
            break;
          case PaymentShippingType::PICKUP:
            selected_shipping_option_error_ = l10n_util::GetStringUTF16(
                IDS_PAYMENTS_UNSUPPORTED_PICKUP_ADDRESS);
            break;
          case PaymentShippingType::SHIPPING:
            selected_shipping_option_error_ = l10n_util::GetStringUTF16(
                IDS_PAYMENTS_UNSUPPORTED_SHIPPING_ADDRESS);
            break;
        }
      }
    }
    return;
  }

  // As per the spec, the selected shipping option should initially be the last
  // one in the array that has its selected field set to true. If none are
  // selected by the merchant, |selected_shipping_option_| stays nullptr.
  auto selected_shipping_option_it = std::find_if(
      details_->shipping_options->rbegin(), details_->shipping_options->rend(),
      [](const payments::mojom::PaymentShippingOptionPtr& element) {
        return element->selected;
      });
  if (selected_shipping_option_it != details_->shipping_options->rend()) {
    selected_shipping_option_ = selected_shipping_option_it->get();
  }
}

void PaymentRequestSpec::NotifyOnSpecUpdated() {
  for (auto& observer : observers_)
    observer.OnSpecUpdated();
}

CurrencyFormatter* PaymentRequestSpec::GetOrCreateCurrencyFormatter(
    const std::string& currency_code,
    const std::string& locale_name) {
  // Create a currency formatter for |currency_code|, or if already created
  // return the cached version.
  std::pair<std::map<std::string, CurrencyFormatter>::iterator, bool>
      emplace_result = currency_formatters_.emplace(
          std::piecewise_construct, std::forward_as_tuple(currency_code),
          std::forward_as_tuple(currency_code, locale_name));

  return &(emplace_result.first->second);
}

}  // namespace payments
