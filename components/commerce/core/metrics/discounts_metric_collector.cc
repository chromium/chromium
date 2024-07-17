// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/metrics/discounts_metric_collector.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/commerce/core/metrics/metrics_utils.h"

namespace commerce::metrics {

void DiscountsMetricCollector::RecordDiscountsBubbleCopyButtonClicked(
    ukm::SourceId ukm_source_id) {
  base::RecordAction(base::UserMetricsAction(
      "Commerce.Discounts.DiscountsBubbleCopyButtonClicked"));
  RecordShoppingActionUKM(ukm_source_id, ShoppingAction::kDiscountCopied);
}

void DiscountsMetricCollector::DiscountsBubbleCopyStatusOnBubbleClosed(
    bool is_copy_button_clicked) {
  base::UmaHistogramBoolean(
      "Commerce.Discounts.DiscountsBubbleCouponCodeIsCopied",
      is_copy_button_clicked);
}

void DiscountsMetricCollector::RecordDiscountsPageActionIconExpandState(
    bool is_expanded) {
  if (is_expanded) {
    base::RecordAction(base::UserMetricsAction(
        "Commerce.Discounts.DiscountsPageActionIcon.Expanded"));
  } else {
    base::RecordAction(base::UserMetricsAction(
        "Commerce.Discounts.DiscountsPageActionIcon.NotExpanded"));
  }
  base::UmaHistogramBoolean(
      "Commerce.Discounts.DiscountsPageActionIconIsExpanded", is_expanded);
}

void DiscountsMetricCollector::RecordDiscountsPageActionIconClicked(
    bool is_expanded) {
  base::RecordAction(base::UserMetricsAction(
      "Commerce.Discounts.DiscountsPageActionIcon.Clicked"));

  base::UmaHistogramBoolean(
      "Commerce.Discounts.DiscountsPageActionIconIsExpandedWhenClicked",
      is_expanded);
}

void DiscountsMetricCollector::RecordDiscountBubbleShown(
    bool is_auto_shown,
    ukm::SourceId ukm_source_id) {
  base::UmaHistogramBoolean("Commerce.Discounts.DiscountsBubbleIsAutoShown",
                            is_auto_shown);

  if (is_auto_shown) {
    base::RecordAction(base::UserMetricsAction(
        "Commerce.Discounts.DiscountsBubble.AutoShown"));
  } else {
    RecordShoppingActionUKM(ukm_source_id, ShoppingAction::kDiscountOpened);
  }
}

}  // namespace commerce::metrics
