// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SUGGESTION_GENERATOR_H_

#include <variant>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/identity_credential/identity_credential.h"
#include "components/autofill/core/browser/data_model/payments/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/suggestions/addresses/address_on_typing_suggestion_data.h"
#include "components/autofill/core/browser/suggestions/compose/compose_availability.h"
#include "components/autofill/core/browser/suggestions/one_time_passwords/one_time_password_suggestion_data.h"
#include "components/autofill/core/browser/suggestions/passkeys/hybrid_passkey_availability.h"
#include "components/autofill/core/browser/suggestions/payments/save_and_fill_suggestion.h"
#include "components/autofill/core/browser/suggestions/payments/virtual_card_suggestion_data.h"
#include "components/autofill/core/browser/suggestions/plus_addresses/plus_address.h"
#include "components/autofill/core/browser/suggestions/plus_addresses/plus_address_for_address_suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_entry.h"

namespace autofill {

// SuggestionGenerator is an interface that is used to generate suggestions for
// a specific `FillingProduct`. Each `FillingProduct` has their own
// implementation of the `SuggestionGenerator` interface.
//
// Generating suggestions consists of two phases:
// 1. All generators are called to fetch the data that is going to be used for
//    creating the suggestions. No assumptions should be made about the order
//    of those calls and some of those calls will be asynchronous.
// 2. Every generator is called again with the data that was fetched for all
//    `FillingProduct`s in step 1, and uses that to generate the suggestions
//    for its specific `FillingProduct`.
//
// Note: Some product suggestions depend on the data from other products.
// E.g. PlusAddresses and Address suggestions needs the data from both
// products, before being able to generate suggestions.
//
// Because of the dependency for each suggestion generator to know about the
// data from other FillingProducts, the generation is split into two phases. The
// second phase is only executed if the first phase finished for all
// `FillingProduct`s. Within each phase there is no guarantee about the order in
// which the suggestion generators are called.
class SuggestionGenerator {
 public:
  // Used to precisely describe what kind of data was fetched to produce a
  // suggestion. One `FillingProduct` may be able to generate different kinds of
  // `SuggestionDataSource`, but cannot fetch data for more than one.
  enum class SuggestionDataSource {
    kAutofillAi,
    kAddress,
    kCreditCard,
    kVirtualStandaloneCvc,
    kSaveAndFillPromo,
    kIban,
    kMerchantPromoCode,
    kAutocomplete,
    kLoyaltyCard,
    kIdentityCredential,
    kPasskey,
    kPlusAddress,
    kPlusAddressForAddress,
    kCompose,
    kOneTimePassword,
    kAddressOnTyping,
    kMaxValue = kAddressOnTyping
  };

  SuggestionGenerator() = default;
  virtual ~SuggestionGenerator() = default;

  using ReturnedSuggestions =
      std::pair<FillingProduct, std::vector<Suggestion>>;
  // Contains the structures used in order to generate various kind of
  // suggestions.
  using SuggestionData = std::variant<EntityInstance,
                                      AutofillProfile,
                                      CreditCard,
                                      Iban,
                                      AutofillOfferData,
                                      AutocompleteEntry,
                                      LoyaltyCard,
                                      IdentityCredential,
                                      HybridPasskeyAvailability,
                                      SaveAndFillSuggestion,
                                      VirtualCardSuggestionData,
                                      OneTimePasswordSuggestionData,
                                      PlusAddress,
                                      PlusAddressForAddressSuggestion,
                                      AddressOnTypingSuggestionData,
                                      ComposeAvailability>;

  // Obtains data that will be used to generate suggestions on a given
  // `trigger_field` that belongs to `form`.
  //
  // Once the data is obtained, `callback` is called with the
  // `SuggestionDataSource` which the data is for and the corresponding list of
  // `SuggestionData`. `form_structure` and `trigger_autofill_field` may be null
  // if the `form` or `trigger_field` wasn't yet parsed.
  //
  // Certain `FillingProduct`s can have different suggestions depending on the
  // `trigger_field` and `form`.  In order to support those cases, fetched data
  // is tracked by the `SuggestionDataSource`, to ensure that correct generation
  // logic is later used.
  // Note that each `FillingProduct` can fetch only one type of
  // `SuggestionDataSource` per `FetchSuggestionData` call.
  //
  // All filtering of suggestion data should happen in this function (relevance
  // filtering, prefix matching, deduplication, ordering, etc.), as the next
  // phase in suggestion generation (see top-level ddocumentation for more
  // details) must generate a Suggestion object for each of the passed
  // `SuggestionData` and in the given order.
  // TODO(crbug.com/409962888): Take `SuggestionContext` here.
  virtual void FetchSuggestionData(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      base::OnceCallback<
          void(std::pair<SuggestionDataSource,
                         std::vector<SuggestionGenerator::SuggestionData>>)>
          callback) = 0;

  // Generates suggestions given `all_suggestion_data` that were fetched by
  // calling `FetchSuggestionData` on all generators (see top-level
  // documentation of `SuggestionGenerator` for more details).
  //
  // Suggestions were triggered on `trigger_field` which belongs to `form`.
  // `callback` is called when generation is complete and a list of `Suggestion`
  // objects is passed along with the corresponding `FillingProduct`.
  // `form_structure` and `trigger_autofill_field` may be null if the `form` or
  // the `trigger_field` weren't parsed yet.
  //
  // We assume no further filtering/mutation of `all_suggestion_data` beyond
  // this point. That is, the function must generate a `Suggestion` object for
  // each of the `SuggestionData` of its corresponding `FillingProduct` and in
  // the given order.
  virtual void GenerateSuggestions(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
          all_suggestion_data,
      base::OnceCallback<void(ReturnedSuggestions)> callback) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SUGGESTION_GENERATOR_H_
