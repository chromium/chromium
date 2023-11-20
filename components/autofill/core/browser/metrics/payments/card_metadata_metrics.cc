// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/card_metadata_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/common/autofill_payments_features.h"

namespace autofill::autofill_metrics {

namespace {

std::string_view GetMetadataAvailabilitySuffix(
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

// Returns true when the card has rich card art, excluding any static card art
// image.
bool HasRichCardArtImageFromMetadata(const CreditCard& card) {
  return card.card_art_url().is_valid() &&
         card.card_art_url().spec() != kCapitalOneLargeCardArtUrl &&
         card.card_art_url().spec() != kCapitalOneCardArtUrl;
}

}  // namespace

CardMetadataLoggingContext::CardMetadataLoggingContext() = default;
CardMetadataLoggingContext::CardMetadataLoggingContext(
    const CardMetadataLoggingContext&) = default;
CardMetadataLoggingContext& CardMetadataLoggingContext::operator=(
    const CardMetadataLoggingContext&) = default;
CardMetadataLoggingContext::~CardMetadataLoggingContext() = default;

// Get histogram suffix based on given card issuer id
std::string_view GetCardIssuerIdSuffix(const std::string& card_issuer_id) {
  if (card_issuer_id == kAmexCardIssuerId) {
    return kAmericanExpress;
  } else if (card_issuer_id == kAnzCardIssuerId) {
    return kAnz;
  } else if (card_issuer_id == kCapitalOneCardIssuerId) {
    return kCapitalOne;
  } else if (card_issuer_id == kChaseCardIssuerId) {
    return kChase;
  } else if (card_issuer_id == kCitiCardIssuerId) {
    return kCiti;
  } else if (card_issuer_id == kDiscoverCardIssuerId) {
    return kDiscover;
  } else if (card_issuer_id == kLloydsCardIssuerId) {
    return kLloyds;
  } else if (card_issuer_id == kMarqetaCardIssuerId) {
    return kMarqeta;
  } else if (card_issuer_id == kNabCardIssuerId) {
    return kNab;
  } else if (card_issuer_id == kNatwestCardIssuerId) {
    return kNatwest;
  } else {
    return "";
  }
}

CardMetadataLoggingContext GetMetadataLoggingContext(
    const std::vector<CreditCard>& cards) {
  CardMetadataLoggingContext metadata_logging_context;

  for (const CreditCard& card : cards) {
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

    if (!card.issuer_id().empty()) {
      metadata_logging_context
          .issuer_to_metadata_availability[card.issuer_id()] |=
          card_has_metadata;
    }

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
    if (GetCardIssuerIdSuffix(issuer) == std::string()) {
      continue;
    }

    switch (event) {
      case CardMetadataLoggingEvent::kShown:
        base::UmaHistogramBoolean(
            base::StrCat({"Autofill.CreditCard.", GetCardIssuerIdSuffix(issuer),
                          ".ShownWithMetadata"}),
            has_metadata);
        if (!has_been_logged.value()) {
          base::UmaHistogramBoolean(base::StrCat({"Autofill.CreditCard.",
                                                  GetCardIssuerIdSuffix(issuer),
                                                  ".ShownWithMetadataOnce"}),
                                    has_metadata);
        }
        break;
      case CardMetadataLoggingEvent::kSelected:
        base::UmaHistogramBoolean(
            base::StrCat({"Autofill.CreditCard.", GetCardIssuerIdSuffix(issuer),
                          ".SelectedWithMetadata"}),
            has_metadata);
        if (!has_been_logged.value()) {
          base::UmaHistogramBoolean(base::StrCat({"Autofill.CreditCard.",
                                                  GetCardIssuerIdSuffix(issuer),
                                                  ".SelectedWithMetadataOnce"}),
                                    has_metadata);
        }
        break;
      case CardMetadataLoggingEvent::kFilled:
        base::UmaHistogramBoolean(
            base::StrCat({"Autofill.CreditCard.", GetCardIssuerIdSuffix(issuer),
                          ".FilledWithMetadata"}),
            has_metadata);
        if (!has_been_logged.value()) {
          base::UmaHistogramBoolean(base::StrCat({"Autofill.CreditCard.",
                                                  GetCardIssuerIdSuffix(issuer),
                                                  ".FilledWithMetadataOnce"}),
                                    has_metadata);
        }
        break;
      case CardMetadataLoggingEvent::kWillSubmit:
        base::UmaHistogramBoolean(
            base::StrCat({"Autofill.CreditCard.", GetCardIssuerIdSuffix(issuer),
                          ".WillSubmitWithMetadataOnce"}),
            has_metadata);
        break;
      case CardMetadataLoggingEvent::kSubmitted:
        base::UmaHistogramBoolean(
            base::StrCat({"Autofill.CreditCard.", GetCardIssuerIdSuffix(issuer),
                          ".SubmittedWithMetadataOnce"}),
            has_metadata);
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
      base::StrCat({histogram_name_prefix,
                    GetMetadataAvailabilitySuffix(suggestion_context)}),
      latency);

  std::string_view issuer_id_suffix =
      GetCardIssuerIdSuffix(selected_card.issuer_id());
  if (issuer_id_suffix.empty()) {
    return;
  }

  base::UmaHistogramMediumTimes(
      base::StrCat({histogram_name_prefix, "CardWithIssuerId.",
                    GetMetadataAvailabilitySuffix(GetMetadataLoggingContext(
                        std::vector<CreditCard>{selected_card})),
                    ".", issuer_id_suffix}),
      latency);
}

}  // namespace autofill::autofill_metrics
