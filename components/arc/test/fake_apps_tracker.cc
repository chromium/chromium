// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/fake_apps_tracker.h"

namespace arc {
namespace data_snapshotd {

FakeAppsTracker::FakeAppsTracker() = default;
FakeAppsTracker::~FakeAppsTracker() = default;

void FakeAppsTracker::StartTracking(
    base::RepeatingCallback<void(int)> update_callback) {
  start_tracking_num_++;
  update_callback_ = std::move(update_callback);
}

void FakeAppsTracker::StopTracking() {
  stop_tracking_num_++;
  update_callback_.Reset();
}

}  // namespace data_snapshotd
}  // namespace arc
