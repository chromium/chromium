// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARD_METADATA_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARD_METADATA_METRICS_H_

#include "components/autofill/core/browser/metrics/autofill_metrics.h"

namespace autofill::autofill_metrics {

// The below issuer names are used for logging purposes, and they thus must be
// consistent with the Autofill.CreditCardIssuerId in the
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
  CardMetadataLoggingContext& operator=(const CardMetadataLoggingContext&);
  ~CardMetadataLoggingContext();

  bool card_metadata_available = false;
  bool card_product_description_shown = false;
  bool card_art_image_shown = false;
  // Keeps record of whether suggestions from issuers had metadata. If the value
  // is true for a particular issuer, at least 1 card suggestion from the issuer
  // had metadata. If it is false, none of the card suggestions from the issuer
  // had metadata.
  base::flat_map<std::string, bool> issuer_to_metadata_availability;
};

std::string_view GetCardIssuerIdSuffix(const std::string& card_issuer_id);

// Get the CardMetadataLoggingContext for the given credit cards.
CardMetadataLoggingContext GetMetadataLoggingContext(
    const std::vector<CreditCard>& cards);

// Log the suggestion event regarding card metadata. `has_been_logged` indicates
// whether the event has already been logged since last page load.
void LogCardWithMetadataFormEventMetric(
    CardMetadataLoggingEvent event,
    const CardMetadataLoggingContext& context,
    HasBeenLogged has_been_logged);

// Log the latency between suggestions being shown and a suggestion was
// selected, in milliseconds, and it is broken down by metadata availability
// provided by the `suggestion_context`.
void LogAcceptanceLatency(base::TimeDelta latency,
                          const CardMetadataLoggingContext& suggestion_context,
                          const CreditCard& selected_card);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARD_METADATA_METRICS_H_
