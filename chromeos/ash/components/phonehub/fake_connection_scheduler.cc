// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/fake_connection_scheduler.h"

namespace ash {
namespace phonehub {

FakeConnectionScheduler::FakeConnectionScheduler() = default;
FakeConnectionScheduler::~FakeConnectionScheduler() = default;

void FakeConnectionScheduler::ScheduleConnectionNow(
    DiscoveryEntryPoint entry_point) {
  ++num_schedule_connection_now_calls_;
}

}  // namespace phonehub
}  // namespace ash
