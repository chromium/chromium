// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_DEVICE_COUNT_METRICS_PROVIDER_H_
#define COMPONENTS_SYNC_DEVICE_INFO_DEVICE_COUNT_METRICS_PROVIDER_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "components/metrics/metrics_provider.h"

namespace syncer {

class DeviceInfoTracker;

// A registerable metrics provider that will emit the number of active syncing
// devices a user has upon UMA upload. When there are multiple active profiles
// that are aware of syncable devices, the largest count is used. This approach
// is useful for several reasons. We're significantly interested in "power"
// users that have multiple devices and profiles, and if we skipped emitting any
// metrics in multi profile cases, then we'd be unaware of this entire segment
// of the population. If we emitted multiple metrics, we would have much more
// noise and discerning which metrics actually belonged to non-syncing users
// would be much trickier.
class DeviceCountMetricsProvider : public metrics::MetricsProvider {
 public:
  using ProvideTrackersCallback = base::RepeatingCallback<void(
      std::vector<const DeviceInfoTracker*>* trackers)>;

  explicit DeviceCountMetricsProvider(
      const ProvideTrackersCallback& provide_trackers);

  DeviceCountMetricsProvider(const DeviceCountMetricsProvider&) = delete;
  DeviceCountMetricsProvider& operator=(const DeviceCountMetricsProvider&) =
      delete;

  ~DeviceCountMetricsProvider() override;

  // MetricsProvider:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

 private:
  // Returns the max number of active devices across all accounts.
  int MaxActiveDeviceCount() const;

  const ProvideTrackersCallback provide_trackers_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_DEVICE_COUNT_METRICS_PROVIDER_H_
