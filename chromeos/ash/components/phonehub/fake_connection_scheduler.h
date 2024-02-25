// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_CONNECTION_SCHEDULER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_CONNECTION_SCHEDULER_H_

#include <stddef.h>

#include "chromeos/ash/components/phonehub/connection_scheduler.h"
#include "chromeos/ash/components/phonehub/phone_hub_structured_metrics_logger.h"

namespace ash {
namespace phonehub {

class FakeConnectionScheduler : public ConnectionScheduler {
 public:
  FakeConnectionScheduler();
  ~FakeConnectionScheduler() override;

  size_t num_schedule_connection_now_calls() const {
    return num_schedule_connection_now_calls_;
  }

 private:
  // ConnectionScheduler:
  void ScheduleConnectionNow(DiscoveryEntryPoint entry_point) override;

  size_t num_schedule_connection_now_calls_ = 0u;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_CONNECTION_SCHEDULER_H_
