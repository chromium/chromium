// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_METRICS_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_METRICS_DELEGATE_H_

#include "base/timer/timer.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_delegate.h"

namespace ash {

// A delegate of `HoldingSpaceKeyedService` tasked with recording metrics.
class HoldingSpaceMetricsDelegate : public HoldingSpaceKeyedServiceDelegate {
 public:
  HoldingSpaceMetricsDelegate(HoldingSpaceKeyedService* service,
                              HoldingSpaceModel* model);
  HoldingSpaceMetricsDelegate(const HoldingSpaceMetricsDelegate&) = delete;
  HoldingSpaceMetricsDelegate& operator=(const HoldingSpaceMetricsDelegate&) =
      delete;
  ~HoldingSpaceMetricsDelegate() override;

 private:
  // HoldingSpaceKeyedServiceDelegate:
  void OnPersistenceRestored() override;
  void OnHoldingSpaceItemsAdded(
      const std::vector<const HoldingSpaceItem*>& items) override;
  void OnHoldingSpaceItemsRemoved(
      const std::vector<const HoldingSpaceItem*>& items) override;

  // Schedules recording of the total counts of all holding space items in the
  // model, invalidating any previously scheduled recording. This is done to
  // give the model time to settle after being modified to debounce recordings.
  void RescheduleRecordTotalItemCounts();

  // Records the total counts of all holding space items in the model.
  void RecordTotalItemCounts();

  // Timer which invokes `RecordTotalItemCounts()` when fired.
  base::OneShotTimer record_total_item_counts_timer_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_METRICS_DELEGATE_H_
