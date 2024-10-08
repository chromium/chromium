// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/shared/browser/page_load_statistics.h"

#include <string_view>

#include "base/check.h"
#include "base/debug/crash_logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/strings/strcat.h"
#include "components/subresource_filter/core/common/time_measurements.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

namespace subresource_filter {

PageLoadStatistics::PageLoadStatistics(
    const mojom::ActivationState& state,
    std::string_view uma_filter_tag,
    content::NavigationHandle* navigation_handle,
    content::RenderFrameHost* frame_host)
    : activation_state_(state),
      uma_filter_tag_(uma_filter_tag),
      details_for_crash_keys_(DetailsForCrashKeys{
          .navigation_url = navigation_handle->GetURL(),
          .navigation_error_code = navigation_handle->GetNetErrorCode(),
          .has_navigation_committed = navigation_handle->HasCommitted(),
          .frame_last_commited_url = frame_host->GetLastCommittedURL()}) {}

PageLoadStatistics::PageLoadStatistics(const mojom::ActivationState& state,
                                       std::string_view uma_filter_tag)
    : activation_state_(state), uma_filter_tag_(uma_filter_tag) {}

PageLoadStatistics::~PageLoadStatistics() = default;

void PageLoadStatistics::OnDocumentLoadStatistics(
    const mojom::DocumentLoadStatistics& statistics) {
  // Note: Chances of overflow are negligible.
  aggregated_document_statistics_.num_loads_total += statistics.num_loads_total;
  aggregated_document_statistics_.num_loads_evaluated +=
      statistics.num_loads_evaluated;
  aggregated_document_statistics_.num_loads_matching_rules +=
      statistics.num_loads_matching_rules;
  aggregated_document_statistics_.num_loads_disallowed +=
      statistics.num_loads_disallowed;

  // TODO(crbug.com/341798380): These CHECKs and crash keys are temporary for
  // narrowing down an issue. Remove once fixed.
  if (!activation_state_.measure_performance && details_for_crash_keys_) {
    SCOPED_CRASH_KEY_STRING1024(
        "bug341798380", "navigation-url",
        details_for_crash_keys_->navigation_url.possibly_invalid_spec());
    SCOPED_CRASH_KEY_STRING256(
        "bug341798380", "navigation-error-code",
        net::ErrorToString(details_for_crash_keys_->navigation_error_code));
    SCOPED_CRASH_KEY_BOOL("bug341798380", "navigation-has-committed",
                          details_for_crash_keys_->has_navigation_committed);
    SCOPED_CRASH_KEY_STRING1024("bug341798380", "last-committed-url",
                                details_for_crash_keys_->frame_last_commited_url
                                    .possibly_invalid_spec());
    SCOPED_CRASH_KEY_NUMBER(
        "bug341798380", "activation-level",
        static_cast<int>(activation_state_.activation_level));
    SCOPED_CRASH_KEY_BOOL("bug341798380", "filtering-disabled",
                          activation_state_.filtering_disabled_for_document);
    SCOPED_CRASH_KEY_BOOL("bug341798380", "generic-rules-disabled",
                          activation_state_.generic_blocking_rules_disabled);

    CHECK(statistics.evaluation_total_wall_duration.is_zero(),
          base::NotFatalUntil::M132);
    CHECK(statistics.evaluation_total_cpu_duration.is_zero(),
          base::NotFatalUntil::M132);
  }
  aggregated_document_statistics_.evaluation_total_wall_duration +=
      statistics.evaluation_total_wall_duration;
  aggregated_document_statistics_.evaluation_total_cpu_duration +=
      statistics.evaluation_total_cpu_duration;
}

void PageLoadStatistics::OnDidFinishLoad() {
  if (activation_state_.activation_level != mojom::ActivationLevel::kDisabled) {
    base::UmaHistogramCounts1000(
        base::StrCat({uma_filter_tag_, ".PageLoad.NumSubresourceLoads.Total"}),
        aggregated_document_statistics_.num_loads_total);
    base::UmaHistogramCounts1000(
        base::StrCat(
            {uma_filter_tag_, ".PageLoad.NumSubresourceLoads.Evaluated"}),
        aggregated_document_statistics_.num_loads_evaluated);
    base::UmaHistogramCounts1000(
        base::StrCat(
            {uma_filter_tag_, ".PageLoad.NumSubresourceLoads.MatchedRules"}),
        aggregated_document_statistics_.num_loads_matching_rules);
    base::UmaHistogramCounts1000(
        base::StrCat(
            {uma_filter_tag_, ".PageLoad.NumSubresourceLoads.Disallowed"}),
        aggregated_document_statistics_.num_loads_disallowed);
  }

  if (activation_state_.measure_performance) {
    CHECK(
        activation_state_.activation_level != mojom::ActivationLevel::kDisabled,
        base::NotFatalUntil::M129);
    base::UmaHistogramCustomTimes(
        base::StrCat({uma_filter_tag_,
                      ".PageLoad.SubresourceEvaluation.TotalWallDuration"}),
        aggregated_document_statistics_.evaluation_total_wall_duration,
        base::Microseconds(1), base::Seconds(10), 50);

    base::UmaHistogramCustomTimes(
        base::StrCat({uma_filter_tag_,
                      ".PageLoad.SubresourceEvaluation.TotalCPUDuration"}),
        aggregated_document_statistics_.evaluation_total_cpu_duration,
        base::Microseconds(1), base::Seconds(10), 50);
  } else {
    CHECK(aggregated_document_statistics_.evaluation_total_wall_duration
              .is_zero(),
          base::NotFatalUntil::M132);
    CHECK(
        aggregated_document_statistics_.evaluation_total_cpu_duration.is_zero(),
        base::NotFatalUntil::M132);
  }
}

}  // namespace subresource_filter
