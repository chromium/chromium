// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARD_METADATA_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARD_METADATA_METRICS_H_

#include "base/containers/flat_set.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"

namespace autofill::autofill_metrics {

// The below issuer, network, and benefit source names are used for logging
// purposes. The issuers, networks, and benefit sources must be consistent with
// the Autofill.CreditCardIssuerId, Autofill.CreditCardNetwork, and
// Autofill.CreditCardBenefitSource respectively, in the
// tools/metrics/histograms/metadata/autofill/histograms.xml file.
constexpr std::string_view kAmericanExpress = "Amex";
constexpr std::string_view kAnz = "Anz";
constexpr std::string_view kBmo = "Bmo";
constexpr std::string_view kCapitalOne = "CapitalOne";
constexpr std::string_view kChase = "Chase";
constexpr std::string_view kCiti = "Citi";
constexpr std::string_view kCurinos = "Curinos";
constexpr std::string_view kDiscover = "Discover";
constexpr std::string_view kLloyds = "Lloyds";
constexpr std::string_view kMarqeta = "Marqeta";
constexpr std::string_view kNab = "Nab";
constexpr std::string_view kNatwest = "Natwest";
constexpr std::string_view kMastercard = "Mastercard";
constexpr std::string_view kVisa = "Visa";

constexpr std::string_view kProductNameAndArtImageBothShownSuffix =
    "ProductDescriptionAndArtImageShown";
constexpr std::string_view kProductNameShownOnlySuffix =
    "ProductDescriptionShown";
constexpr std::string_view kArtImageShownOnlySuffix = "ArtImageShown";
constexpr std::string_view kProductNameAndArtImageNotShownSuffix =
    "MetadataNotShown";

// Enum for different types of form events. Used for metrics logging.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CardMetadataLoggingEvent {
  // Suggestions were shown.
  kShown = 0,
  // Suggestion was selected.
  kSelected = 1,
  // Suggestion was filled into the form.
  kFilled = 2,
  // Form was about to be submitted, after being filled with a suggestion.
  kWillSubmit = 3,
  // Form was submitted, after being filled with a suggestion.
  kSubmitted = 4,
  kMaxValue = kSubmitted,
};

// LINT.IfChange(CardBenefitFormEvent)

// All server cards with card benefit available Form Events are logged once per
// page load.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CardBenefitFormEvent {
  // Suggestions containing cards with a benefit available were shown.
  kSuggestionWithBenefitShown = 0,

  // Suggestions containing cards with a benefit available were shown when the
  // user had two or more server cards.
  kSuggestionWithBenefitShownWithMultipleServerCards = 1,

  // A suggestion of a masked server card with a benefit available was selected.
  kSuggestionWithBenefitSelected = 2,

  // `kSuggestionWithoutBenefitSelected` was planned to be an enum with the
  // value "3". This enum will not be implemented anymore since there is no
  // possible single card use case for it. This form event enum would be logged
  // when a masked server card without a benefit available was selected, and at
  // least one masked server card with a benefit available was present in the
  // suggestions. `kSuggestionWithoutBenefitSelectedWithMultipleServerCards` is
  // already being logged for this.

  // A suggestion of a masked server card with a benefit available was selected
  // when the user had two or more server cards.
  kSuggestionWithBenefitSelectedWithMultipleServerCards = 4,

  // A suggestion of a masked server card without a benefit available was
  // selected when the user had two or more server cards, and at least one had a
  // benefit available.
  kSuggestionWithoutBenefitSelectedWithMultipleServerCards = 5,

  // A suggestion of a masked server card with a benefit available was filled.
  kSuggestionWithBenefitFilled = 6,

  // `kSuggestionWithoutBenefitFilled` was planned to be an enum with the
  // value "7". This enum will not be implemented anymore since there is no
  // possible single card use case for it. This form event enum would be logged
  // when a masked server card without a benefit available was filled, and at
  // least one masked server card with a benefit available was present in the
  // suggestions. `kSuggestionWithoutBenefitFilledWithMultipleServerCards` is
  // already being logged for this.

  // A suggestion of a masked server card with a benefit available was filled
  // when the user had two or more server cards.
  kSuggestionWithBenefitFilledWithMultipleServerCards = 8,

  // A suggestion of a masked server card without a benefit available was
  // filled when the user had two or more server cards, and at least one had a
  // benefit available.
  kSuggestionWithoutBenefitFilledWithMultipleServerCards = 9,

  // A suggestion of a masked server card with a benefit available was
  // submitted.
  kSuggestionWithBenefitSubmitted = 10,

  // `kSuggestionWithoutBenefitSubmitted` was planned to be an enum with the
  // value "11". This enum will not be implemented anymore since there is no
  // possible single card use case for it. This form event enum would be logged
  // when a masked server card without a benefit available was submitted, and at
  // least one masked server card with a benefit available was present in the
  // suggestions. `kSuggestionWithoutBenefitSubmittedWithMultipleServerCards` is
  // already being logged for this.

  // A suggestion of a masked server card with a benefit available was submitted
  // when the user had two or more server cards.
  kSuggestionWithBenefitSubmittedWithMultipleServerCards = 12,

  // A suggestion of a masked server card without a benefit available was
  // submitted when the user had two or more server cards, and at least one had
  // a benefit available.
  kSuggestionWithoutBenefitSubmittedWithMultipleServerCards = 13,

  kMaxValue = kSuggestionWithoutBenefitSubmittedWithMultipleServerCards
};

// LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:CardBenefitFormEvent)

using HasBeenLogged = base::StrongAlias<class HasBeenLoggedTag, bool>;

// Struct that groups metadata-related information together for some
// set of credit cards. Used for metrics logging whether metadata is
// available and/or shown with credit card suggestions, including
// product descriptions, card art images, and card benefits.
struct CardMetadataLoggingContext {
  CardMetadataLoggingContext();
  CardMetadataLoggingContext(const CardMetadataLoggingContext&);
  CardMetadataLoggingContext(CardMetadataLoggingContext&&);
  CardMetadataLoggingContext& operator=(const CardMetadataLoggingContext&);
  CardMetadataLoggingContext& operator=(CardMetadataLoggingContext&&);
  ~CardMetadataLoggingContext();

  // Returns if any shown suggestion's card has a benefit available.
  bool DidShowCardWithBenefitAvailable() const;

  // Returns if the selected suggestion's card has a benefit available.
  bool SelectedCardHasBenefitAvailable() const;

  // Returns if the selected suggestion's card has card metadata shown.
  bool SelectedCardHasMetadataAvailable() const;

  // Updates `selected_card_has_metadata_available` and
  // `selected_issuer_or_network_to_metadata_availability` with the
  // `credit_card` information.
  void SetSelectedCardInfo(const CreditCard& credit_card);

  // Keeps record of what type of metadata was shown to the user when credit
  // card suggestions are presented. When set to true, implies at least one
  // suggestion shown to the user had the listed metadata attribute.
  bool card_product_description_shown = false;
  bool card_art_image_shown = false;

  // Keeps record of which issuers and networks with metadata were not selected.
  // Only available when logging the selected form event.
  base::flat_set<std::string> not_selected_issuer_ids_and_networks;

  // Keeps record of whether suggestions from issuers or networks had metadata.
  // If the value is true for a particular issuer or network, at least 1 card
  // suggestion from the issuer or network had metadata. If it is false, none of
  // the card suggestions from the issuer or network had metadata.
  base::flat_map<std::string, bool> issuer_or_network_to_metadata_availability;

  // Keeps record of which credit cards shown to the user had metadata
  // available.
  base::flat_set<int64_t> instruments_with_metadata_available;

  // Keeps record of the selected card's issuer and network and if the card had
  // metadata available. If there is no selected card,
  // `selected_issuer_or_network_to_metadata_availability` has no value.
  std::optional<base::flat_map<std::string, bool>>
      selected_issuer_or_network_to_metadata_availability;

  // Keeps record of the instrument ids to benefit sources for credit card
  // suggestions shown to the user with a card benefit.
  base::flat_map<int64_t, std::string>
      instrument_ids_to_available_benefit_sources;

  // Keeps record of the selected card benefit source for later events logging.
  std::string selected_benefit_source;

  // Keeps record of the selected card instrument id for later events logging.
  int64_t selected_card_instrument_id;

  // Keeps record of the number of masked server card suggestions.
  uint8_t masked_server_card_count = 0;
};

// Get histogram suffix based on a given card issuer id or network.
std::string_view GetCardIssuerIdOrNetworkSuffix(
    std::string_view card_issuer_id_or_network);

// Get histogram suffix based on a given card benefit source.
std::string_view GetCardBenefitSourceSuffix(
    std::string_view card_benefit_source);

// Get the CardMetadataLoggingContext for the given credit cards.
CardMetadataLoggingContext GetMetadataLoggingContext(
    base::span<const CreditCard> cards);

// Log the suggestion event regarding card metadata. `has_been_logged` indicates
// whether the event has already been logged since last page load.
void LogCardWithMetadataFormEventMetric(
    CardMetadataLoggingEvent event,
    const CardMetadataLoggingContext& context,
    HasBeenLogged has_been_logged);

// Log the suggestion event for card benefits on a credit card level and benefit
// or issuer level. Metrics are only logged once per page load.
void LogCardBenefitFormEventMetrics(CardMetadataLoggingEvent event,
                                    const CardMetadataLoggingContext& context);

// Log the latency between suggestions being shown and a suggestion was
// selected, in milliseconds, and it is broken down by metadata availability
// provided by the `suggestion_context`.
void LogAcceptanceLatency(base::TimeDelta latency,
                          const CardMetadataLoggingContext& suggestion_context,
                          const CreditCard& selected_card);

// Logs if credit card benefits are enabled when a new profile is launched.
void LogIsCreditCardBenefitsEnabledAtStartup(bool enabled);

// Log the given `event` to the general benefit histogram, as well as to the
// benefit-source-specific subhistogram for all benefit sources present in the
// suggestion list.
void LogBenefitFormEventToAllBenefitHistograms(
    const base::flat_map<int64_t, std::string>&
        instrument_ids_to_available_benefit_sources,
    CardBenefitFormEvent event);

// Log the given `event` to the general benefit histogram, as well as to the
// `benefit_source`'s specific subhistogram.
void LogBenefitFormEventToAllBenefitHistograms(std::string_view benefit_source,
                                               CardBenefitFormEvent event);

// Log the given `event` for card benefits on a benefit source level.
// TODO(crbug.com/417228483): Remove this function after adding benefit form
// event enums to a new histogram with a new enum class.
void LogBenefitFormEventToBenefitSourceHistogramDeprecated(
    std::string_view benefit_source,
    FormEvent event);

// Log the given `event` for every card benefit source with benefits available
// shown.
// TODO(crbug.com/417228483): Remove this function after adding benefit form
// event enums to a new histogram with a new enum class.
void LogBenefitFormEventForAllBenefitSourcesWithBenefitAvailableDeprecated(
    const base::flat_map<int64_t, std::string>&
        instrument_ids_to_available_benefit_sources,
    FormEvent event);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARD_METADATA_METRICS_H_
