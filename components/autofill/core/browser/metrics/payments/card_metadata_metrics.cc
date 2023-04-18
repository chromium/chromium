// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/card_metadata_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/common/autofill_payments_features.h"

namespace autofill::autofill_metrics {

namespace {

std::string GetMetadataAvailabilitySuffix(
    const CardMetadataLoggingContext& context) {
  if (context.card_product_description_shown && context.card_art_image_shown) {
    return kProductNameAndArtImageBothShownSuffix;
  }
  if (context.card_product_description_shown) {
    return kProductNameShownOnlySuffix;
  }
  if (context.card_art_image_shown) {
    return kArtImageShownOnlySuffix;
  }
  return kProductNameAndArtImageNotShownSuffix;
}

std::string GetCardIssuerIdSuffix(const std::string& card_issuer_id) {
  if (!card_issuer_id.empty()) {
    if (card_issuer_id == kAmexCardIssuerId) {
      return kAmericanExpress;
    }
    if (card_issuer_id == kCapitalOneCardIssuerId) {
      return kCapitalOne;
    }
    if (card_issuer_id == kChaseCardIssuerId) {
      return kChase;
    }
    if (card_issuer_id == kMarqetaCardIssuerId) {
      return kMarqeta;
    }

    // Found an unknown issuer id.
    DLOG(WARNING) << "It seems a new issuer was added, but the suggestion "
                     "acceptance latency logging logic was not updated. Please "
                     "update this logging if this issuer should be included in "
                     "this logging. Ignore if you have already updated it and "
                     "this is from older versions or you don't care about the "
                     "newly added issuer.";
  }

  return std::string();
}

// Returns true when the card has rich card art, excluding any static card art
// image.
bool HasRichCardArtImageFromMetadata(const CreditCard& card) {
  return card.card_art_url().is_valid() &&
         card.card_art_url().spec() !=
             (base::FeatureList::IsEnabled(
                  features::kAutofillEnableNewCardArtAndNetworkImages)
                  ? kCapitalOneLargeCardArtUrl
                  : kCapitalOneCardArtUrl);
}

}  // namespace

CardMetadataLoggingContext::CardMetadataLoggingContext() = default;
CardMetadataLoggingContext::CardMetadataLoggingContext(
    const CardMetadataLoggingContext&) = default;
CardMetadataLoggingContext& CardMetadataLoggingContext::operator=(
    const CardMetadataLoggingContext&) = default;
CardMetadataLoggingContext::~CardMetadataLoggingContext() = default;

CardMetadataLoggingContext GetMetadataLoggingContext(
    const std::vector<CreditCard>& cards) {
  CardMetadataLoggingContext metadata_logging_context;

  for (const CreditCard& card : cards) {
    if (card.issuer_id().empty()) {
      continue;
    }

    // If there is a product description, denote in the
    // `metadata_logging_context` that we have shown at least one product
    // description so we can log it later.
    if (!card.product_description().empty()) {
      metadata_logging_context.card_product_description_shown =
          base::FeatureList::IsEnabled(
              features::kAutofillEnableCardProductName);
    }

    // If there is rich card art we received from the metadata for this card,
    // denote in the `metadata_logging_context` that we have shown an enriched
    // card art so we can log it later.
    if (HasRichCardArtImageFromMetadata(card)) {
      metadata_logging_context.card_art_image_shown =
          base::FeatureList::IsEnabled(features::kAutofillEnableCardArtImage);
    }

    bool card_has_metadata = !card.product_description().empty() ||
                             HasRichCardArtImageFromMetadata(card);
    metadata_logging_context
        .issuer_to_metadata_availability[card.issuer_id()] |= card_has_metadata;
    // If there is at least one card having product description or rich card
    // art, denote in the `metadata_logging_context`.
    if (card_has_metadata) {
      metadata_logging_context.card_metadata_available = true;
    }
  }

  return metadata_logging_context;
}

void LogCardWithMetadataFormEventMetric(
    CardMetadataLoggingEvent event,
    const CardMetadataLoggingContext& context,
    HasBeenLogged has_been_logged) {
  for (const auto& [issuer, has_metadata] :
       context.issuer_to_metadata_availability) {
    switch (event) {
      case CardMetadataLoggingEvent::kShown:
        base::UmaHistogramBoolean("Autofill.CreditCard." +
                                      GetCardIssuerIdSuffix(issuer) +
                                      ".ShownWithMetadata",
                                  has_metadata);
        if (!has_been_logged.value()) {
          base::UmaHistogramBoolean("Autofill.CreditCard." +
                                        GetCardIssuerIdSuffix(issuer) +
                                        ".ShownWithMetadataOnce",
                                    has_metadata);
        }
        break;
      case CardMetadataLoggingEvent::kSelected:
        base::UmaHistogramBoolean("Autofill.CreditCard." +
                                      GetCardIssuerIdSuffix(issuer) +
                                      ".SelectedWithMetadata",
                                  has_metadata);
        if (!has_been_logged.value()) {
          base::UmaHistogramBoolean("Autofill.CreditCard." +
                                        GetCardIssuerIdSuffix(issuer) +
                                        ".SelectedWithMetadataOnce",
                                    has_metadata);
        }
        break;
    }
  }
}

void LogAcceptanceLatency(base::TimeDelta latency,
                          const CardMetadataLoggingContext& suggestion_context,
                          const CreditCard& selected_card) {
  std::string histogram_name_prefix =
      "Autofill.CreditCard.SelectionLatencySinceShown.";
  base::UmaHistogramMediumTimes(
      histogram_name_prefix + GetMetadataAvailabilitySuffix(suggestion_context),
      latency);

  std::string issuer_id_suffix =
      GetCardIssuerIdSuffix(selected_card.issuer_id());
  if (issuer_id_suffix.empty()) {
    return;
  }

  base::UmaHistogramMediumTimes(
      histogram_name_prefix + "CardWithIssuerId." +
          GetMetadataAvailabilitySuffix(GetMetadataLoggingContext(
              std::vector<CreditCard>{selected_card})) +
          "." + issuer_id_suffix,
      latency);
}

}  // namespace autofill::autofill_metrics
