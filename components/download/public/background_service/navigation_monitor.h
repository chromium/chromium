// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_NAVIGATION_MONITOR_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_NAVIGATION_MONITOR_H_

#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"

namespace download {

// Enum used to describe navigation event from WebContentsObserver.
enum class NavigationEvent {
  START_NAVIGATION = 0,
  NAVIGATION_COMPLETE = 1,
};

// NavigationMonitor receives and forwards navigation events from any web
// contents to download service.
//
// NavigationMonitor outlives any WebContentsObserver that send navigation
// events to it.
//
// NavigationMonitor does NOT has ownership of WebContentsObserver, and is
// essentially a decoupled singleton that glues download service with
// WebContents and WebContentsObserver.
class NavigationMonitor : public KeyedService {
 public:
  // Used to propagates the navigation events.
  class Observer {
   public:
    virtual void OnNavigationEvent() = 0;
    virtual ~Observer() = default;
  };

  // Start to listen to navigation event.
  virtual void SetObserver(NavigationMonitor::Observer* observer) = 0;

  // Called when navigation events happen.
  virtual void OnNavigationEvent(NavigationEvent event) = 0;

  // Returns whether the system is idle or busy with navigations.
  virtual bool IsNavigationInProgress() const = 0;

  // Called to configure various delays used before notifying the observers.
  // |navigation_completion_delay| is the amount of delay during which if there
  // is no navigation activity, the system is considered to be idle.
  // |navigation_timeout_delay| is the maximum timeout starting from the
  // beginning of a navigation after which the navigation is considered to be
  // lost so that download service can resume.
  virtual void Configure(base::TimeDelta navigation_completion_delay,
                         base::TimeDelta navigation_timeout_delay) = 0;

 protected:
  ~NavigationMonitor() override = default;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_NAVIGATION_MONITOR_H_
