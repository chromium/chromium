// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_COMMERCE_HEURISTICS_DATA_METRICS_HELPER_H_
#define COMPONENTS_COMMERCE_CORE_COMMERCE_HEURISTICS_DATA_METRICS_HELPER_H_

class CommerceHeuristicsDataMetricsHelper {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. Represent the source of commerce
  // heuristics.
  enum class HeuristicsSource {
    // Heuristics are from component updater.
    FROM_COMPONENT = 0,
    // Heuristics are from resources.
    FROM_RESOURCE = 1,
    // Heuristics are from feature parameter.
    FROM_FEATURE_PARAMETER = 2,
    // Heuristics are missing.
    MISSING = 3,
    kMaxValue = MISSING,
  };

  // Gets called when we try to get merchant name for a domain to record the
  // source of this name data.
  static void RecordMerchantNameSource(HeuristicsSource source);

  // Gets called when we try to get general checkout URL detection pattern to
  // record the source of this pattern data.
  static void RecordCheckoutURLGeneralPatternSource(HeuristicsSource source);

  // Gets called when we try to get the cart extraction script to record the
  // source of this script data.
  static void RecordCartExtractionScriptSource(HeuristicsSource source);

  // Gets called when we try to get the pattern to decide which merchant is a
  // partner merchant. Record the source of the pattern data.
  static void RecordPartnerMerchantPatternSource(HeuristicsSource source);

  // Gets called when we try to get the pattern to decide whether we should
  // skip a product from the extraction results. Record the source of the
  // pattern data.
  static void RecordSkipProductPatternSource(HeuristicsSource source);

  // Gets called when we try to get the product ID extraction pattern. Record
  // the source of the pattern data.
  static void RecordProductIDExtractionPatternSource(HeuristicsSource source);
};

#endif  // COMPONENTS_COMMERCE_CORE_COMMERCE_HEURISTICS_DATA_METRICS_HELPER_H_
