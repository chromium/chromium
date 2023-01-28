// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/card_metadata_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "components/autofill/core/common/autofill_payments_features.h"

namespace autofill::autofill_metrics {

namespace {
std::string GetHistogramSuffix(const CardMetadataLoggingContext& context) {
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
}  // namespace

CardMetadataLoggingContext GetMetadataLoggingContext(
    const std::vector<CreditCard*>& cards) {
  bool card_product_description_available = false;
  bool card_art_image_available = false;
  bool virtual_card_with_card_art_image = false;

  for (const auto* card : cards) {
    if (!card->product_description().empty()) {
      card_product_description_available = true;
    }

    if (card->card_art_url().is_valid()) {
      card_art_image_available = true;
      if (card->virtual_card_enrollment_state() ==
          CreditCard::VirtualCardEnrollmentState::ENROLLED) {
        virtual_card_with_card_art_image = true;
      }
    }
  }

  autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
  metadata_logging_context.card_metadata_available =
      card_product_description_available || card_art_image_available;

  metadata_logging_context.card_product_description_shown =
      card_product_description_available &&
      base::FeatureList::IsEnabled(features::kAutofillEnableCardProductName);

  // `card_art_image_shown` is set to true if art image is available and
  // 1. the experiment is enabled or
  // 2. the card with art image has a linked virtual card (for virtual cards,
  // the card art image is always shown if available).
  metadata_logging_context.card_art_image_shown =
      card_art_image_available &&
      (base::FeatureList::IsEnabled(features::kAutofillEnableCardArtImage) ||
       virtual_card_with_card_art_image);

  return metadata_logging_context;
}

void LogAcceptanceLatency(base::TimeDelta latency,
                          const CardMetadataLoggingContext& suggestion_context,
                          const CreditCard& selected_card) {
  if (!suggestion_context.card_metadata_available) {
    return;
  }
  std::string histogram_name_prefix =
      "Autofill.CreditCard.SelectionLatencySinceShown";
  base::UmaHistogramMediumTimes(histogram_name_prefix + ".AnyCardWithMetadata" +
                                    GetHistogramSuffix(suggestion_context),
                                latency);

  CreditCard duplicate = selected_card;
  auto selected_card_context = GetMetadataLoggingContext({&duplicate});
  if (!selected_card_context.card_metadata_available) {
    return;
  }

  base::UmaHistogramMediumTimes(histogram_name_prefix +
                                    ".SelectedCardWithMetadata" +
                                    GetHistogramSuffix(selected_card_context),
                                latency);

  if (!selected_card.issuer_id().empty()) {
    std::string issuer_id_string;
    if (selected_card.issuer_id() == "amex") {
      issuer_id_string = kAmericanExpress;
    } else if (selected_card.issuer_id() == "capitalone") {
      issuer_id_string = kCapitalOne;
    } else {
      NOTREACHED() << "Update logic when adding new issuers.";
      return;
    }
    base::UmaHistogramMediumTimes(
        histogram_name_prefix + ".SelectedCardWithMetadata" +
            GetHistogramSuffix(selected_card_context) + "." + issuer_id_string,
        latency);
  }
}

}  // namespace autofill::autofill_metrics
