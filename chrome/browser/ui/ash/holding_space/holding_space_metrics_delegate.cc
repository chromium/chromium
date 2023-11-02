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
  if (record_item_counts_timer_.IsRunning())
    record_item_counts_timer_.FireNow();
}

void HoldingSpaceMetricsDelegate::OnPersistenceRestored() {
  RescheduleRecordItemCounts();
}

void HoldingSpaceMetricsDelegate::OnHoldingSpaceItemsAdded(
    const std::vector<const HoldingSpaceItem*>& items) {
  if (!is_restoring_persistence())
    RescheduleRecordItemCounts();
}

void HoldingSpaceMetricsDelegate::OnHoldingSpaceItemsRemoved(
    const std::vector<const HoldingSpaceItem*>& items) {
  if (!is_restoring_persistence())
    RescheduleRecordItemCounts();
}

void HoldingSpaceMetricsDelegate::RescheduleRecordItemCounts() {
  // NOTE: It is intentional that any previously scheduled recordings are
  // invalidated. This is done to give the model time to settle after being
  // modified to debounce recordings.
  record_item_counts_timer_.Start(
      FROM_HERE, base::Seconds(30),
      base::BindRepeating(&HoldingSpaceMetricsDelegate::RecordItemCounts,
                          base::Unretained(this)));
}

void HoldingSpaceMetricsDelegate::RecordItemCounts() {
  std::vector<const HoldingSpaceItem*> items;
  for (const auto& item : model()->items())
    items.push_back(item.get());
  holding_space_metrics::RecordItemCounts(items);
}

}  // namespace ash
