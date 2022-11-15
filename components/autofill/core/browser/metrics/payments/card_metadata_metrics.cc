// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/card_metadata_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace autofill::autofill_metrics {

void LogCardSuggestionAcceptanceLatencyMetric(
    base::TimeDelta latency,
    const CardMetadataLoggingContext& context) {
  std::string histogram_name =
      "Autofill.CreditCard.SuggestionAcceptanceLatencySinceShown";

  if (context.card_product_description_shown && context.card_art_image_shown)
    histogram_name += ".ProductDescriptionAndArtImageShown";
  else if (context.card_product_description_shown)
    histogram_name += ".ProductDescriptionShown";
  else if (context.card_art_image_shown)
    histogram_name += ".ArtImageShown";
  else
    histogram_name += ".MetadataNotShown";

  base::UmaHistogramMediumTimes(histogram_name, latency);
}

}  // namespace autofill::autofill_metrics
