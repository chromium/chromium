// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/payments/payments_sync_bridge_test_util.h"

#include "base/strings/string_number_conversions.h"

namespace autofill {

CreditCard CreateServerCreditCard(const std::string& server_id) {
  // TODO(sebsg): Set data.
  return CreditCard(CreditCard::RecordType::kMaskedServerCard, server_id);
}

Iban CreateServerIban(Iban::InstrumentId instrument_id) {
  Iban iban(instrument_id);
  iban.set_prefix(u"BE71");
  iban.set_suffix(u"8676");
  iban.set_nickname(u"My sister's IBAN");
  return iban;
}

sync_pb::AutofillWalletSpecifics CreateAutofillWalletSpecificsForCard(
    const std::string& client_tag,
    const std::string& billing_address_id,
    const std::string& nickname) {
  sync_pb::AutofillWalletSpecifics wallet_specifics;
  wallet_specifics.set_type(
      sync_pb::AutofillWalletSpecifics_WalletInfoType::
          AutofillWalletSpecifics_WalletInfoType_MASKED_CREDIT_CARD);

  sync_pb::WalletMaskedCreditCard* card_specifics =
      wallet_specifics.mutable_masked_card();
  card_specifics->set_id(client_tag);
  card_specifics->mutable_card_issuer()->set_issuer(
      sync_pb::CardIssuer::EXTERNAL_ISSUER);
  card_specifics->mutable_card_issuer()->set_issuer_id("capitalone");
  card_specifics->set_billing_address_id(billing_address_id);
  if (!nickname.empty())
    card_specifics->set_nickname(nickname);
  return wallet_specifics;
}

sync_pb::AutofillWalletSpecifics
CreateAutofillWalletSpecificsForPaymentsCustomerData(
    const std::string& client_tag) {
  sync_pb::AutofillWalletSpecifics wallet_specifics;
  wallet_specifics.set_type(
      sync_pb::AutofillWalletSpecifics_WalletInfoType::
          AutofillWalletSpecifics_WalletInfoType_CUSTOMER_DATA);

  sync_pb::PaymentsCustomerData* customer_data_specifics =
      wallet_specifics.mutable_customer_data();
  customer_data_specifics->set_id(client_tag);
  return wallet_specifics;
}

sync_pb::AutofillWalletSpecifics
CreateAutofillWalletSpecificsForCreditCardCloudTokenData(
    const std::string& client_tag) {
  sync_pb::AutofillWalletSpecifics wallet_specifics;
  wallet_specifics.set_type(
      sync_pb::AutofillWalletSpecifics_WalletInfoType::
          AutofillWalletSpecifics_WalletInfoType_CREDIT_CARD_CLOUD_TOKEN_DATA);

  sync_pb::WalletCreditCardCloudTokenData* cloud_token_data_specifics =
      wallet_specifics.mutable_cloud_token_data();
  cloud_token_data_specifics->set_instrument_token(client_tag);
  return wallet_specifics;
}

sync_pb::AutofillWalletSpecifics CreateAutofillWalletSpecificsForIban(
    const std::string& client_tag) {
  sync_pb::AutofillWalletSpecifics wallet_specifics;
  wallet_specifics.set_type(
      sync_pb::AutofillWalletSpecifics_WalletInfoType::
          AutofillWalletSpecifics_WalletInfoType_PAYMENT_INSTRUMENT);

  sync_pb::PaymentInstrument* payment_instrument_specifics =
      wallet_specifics.mutable_payment_instrument();
  int64_t instrument_id;
  CHECK(base::StringToInt64(client_tag, &instrument_id));
  payment_instrument_specifics->set_instrument_id(instrument_id);
  payment_instrument_specifics->set_nickname("My IBAN");
  sync_pb::WalletMaskedIban* masked_iban =
      payment_instrument_specifics->mutable_iban();
  masked_iban->set_prefix("FR76");
  masked_iban->set_suffix("0189");
  masked_iban->set_length(27);
  return wallet_specifics;
}

sync_pb::AutofillWalletSpecifics CreateAutofillWalletSpecificsForBankAccount(
    std::string_view client_tag,
    std::string nickname,
    const GURL& display_icon_url,
    std::string bank_name,
    std::string account_number_suffix,
    sync_pb::BankAccountDetails::AccountType account_type) {
  sync_pb::AutofillWalletSpecifics wallet_specifics;
  wallet_specifics.set_type(
      sync_pb::AutofillWalletSpecifics_WalletInfoType::
          AutofillWalletSpecifics_WalletInfoType_PAYMENT_INSTRUMENT);

  sync_pb::PaymentInstrument* payment_instrument_specifics =
      wallet_specifics.mutable_payment_instrument();

  int64_t instrument_id;
  // The client tag is expected to of the format:
  // 'payment_instrument:<instrument_id>', so we first find the index of ':' and
  // then convert the succeeding characters to a number.
  size_t index = client_tag.find(":");
  if (index != std::string::npos &&
      base::StringToInt64(client_tag.substr(index + 1), &instrument_id)) {
    payment_instrument_specifics->set_instrument_id(instrument_id);
  }
  payment_instrument_specifics->set_nickname(nickname);
  payment_instrument_specifics->set_display_icon_url(display_icon_url.spec());
  payment_instrument_specifics->add_supported_rails(
      sync_pb::PaymentInstrument_SupportedRail::
          PaymentInstrument_SupportedRail_PIX);
  sync_pb::BankAccountDetails* bank_account_details =
      payment_instrument_specifics->mutable_bank_account();
  bank_account_details->set_bank_name(bank_name);
  bank_account_details->set_account_number_suffix(account_number_suffix);
  bank_account_details->set_account_type(account_type);

  return wallet_specifics;
}

sync_pb::AutofillWalletSpecifics CreateAutofillWalletSpecificsForEwalletAccount(
    std::string_view client_tag,
    std::string nickname,
    const GURL& display_icon_url,
    std::string ewallet_name,
    std::string account_display_name,
    bool is_fido_enrolled) {
  sync_pb::AutofillWalletSpecifics wallet_specifics;
  wallet_specifics.set_type(
      sync_pb::AutofillWalletSpecifics_WalletInfoType::
          AutofillWalletSpecifics_WalletInfoType_PAYMENT_INSTRUMENT);

  sync_pb::PaymentInstrument* payment_instrument_specifics =
      wallet_specifics.mutable_payment_instrument();

  int64_t instrument_id;
  // The client tag is expected to of the format:
  // 'payment_instrument:<instrument_id>', so we first find the index of ':' and
  // then convert the succeeding characters to a number.
  size_t index = client_tag.find(":");
  if (index != std::string::npos &&
      base::StringToInt64(client_tag.substr(index + 1), &instrument_id)) {
    payment_instrument_specifics->set_instrument_id(instrument_id);
  }
  payment_instrument_specifics->set_nickname(nickname);
  payment_instrument_specifics->set_display_icon_url(display_icon_url.spec());
  payment_instrument_specifics->add_supported_rails(
      sync_pb::PaymentInstrument_SupportedRail::
          PaymentInstrument_SupportedRail_PAYMENT_HYPERLINK);
  sync_pb::EwalletDetails* ewallet_details =
      payment_instrument_specifics->mutable_ewallet_details();
  ewallet_details->set_ewallet_name(ewallet_name);
  ewallet_details->set_account_display_name(account_display_name);
  ewallet_details->add_supported_payment_link_uris("fake_payment_link_regex");
  sync_pb::DeviceDetails* device_details =
      payment_instrument_specifics->mutable_device_details();
  device_details->set_is_fido_enrolled(is_fido_enrolled);
  return wallet_specifics;
}

}  // namespace autofill
