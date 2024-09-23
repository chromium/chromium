// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CHROMEOS_MAGIC_BOOST_MAGIC_BOOST_METRICS_H_
#define CHROME_BROWSER_UI_CHROMEOS_MAGIC_BOOST_MAGIC_BOOST_METRICS_H_

#include "chromeos/crosapi/mojom/magic_boost.mojom.h"

namespace chromeos::magic_boost {

using OptInFeatures = crosapi::mojom::MagicBoostController::OptInFeatures;

inline constexpr char kMagicBoostOptInCardHistogram[] =
    "ChromeOS.MagicBoost.OptInCard.";

// Please keep in sync with the `OptInCardAction` enum found in
// //tools/metrics/histograms/metadata/chromeos/enums.xml.
enum class OptInCardAction {
  kShowCard = 0,
  kAcceptButtonPressed = 1,
  kDeclineButtonPressed = 2,
  kMaxValue = kDeclineButtonPressed,
};

// Records metrics for user interactions with an opt-in card. This function
// tracks The specific action the user took on the card with the specific
// `OptInFeature` that triggered the card's display. What's more, it records the
// total number of times of actions using `OptInFeatures::kTotal` for overall
// tracking.
// For example:
//        ChromeOS.MagicBoost.OptInCard.OrcaAndHmr -> DeclineButtonPressed
//    Indicates the opt in card was shown due to both Orca and HMR features, and
//    the user clicked the declied button.
//        ChromeOS.MagicBoost.OptInCard.Total -> ShowCard
//    Records a overall showing times of the opt in card.
void RecordOptInCardActionMetrics(OptInFeatures opt_in_features,
                                  OptInCardAction action);

}  // namespace chromeos::magic_boost

#endif  // CHROME_BROWSER_UI_CHROMEOS_MAGIC_BOOST_MAGIC_BOOST_METRICS_H_
