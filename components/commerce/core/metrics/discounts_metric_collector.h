// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_METRICS_DISCOUNTS_METRIC_COLLECTOR_H_
#define COMPONENTS_COMMERCE_CORE_METRICS_DISCOUNTS_METRIC_COLLECTOR_H_

#include <vector>

#include "components/commerce/core/commerce_types.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace commerce::metrics {
class DiscountsMetricCollector {
 public:
  // Record when the copy button in the DiscountsBubbleDialogView is clicked.
  static void RecordDiscountsBubbleCopyButtonClicked(
      ukm::SourceId ukm_source_id);

  // Records when the DiscountsBubbleDialogView is closed. This records the copy
  // button status when bubble is closed.
  static void DiscountsBubbleCopyStatusOnBubbleClosed(
      bool is_copy_button_clicked,
      const std::vector<DiscountInfo>& discounts);

  // Record when all shopping related page action icons finished computing which
  // icon to expand.
  static void RecordDiscountsPageActionIconExpandState(
      bool is_expanded,
      const std::vector<DiscountInfo>& discounts);

  // Record when the Discounts page action icon is clicked.
  static void RecordDiscountsPageActionIconClicked(
      bool is_expanded,
      const std::vector<DiscountInfo>& discounts);

  // Record when the discounts bubble is shown.
  static void RecordDiscountBubbleShown(
      bool is_auto_shown,
      ukm::SourceId ukm_source_id,
      const std::vector<DiscountInfo>& discounts);
};
}  // namespace commerce::metrics

#endif  // COMPONENTS_COMMERCE_CORE_METRICS_DISCOUNTS_METRIC_COLLECTOR_H_
