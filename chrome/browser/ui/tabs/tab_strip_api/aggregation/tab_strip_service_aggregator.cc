// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/aggregation/tab_strip_service_aggregator.h"

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service.h"

namespace tabs_api {

TabStripServiceAggregator::TabStripServiceAggregator(
    std::unique_ptr<TabStripServiceTracker> tracker,
    OnTabEventsCallback on_events_callback)
    : tracker_(std::move(tracker)),
      on_events_callback_(std::move(on_events_callback)) {
  tracker_->SetOnAddedCallback(base::BindRepeating(
      &TabStripServiceAggregator::OnServiceAdded, base::Unretained(this)));
  tracker_->SetOnRemovedCallback(base::BindRepeating(
      &TabStripServiceAggregator::OnServiceRemoved, base::Unretained(this)));
  for (TabStripService* service : tracker_->GetExistingServices()) {
    OnServiceAdded(service);
  }
}

TabStripServiceAggregator::~TabStripServiceAggregator() {
  tracker_->SetOnAddedCallback(base::NullCallback());
  tracker_->SetOnRemovedCallback(base::NullCallback());
  for (auto service : observed_services_) {
    service->RemoveObserver(this);
  }
  observed_services_.clear();
}

void TabStripServiceAggregator::OnServiceAdded(TabStripService* service) {
  if (observed_services_.contains(service)) {
    return;
  }

  service->AddObserver(this);
  observed_services_.insert(service);
}

void TabStripServiceAggregator::OnServiceRemoved(TabStripService* service) {
  if (observed_services_.erase(service)) {
    service->RemoveObserver(this);
  }
}

void TabStripServiceAggregator::OnTabEvents(
    const std::vector<mojom::TabsEventPtr>& events) {
  if (on_events_callback_) {
    on_events_callback_.Run(events);
  }
}

}  // namespace tabs_api
