// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_metrics_delegate.h"

#include <vector>

#include "ash/public/cpp/holding_space/holding_space_metrics.h"

namespace ash {

HoldingSpaceMetricsDelegate::HoldingSpaceMetricsDelegate(
    HoldingSpaceKeyedService* service,
    HoldingSpaceModel* model)
    : HoldingSpaceKeyedServiceDelegate(service, model) {}

HoldingSpaceMetricsDelegate::~HoldingSpaceMetricsDelegate() {
  // Scheduled recordings should be immediately run on destruction so as to
  // prevent metrics loss.
  if (record_total_item_counts_timer_.IsRunning()) {
    record_total_item_counts_timer_.FireNow();
  }
}

void HoldingSpaceMetricsDelegate::OnPersistenceRestored() {
  RescheduleRecordTotalItemCounts();
}

void HoldingSpaceMetricsDelegate::OnHoldingSpaceItemsAdded(
    const std::vector<const HoldingSpaceItem*>& items) {
  if (!is_restoring_persistence())
    RescheduleRecordTotalItemCounts();
}

void HoldingSpaceMetricsDelegate::OnHoldingSpaceItemsRemoved(
    const std::vector<const HoldingSpaceItem*>& items) {
  if (!is_restoring_persistence())
    RescheduleRecordTotalItemCounts();
}

void HoldingSpaceMetricsDelegate::RescheduleRecordTotalItemCounts() {
  // NOTE: It is intentional that any previously scheduled recordings are
  // invalidated. This is done to give the model time to settle after being
  // modified to debounce recordings.
  record_total_item_counts_timer_.Start(
      FROM_HERE, base::Seconds(30),
      base::BindRepeating(&HoldingSpaceMetricsDelegate::RecordTotalItemCounts,
                          base::Unretained(this)));
}

void HoldingSpaceMetricsDelegate::RecordTotalItemCounts() {
  std::vector<const HoldingSpaceItem*> items;
  for (const auto& item : model()->items())
    items.push_back(item.get());
  holding_space_metrics::RecordTotalItemCounts(items);
}

}  // namespace ash
