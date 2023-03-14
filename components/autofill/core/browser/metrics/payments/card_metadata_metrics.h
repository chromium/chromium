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
constexpr char kAmericanExpress[] = "Amex";
constexpr char kCapitalOne[] = "CapitalOne";
constexpr char kChase[] = "Chase";
constexpr char kMarqeta[] = "Marqeta";

constexpr char kProductNameAndArtImageBothShownSuffix[] =
    "ProductDescriptionAndArtImageShown";
constexpr char kProductNameShownOnlySuffix[] = "ProductDescriptionShown";
constexpr char kArtImageShownOnlySuffix[] = "ArtImageShown";
constexpr char kProductNameAndArtImageNotShownSuffix[] = "MetadataNotShown";

// Struct that groups some metadata related information together. Used for
// metrics logging.
struct CardMetadataLoggingContext {
  bool IsCardMetadataShown() const {
    return card_product_description_shown || card_art_image_shown;
  }

  bool card_metadata_available = false;
  bool card_product_description_shown = false;
  bool card_art_image_shown = false;
};

// Get the CardMetadataLoggingContext for the given credit cards.
CardMetadataLoggingContext GetMetadataLoggingContext(
    const std::vector<CreditCard>& cards);

// Log the latency between suggestions being shown and a suggestion was
// selected, in milliseconds, and it is broken down by metadata availability
// provided by the `suggestion_context`.
void LogAcceptanceLatency(base::TimeDelta latency,
                          const CardMetadataLoggingContext& suggestion_context,
                          const CreditCard& selected_card);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARD_METADATA_METRICS_H_
