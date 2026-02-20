// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_metrics.h"

#include <cmath>

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom.h"
#include "net/base/net_errors.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace action_chips {
namespace {
constexpr int32_t kMaxSuggestions = 11;
}

void RecordActionChipsRequestStatus(
    const RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult& result) {
  if (result.has_value()) {
    base::UmaHistogramEnumeration("NewTabPage.ActionChips.RequestStatus",
                                  ActionChipsRequestStatus::kSuccess);
    base::UmaHistogramExactLinear("NewTabPage.ActionChips.SuggestionCount",
                                  result->size(), kMaxSuggestions);
    return;
  }

  std::visit(absl::Overload{
                 [](const RemoteSuggestionsServiceSimple::NetworkError& error) {
                   if (error.net_error != net::OK) {
                     base::UmaHistogramEnumeration(
                         "NewTabPage.ActionChips.RequestStatus",
                         ActionChipsRequestStatus::kNetworkError);
                   } else {
                     base::UmaHistogramEnumeration(
                         "NewTabPage.ActionChips.RequestStatus",
                         ActionChipsRequestStatus::kHttpError);
                   }
                   base::UmaHistogramSparse(
                       "NewTabPage.ActionChips.RequestStatus.NetworkError",
                       std::abs(error.net_error));
                 },
                 [](const RemoteSuggestionsServiceSimple::ParseError& error) {
                   base::UmaHistogramEnumeration(
                       "NewTabPage.ActionChips.RequestStatus",
                       ActionChipsRequestStatus::kParseError);
                   base::UmaHistogramEnumeration(
                       "NewTabPage.ActionChips.RequestStatus.ParseError",
                       error.parse_failure_reason);
                 }},
             result.error());
}

void RecordImpressionMetrics(
    base::span<const action_chips::mojom::ActionChipPtr> chips) {
  for (const auto& chip : chips) {
    base::UmaHistogramEnumeration("NewTabPage.ActionChips.Shown2",
                                  chip->suggest_template_info->type_icon);
  }
}

void RecordActionChipsAnyShown(bool any_shown) {
  base::UmaHistogramBoolean("NewTabPage.ActionChips.AnyShown", any_shown);
}

// Helper method to record latency metrics for action chips retrieval.
void RecordActionChipsRetrievalLatencyMetrics(base::TimeDelta latency) {
  base::UmaHistogramTimes(
      "NewTabPage.ActionChips.Handler.ActionChipsRetrievalLatency", latency);
}

}  // namespace action_chips
