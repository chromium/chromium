// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/net/time_sync_tracker.h"

namespace chromecast {

TimeSyncTracker::TimeSyncTracker() {}

TimeSyncTracker::~TimeSyncTracker() = default;

void TimeSyncTracker::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void TimeSyncTracker::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void TimeSyncTracker::Notify() {
  for (Observer& observer : observer_list_) {
    observer.OnTimeSynced();
  }
}

}  // namespace chromecast
