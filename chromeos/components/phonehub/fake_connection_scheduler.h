// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_FAKE_CONNECTION_SCHEDULER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_FAKE_CONNECTION_SCHEDULER_H_

#include <memory>

#include "chromeos/components/phonehub/connection_scheduler.h"

namespace chromeos {
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
  void ScheduleConnectionNow() override;

  size_t num_schedule_connection_now_calls_;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_FAKE_CONNECTION_SCHEDULER_H_