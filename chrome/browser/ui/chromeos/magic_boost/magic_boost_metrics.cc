// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_metrics.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"

namespace chromeos::magic_boost {

// Please keep in sync with the `ChromeOS.MagicBoost.OptInCard.{OptInFeatures}`
// histogram name found in
// //tools/metrics/histograms/metadata/chromeos/histograms.xml.
void RecordOptInCardActionMetrics(OptInFeatures opt_in_features,
                                  OptInCardAction action) {
  std::string histogram_name = kMagicBoostOptInCardHistogram;
  auto total_histogram_name = histogram_name + "Total";
  switch (opt_in_features) {
    case OptInFeatures::kHmrOnly:
      histogram_name += "HmrOnly";
      break;
    case OptInFeatures::kOrcaAndHmr:
      histogram_name += "OrcaAndHmr";
      break;
    default:
      NOTREACHED();
  }

  base::UmaHistogramEnumeration(histogram_name, action);
  base::UmaHistogramEnumeration(total_histogram_name, action);
}

}  // namespace chromeos::magic_boost
