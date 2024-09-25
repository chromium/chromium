// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/metrics/discounts_metric_collector.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/metrics/metrics_utils.h"

namespace commerce::metrics {

void DiscountsMetricCollector::RecordDiscountsBubbleCopyButtonClicked(
    ukm::SourceId ukm_source_id) {
  base::RecordAction(base::UserMetricsAction(
      "Commerce.Discounts.DiscountsBubbleCopyButtonClicked"));
  RecordShoppingActionUKM(ukm_source_id, ShoppingAction::kDiscountCopied);
}

void DiscountsMetricCollector::DiscountsBubbleCopyStatusOnBubbleClosed(
    bool is_copy_button_clicked,
    const std::vector<DiscountInfo>& discounts) {
  base::UmaHistogramBoolean(
      "Commerce.Discounts.DiscountsBubbleCouponCodeIsCopied",
      is_copy_button_clicked);

  if (is_copy_button_clicked && commerce::kDiscountOnShoppyPage.Get()) {
    base::UmaHistogramEnumeration(
        "Commerce.Discounts.DiscountsBubble.TypeOnCopy",
        discounts[0].cluster_type);
  }
}

void DiscountsMetricCollector::RecordDiscountsPageActionIconExpandState(
    bool is_expanded,
    const std::vector<DiscountInfo>& discounts) {
  if (is_expanded) {
    base::RecordAction(base::UserMetricsAction(
        "Commerce.Discounts.DiscountsPageActionIcon.Expanded"));
  } else {
    base::RecordAction(base::UserMetricsAction(
        "Commerce.Discounts.DiscountsPageActionIcon.NotExpanded"));
  }
  base::UmaHistogramBoolean(
      "Commerce.Discounts.DiscountsPageActionIconIsExpanded", is_expanded);

  if (commerce::kDiscountOnShoppyPage.Get()) {
    base::UmaHistogramEnumeration(
        "Commerce.Discounts.PageActionIcon.TypeOnShown",
        discounts[0].cluster_type);
  }
}

void DiscountsMetricCollector::RecordDiscountsPageActionIconClicked(
    bool is_expanded,
    const std::vector<DiscountInfo>& discounts) {
  base::RecordAction(base::UserMetricsAction(
      "Commerce.Discounts.DiscountsPageActionIcon.Clicked"));

  base::UmaHistogramBoolean(
      "Commerce.Discounts.DiscountsPageActionIconIsExpandedWhenClicked",
      is_expanded);

  if (commerce::kDiscountOnShoppyPage.Get()) {
    base::UmaHistogramEnumeration(
        "Commerce.Discounts.PageActionIcon.TypeOnClick",
        discounts[0].cluster_type);
  }
}

void DiscountsMetricCollector::RecordDiscountBubbleShown(
    bool is_auto_shown,
    ukm::SourceId ukm_source_id,
    const std::vector<DiscountInfo>& discounts) {
  base::UmaHistogramBoolean("Commerce.Discounts.DiscountsBubbleIsAutoShown",
                            is_auto_shown);

  if (commerce::kDiscountOnShoppyPage.Get()) {
    base::UmaHistogramEnumeration(
        "Commerce.Discounts.DiscountBubble.TypeOnShow",
        discounts[0].cluster_type);
  }

  if (is_auto_shown) {
    base::RecordAction(base::UserMetricsAction(
        "Commerce.Discounts.DiscountsBubble.AutoShown"));
  } else {
    RecordShoppingActionUKM(ukm_source_id, ShoppingAction::kDiscountOpened);
  }
}

}  // namespace commerce::metrics
