// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARD_METADATA_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARD_METADATA_METRICS_H_

#include "components/autofill/core/browser/metrics/autofill_metrics.h"

namespace autofill::autofill_metrics {

constexpr char kAmericanExpress[] = "Amex";
constexpr char kCapitalOne[] = "CapitalOne";

constexpr char kProductNameAndArtImageBothShownSuffix[] =
    ".ProductDescriptionAndArtImageShown";
constexpr char kProductNameShownOnlySuffix[] = ".ProductDescriptionShown";
constexpr char kArtImageShownOnlySuffix[] = ".ArtImageShown";
constexpr char kProductNameAndArtImageNotShownSuffix[] = ".MetadataNotShown";

// Struct that groups some metadata related information together. Used for
// metrics logging.
struct CardMetadataLoggingContext {
  bool card_metadata_available = false;
  bool card_product_description_shown = false;
  bool card_art_image_shown = false;
};

// Get the CardMetadataLoggingContext for the given credit cards.
CardMetadataLoggingContext GetMetadataLoggingContext(
    const std::vector<CreditCard*>& cards);

// Log the latency between suggestions being shown and a suggestion was
// selected, in milliseconds.
void LogAcceptanceLatency(base::TimeDelta latency,
                          const CardMetadataLoggingContext& suggestion_context,
                          const CreditCard& selected_card);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARD_METADATA_METRICS_H_
