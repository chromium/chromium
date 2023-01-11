// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_DOMAIN_MIXING_METRICS_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_DOMAIN_MIXING_METRICS_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"

namespace history {

constexpr int kOneDay = 1;
constexpr int kOneWeek = 7;
constexpr int kTwoWeeks = 14;
constexpr int kOneMonth = 30;

// Emits domain mixing metrics given a list of domain visits and the start of
// the first day to compute metrics for.
//
// See http://goto.google.com/chrome-no-searchdomaincheck for more details on
// what domain mixing metrics are and how they are computed.
//
// The domain_visits vector is expected to contain exactly all the domain visits
// made by the user from start_of_first_day_to_emit - 29 days (to compute the 30
// day domain mixing metric for the first day) until the end of the last day to
// compute metrics for.
//
// This method wraps the business logic required to compute domain mixing
// metrics and is exposed for testing purposes to decouple testing the logic
// that computes the metrics from the database logic.
void EmitDomainMixingMetrics(const std::vector<DomainVisit>& domain_visits,
                             base::Time start_of_first_day_to_emit);

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_DOMAIN_MIXING_METRICS_H_
