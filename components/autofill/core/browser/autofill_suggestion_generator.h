// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SUGGESTION_GENERATOR_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/aliases.h"

namespace base {
class Time;
}  // namespace base

namespace autofill {

namespace autofill_metrics {
struct CardMetadataLoggingContext;
}

class AutofillClient;
class AutofillOfferData;
class CreditCard;
struct FormFieldData;
class Iban;
class PersonalDataManager;

// Helper class to generate Autofill suggestions, such as for credit card and
// address profile Autofill.
class AutofillSuggestionGenerator {
 public:
  // As of November 2018, displaying 10 suggestions cover at least 99% of the
  // indices clicked by our users. The suggestions will also refine as they
  // type.
  static constexpr size_t kMaxUniqueSuggestedProfilesCount = 10;

  // As of November 2018, 50 profiles should be more than enough to cover at
  // least 99% of all times the dropdown is shown.
  static constexpr size_t kMaxSuggestedProfilesCount = 50;

  AutofillSuggestionGenerator(AutofillClient& autofill_client,
                              PersonalDataManager& personal_data);
  ~AutofillSuggestionGenerator();
  AutofillSuggestionGenerator(const AutofillSuggestionGenerator&) = delete;
  AutofillSuggestionGenerator& operator=(const AutofillSuggestionGenerator&) =
      delete;

  // Generates suggestions for a form containing the given `field_types`. It
  // considers all available profiles, deduplicates them based on the types and
  // returns one suggestion per remaining profile.
  // `last_targeted_fields` is used to know which fields were targeted on a
  // prior form interaction. In the context of granular filling, this could lead
  // the user to be in one of the available filling granularities, field by
  // field filling, group filling or full form (default). `field_types` are the
  // relevant types for the current suggestions.
  std::vector<Suggestion> GetSuggestionsForProfiles(
      const FieldTypeSet& field_types,
      const FormFieldData& trigger_field,
      FieldType trigger_field_type,
      std::optional<FieldTypeSet> last_targeted_fields,
      AutofillSuggestionTriggerSource trigger_source);

  // Returns a list of profiles that will be displayed as suggestions to the
  // user, sorted by their relevance. This involves many steps from fetching the
  // profiles to matching with `field_contents`, and deduplicating based on
  // `field_types`, which are the relevant types for the current suggestion.
  std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
  GetProfilesToSuggest(FieldType trigger_field_type,
                       const std::u16string& field_contents,
                       bool field_is_autofilled,
                       const FieldTypeSet& field_types);

  // Returns a list of Suggestion objects, each representing an element in
  // `profiles`.
  // `field_types` holds the type of fields relevant for the current suggestion.
  // The profiles passed to this function should already have been matched on
  // `trigger_field_contents_canon` and deduplicated.
  // `previously_hidden_profiles_guid` stores the guids of the profiles that
  // were not displayed prior to the effects of the Finch feature
  // kAutofillUseAddressRewriterInProfileSubsetComparison.
  std::vector<Suggestion> CreateSuggestionsFromProfiles(
      const std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>&
          profiles,
      const FieldTypeSet& field_types,
      std::optional<FieldTypeSet> last_targeted_fields,
      FieldType trigger_field_type,
      uint64_t trigger_field_max_length,
      const std::set<std::string>& previously_hidden_profiles_guid = {});

  // Generates suggestions for all available credit cards based on the
  // `trigger_field_type`, `trigger_field` and `trigger_source`.
  // `with_offer` is set to true if ANY card has card-linked offers.
  // `with_cvc` is set to true if ANY card has cvc saved.
  // `metadata_logging_context` contains card metadata related information used
  // for metrics logging.
  // TODO(crbug.com/1519179): Merging out-parameters into a struct.
  std::vector<Suggestion> GetSuggestionsForCreditCards(
      const FormFieldData& trigger_field,
      FieldType trigger_field_type,
      AutofillSuggestionTriggerSource trigger_source,
      bool should_show_scan_credit_card,
      bool should_show_cards_from_account,
      bool& with_offer,
      bool& with_cvc,
      autofill_metrics::CardMetadataLoggingContext& metadata_logging_context);

  // Generates suggestions for standalone CVC fields. These only apply to
  // virtual cards that are saved on file to a merchant. In these cases,
  // we only display the virtual card option and do not show FPAN option.
  std::vector<Suggestion> GetSuggestionsForVirtualCardStandaloneCvc(
      const FormFieldData& trigger_field,
      autofill_metrics::CardMetadataLoggingContext& metadata_logging_context,
      base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>&
          virtual_card_guid_to_last_four_map);

  // Generates a separator suggestion.
  static Suggestion CreateSeparator();

  // Generates a footer suggestion "Manage addresses..." menu item which will
  // redirect to Chrome address settings page.
  static Suggestion CreateManageAddressesEntry();

  // Generates a footer suggestion "Manage payment methods..." menu item which
  // will redirect to Chrome payment settings page. `with_gpay_logo` is used to
  // conditionally add GPay logo icon to the manage payment methods suggestion.
  static Suggestion CreateManagePaymentMethodsEntry(bool with_gpay_logo);

  // Generate "Clear form" suggestion.
  static Suggestion CreateClearFormSuggestion();

  // Returns the local and server cards ordered by the Autofill ranking. The
  // cards which are expired and disused aren't included if
  // |suppress_disused_cards| is true.
  static std::vector<CreditCard> GetOrderedCardsToSuggest(
      AutofillClient& autofill_client,
      bool suppress_disused_cards);

  // Generates suggestions for all available IBANs.
  static std::vector<Suggestion> GetSuggestionsForIbans(
      const std::vector<const Iban*>& ibans);

  // Converts the vector of promo code offers that is passed in to a vector of
  // suggestions that can be displayed to the user for a promo code field.
  static std::vector<Suggestion> GetPromoCodeSuggestionsFromPromoCodeOffers(
      const std::vector<const AutofillOfferData*>& promo_code_offers);

  // Removes expired local credit cards not used since `min_last_used` from
  // `cards`. The relative ordering of `cards` is maintained.
  static void RemoveExpiredLocalCreditCardsNotUsedSinceTimestamp(
      base::Time min_last_used,
      std::vector<CreditCard*>& cards);

  // Return a nickname for the |card| to display. This is generally the nickname
  // stored in |card|, unless |card| exists as a local and a server copy. In
  // this case, we prefer the nickname of the local if it is defined. If only
  // one copy has a nickname, take that.
  std::u16string GetDisplayNicknameForCreditCard(const CreditCard& card) const;

  // Helper function to decide whether to show the virtual card option for
  // `candidate_card`.
  bool ShouldShowVirtualCardOption(const CreditCard* candidate_card) const;

 protected:
  // Creates a suggestion for the given `credit_card`. `virtual_card_option`
  // suggests whether the suggestion is a virtual card option.
  // `card_linked_offer_available` indicates whether a card-linked offer is
  // attached to the `credit_card`. `origin` is the webpage that the suggestion
  // will be displayed on.
  Suggestion CreateCreditCardSuggestion(const CreditCard& credit_card,
                                        FieldType trigger_field_type,
                                        bool virtual_card_option,
                                        bool card_linked_offer_available,
                                        const url::Origin& origin) const;

 private:
  // Dedupes the given profiles based on if one is a subset of the other for
  // suggestions represented by `field_types`. The function returns at most
  // `kMaxUniqueSuggestedProfilesCount` profiles. `field_types` stores all of
  // the FieldTypes relevant for the current suggestions, including that
  // of the field on which the user is currently focused.
  std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
  DeduplicatedProfilesForSuggestions(
      const std::vector<const AutofillProfile*>& matched_profiles,
      FieldType trigger_field_type,
      const FieldTypeSet& field_types,
      const AutofillProfileComparator& comparator);

  // Matches based on prefix search, and limits number of profiles.
  // Returns the top matching profiles based on prefix search. At most
  // `kMaxSuggestedProfilesCount` are returned.
  std::vector<const AutofillProfile*> GetPrefixMatchedProfiles(
      const std::vector<AutofillProfile*>& profiles,
      FieldType trigger_field_type,
      const std::u16string& raw_field_contents,
      const std::u16string& field_contents_canon,
      bool field_is_autofilled);

  // Removes profiles that haven't been used after `min_last_used` from
  // |profiles|. The relative ordering of `profiles` is maintained.
  void RemoveProfilesNotUsedSinceTimestamp(
      base::Time min_last_used,
      std::vector<AutofillProfile*>& profiles);

  // Creates nested/child suggestions for `suggestion` with the `profile`
  // information. Uses `trigger_field_type` to define what group filling
  // suggestion to add (name, address or phone). The existence of child
  // suggestions defines whether the autofill popup will have submenus.
  // `last_targeted_fields` specified the last set of fields target by the user.
  // When not present, we default to full form.
  void AddAddressGranularFillingChildSuggestions(
      std::optional<FieldTypeSet> last_targeted_fields,
      FieldType trigger_field_type,
      const AutofillProfile& profile,
      Suggestion& suggestion) const;

  // Creates nested/child suggestions for `suggestion` with the `credit_card`
  // information. The number of nested suggestions added depends on the
  // information present in the `credit_card`.
  void AddPaymentsGranularFillingChildSuggestions(const CreditCard& credit_card,
                                                  Suggestion& suggestion) const;

  // Return the texts shown as the first line of the suggestion, based on the
  // `credit_card` and the `trigger_field_type`. The first index in the pair
  // represents the main text, and the second index represents the minor text.
  // The minor text can be empty, in which case the main text should be rendered
  // as the entire first line. If the minor text is not empty, they should be
  // combined. This splitting is implemented for situations where the first part
  // of the first line of the suggestion should be truncated.
  std::pair<Suggestion::Text, Suggestion::Text>
  GetSuggestionMainTextAndMinorTextForCard(const CreditCard& credit_card,
                                           FieldType trigger_field_type) const;

  // Return the labels to be shown in the suggestion. Note this does not account
  // for virtual cards or card-linked offers.
  std::vector<Suggestion::Text> GetSuggestionLabelsForCard(
      const CreditCard& credit_card,
      FieldType trigger_field_type,
      const url::Origin& origin) const;

  // Returns the benefit text to display in credit card suggestions if it is
  // available.
  std::optional<Suggestion::Text> GetCreditCardBenefitSuggestionLabel(
      const CreditCard& credit_card,
      const url::Origin& origin) const;

  // Adjust the content of `suggestion` if it is a virtual card suggestion.
  void AdjustVirtualCardSuggestionContent(Suggestion& suggestion,
                                          const CreditCard& credit_card,
                                          FieldType trigger_field_type,
                                          const url::Origin& origin) const;

  // Set the URL for the card art image to be shown in the `suggestion`.
  void SetCardArtURL(Suggestion& suggestion,
                     const CreditCard& credit_card,
                     bool virtual_card_option) const;

  // Returns non address suggestions which are displayed below address
  // suggestions in the Autofill popup. `is_autofilled` is used to conditionally
  // add suggestion for clearing all autofilled fields.
  std::vector<Suggestion> GetAddressFooterSuggestions(bool is_autofilled) const;

  // Returns non credit card suggestions which are displayed below credit card
  // suggestions in the Autofill popup. `should_show_scan_credit_card` is used
  // to conditionally add scan credit card suggestion,
  // `should_show_cards_from_account` - conditionally add suggestions for
  // showing cards from account. `is_autofilled` is used to conditionally add
  // suggestion for clearing all autofilled fields. `with_gpay_logo` is used to
  // conditionally add GPay logo icon to the manage payment methods suggestion.
  std::vector<Suggestion> GetCreditCardFooterSuggestions(
      bool should_show_scan_credit_card,
      bool should_show_cards_from_account,
      bool is_autofilled,
      bool with_gpay_logo) const;

  // Returns true if we should show a virtual card option for the server card
  // `card`, false otherwise.
  bool ShouldShowVirtualCardOptionForServerCard(const CreditCard* card) const;

  // autofill_client_ and the generator are both one per tab, and have the same
  // lifecycle.
  base::raw_ref<AutofillClient> autofill_client_;

  // personal_data_ should outlive the generator.
  base::raw_ref<PersonalDataManager> personal_data_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SUGGESTION_GENERATOR_H_
