// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_webdata_backend_util.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_entry.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend_impl.h"
#include "components/autofill/core/common/autofill_clock.h"

namespace autofill {

namespace util {

namespace {

// The length of a local profile GUID.
const int LOCAL_GUID_LENGTH = 36;

// TODO(crbug.com/687975): Reuse MergeProfile in this function.
std::string MergeServerAddressesIntoProfilesAndUpdateDb(
    const AutofillProfile& server_address,
    std::vector<std::unique_ptr<AutofillProfile>>* existing_profiles,
    const std::string& app_locale,
    const std::string& primary_account_email,
    AutofillWebDataBackendImpl* backend,
    WebDatabase* db) {
  // If there is already a local profile that is very similar, merge in any
  // missing values. Only merge with the first match.
  AutofillProfileComparator comparator(app_locale);
  for (auto& local_profile : *existing_profiles) {
    if (comparator.AreMergeable(server_address, *local_profile) &&
        local_profile->SaveAdditionalInfo(server_address, app_locale)) {
      local_profile->set_modification_date(AutofillClock::Now());
      backend->UpdateAutofillProfile(*local_profile, db);
      AutofillMetrics::LogWalletAddressConversionType(
          AutofillMetrics::CONVERTED_ADDRESS_MERGED);
      return local_profile->guid();
    }
  }

  // If the server address was not merged with a local profile, add it to the
  // list.
  existing_profiles->push_back(
      std::make_unique<AutofillProfile>(server_address));
  // Set the profile as being local.
  existing_profiles->back()->set_record_type(AutofillProfile::LOCAL_PROFILE);
  existing_profiles->back()->set_modification_date(AutofillClock::Now());

  // Wallet addresses don't have an email address, use the one from the
  // currently signed-in account.
  base::string16 email = base::UTF8ToUTF16(primary_account_email);
  if (!email.empty())
    existing_profiles->back()->SetRawInfo(EMAIL_ADDRESS, email);

  backend->AddAutofillProfile(*existing_profiles->back(), db);
  AutofillMetrics::LogWalletAddressConversionType(
      AutofillMetrics::CONVERTED_ADDRESS_ADDED);

  return existing_profiles->back()->guid();
}

bool ConvertWalletAddressesToLocalProfiles(
    const std::string& app_locale,
    const std::string& primary_account_email,
    const std::vector<std::unique_ptr<AutofillProfile>>& server_profiles,
    std::vector<std::unique_ptr<AutofillProfile>>* local_profiles,
    std::unordered_map<std::string, AutofillProfile*>* server_id_profiles_map,
    std::unordered_map<std::string, std::string>* guids_merge_map,
    AutofillWebDataBackendImpl* backend,
    WebDatabase* db) {
  bool has_converted_addresses = false;
  for (const std::unique_ptr<AutofillProfile>& wallet_address :
       server_profiles) {
    // Add the profile to the map.
    server_id_profiles_map->emplace(wallet_address->server_id(),
                                    wallet_address.get());

    // If the address has not been converted yet, convert it.
    if (!wallet_address->has_converted()) {
      // Try to merge the server address into a similar local profile, or create
      // a new local profile if no similar profile is found.
      std::string address_guid = MergeServerAddressesIntoProfilesAndUpdateDb(
          *wallet_address, local_profiles, app_locale, primary_account_email,
          backend, db);

      // Update the map to transfer the billing address relationship from the
      // server address to the converted/merged local profile.
      guids_merge_map->emplace(wallet_address->server_id(), address_guid);

      // Update the wallet addresses metadata to record the conversion.
      AutofillProfile updated_address = *wallet_address;
      updated_address.set_has_converted(true);
      backend->UpdateServerAddressMetadata(updated_address, db);

      has_converted_addresses = true;
    }
  }

  return has_converted_addresses;
}

bool UpdateWalletCardsAlreadyConvertedBillingAddresses(
    const std::vector<std::unique_ptr<AutofillProfile>>& local_profiles,
    const std::vector<std::unique_ptr<CreditCard>>& server_cards,
    const std::unordered_map<std::string, AutofillProfile*>&
        server_id_profiles_map,
    const std::string& app_locale,
    std::unordered_map<std::string, std::string>* guids_merge_map) {
  // Look for server cards that still refer to server addresses but for which
  // there is no mapping. This can happen if it's a new card for which the
  // billing address has already been converted. This should be a no-op for most
  // situations. Otherwise, it should affect only one Wallet card, sinces users
  // do not add a lot of credit cards.
  AutofillProfileComparator comparator(app_locale);
  bool should_update_cards = false;
  for (const std::unique_ptr<CreditCard>& wallet_card : server_cards) {
    std::string billing_address_id = wallet_card->billing_address_id();

    // If billing address refers to a server id and that id is not a key in the
    // |guids_merge_map|, it means that the card is new but the address was
    // already converted. Look for the matching converted profile.
    if (!billing_address_id.empty() &&
        billing_address_id.length() != LOCAL_GUID_LENGTH &&
        guids_merge_map->find(billing_address_id) == guids_merge_map->end()) {
      // Get the profile.
      auto it = server_id_profiles_map.find(billing_address_id);
      if (it != server_id_profiles_map.end()) {
        const AutofillProfile* billing_address = it->second;

        // Look for a matching local profile (DO NOT MERGE).
        for (const auto& local_profile : local_profiles) {
          if (comparator.AreMergeable(*billing_address, *local_profile)) {
            // The Wallet address matches this local profile. Add this to the
            // merge mapping.
            guids_merge_map->emplace(billing_address_id, local_profile->guid());
            should_update_cards = true;
            break;
          }
        }
      }
    }
  }

  return should_update_cards;
}

// TODO(crbug.com/911133): This function implements the same logic as the
// function with the same name in PDM. Move ApplyDedupingRoutine to the backend
// thread as well and thus get rid of the code duplicity.
void UpdateCardsBillingAddressReference(
    const std::unordered_map<std::string, std::string>& guids_merge_map,
    std::vector<std::unique_ptr<CreditCard>> credit_cards,
    AutofillWebDataBackendImpl* backend,
    WebDatabase* db) {
  /*  Here is an example of what the graph might look like.

      A -> B
             \
               -> E
             /
      C -> D
  */

  for (std::unique_ptr<CreditCard>& credit_card : credit_cards) {
    // If the credit card is not associated with a billing address, skip it.
    if (credit_card->billing_address_id().empty())
      break;

    // If the billing address profile associated with the card has been merged,
    // replace it by the id of the profile in which it was merged. Repeat the
    // process until the billing address has not been merged into another one.
    bool was_modified = false;
    auto it = guids_merge_map.find(credit_card->billing_address_id());
    while (it != guids_merge_map.end()) {
      was_modified = true;
      credit_card->set_billing_address_id(it->second);
      it = guids_merge_map.find(credit_card->billing_address_id());
    }

    if (was_modified) {
      if (credit_card->record_type() == CreditCard::LOCAL_CARD) {
        backend->UpdateCreditCard(*credit_card, db);
      } else {
        backend->UpdateServerCardMetadata(*credit_card, db);
      }
    }
  }
}

}  // namespace

WebDatabase::State ConvertWalletAddressesAndUpdateWalletCards(
    const std::string& app_locale,
    const std::string& primary_account_email,
    AutofillWebDataBackendImpl* backend,
    WebDatabase* db) {
  AutofillTable* table = AutofillTable::FromWebDatabase(db);

  // Get a copy of local profiles.
  std::vector<std::unique_ptr<AutofillProfile>> local_profiles;
  std::vector<std::unique_ptr<AutofillProfile>> server_profiles;
  std::vector<std::unique_ptr<CreditCard>> server_cards;
  if (!table->GetAutofillProfiles(&local_profiles) ||
      !table->GetServerProfiles(&server_profiles) ||
      !table->GetServerCreditCards(&server_cards)) {
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  // Since we are already iterating on all the server profiles to convert Wallet
  // addresses and we will need to access them by guid later to update the
  // Wallet cards, create a map here.
  std::unordered_map<std::string, AutofillProfile*> server_id_profiles_map;

  // Create the map used to update credit card's billing addresses after the
  // conversion/merge.
  std::unordered_map<std::string, std::string> guids_merge_map;

  bool has_converted_addresses = ConvertWalletAddressesToLocalProfiles(
      app_locale, primary_account_email, server_profiles, &local_profiles,
      &server_id_profiles_map, &guids_merge_map, backend, db);
  bool should_update_cards = UpdateWalletCardsAlreadyConvertedBillingAddresses(
      local_profiles, server_cards, server_id_profiles_map, app_locale,
      &guids_merge_map);

  if (should_update_cards || has_converted_addresses) {
    std::vector<std::unique_ptr<CreditCard>> all_cards;
    if (!table->GetCreditCards(&all_cards)) {
      return WebDatabase::COMMIT_NEEDED;
    }
    for (std::unique_ptr<CreditCard>& server_card : server_cards) {
      all_cards.push_back(std::move(server_card));
    }

    // Update the credit cards billing address relationship.
    UpdateCardsBillingAddressReference(guids_merge_map, std::move(all_cards),
                                       backend, db);
    // Notify the PDM about the conversion being completed.
    backend->NotifyOfAddressConversionCompleted();
    return WebDatabase::COMMIT_NEEDED;
  }

  // We need to notify the PDM even if we do not change any data (it relies on
  // it to refresh its local view).
  backend->NotifyOfAddressConversionCompleted();
  return WebDatabase::COMMIT_NOT_NEEDED;
}

}  // namespace util
}  // namespace autofill
