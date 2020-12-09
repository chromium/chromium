// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_APPS_TRACKER_H_
#define COMPONENTS_ARC_TEST_FAKE_APPS_TRACKER_H_

#include "base/callback.h"
#include "components/arc/enterprise/arc_apps_tracker.h"

namespace arc {
namespace data_snapshotd {

// Fake implementation of ArcAppsTracker for tests.
class FakeAppsTracker : public ArcAppsTracker {
 public:
  FakeAppsTracker();
  FakeAppsTracker(const FakeAppsTracker&) = delete;
  FakeAppsTracker& operator=(const FakeAppsTracker&) = delete;
  ~FakeAppsTracker() override;

  // ArcAppsTracker overrides:
  void StartTracking(
      base::RepeatingCallback<void(int)> update_callback) override;
  void StopTracking() override;

  base::RepeatingCallback<void(int)>& update_callback() {
    return update_callback_;
  }
  int start_tracking_num() const { return start_tracking_num_; }
  int stop_tracking_num() const { return stop_tracking_num_; }

 private:
  int start_tracking_num_ = 0;
  int stop_tracking_num_ = 0;
  base::RepeatingCallback<void(int)> update_callback_;
};

}  // namespace data_snapshotd
}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_APPS_TRACKER_H_
