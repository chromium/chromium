// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/renderer/ad_resource_tracker.h"
#include "base/observer_list.h"

namespace subresource_filter {

AdResourceTracker::AdResourceTracker() {}

AdResourceTracker::~AdResourceTracker() {
  // Notify observers that the AdResourceTracker is being destroyed.
  for (auto& obs : observers_) {
    obs.OnAdResourceTrackerGoingAway();
  }
}

void AdResourceTracker::AddObserver(
    AdResourceTracker::Observer* ad_resource_observer) {
  observers_.AddObserver(ad_resource_observer);
}

void AdResourceTracker::RemoveObserver(
    AdResourceTracker::Observer* ad_resource_observer) {
  observers_.RemoveObserver(ad_resource_observer);
}

void AdResourceTracker::NotifyAdResourceObserved(int request_id) {
  for (auto& obs : observers_) {
    obs.OnAdResourceObserved(request_id);
  }
}

}  // namespace subresource_filter
