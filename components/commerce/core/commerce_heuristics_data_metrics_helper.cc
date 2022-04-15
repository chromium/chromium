// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/commerce_heuristics_data_metrics_helper.h"

#include "base/metrics/histogram_functions.h"

void CommerceHeuristicsDataMetricsHelper::RecordMerchantNameSource(
    HeuristicsSource source) {
  base::UmaHistogramEnumeration("Commerce.Heuristics.MerchantNameSource",
                                source);
}

void CommerceHeuristicsDataMetricsHelper::RecordCheckoutURLGeneralPatternSource(
    HeuristicsSource source) {
  base::UmaHistogramEnumeration(
      "Commerce.Heuristics.CheckoutURLGeneralPatternSource", source);
}

void CommerceHeuristicsDataMetricsHelper::RecordCartExtractionScriptSource(
    HeuristicsSource source) {
  base::UmaHistogramEnumeration(
      "Commerce.Heuristics.CartExtractionScriptSource", source);
}
