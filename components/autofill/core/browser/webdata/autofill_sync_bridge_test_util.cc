// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_sync_bridge_test_util.h"

#include "components/sync/protocol/autofill_specifics.pb.h"

namespace autofill {

CreditCard CreateServerCreditCard(const std::string& server_id) {
  // TODO(sebsg): Set data.
  return CreditCard(CreditCard::RecordType::kMaskedServerCard, server_id);
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
          AutofillWalletSpecifics_WalletInfoType_MASKED_IBAN);

  sync_pb::WalletMaskedIban* iban_specifics =
      wallet_specifics.mutable_masked_iban();
  iban_specifics->set_instrument_id(client_tag);
  iban_specifics->set_prefix("FR76");
  iban_specifics->set_suffix("0189");
  iban_specifics->set_length(27);
  iban_specifics->set_nickname("My IBAN");
  return wallet_specifics;
}

}  // namespace autofill
