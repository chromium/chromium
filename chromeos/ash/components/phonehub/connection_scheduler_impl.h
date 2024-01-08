// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_CONNECTION_SCHEDULER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_CONNECTION_SCHEDULER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/components/phonehub/connection_scheduler.h"
#include "chromeos/ash/components/phonehub/feature_status.h"
#include "chromeos/ash/components/phonehub/feature_status_provider.h"
#include "chromeos/ash/components/phonehub/phone_hub_structured_metrics_logger.h"
#include "net/base/backoff_entry.h"

namespace ash {

namespace secure_channel {
class ConnectionManager;
}

namespace phonehub {

// ConnectionScheduler implementation that schedules calls to ConnectionManager
// in order to establish a connection to the user's phone.
class ConnectionSchedulerImpl : public ConnectionScheduler,
                                public FeatureStatusProvider::Observer {
 public:
  ConnectionSchedulerImpl(
      secure_channel::ConnectionManager* connection_manager,
      FeatureStatusProvider* feature_status_provider,
      PhoneHubStructuredMetricsLogger* phone_hub_structured_metrics_logger);
  ~ConnectionSchedulerImpl() override;

  void ScheduleConnectionNow(DiscoveryEntryPoint entry_point) override;

 private:
  friend class ConnectionSchedulerImplTest;

  // FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override;

  // Invalidate all pending backoff attempts and disconnects the current
  // connection attempt.
  void DisconnectAndClearBackoffAttempts();

  void ClearBackoffAttempts();

  // Test only functions.
  base::TimeDelta GetCurrentBackoffDelayTimeForTesting();
  int GetBackoffFailureCountForTesting();

  raw_ptr<secure_channel::ConnectionManager> connection_manager_;
  raw_ptr<FeatureStatusProvider> feature_status_provider_;
  raw_ptr<PhoneHubStructuredMetricsLogger> phone_hub_structured_metrics_logger_;
  // Provides us the backoff timers for RequestConnection().
  net::BackoffEntry retry_backoff_;
  FeatureStatus current_feature_status_;
  base::WeakPtrFactory<ConnectionSchedulerImpl> weak_ptr_factory_{this};
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_CONNECTION_SCHEDULER_IMPL_H_
