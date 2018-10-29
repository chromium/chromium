// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_SYNC_BRIDGE_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_SYNC_BRIDGE_UTIL_H_

#include <memory>
#include <string>

#include "components/sync/model/entity_change.h"
#include "components/sync/model/entity_data.h"

namespace autofill {

class AutofillProfile;
class AutofillTable;
class CreditCard;
struct PaymentsCustomerData;

// Returns the specified |server_id| encoded in base 64.
std::string GetBase64EncodedServerId(const std::string& server_id);

// Returns the wallet data specifics id for the specified |server_id|.
std::string GetSpecificsIdForEntryServerId(const std::string& server_id);

// Returns the wallet metadata specifics id for the specified |metadata_id|.
std::string GetSpecificsIdForMetadataId(const std::string& metadata_id);

// Returns the storage key for the specified |specifics_id|.
std::string GetStorageKeyForSpecificsId(const std::string& specifics_id);

// Returns the wallet data specifics storage key for the specified
// |server_id|.
std::string GetStorageKeyForEntryServerId(const std::string& server_id);

// Returns the wallet metadata specifics storage key for the specified
// |metadata_id|.
std::string GetStorageKeyForMetadataId(const std::string& metadata_id);

// Returns the client tag for the specified wallet |type| and
// |wallet_data_specifics_id|.
std::string GetClientTagForSpecificsId(
    sync_pb::AutofillWalletSpecifics::WalletInfoType type,
    const std::string& wallet_data_specifics_id);

// Sets the fields of the |wallet_specifics| based on the the specified
// |address|.
void SetAutofillWalletSpecificsFromServerProfile(
    const AutofillProfile& address,
    sync_pb::AutofillWalletSpecifics* wallet_specifics);

// Creates a EntityData object corresponding to the specified |address|.
std::unique_ptr<syncer::EntityData> CreateEntityDataFromAutofillServerProfile(
    const AutofillProfile& address);

// Creates an AutofillProfile from the specified |address| specifics.
AutofillProfile ProfileFromSpecifics(
    const sync_pb::WalletPostalAddress& address);

// Sets the fields of the |wallet_specifics| based on the the specified |card|.
void SetAutofillWalletSpecificsFromServerCard(
    const CreditCard& card,
    sync_pb::AutofillWalletSpecifics* wallet_specifics);

// Creates a EntityData object corresponding to the specified |card|.
std::unique_ptr<syncer::EntityData> CreateEntityDataFromCard(
    const CreditCard& card);

// Creates an AutofillProfile from the specified |card| specifics.
CreditCard CardFromSpecifics(const sync_pb::WalletMaskedCreditCard& card);

// Creates a EntityData object corresponding to the specified |customer_data|.
std::unique_ptr<syncer::EntityData> CreateEntityDataFromPaymentsCustomerData(
    const PaymentsCustomerData& customer_data);

// Sets the fields of the |wallet_specifics| based on the specified
// |customer_data|.
void SetAutofillWalletSpecificsFromPaymentsCustomerData(
    const PaymentsCustomerData& customer_data,
    sync_pb::AutofillWalletSpecifics* wallet_specifics);

// Creates a PaymentCustomerData object corresponding to the sync datatype
// |customer_data|.
PaymentsCustomerData CustomerDataFromSpecifics(
    const sync_pb::PaymentsCustomerData& customer_data);

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
    std::vector<PaymentsCustomerData>* customer_data);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_SYNC_BRIDGE_UTIL_H_
