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
class AutofillType;
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

  AutofillSuggestionGenerator(AutofillClient* autofill_client,
                              PersonalDataManager* personal_data);
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
  // field filling, group filling or full form (default). `skip_statuses` is
  // used to know which fields are skipped during filling and which are not, and
  // only use fillable fields for suggestion deduplication and label generation.
  // It is assumed that skip_statuses and form_structure have the sane size.
  std::vector<Suggestion> GetSuggestionsForProfiles(
      const ServerFieldTypeSet& field_types,
      const FormFieldData& triggering_field,
      AutofillType triggering_field_type,
      absl::optional<ServerFieldTypeSet> last_targeted_fields,
      AutofillSuggestionTriggerSource trigger_source);

  // Returns a list of profiles that will be displayed as suggestions to the
  // user, sorted by their relevance. This involves many steps from fetching the
  // profiles to matching with `field_contents`, and deduplicating based on
  // `field_types`, which are the relevant types for the current suggestion.
  std::vector<const AutofillProfile*> GetProfilesToSuggest(
      const AutofillType& type,
      const std::u16string& field_contents,
      bool field_is_autofilled,
      const ServerFieldTypeSet& field_types);

  // Returns a list of Suggestion objects, each representing an element in
  // `profiles`.
  // `field_types` holds the type of fields relevant for the current suggestion.
  // The profiles passed to this function should already have been matched on
  // `trigger_field_contents_canon` and deduplicated.
  std::vector<Suggestion> CreateSuggestionsFromProfiles(
      const std::vector<const AutofillProfile*>& profiles,
      const ServerFieldTypeSet& field_types,
      absl::optional<ServerFieldTypeSet> last_targeted_fields,
      const AutofillType& trigger_field_type,
      uint64_t trigger_field_max_length);

  // Generates suggestions for all available credit cards based on the `type`
  // and the value of `field`. `should_display_gpay_logo` will be set to true if
  // there are no credit card suggestions, or all suggestions come from Payments
  // server. `with_offer` is set to true if ANY card has card-linked offers.
  // `metadata_logging_context` contains card metadata related information used
  // for metrics logging.
  std::vector<Suggestion> GetSuggestionsForCreditCards(
      const FormFieldData& field,
      const AutofillType& type,
      bool& should_display_gpay_logo,
      bool& with_offer,
      autofill_metrics::CardMetadataLoggingContext& metadata_logging_context);

  // Generates suggestions for standalone CVC fields. These only apply to
  // virtual cards that are saved on file to a merchant. In these cases,
  // we only display the virtual card option and do not show FPAN option.
  std::vector<Suggestion> GetSuggestionsForVirtualCardStandaloneCvc(
      autofill_metrics::CardMetadataLoggingContext& metadata_logging_context,
      base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>&
          virtual_card_guid_to_last_four_map);

  // Generates a separator suggestion.
  static Suggestion CreateSeparator();

  // Generates a footer suggestion "Manage payment methods..." menu item which
  // will redirect to Chrome payment settings page.
  static Suggestion CreateManagePaymentMethodsEntry();

  // Returns the local and server cards ordered by the Autofill ranking. The
  // cards which are expired and disused aren't included if
  // |suppress_disused_cards| is true.
  static std::vector<CreditCard> GetOrderedCardsToSuggest(
      AutofillClient* autofill_client,
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

  // Checks whether the suggestion accepted by the user, generated from the
  // profile with `backend_id`, would've been hidden prior to landing the
  // feature `kAutofillUseAddressRewriterInProfileSubsetComparison`.
  // `skip_statuses` denotes for each field if it is relevant for the current
  // suggestion or not.
  // TODO(crbug/1439742): Remove when
  // `kAutofillUseAddressRewriterInProfileSubsetComparison` launches.
  bool WasProfileSuggestionPreviouslyHidden(
      const FormStructure& form,
      const AutofillField& field,
      Suggestion::BackendId backend_id,
      const std::vector<FieldFillingSkipReason>& skip_reasons);

 protected:
  // Creates a suggestion for the given `credit_card`. `type` denotes the
  // AutofillType of the field that is focused when the query is triggered.
  // `virtual_card_option` suggests whether the suggestion is a virtual card
  // option. `card_linked_offer_available` indicates whether a card-linked offer
  // is attached to the `credit_card`.
  Suggestion CreateCreditCardSuggestion(const CreditCard& credit_card,
                                        const AutofillType& type,
                                        bool virtual_card_option,
                                        bool card_linked_offer_available) const;

 private:
  // Dedupes the given profiles based on if one is a subset of the other for
  // suggestions represented by `field_types`. The function returns at most
  // `kMaxUniqueSuggestedProfilesCount` profiles. `field_types` stores all of
  // the ServerFieldTypes relevant for the current suggestions, including that
  // of the field on which the user is currently focused.
  std::vector<const AutofillProfile*> DeduplicatedProfilesForSuggestions(
      const std::vector<const AutofillProfile*>& matched_profiles,
      const AutofillType& trigger_field_type,
      const ServerFieldTypeSet& field_types,
      const AutofillProfileComparator& comparator);

  // Matches based on prefix search, and limits number of profiles.
  // Returns the top matching profiles based on prefix search. At most
  // `kMaxSuggestedProfilesCount` are returned.
  std::vector<const AutofillProfile*> GetPrefixMatchedProfiles(
      const std::vector<AutofillProfile*>& profiles,
      const AutofillType& trigger_field_type,
      const std::u16string& raw_field_contents,
      const std::u16string& field_contents_canon,
      bool field_is_autofilled);

  // Removes profiles that haven't been used after `min_last_used` from
  // |profiles|. The relative ordering of `profiles` is maintained.
  void RemoveProfilesNotUsedSinceTimestamp(
      base::Time min_last_used,
      std::vector<AutofillProfile*>& profiles);

  // In addition to just getting the values out of the profile, this function
  // handles type-specific formatting.
  std::u16string GetProfileSuggestionMainText(const AutofillProfile* profile,
                                              const AutofillType& type);

  // Creates nested/child suggestions for `suggestion` with the `profile`
  // information. Uses `trigger_field_type_group` to define what group filling
  // suggestion to add (name, address or phone). The existence of child
  // suggestions defines whether the autofill popup will have submenus.
  // `last_targeted_fields` specified the last set of fields target by the user.
  // When not present, we default to full form.
  void AddGranularFillingChildSuggestions(
      FieldTypeGroup trigger_field_type_group,
      absl::optional<ServerFieldTypeSet> last_targeted_fields,
      const AutofillProfile& profile,
      Suggestion& suggestion);

  // Return the texts shown as the first line of the suggestion, based on the
  // `credit_card` and the focused field `type`. The first index in the pair
  // represents the main text, and the second index represents the minor text.
  // The minor text can be empty, in which case the main text should be rendered
  // as the entire first line. If the minor text is not empty, they should be
  // combined. This splitting is implemented for situations where the first part
  // of the first line of the suggestion should be truncated.
  std::pair<Suggestion::Text, Suggestion::Text>
  GetSuggestionMainTextAndMinorTextForCard(const CreditCard& credit_card,
                                           const AutofillType& type) const;

  // Return the labels to be shown in the suggestion. Note this does not account
  // for virtual cards or card-linked offers.
  std::vector<Suggestion::Text> GetSuggestionLabelsForCard(
      const CreditCard& credit_card,
      const AutofillType& type) const;

  // Adjust the content of |suggestion| if it is a virtual card suggestion.
  void AdjustVirtualCardSuggestionContent(Suggestion& suggestion,
                                          const CreditCard& credit_card,
                                          const AutofillType& type) const;

  // Set the URL for the card art image to be shown in the `suggestion`.
  void SetCardArtURL(Suggestion& suggestion,
                     const CreditCard& credit_card,
                     bool virtual_card_option) const;

  // Returns true if we should show a virtual card option for the server card
  // `card`, false otherwise.
  bool ShouldShowVirtualCardOptionForServerCard(const CreditCard* card) const;

  // autofill_client_ and the generator are both one per tab, and have the same
  // lifecycle.
  raw_ptr<AutofillClient> autofill_client_;

  // personal_data_ should outlive the generator.
  raw_ptr<PersonalDataManager> personal_data_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SUGGESTION_GENERATOR_H_
