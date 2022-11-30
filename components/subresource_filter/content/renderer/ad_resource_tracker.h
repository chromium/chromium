// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_AD_RESOURCE_TRACKER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_AD_RESOURCE_TRACKER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace subresource_filter {

// This class tracks observers of resources that have been identified as ads in
// blink.
class AdResourceTracker {
 public:
  AdResourceTracker();
  ~AdResourceTracker();

  // Observes resource loads that are identified as ad resources.
  class Observer : public base::CheckedObserver {
   public:
    // This method is called when the subresource filter is notified of a
    // new resource that is tagged as an ad.
    virtual void OnAdResourceObserved(int request_id) = 0;

    // Called before the AdResourceTracker is destroyed. Observers must
    // unregister themselves by this point.
    virtual void OnAdResourceTrackerGoingAway() = 0;
  };

  // Add an observer that will listen for ad resource request ids.
  void AddObserver(Observer* ad_resource_observer);
  void RemoveObserver(Observer* ad_resource_observer);

  // Report the observed request_id as an ad resource.
  void NotifyAdResourceObserved(int request_id);

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_AD_RESOURCE_TRACKER_H_
