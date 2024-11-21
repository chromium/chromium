// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARD_METADATA_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARD_METADATA_METRICS_H_

#include "base/containers/flat_set.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"

namespace autofill::autofill_metrics {

// The below issuer and network names are used for logging purposes. The issuer
// names must be consistent with the Autofill.CreditCardIssuerId in the
// autofill/histograms.xml file.
constexpr std::string_view kAmericanExpress = "Amex";
constexpr std::string_view kAnz = "Anz";
constexpr std::string_view kCapitalOne = "CapitalOne";
constexpr std::string_view kChase = "Chase";
constexpr std::string_view kCiti = "Citi";
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

using HasBeenLogged = base::StrongAlias<class HasBeenLoggedTag, bool>;

// Struct that groups metadata-related information together for some set of
// credit cards. Used for metrics logging.
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

  // Keeps record of the instrument ids to issuer ids for credit card
  // suggestions shown to the user with a card benefit.
  base::flat_map<int64_t, std::string>
      instrument_ids_to_issuer_ids_with_benefits_available;

  // Keeps record of the issuer of a selected card suggestion.
  std::string selected_issuer_id;

  // Keeps record of the selected card instrument id for later events logging.
  int64_t selected_card_instrument_id;
};

// Get histogram suffix based on given card issuer id or network.
std::string_view GetCardIssuerIdOrNetworkSuffix(
    const std::string& card_issuer_id_or_network);

// Get the CardMetadataLoggingContext for the given credit cards.
CardMetadataLoggingContext GetMetadataLoggingContext(
    const std::vector<CreditCard>& cards);

// Log the suggestion event regarding card metadata. `has_been_logged` indicates
// whether the event has already been logged since last page load.
void LogCardWithMetadataFormEventMetric(
    CardMetadataLoggingEvent event,
    const CardMetadataLoggingContext& context,
    HasBeenLogged has_been_logged);

// Log the suggestion event for card benefits on an issuer level. Metrics are
// only logged once per page load.
void LogCardWithBenefitFormEventMetric(
    CardMetadataLoggingEvent event,
    const CardMetadataLoggingContext& context);

// Log the latency between suggestions being shown and a suggestion was
// selected, in milliseconds, and it is broken down by metadata availability
// provided by the `suggestion_context`.
void LogAcceptanceLatency(base::TimeDelta latency,
                          const CardMetadataLoggingContext& suggestion_context,
                          const CreditCard& selected_card);

// Logs if credit card benefits are enabled when a new profile is launched.
void LogIsCreditCardBenefitsEnabledAtStartup(bool enabled);

void LogBenefitFormEventToIssuerHistogram(const std::string& issuer_id,
                                          FormEvent event);

// Log the given `event` for every issuer with card with benefits available
// shown.
void LogBenefitFormEventForAllIssuersWithBenefitAvailable(
    const base::flat_map<int64_t, std::string>&
        instrument_ids_to_issuer_ids_with_benefits_available,
    FormEvent event);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARD_METADATA_METRICS_H_
