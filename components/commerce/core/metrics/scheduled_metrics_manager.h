// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_METRICS_SCHEDULED_METRICS_MANAGER_H_
#define COMPONENTS_COMMERCE_CORE_METRICS_SCHEDULED_METRICS_MANAGER_H_

#include <memory>

#include "base/cancelable_callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

class PrefService;

namespace commerce {
class ShoppingService;
}  // namespace commerce

namespace commerce::metrics {

extern const char kPriceNotificationEmailHistogramName[];
extern const char kTrackedProductCountHistogramName[];

// Possible options for the state of the price notification emails setting. This
// enum needs to match the values in enums.xml.
enum class PriceNotificationEmailState {
  kEnabled = 0,
  kDisabled = 1,

  // This value is used if a user has not yet tracked a product.
  kNotResponded = 2,

  // This enum must be last and is only used for histograms.
  kMaxValue = kNotResponded
};

class ScheduledMetricsManager {
 public:
  ScheduledMetricsManager(PrefService* prefs,
                          ShoppingService* shopping_service);
  ScheduledMetricsManager(const ScheduledMetricsManager&) = delete;
  ScheduledMetricsManager& operator=(const ScheduledMetricsManager&) = delete;
  ~ScheduledMetricsManager();

 private:
  void RunDailyTask();

  raw_ptr<PrefService> pref_service_;
  raw_ptr<ShoppingService> shopping_service_;

  // Keep track of the last run time in memory in case there is a failure in
  // the pref service.
  base::Time daily_last_run_;
  std::unique_ptr<base::CancelableRepeatingClosure> daily_scheduled_task_;

  base::WeakPtrFactory<ScheduledMetricsManager> weak_ptr_factory_{this};
};

}  // namespace commerce::metrics

#endif  // COMPONENTS_COMMERCE_CORE_METRICS_SCHEDULED_METRICS_MANAGER_H_
