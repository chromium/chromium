// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SUGGESTION_GENERATOR_H_

#include <array>
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
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/suggestions/payments/save_and_fill_suggestion.h"
#include "components/autofill/core/browser/suggestions/payments/virtual_card_suggestion_data.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_entry.h"

namespace autofill {

// SuggestionGenerator is an interface that is used to generate suggestions for
// a specific `FillingProduct`. Each `FillingProduct` has their own
// implementation of the `SuggestionGenerator` interface.
//
// It can and should be assumed that suggestion generation for each filling
// product runs independently from each other, which allows easy handling of
// asynchronous generation.
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
    kCompose,
    kOneTimePassword,
    kAddressOnTyping,
    kMaxValue = kAddressOnTyping
  };

  // Priority order for cases, when we have suggestion data from multiple
  // SuggestionGenerators.
  // clang-format off
  constexpr static auto kOrderedPrioritizedSources =
      std::to_array<SuggestionDataSource>({
          SuggestionDataSource::kAutofillAi,
          SuggestionDataSource::kCreditCard,
          SuggestionDataSource::kVirtualStandaloneCvc,
          SuggestionDataSource::kSaveAndFillPromo,
          SuggestionDataSource::kOneTimePassword,
          SuggestionDataSource::kAddress,
          SuggestionDataSource::kIdentityCredential,
          SuggestionDataSource::kLoyaltyCard,
          SuggestionDataSource::kCompose,
          SuggestionDataSource::kMerchantPromoCode,
          SuggestionDataSource::kIban,
          SuggestionDataSource::kAutocomplete,
          SuggestionDataSource::kAddressOnTyping,
          SuggestionDataSource::kPasskey,
      });
  // clang-format on

  static_assert(kOrderedPrioritizedSources.size() ==
                static_cast<size_t>(SuggestionDataSource::kMaxValue) + 1);

  // Defines which lower-priority sources can be merged into a higher-priority
  // source (see `kOrderedPrioritizedSources` for more info about priorities).
  static constexpr auto kSupportedMerges = base::MakeFixedFlatMap<
      SuggestionDataSource,
      DenseSet<SuggestionDataSource>>(
      {{SuggestionDataSource::kAutofillAi, {SuggestionDataSource::kPasskey}},
       {SuggestionDataSource::kCreditCard, {SuggestionDataSource::kPasskey}},
       {SuggestionDataSource::kVirtualStandaloneCvc,
        {SuggestionDataSource::kPasskey}},
       {SuggestionDataSource::kSaveAndFillPromo,
        {SuggestionDataSource::kPasskey}},
       {SuggestionDataSource::kOneTimePassword,
        {SuggestionDataSource::kPasskey}},
       // Address can merge with:
       // - Plus Addresses: To replace GAIA emails or show inline.
       // - Verified Identity: To show verified details alongside profiles.
       // - Loyalty Cards: To extend email suggestions.
       // - Passkeys: Every SuggestionDataSource can be merged with passkeys.
       {SuggestionDataSource::kAddress,
        {SuggestionDataSource::kIdentityCredential,
         SuggestionDataSource::kLoyaltyCard, SuggestionDataSource::kPasskey}},
       {SuggestionDataSource::kIdentityCredential,
        {SuggestionDataSource::kPasskey}},
       {SuggestionDataSource::kLoyaltyCard, {SuggestionDataSource::kPasskey}},
       {SuggestionDataSource::kCompose, {SuggestionDataSource::kPasskey}},
       {SuggestionDataSource::kMerchantPromoCode,
        {SuggestionDataSource::kPasskey}},
       {SuggestionDataSource::kIban, {SuggestionDataSource::kPasskey}},
       {SuggestionDataSource::kAutocomplete, {SuggestionDataSource::kPasskey}},
       {SuggestionDataSource::kAddressOnTyping,
        {SuggestionDataSource::kPasskey}},
       {SuggestionDataSource::kPasskey, {}}});

  static_assert(kSupportedMerges.size() ==
                static_cast<size_t>(SuggestionDataSource::kMaxValue) + 1);

  SuggestionGenerator() = default;
  virtual ~SuggestionGenerator() = default;

  using ReturnedSuggestions =
      std::pair<SuggestionDataSource, std::vector<Suggestion>>;

  // Obtains data that will be used to generate suggestions on a given
  // `trigger_field` that belongs to `form`. Then generates `Suggestion` objects
  // from the fetched data.
  //
  // Once the suggestions are obtained, `callback` is called with the
  // generated suggestions. `form_structure` and `trigger_autofill_field` may be
  // null if the `form` or `trigger_field` wasn't yet parsed.
  //
  // Certain `FillingProduct`s can have different suggestions depending on the
  // `trigger_field` and `form`.  In order to support those cases, generated
  // suggestions are tracked by the `SuggestionDataSource`, to ensure that
  // correct prioritization is later used. Note that each `FillingProduct` can
  // generate only one type of `SuggestionDataSource`.
  virtual void GenerateSuggestions(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      base::OnceCallback<void(ReturnedSuggestions)> callback) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SUGGESTION_GENERATOR_H_
