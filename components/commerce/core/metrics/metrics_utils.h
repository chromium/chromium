// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_METRICS_METRICS_UTILS_H_
#define COMPONENTS_COMMERCE_CORE_METRICS_METRICS_UTILS_H_

#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/prefs/pref_service.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class GURL;

namespace commerce {
class AccountChecker;
}  // namespace commerce

namespace commerce::metrics {

extern const char kPDPNavShoppingListEligibleHistogramName[];
extern const char kPDPStateHistogramName[];
extern const char kPDPStateWithLocalMetaName[];
extern const char kShoppingListIneligibleHistogramName[];

// Possible options for the state of a product details page (PDP). These must be
// kept in sync with the values in enums.xml.
enum class ShoppingPDPState {
  kNotPDP = 0,

  // The cluster ID is used to identify a product that is not specific to a
  // particular merchant (i.e. many merchants sell this). This is the
  // counterpart to offer ID which identifies a product for a specific merchant.
  kIsPDPWithoutClusterId = 1,
  kIsPDPWithClusterId = 2,

  // This enum must be last and is only used for histograms.
  kMaxValue = kIsPDPWithClusterId
};

// The possible ways a product details page (PDP) can be detected. These must be
// kept in sync with the values in enums.xml.
enum class ShoppingPDPDetectionMethod {
  kNotPDP = 0,
  kPDPServerOnly = 1,
  kPDPLocalMetaOnly = 2,
  kPDPServerAndLocalMeta = 3,

  // This enum must be last and is only used for histograms.
  kMaxValue = kPDPServerAndLocalMeta
};

// Reasons why a user may be ineligible for a particular feature. These must be
// kept in sync with the values in enums.xml.
enum class ShoppingFeatureIneligibilityReason {
  kOther = 0,
  kUnsupportedCountryOrLocale = 1,
  kEnterprisePolicy = 2,
  kSignin = 3,
  kSync = 4,
  // Make search and browsing better.
  kMSBB = 5,
  // Web and app activity.
  kWAA = 6,
  kParentalControls = 7,

  // This enum must be last and is only used for histograms.
  kMaxValue = kParentalControls
};

// The possible actions that user can take on a shopping page. These must be
// kept in sync with Shopping.ShoppingActions in ukm.xml.
enum class ShoppingAction {
  kDiscountCopied = 0,
  kDiscountOpened = 1,
  kPriceInsightsOpened = 2,
  kPriceTracked = 3,
};

// Shopping features that are contextual. These must be kept in sync with the
// values in enums.xml.
enum class ShoppingContextualFeature {
  kPriceTracking = 0,
  kPriceInsights = 1,
  kDiscounts = 2,
};

// Record the state of a PDP for a navigation.
void RecordPDPMetrics(optimization_guide::OptimizationGuideDecision decision,
                      const optimization_guide::OptimizationMetadata& metadata,
                      PrefService* pref_service,
                      bool is_off_the_record,
                      bool is_shopping_list_eligible,
                      const GURL& url);

// Record how a PDP was detected.
void RecordPDPStateWithLocalMeta(bool detected_by_server,
                                 bool detected_by_client,
                                 ukm::SourceId source_id);

// Record reasons why a user was ineligible for the shopping list feature.
void RecordShoppingListIneligibilityReasons(PrefService* pref_service,
                                            AccountChecker* account_checker,
                                            bool is_off_the_record,
                                            bool supported_country);

// Record UKM for shopping actions that users take.
void RecordShoppingActionUKM(ukm::SourceId ukm_source_id,
                             ShoppingAction action);

}  // namespace commerce::metrics

#endif  // COMPONENTS_COMMERCE_CORE_METRICS_METRICS_UTILS_H_
