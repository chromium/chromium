// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_SYNC_BRIDGE_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_SYNC_BRIDGE_UTIL_H_

#include <memory>
#include <string>

#include "components/sync/model/entity_change.h"
#include "components/sync/protocol/autofill_wallet_credential_specifics.pb.h"

namespace autofill {

class AutofillOfferData;
struct ServerCvc;
class AutofillWalletUsageData;
class AutofillProfile;
class AutofillTable;
class CreditCard;
struct CreditCardCloudTokenData;
struct PaymentsCustomerData;
class VirtualCardUsageData;

// Returns the specified |id| encoded in / decoded from base 64.
std::string GetBase64EncodedId(const std::string& id);
std::string GetBase64DecodedId(const std::string& id);

// Returns the storage key to be used for wallet metadata for the specified
// wallet metadata |type| and |specifics_id|.
std::string GetStorageKeyForWalletMetadataTypeAndSpecificsId(
    sync_pb::WalletMetadataSpecifics::Type type,
    const std::string& specifics_id);

// Sets the fields of the |wallet_specifics| based on the the specified
// |address|. If |enforce_utf8|, ids are encoded into UTF-8.
void SetAutofillWalletSpecificsFromServerProfile(
    const AutofillProfile& address,
    sync_pb::AutofillWalletSpecifics* wallet_specifics,
    bool enforce_utf8 = false);

// Sets the fields of the |wallet_specifics| based on the the specified |card|.
// If |enforce_utf8|, ids are encoded into UTF-8.
void SetAutofillWalletSpecificsFromServerCard(
    const CreditCard& card,
    sync_pb::AutofillWalletSpecifics* wallet_specifics,
    bool enforce_utf8 = false);

// Sets the fields of the |wallet_specifics| based on the specified
// |customer_data|.
void SetAutofillWalletSpecificsFromPaymentsCustomerData(
    const PaymentsCustomerData& customer_data,
    sync_pb::AutofillWalletSpecifics* wallet_specifics);

// Sets the field of the |wallet_specifics| based on the specified
// |cloud_token_data|. If |enforce_utf8|, ids are encoded into UTF-8.
void SetAutofillWalletSpecificsFromCreditCardCloudTokenData(
    const CreditCardCloudTokenData& cloud_token_data,
    sync_pb::AutofillWalletSpecifics* wallet_specifics,
    bool enforce_utf8 = false);

// Sets the field of the `wallet_usage_specifics` based on the specified
// `wallet_usage_data`.
void SetAutofillWalletUsageSpecificsFromAutofillWalletUsageData(
    const AutofillWalletUsageData& wallet_usage_data,
    sync_pb::AutofillWalletUsageSpecifics* wallet_usage_specifics);

// Sets the fields of the |offer_specifics| based on the specified |offer_data|.
void SetAutofillOfferSpecificsFromOfferData(
    const AutofillOfferData& offer_data,
    sync_pb::AutofillOfferSpecifics* offer_specifics);

// Creates an AutofillOfferData from the specified |offer_specifics|.
// |offer_specifics| must be valid (as per IsOfferSpecificsValid()).
AutofillOfferData AutofillOfferDataFromOfferSpecifics(
    const sync_pb::AutofillOfferSpecifics& offer_specifics);

// Returns a AutofillWalletCredentialSpecifics object based on the specified
// `server_cvc` data. The CVC must be present in the `server_cvc`.
sync_pb::AutofillWalletCredentialSpecifics
AutofillWalletCredentialSpecificsFromStructData(const ServerCvc& server_cvc);

// Returns a ServerCvc struct data based on the specified
// `wallet_credential_specifics` data.
// The passed-in specifics must be valid (as per
// IsAutofillWalletCredentialDataSpecificsValid).
ServerCvc AutofillWalletCvcStructDataFromWalletCredentialSpecifics(
    const sync_pb::AutofillWalletCredentialSpecifics&
        wallet_credential_specifics);

// Creates a VirtualCardUsageData from the specified |usage_specifics|.
// |usage_specifics| must be valid (as per
// IsVirtualCardUsageDataSpecificsValid()).
VirtualCardUsageData VirtualCardUsageDataFromUsageSpecifics(
    const sync_pb::AutofillWalletUsageSpecifics& usage_specifics);

// Creates an AutofillProfile from the specified |address| specifics.
AutofillProfile ProfileFromSpecifics(
    const sync_pb::WalletPostalAddress& address);

// TODO(sebsg): This should probably copy the converted state for the address
// too.
// Copies the metadata from the local cards (if present) to the corresponding
// server cards so that they don't get overwritten. This is because the wallet
// data does not include those. They are handled by the
// AutofillWalletMetadataSyncBridge.
void CopyRelevantWalletMetadataFromDisk(
    const AutofillTable& table,
    std::vector<CreditCard>* cards_from_server);

// Populates the wallet datatypes from the sync data and uses the sync data to
// link the card to its billing address. If |wallet_addresses| is a nullptr,
// this function will not extract addresses.
void PopulateWalletTypesFromSyncData(
    const ::syncer::EntityChangeList& entity_data,
    std::vector<CreditCard>* wallet_cards,
    std::vector<AutofillProfile>* wallet_addresses,
    std::vector<PaymentsCustomerData>* customer_data,
    std::vector<CreditCardCloudTokenData>* cloud_token_data);

// A helper function to compare two sets of data. Returns true if there is
// any difference. It uses the Compare() of the Item class instead of comparison
// operators and does not care about the order of items in the dataset.
template <class Item>
bool AreAnyItemsDifferent(const std::vector<std::unique_ptr<Item>>& old_data,
                          const std::vector<Item>& new_data);

// Returns whether the Virtual Card Usage Data |specifics| is valid data.
bool IsVirtualCardUsageDataSpecificsValid(
    const sync_pb::AutofillWalletUsageSpecifics::VirtualCardUsageData&
        specifics);

// Returns whether the Wallet Offer |specifics| is valid data.
bool IsOfferSpecificsValid(const sync_pb::AutofillOfferSpecifics specifics);

// Returns whether the fields of VirtualCardUsageData `virtual_card_usage_data`
// were initialized and set.
bool IsVirtualCardUsageDataSet(
    const VirtualCardUsageData& virtual_card_usage_data);

// Returns true if the `wallet_credential_specifics` is valid, otherwise false.
bool IsAutofillWalletCredentialDataSpecificsValid(
    const sync_pb::AutofillWalletCredentialSpecifics&
        wallet_credential_specifics);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_SYNC_BRIDGE_UTIL_H_
